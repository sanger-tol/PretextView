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

#define Number_Of_Texture_Buffers_Per_Queue 8
#define Number_Of_Texture_Buffer_Queues 8

struct
texture_buffer
{
    u08 *texture;
    u08 *compressionBuffer;
    libdeflate_decompressor *decompressor;
    FILE *file;
    u32 homeIndex;
    u16 x;
    u16 y;
    texture_buffer *prev;
};

struct
single_texture_buffer_queue
{
    u32 queueLength;
    u32 pad;
    mutex rwMutex;
    texture_buffer *front;
    texture_buffer *rear;
};

struct
texture_buffer_queue
{
    single_texture_buffer_queue **queues;
    threadSig index;
    u32 pad;
};

global_function
void
InitialiseSingleTextureBufferQueue(single_texture_buffer_queue *queue)
{
    InitialiseMutex(queue->rwMutex);
    queue->queueLength = 0;
}

global_function
void
AddSingleTextureBufferToQueue(single_texture_buffer_queue *queue, texture_buffer *buffer);

#define Compression_Header_Size 128

global_function
void
InitialiseTextureBufferQueue(memory_arena *arena, texture_buffer_queue *queue, u32 nBytesForTextureBuffer, const char *fileName)
{
    queue->queues = PushArrayP(arena, single_texture_buffer_queue *, Number_Of_Texture_Buffer_Queues);
    queue->index = 0;
    u32 nAdded = 0;
    u32 nFileHandles = 0;

    ForLoop(Number_Of_Texture_Buffer_Queues)
    {
        queue->queues[index] = PushStructP(arena, single_texture_buffer_queue);
        InitialiseSingleTextureBufferQueue(queue->queues[index]);

        ForLoop2(Number_Of_Texture_Buffers_Per_Queue)
        {
            texture_buffer *buffer = PushStructP(arena, texture_buffer);
            buffer->texture = PushArrayP(arena, u08, nBytesForTextureBuffer);
            buffer->compressionBuffer = PushArrayP(arena, u08, nBytesForTextureBuffer + Compression_Header_Size);
            buffer->decompressor = libdeflate_alloc_decompressor();
            buffer->file = fopen(fileName, "rb");
            buffer->homeIndex = index;
            if (buffer->decompressor)
            {
                ++nAdded;
                if (buffer->file)
                {
                    ++nFileHandles;
                    AddSingleTextureBufferToQueue(queue->queues[index], buffer);
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
        fprintf(stderr, "Could not open input file %s\n", (const char *)fileName);
    }
}

global_function
void
AddSingleTextureBufferToQueue(single_texture_buffer_queue *queue, texture_buffer *buffer)
{
    LockMutex(queue->rwMutex);
    buffer->prev = 0;

    switch (queue->queueLength)
    {
        case 0:
            queue->front = buffer;
            queue->rear = buffer;
            break;

        default:
            queue->rear->prev = buffer;
            queue->rear = buffer;
    }

    ++queue->queueLength;
    UnlockMutex(queue->rwMutex);
}

global_function
void
CloseSingleTextureBufferQueueFiles(single_texture_buffer_queue *queue)
{
    LockMutex(queue->rwMutex);

    for (   texture_buffer *buffer = queue->front;
            buffer;
            buffer = buffer->prev )
    {
        fclose(buffer->file);
        free(buffer->decompressor);
    }

    UnlockMutex(queue->rwMutex);
}

global_function
void
CloseTextureBufferQueueFiles(texture_buffer_queue *queue)
{
    ForLoop(Number_Of_Texture_Buffer_Queues)
    {
        CloseSingleTextureBufferQueueFiles(queue->queues[index]);
    }
}

global_function
void
AddTextureBufferToQueue(texture_buffer_queue *queue, texture_buffer *buffer)
{
    single_texture_buffer_queue *singleQueue = queue->queues[buffer->homeIndex];
    AddSingleTextureBufferToQueue(singleQueue, buffer);
}

global_function
texture_buffer *
TakeSingleTextureBufferFromQueue(single_texture_buffer_queue *queue)
{
    LockMutex(queue->rwMutex);
    texture_buffer *buffer = queue->front;

    switch (queue->queueLength)
    {
        case 0:
            break;

        case 1:
            queue->front = 0;
            queue->rear = 0;
            queue->queueLength = 0;
            break;

        default:
            queue->front = buffer->prev;
            --queue->queueLength;
    }

    UnlockMutex(queue->rwMutex);

    return(buffer);
}

global_function
single_texture_buffer_queue *
GetSingleTextureBufferQueue(texture_buffer_queue *queue)
{
    u32 index = __atomic_fetch_add(&queue->index, 1, 0) % Number_Of_Texture_Buffer_Queues;
    return(queue->queues[index]);
}

global_function
texture_buffer *
TakeTextureBufferFromQueue(texture_buffer_queue *queue)
{
    return(TakeSingleTextureBufferFromQueue(GetSingleTextureBufferQueue(queue)));
}

global_function
texture_buffer *
TakeTextureBufferFromQueue_Wait(texture_buffer_queue *queue)
{
    texture_buffer *buffer = 0;
    while (!buffer)
    {
        buffer = TakeTextureBufferFromQueue(queue);
    }
    return(buffer);
}
