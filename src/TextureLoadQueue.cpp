/*
Copyright (c) 2021 Ed Harry, Wellcome Sanger Institute

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
    u32 homeIndex;                           // texture buffer queue index
    u16 x;                                   // raw number of the texture
    u16 y;                                   // column number 
    texture_buffer *prev;                    // previous buffer
};

// Single texture buffer queue
struct single_texture_buffer_queue   
{
    u32 queueLength;          // number of buffers in the queue
    u32 pad;                  // padding bytes for alignment
    mutex rwMutex;            // read/write mutex; typedef pthread_mutex_t mutex (from pthread)
    texture_buffer *front;    // head node pointer
    texture_buffer *rear;     // tail node pointer
};

// Aggregate texture buffer queue (multiple single queues)
struct texture_buffer_queue
{
    single_texture_buffer_queue **queues;  // array of pointers to single queues
    threadSig index;                       // queue index; typedef volatile u32 threadSig
    u32 pad;                               // padding bytes for alignment
};

// Initialize a single buffer queue
global_function
void
InitialiseSingleTextureBufferQueue(single_texture_buffer_queue *queue)
{
    InitialiseMutex(queue->rwMutex);  // InitialiseMutex(x) expands to pthread_mutex_init(&(x), NULL)
    queue->queueLength = 0;           // length 0 means the queue is empty
}

global_function
void
AddSingleTextureBufferToQueue(single_texture_buffer_queue *queue, texture_buffer *buffer)
{   // Append buffer as the new rear of the queue
    LockMutex(queue->rwMutex); // lock this queue
    buffer->prev = 0;

    switch (queue->queueLength)
    {
        case 0: // first buffer in the queue
            queue->front = buffer;  // front->prev is not set here; set when linking the next node
            queue->rear = buffer;
            break;

        default: // append after existing rear
            queue->rear->prev = buffer; // previous rear's prev points to the new buffer
            // ?? Why not buffer->prev = queue->rear; then queue->rear = buffer;
            queue->rear = buffer;       // new buffer becomes rear
    }

    ++queue->queueLength; // update buffer count
    UnlockMutex(queue->rwMutex); // unlock when done
}

#define Compression_Header_Size 128

global_function
void
InitialiseTextureBufferQueue(memory_arena *arena, texture_buffer_queue *queue, u32 nBytesForTextureBuffer, const char *fileName)
{   // Allocate memory for all single queues and their texture buffers
    queue->queues = PushArrayP(arena, single_texture_buffer_queue *, Number_Of_Texture_Buffer_Queues);  // allocate pointer array
    queue->index = 0;      // thread index starts at 0 (may change later; threadSig is volatile u32)
    u32 nAdded = 0;        // count of successfully added decompressors
    u32 nFileHandles = 0;  // count of successfully opened file handles

    ForLoop(Number_Of_Texture_Buffer_Queues) // 8 queues total
    {   // like a 2D array of queues and buffers
        queue->queues[index] = PushStructP(arena, single_texture_buffer_queue); // allocate spaces for each queues
        InitialiseSingleTextureBufferQueue(queue->queues[index]);  // queue->queues[index] is a pointer to one single queue

        ForLoop2(Number_Of_Texture_Buffers_Per_Queue) // 8 buffers per queue
        {
            texture_buffer *buffer = PushStructP(arena, texture_buffer); // allocate space for texture buffer 
            buffer->texture = PushArrayP(arena, u08, nBytesForTextureBuffer); // space for texture
            buffer->compressionBuffer = PushArrayP(arena, u08, nBytesForTextureBuffer + Compression_Header_Size); // space for buffer and the compression header
            buffer->decompressor = libdeflate_alloc_decompressor(); // decompressor
            buffer->file = fopen(fileName, "rb");    // file pointer
            buffer->homeIndex = index;               // which single queue owns this buffer (8 textures per queue)
            if (buffer->decompressor)                // ensure decompressor was allocated
            {
                ++nAdded;
                if (buffer->file)                    // ensure file handle was opened
                {
                    ++nFileHandles;
                    AddSingleTextureBufferToQueue(queue->queues[index], buffer); // add buffer used for reading textures
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
    LockMutex(queue->rwMutex);
    for (texture_buffer* tmp = queue->front; tmp; tmp = tmp->prev){  // iteration to close all of the file and decompressor
        fclose(tmp->file);
        free(tmp->decompressor);
    }
    UnlockMutex(queue->rwMutex);
    return ;
}

global_function
void
CloseTextureBufferQueueFiles(texture_buffer_queue *queue)
{   // Close file handles and free decompressors
    ForLoop(Number_Of_Texture_Buffer_Queues)
    {
        close_file_in_single_queue(queue->queues[index]);
    }
    return ;
}


global_function
void
AddTextureBufferToQueue(texture_buffer_queue *queue, texture_buffer *buffer)
{   // Return buffer to its home single queue
    single_texture_buffer_queue *singleQueue = queue->queues[buffer->homeIndex]; // pointer to the queue for this buffer
    AddSingleTextureBufferToQueue(singleQueue, buffer); // append to that queue's rear
}

global_function
texture_buffer *
TakeSingleTextureBufferFromQueue(single_texture_buffer_queue *queue)
{   // Take a texture_buffer from a single queue (from the front)
    // front node of the single queue
    LockMutex(queue->rwMutex);  // lock the single queue
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
{   // Round-robin pick one of the single queues
    // __atomic_fetch_add atomically adds and returns the new value; used here for thread-safe index advancement
    u32 index = __atomic_fetch_add(&queue->index, 1, 0) % Number_Of_Texture_Buffer_Queues; 
    return(queue->queues[index]);
}

global_function
texture_buffer *
TakeTextureBufferFromQueue(texture_buffer_queue *queue)
{   // Pick a single queue, then take a buffer from it
    return(TakeSingleTextureBufferFromQueue(GetSingleTextureBufferQueue(queue)));
}

global_function
texture_buffer *
TakeTextureBufferFromQueue_Wait(texture_buffer_queue *queue)
{   
    texture_buffer *buffer = 0;
    while (!buffer) // spin until a buffer is available
    {
        buffer = TakeTextureBufferFromQueue(queue);
    } // loops forever if all queues are empty
    return(buffer);
}
