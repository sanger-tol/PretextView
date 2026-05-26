/*
Copyright (c) 2026 Wellcome Sanger Institute
author: Shaoheng Guan, sg3@sanger.ac.uk, Wellcome Sanger Institute
translated by Yumi Sims, yy5@sanger.ac.uk, Wellcome Sanger Institute

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "Header.h"


#define Number_Of_Texture_Buffers_Per_Queue 8
#define Number_Of_Texture_Buffer_Queues 8

// 
struct texture_buffer 
{
    u08 *texture;                            // decompressed texture
    u08 *compressionBuffer;                  // compressed texture
    libdeflate_decompressor *decompressor;   // decompressor
    FILE *file;                              // file pointer
    u32 homeIndex;                           // index of the texture buffer
    u16 x;                                   // raw number of the texture
    u16 y;                                   // column number 
    texture_buffer *prev;                    // previous buffer
};

// define the single texture buffer queue
struct single_texture_buffer_queue   
{
    u32 queueLength;          // number of buffers in the queue
    u32 pad;                  // padding bytes, for alignment
    mutex rwMutex;            // read-write mutex, prevent errors in the queue reading  typedef pthread_mutex_t mutex; defined in the multi-thread library
    texture_buffer *front;    // pointer to the head node
    texture_buffer *rear;     // pointer to the last node
};

// define the buffer queue structure
struct texture_buffer_queue
{
    single_texture_buffer_queue **queues;  // pointer array of multiple single queue buffers, queues is an address, the stored *queues is an address, this address stores a buffer queue**queues，
    threadSig index;                       // index of the queue typedef volatile u32 threadSig;
    u32 pad;                               // padding bytes, for alignment
};

// define the function to initialize the single buffer queue
global_function
void
InitialiseSingleTextureBufferQueue(single_texture_buffer_queue *queue)
{
    InitialiseMutex(queue->rwMutex);  // initialize the mutex #define InitialiseMutex(x) pthread_mutex_init(&(x), NULL)，Set the mutex of a single queue to available.
    queue->queueLength = 0;           // length of 0 indicates that the queue is empty
}

global_function
void
AddSingleTextureBufferToQueue(single_texture_buffer_queue *queue, texture_buffer *buffer)
{   // add the last buffer to the rear of the queue
    LockMutex(queue->rwMutex); // lock the current queue
    buffer->prev = 0;

    switch (queue->queueLength)
    {
        case 0: // set the first buffer
            queue->front = buffer;  // here we don't define the previous node of the front node, only define the next node
            queue->rear = buffer;
            break;

        default: // set the buffer after the last one
            queue->rear->prev = buffer; // set the previous node of the last node to the buffer
            // ?? why not set the previous node of the last node to the buffer? buffer->prev = queue->rear;
            queue->rear = buffer;       // set the buffer to the last node
    }

    ++queue->queueLength; // update the buffer length
    UnlockMutex(queue->rwMutex); // unlock the current queue after the operation
}

#define Compression_Header_Size 128

global_function
void
ShutdownTextureBufferQueue(texture_buffer_queue *queue);

global_function
void
InitialiseTextureBufferQueue(memory_arena *arena, texture_buffer_queue *queue, u32 nBytesForTextureBuffer, const char *fileName)
{   // initialize the memory for all the single queues and the textures in them
    if (queue->queues)
    {
        ShutdownTextureBufferQueue(queue);
    }

    queue->queues = PushArrayP(arena, single_texture_buffer_queue *, Number_Of_Texture_Buffer_Queues);  // allocate pointers for 一个queue
    queue->index = 0;      // set the thread index to 0, it may be modified later, because threadsig is volatile u32
    u32 nAdded = 0;        // number of successfully added decompressors
    u32 nFileHandles = 0;  // number of successfully added file pointers

    ForLoop(Number_Of_Texture_Buffer_Queues) // there are 8 queues
    {   // similar to a two-dimensional array
        queue->queues[index] = PushStructP(arena, single_texture_buffer_queue); // allocate spaces for each queues
        InitialiseSingleTextureBufferQueue(queue->queues[index]);  // **queue is a pointer to a queue variable, *(queue+index) or *queue[index] represents a pointer to a queue variable

        ForLoop2(Number_Of_Texture_Buffers_Per_Queue) // each queue will have 8 buffers, similar to a two-dimensional array
        {
            texture_buffer *buffer = PushStructP(arena, texture_buffer); // allocate space for texture buffer 
            buffer->texture = PushArrayP(arena, u08, nBytesForTextureBuffer); // space for texture
            buffer->compressionBuffer = PushArrayP(arena, u08, nBytesForTextureBuffer + Compression_Header_Size); // space for buffer and the compression header
            buffer->decompressor = libdeflate_alloc_decompressor(); // decompressor
            buffer->file = fopen(fileName, "rb");    // file pointer
            buffer->homeIndex = index;               // the index of the texture is the address of the queue, each queue has 8 textures
            if (buffer->decompressor)                // ensure the buffer is initialized successfully, successfully allocated the decompressor
            {
                ++nAdded;
                if (buffer->file)                    // ensure the file pointer is successfully allocated
                {
                    ++nFileHandles;
                    AddSingleTextureBufferToQueue(queue->queues[index], buffer); // add the buffer to the queue for reading the texture
                }
            }
        }
    }

    if (!nAdded)
    {
        fprintf(stderr, "Could not allocate memory for libdeflate decompressors\n");
        exit(1);
    }
    if (!nFileHandles)
    {
        fprintf(stderr, "Could not open input file %s\n", (const char *)fileName); exit(1);
    }
}

global_function
void close_file_in_single_queue(single_texture_buffer_queue* queue){
    if (!queue)
    {
        return;
    }

    LockMutex(queue->rwMutex);
    for (texture_buffer* tmp = queue->front; tmp; tmp = tmp->prev)
    {
        if (tmp->file)
        {
            fclose(tmp->file);
            tmp->file = 0;
        }
        if (tmp->decompressor)
        {
            libdeflate_free_decompressor(tmp->decompressor);
            tmp->decompressor = 0;
        }
    }
    queue->front = 0;
    queue->rear = 0;
    queue->queueLength = 0;
    UnlockMutex(queue->rwMutex);
}

global_function
void
ShutdownTextureBufferQueue(texture_buffer_queue *queue)
{
    if (!queue || !queue->queues)
    {
        return;
    }

    ForLoop(Number_Of_Texture_Buffer_Queues)
    {
        if (queue->queues[index])
        {
            close_file_in_single_queue(queue->queues[index]);
        }
    }

    queue->queues = 0;
    queue->index = 0;
}

global_function
void
CloseTextureBufferQueueFiles(texture_buffer_queue *queue)
{   // close the file reader, release the decompressor pointer
    if (!queue || !queue->queues)
    {
        return;
    }

    ForLoop(Number_Of_Texture_Buffer_Queues)
    {
        close_file_in_single_queue(queue->queues[index]);
    }
}


global_function
void
AddTextureBufferToQueue(texture_buffer_queue *queue, texture_buffer *buffer)
{   // add the buffer to the corresponding single queue
    single_texture_buffer_queue *singleQueue = queue->queues[buffer->homeIndex]; // obtain the pointer of the buffer of queue
    AddSingleTextureBufferToQueue(singleQueue, buffer); // add the buffer to the tail of this queue
}

global_function
texture_buffer *
TakeSingleTextureBufferFromQueue(single_texture_buffer_queue *queue)
{   // get a buffer_texture from single texture
    // get the head node of single texture
    LockMutex(queue->rwMutex);  // lock the single texture
    texture_buffer *buffer = queue->front;

    switch (queue->queueLength)
    {
        case 0:
            break;

        case 1:
            queue->front = 0;
            queue->rear = 0;
            -- queue->queueLength;
            break;

        default:
            queue->front = queue->front->prev;
            --queue->queueLength;
    }

    UnlockMutex(queue->rwMutex);

    return(buffer);
}

global_function
single_texture_buffer_queue *
GetSingleTextureBufferQueue(texture_buffer_queue *queue)
  // I don't quite understand atmonic operation, therefore the following translation cannot be too accurate -yy5.
{   // choose a queue from all the queues by atomic operation
    // __atomic_fetch_add is a built-in function that atomically executes an addition operation and returns the result. 
    // It applies an atomic addition to the value at the specified memory location, returning the updated value. 
    // This function facilitates the safe update of shared variables in multithreaded environments by guaranteeing operational atomicity; 
    // the addition is executed without interruption or interference from concurrent threads.
    u32 index = __atomic_fetch_add(&queue->index, 1, 0) % Number_Of_Texture_Buffer_Queues; 
    return(queue->queues[index]);
}

global_function
texture_buffer *
TakeTextureBufferFromQueue(texture_buffer_queue *queue)
{   // get a buffer from the total queue, first get a queue, then get a buffer from the queue
    return(TakeSingleTextureBufferFromQueue(GetSingleTextureBufferQueue(queue)));
}

global_function
texture_buffer *
TakeTextureBufferFromQueue_Wait(texture_buffer_queue *queue)
{   
    texture_buffer *buffer = 0;
    while (!buffer) // if the buffer is empty
    {
        buffer = TakeTextureBufferFromQueue(queue);
    } // if all the buffers in the queue are empty, the loop will continue
    return(buffer);
}
