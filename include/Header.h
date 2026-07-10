/*
Copyright (c) 2021 Ed Harry, Wellcome Sanger Institute, Genome Research Limited
Copyright (c) 2024 Shaoheng Guan, Wellcome Sanger Institute

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

#ifndef HEADER_H
#define HEADER_H

#include "utilsPretextView.h"

#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wreserved-id-macro"
#define __STDC_FORMAT_MACROS
#pragma clang diagnostic pop


#ifdef _WIN32
#define WINVER 0x0601 // Target Windows 7 as a Minimum Platform
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <Knownfolders.h>
#include <Shlobj.h>
#endif // _WIN32

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
//#include <math.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


// use the cpp std library
#include <iostream>
#include <memory>

/*
    original version not commented here
*/
// #ifdef _WIN32
// #include <intrin.h>
// #else  // problem is here
// #include <x86intrin.h>  // please use ```arch -x86_64 zsh```, before run the compiling
// #endif
#if defined(_WIN32) || defined(_WIN64)
    // Windows-specific includes
    #include <intrin.h>
    #if defined(_M_ARM64) || defined(_M_ARM)
        #define ARCH_ARM 1
    #else
        #define ARCH_X86 1
    #endif
#elif defined(__x86_64__) || defined(__i386__)
    // Non-Windows x86 architectures
    #include <x86intrin.h>
    #define ARCH_X86 1
#elif defined(__arm__) || defined(__aarch64__)
    // ARM architectures
    #include <arm_neon.h> // Example: ARM NEON intrinsics
    #define ARCH_ARM 1
#else
    #error "Unsupported architecture or operating system"
#endif // defined(_WIN32) || defined(_WIN64)


#include "libdeflate.h"


#ifndef _WIN32
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#else
#define __atomic_fetch_add(x, y, z) _InterlockedExchangeAdd(x, y)
#define __atomic_add_fetch(x, y, z) (y + _InterlockedExchangeAdd(x, y))
#define __sync_fetch_and_add(x, y) _InterlockedExchangeAdd(x, y)
#define __sync_fetch_and_sub(x, y) _InterlockedExchangeAdd(x, -y)
#define __atomic_store(x, y, z) _InterlockedCompareExchange(x, *y, *x)
#endif // _WIN32

#ifndef _WIN32
#define ThreadFence __asm__ volatile("" ::: "memory")
#elif defined(_M_ARM64) || defined(_M_ARM)
#define ThreadFence MemoryBarrier()
#else
#define ThreadFence _mm_mfence()
#endif // _WIN32

/*
defined a macro named FenceIn, its purpose is to insert a thread fence (Thread Fence) in a code block, 
it will first execute the first thread fence ThreadFence, then execute your code, and finally execute 
the second thread fence ThreadFence, to ensure the correct execution order of the inserted code.

*/
#define FenceIn(x) ThreadFence; \
	x; \
	ThreadFence 

typedef volatile u32 threadSig;

/* 定义用于 color map 的变量 */
extern f32 Color_Map_Data[][768];
extern const char * Color_Map_Names[];

#ifndef _WIN32
typedef pthread_t thread;
typedef pthread_mutex_t mutex;
typedef pthread_cond_t cond;

//The error message indicates that the compiler recognise PTHREAD_MUTEX_INITIALIZER is not a valid expression. Usually, 
// PTHREAD_MUTEX_INITIALIZER is a macro, it will be expanded into a structure initializer, instead of a single expression.
// #define InitialiseMutex(x) x = PTHREAD_MUTEX_INITIALIZER
// #define InitialiseCond(x) x = PTHREAD_COND_INITIALIZER
#define InitialiseMutex(x) pthread_mutex_init(&(x), NULL)
#define InitialiseCond(x)  pthread_cond_init(&(x), NULL)

#define LaunchThread(thread, func, dataIn) pthread_create(&thread, NULL, func, dataIn)
#define WaitForThread(x) pthread_join(*x, NULL)
#define DetachThread(thread) pthread_detach(thread)

#define LockMutex(x) pthread_mutex_lock(&x)
#define UnlockMutex(x) pthread_mutex_unlock(&x)
#define WaitOnCond(cond, mutex) pthread_cond_wait(&cond, &mutex) 
#define SignalCondition(x) pthread_cond_signal(&x)
#define BroadcastCondition(x) pthread_cond_broadcast(&x)
#else
typedef HANDLE thread;
typedef CRITICAL_SECTION mutex;
typedef CONDITION_VARIABLE cond;

#define InitialiseMutex(x) InitializeCriticalSection(&x)
#define InitialiseCond(x) InitializeConditionVariable(&x)

#define LaunchThread(thread, func, dataIn) thread = CreateThread(NULL, 0, func, dataIn, 0, NULL)

#define LockMutex(x) EnterCriticalSection(&x)
#define UnlockMutex(x) LeaveCriticalSection(&x)
#define WaitOnCond(cond, mutex) SleepConditionVariableCS(&cond, &mutex, INFINITE) 
#define SignalCondition(x) WakeConditionVariable(&x)
#define BroadcastCondition(x) WakeAllConditionVariable(&x)
#endif // _WIN32

#if defined(__AVX2__) && !defined(NoAVX)
#define UsingAVX
#endif // __AVX2__

// https://www.flipcode.com/archives/Fast_log_Function.shtml
global_function
f32
Log2(f32 val)
{
   s32 *expPtr = (s32 *)(&val);
   s32 x = *expPtr;
   s32 log2 = ((x >> 23) & 255) - 128;
   x &= ~(255 << 23);
   x += 127 << 23;
   *expPtr = x;

   val = ((((-1.0f/3.0f) * val) + 2.0f) * val) - (2.0f/3.0f);

   return(val + (f32)log2);
} 

struct
binary_semaphore
{
    mutex mut;
    cond con;
    u64 v;
};

struct
thread_job
{
    thread_job *prev;
    void (*function)(void *arg);
    void *arg;
};

struct
job_queue
{
    mutex rwMutex;
    thread_job *front;
    thread_job *rear;
    binary_semaphore *hasJobs;
    u32 len;
    u32 nFree;
    binary_semaphore *hasFree;
    thread_job *freeFront;
    thread_job *freeRear;
};

struct thread_context;

struct
thread_pool
{
    thread_context **threads;
    threadSig numThreadsAlive;
    threadSig numThreadsWorking;
    mutex threadCountLock;
    cond threadsAllIdle;
    job_queue jobQueue;
};

struct
thread_context
{
    u64	id;
    thread th;
    thread_pool *pool;
};

#ifdef DEBUG
#include <assert.h>
#define Assert(x) assert(x)
#else
#define Assert(x)
#endif // DEBUG

#define KiloByte(x) 1024*x
#define MegaByte(x) 1024*KiloByte(x)
#define GigaByte(x) 1024*MegaByte(x)

#define Default_Memory_Alignment_Pow2 4  // 对齐的字节数为4

struct memory_arena
{
   memory_arena *next;     // 指向下一个内存池的指针，用于支持内存池链表结构
   u08 *base;              // 内存池的基地址，指向分配给该内存池的内存块的起始位置
   u64 currentSize;        // 当前内存池已使用的字节数，用于跟踪内存使用情况
   u64 maxSize;            // 内存池的最大容量，指示内存池可以容纳的最大字节数
   u64 active;             // 标记内存池是否处于活动状态，通常用于判断内存池是否可用
};


struct
memory_arena_snapshot
{
    u64 size;
};

global_function
void
TakeMemoryArenaSnapshot(memory_arena *arena, memory_arena_snapshot *snapshot)
{
    snapshot->size = arena->currentSize;
}

global_function
void
RestoreMemoryArenaFromSnapshot(memory_arena *arena, memory_arena_snapshot *snapshot)
{
    arena->currentSize = snapshot->size;
}

global_function
u64
GetAlignmentPadding(u64 base, u32 alignment_pow2)
{ // 通常内存是由一个个字节组成的，cpu在存取数据时，并不是以字节为单位存储，而是以块为单位存取。块的大小为内存存取力度，频繁存取未对齐的数据会极大降低cpu的性能
  // 如果一个int型（假设为32位系统）如果存放在偶地址开始的地方，那么一个读周期就可以读出这32bit，而如果存放在奇地址开始的地方，就需要2个读周期，并对两次读出的结果的高低字节进行拼凑才能得到该32bit数据
	u64 alignment = (u64)Pow2(alignment_pow2); 
	u64 result = ((base + alignment - 1) & ~(alignment - 1)) - base;

	return(result);
}

global_function
u32
AlignUp(u32 x, u32 alignment_pow2)
{ // 向上对齐
	u32 alignment_m1 = Pow2(alignment_pow2) - 1;
	u32 result = (x + alignment_m1) & ~alignment_m1;

	return(result);
}

global_function
void
CreateMemoryArena_(memory_arena *arena, u64 size, u32 alignment_pow2 = Default_Memory_Alignment_Pow2)
{ // 初始化当前的arena （调用时候输入是arena->next）
   u64 linkSize = sizeof(memory_arena);
   linkSize += GetAlignmentPadding(linkSize, alignment_pow2); // 对齐
   u64 realSize = size + linkSize;
   arena->currentSize = 0; // 定义当前arena的特征
   arena->active = 1;      // 激活当前arena
   arena->maxSize = size;  // 设置当前arena能存储的最大值，note：这三个的空间在初始化上一个arena的时候已经分配了

#ifndef _WIN32 
   posix_memalign((void **)&arena->base, Pow2(alignment_pow2), realSize); //如果不是在win32则使用该方法初始化，将内存块的地址给到arena->base， arena->next, arena->currentSize, arena->maxSize, arena->active的地址已经初始化上一个arena的时候分配了
#else
#include <memoryapi.h>
   (void)alignment_pow2;
   arena->base = (u08 *)VirtualAlloc(NULL, realSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#endif  // _WIN32
#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"	
   arena->next = (memory_arena *)arena->base; // 初始化出来的空间的前48个字节存储下一个arena的信息
#pragma clang diagnostic pop
   arena->base += linkSize;  // 当前开始的地址, 实际开始存储信息的地址要排除arena自己所需的5个存储指针的内存（40 + 补齐值 8）
   arena->next->base = 0; // 应该指向下一个arena的base，但是下一个还没有被分配，因此为0. 
}

#define CreateMemoryArena(arena, size, ...) CreateMemoryArena_(&arena, size, ##__VA_ARGS__)
#define CreateMemoryArenaP(arena, size, ...) CreateMemoryArena_(arena, size, ##__VA_ARGS__)

global_function
void
ResetMemoryArena_(memory_arena *arena)
{
   if (arena->next)
   {
      if (arena->next->base)
      {
	 ResetMemoryArena_(arena->next);
      }
      arena->currentSize = 0;
   }
}

#define ResetMemoryArena(arena) ResetMemoryArena_(&arena)
#define ResetMemoryArenaP(arena) ResetMemoryArena_(arena)

global_function
void
FreeMemoryArena_(memory_arena *arena)
{
   if (arena->next)
   {
      if (arena->next->base)
      {
	 FreeMemoryArena_(arena->next);
      }
      free(arena->next);
   }
}

#define FreeMemoryArena(arena) FreeMemoryArena_(&arena)
#define FreeMemoryArenaP(arena) FreeMemoryArena_(arena)

global_function
void *
PushSize_(memory_arena *arena, u64 size, u32 alignment_pow2 = Default_Memory_Alignment_Pow2)
{
   if (!arena->active && arena->next && arena->next->base && !arena->next->currentSize)
   {
      arena->active = 1; // 如果当前区域没有被激活，存在下一个分配区域，已经初始化但是并未使用，则将当前区域设置为激活
   }
   
   u64 padding = GetAlignmentPadding((u64)(arena->base + arena->currentSize), alignment_pow2); // 向上取4字节对齐，得到需要补充的字节数padding

   void *result;
   if (!arena->active || ((size + arena->currentSize + padding + sizeof(u64)) > arena->maxSize)) // 如果当前没有被激活或者是当前申请的空间大于最大的容许值, 则激活下一个
   {
      arena->active = 0; // 设置当前为未激活 
      if (arena->next)  // 如果存在下一个arena空间
      {
        if (arena->next->base)  // 下一个arena已经被初始化
        {
            result = PushSize_(arena->next, size, alignment_pow2);  // 从下一个arena 申请size 
        }
        else
        {
            u64 linkSize = sizeof(memory_arena);  // 获取该struct的大小, 40
            linkSize += GetAlignmentPadding(linkSize, alignment_pow2);  // padding  = 8 
            u64 realSize = size + padding + sizeof(u64) + linkSize;
            realSize = my_Max(realSize, arena->maxSize);
            
            CreateMemoryArenaP(arena->next, realSize, alignment_pow2);  // 初始化下一个arena，主要是初始化下一个arena的base的值，arena->next指向下一个arena，arena->next->base = arena->next + 48 即开始存储数据的位置，因为前40个存储了3个u64和两个指针，以及8个补全
            result = PushSize_(arena->next, size, alignment_pow2);  // 初始化空间之后返回当前result的地址
        }
      }
      else // 如果不存在arena->next，这是不可能的，因为在初始化的时候arena->next指向了arena->base，因此不可能为空，如果确实发生则会报错
      {
	    result = 0;
#if defined(__APPLE__) || defined(_WIN32)
#ifdef PrintError
	 PrintError("Push of %llu bytes failed, out of memory", size);
#else
	 fprintf(stderr, "Push of %llu bytes failed, out of memory.\n", size);
#endif // PrintError
#else
#ifdef PrintError
	 PrintError("Push of %lu bytes failed, out of memory", size);
#else
	 fprintf(stderr, "Push of %lu bytes failed, out of memory.\n", size);
#endif // PrintError
#endif	// __APPLE__ || _WIN32
	 *((volatile u32 *)0) = 0;
      }
   }
   else
   {  // 此处为递归函数的出口
      result = arena->base + arena->currentSize + padding;  // 存储在同一个arena中
      arena->currentSize += (size + padding + sizeof(u64)); // 更新大小
#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"		
      *((u64 *)(arena->base + arena->currentSize - sizeof(u64))) = (size + padding); // 多申请的8个字节表示
#pragma clang diagnostic pop
   }

   return(result);
}

global_function
void
FreeLastPush_(memory_arena *arena)
{
   if (!arena->active && arena->next && arena->next->base)
   {
      if (arena->next->active && !arena->next->currentSize)
      {
	 arena->active = 1;
	 FreeLastPush_(arena);
      }
      else
      {
	 FreeLastPush_(arena->next);
      }
   }
   else if (arena->currentSize)
   {
#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
      u64 sizeToRemove = *((u64 *)(arena->base + arena->currentSize - sizeof(u64)));
#pragma clang diagnostic pop		
      arena->currentSize -= (sizeToRemove + sizeof(u64));
   }
}

#define PushStruct(arena, type, ...) (type *)PushSize_(&arena, sizeof(type), ##__VA_ARGS__)
#define PushArray(arena, type, n, ...) (type *)PushSize_(&arena, sizeof(type) * n, ##__VA_ARGS__)
#define PushStructP(arena, type, ...) (type *)PushSize_(arena, sizeof(type), ##__VA_ARGS__)
#define PushArrayP(arena, type, n, ...) (type *)PushSize_(arena, sizeof(type) * n, ##__VA_ARGS__)

#define FreeLastPush(arena) FreeLastPush_(&arena)
#define FreeLastPushP(arena) FreeLastPush_(arena)

global_function
memory_arena *
PushSubArena_(memory_arena *mainArena, u64 size, u32 alignment_pow2 = Default_Memory_Alignment_Pow2)
{
   memory_arena *subArena = PushStructP(mainArena, memory_arena, alignment_pow2);
   subArena->base = PushArrayP(mainArena, u08, size, alignment_pow2);
   subArena->currentSize = 0;
   subArena->maxSize = size;
   subArena->next = 0;
   subArena->active = 1;

   return(subArena);
}

#define PushSubArena(arena, size, ...) PushSubArena_(&arena, size, ##__VA_ARGS__)
#define PushSubArenaP(arena, size, ...) PushSubArena_(arena, size, ##__VA_ARGS__)

global_variable
threadSig
Threads_KeepAlive;

global_function
void
BinarySemaphoreInit(binary_semaphore *bsem, u32 value)
{
    InitialiseMutex(bsem->mut); // 错误消息表明编译器认为 PTHREAD_MUTEX_INITIALIZER 不是一个有效的表达式。通常情况下，PTHREAD_MUTEX_INITIALIZER 是一个宏，它会被展开为一个结构体初始化器，而不是一个单独的表达式。
    InitialiseCond(bsem->con);
    bsem->v = value;
}

global_function
void
BinarySemaphoreWait(binary_semaphore *bsem)
{
    LockMutex(bsem->mut);
    
    while (bsem->v != 1)
    {
	WaitOnCond(bsem->con, bsem->mut);
    }
    
    bsem->v = 0;
    UnlockMutex(bsem->mut);
}

global_function
void
BinarySemaphorePost(binary_semaphore *bsem)
{
    LockMutex(bsem->mut);
    bsem->v = 1;
    SignalCondition(bsem->con);
    UnlockMutex(bsem->mut);
}

global_function
void
BinarySemaphorePostAll(binary_semaphore *bsem)
{
    LockMutex(bsem->mut);
    bsem->v = 1;
    BroadcastCondition(bsem->con);
    UnlockMutex(bsem->mut);
}

global_function
thread_job *
JobQueuePull(job_queue *jobQueue)
{
    LockMutex(jobQueue->rwMutex);
    thread_job *job = jobQueue->front;
    
    switch (jobQueue->len)
    {
	case 0:
	    break;

	case 1:
	    jobQueue->front = 0;
	    jobQueue->rear  = 0;
	    jobQueue->len = 0;
	    break;

	default:
	    jobQueue->front = job->prev;
	    --jobQueue->len;
	    BinarySemaphorePost(jobQueue->hasJobs);
    }

    UnlockMutex(jobQueue->rwMutex);
    
    return(job);
}

global_function
thread_job *
GetFreeThreadJob(job_queue *jobQueue)
{
    LockMutex(jobQueue->rwMutex);
    thread_job *job = jobQueue->freeFront;

    switch (jobQueue->nFree)
    {
	case 0:
	    break;
	
	case 1:
	    jobQueue->freeFront = 0;
	    jobQueue->freeRear = 0;
	    jobQueue->nFree = 0;
	    break;
	
	default:
	    jobQueue->freeFront = job->prev;
	    --jobQueue->nFree;
	    BinarySemaphorePost(jobQueue->hasFree);
    }

    UnlockMutex(jobQueue->rwMutex);

    return(job);
}

global_function
void
FreeThreadJob(job_queue *jobQueue, thread_job *job)
{
    LockMutex(jobQueue->rwMutex);
    job->prev = 0;

    switch (jobQueue->nFree)
    {
	case 0:
	    jobQueue->freeFront = job;
	    jobQueue->freeRear  = job;
	    break;

	default:
	    jobQueue->freeRear->prev = job;
	    jobQueue->freeRear = job;
    }
    ++jobQueue->nFree;

    BinarySemaphorePost(jobQueue->hasFree);	
    UnlockMutex(jobQueue->rwMutex);   
}

global_function
#ifndef _WIN32
void *
#else
DWORD WINAPI
#endif // _WIN32
ThreadFunc(void *in)
{
    thread_context *context = (thread_context *)in;
    
    thread_pool *pool = context->pool;

    LockMutex(pool->threadCountLock);
    pool->numThreadsAlive += 1;
    UnlockMutex(pool->threadCountLock);

    while (Threads_KeepAlive)
    {
	BinarySemaphoreWait(pool->jobQueue.hasJobs);

	if (Threads_KeepAlive)
	{
	    LockMutex(pool->threadCountLock);
	    ++pool->numThreadsWorking;
	    UnlockMutex(pool->threadCountLock);
			
	    void (*funcBuff)(void*);
	    void *argBuff;
	    thread_job *job = JobQueuePull(&pool->jobQueue);

	    if (job)
	    {
		funcBuff = job->function;
		argBuff  = job->arg;
		funcBuff(argBuff);
		FreeThreadJob(&pool->jobQueue, job);
	    }
			
	    LockMutex(pool->threadCountLock);
	    --pool->numThreadsWorking;

	    if (!pool->numThreadsWorking)
	    {
		SignalCondition(pool->threadsAllIdle);
	    }

	    UnlockMutex(pool->threadCountLock);
	}
    }
    
    LockMutex(pool->threadCountLock);
    --pool->numThreadsAlive;
    UnlockMutex(pool->threadCountLock);

    return(NULL);
}

global_function
void 
ThreadInit(memory_arena *arena, thread_pool *pool, thread_context **context, u32 id)
{
    *context = PushStructP(arena, thread_context);

    (*context)->pool = pool;
    (*context)->id = id;

    LaunchThread((*context)->th, ThreadFunc, *context);
#ifndef _WIN32
    DetachThread((*context)->th);
#endif // _WIN32
}

#define Number_Thread_Jobs 1024
global_function
void
JobQueueInit(memory_arena *arena, job_queue *jobQueue)
{
    jobQueue->hasJobs = PushStructP(arena, binary_semaphore);
    jobQueue->hasFree = PushStructP(arena, binary_semaphore);

    InitialiseMutex(jobQueue->rwMutex); // 错误消息表明编译器认为 PTHREAD_MUTEX_INITIALIZER 不是一个有效的表达式。通常情况下，PTHREAD_MUTEX_INITIALIZER 是一个宏，它会被展开为一个结构体初始化器，而不是一个单独的表达式。

    BinarySemaphoreInit(jobQueue->hasJobs, 0);
    BinarySemaphoreInit(jobQueue->hasFree, 0);

    jobQueue->len = 0;
    jobQueue->front = 0;
    jobQueue->rear = 0;

    jobQueue->nFree = 0;
    for (   u32 index = 0;
	    index < Number_Thread_Jobs;
	    ++index )
    {
	thread_job *job = PushStructP(arena, thread_job);
	FreeThreadJob(jobQueue, job);
    }
}

global_function
void
JobQueueClear(job_queue *jobQueue)
{
    while (jobQueue->len)
    {
	thread_job *job = JobQueuePull(jobQueue);
	FreeThreadJob(jobQueue, job);
    }

    jobQueue->front = 0;
    jobQueue->rear  = 0;
    BinarySemaphoreInit(jobQueue->hasJobs, 0);
    jobQueue->len = 0;
}

global_function
void
JobQueuePush(job_queue *jobQueue, thread_job *job)
{
    LockMutex(jobQueue->rwMutex);
    job->prev = 0;

    switch (jobQueue->len)
    {
	case 0:
	    jobQueue->front = job;
	    jobQueue->rear  = job;
	    break;

	default:
	    jobQueue->rear->prev = job;
	    jobQueue->rear = job;
    }
    ++jobQueue->len;

    BinarySemaphorePost(jobQueue->hasJobs);	
    UnlockMutex(jobQueue->rwMutex);
}

global_function
thread_pool *
ThreadPoolInit(memory_arena *arena, u32 nThreads)
{
    Threads_KeepAlive = 1;

    thread_pool *threadPool = PushStructP(arena, thread_pool);
    threadPool->numThreadsAlive = 0;
    threadPool->numThreadsWorking = 0;

    JobQueueInit(arena, &threadPool->jobQueue);

    threadPool->threads = PushArrayP(arena, thread_context*, nThreads);
	
    InitialiseMutex(threadPool->threadCountLock); //错误消息表明编译器认为 PTHREAD_MUTEX_INITIALIZER 不是一个有效的表达式。通常情况下，PTHREAD_MUTEX_INITIALIZER 是一个宏，它会被展开为一个结构体初始化器，而不是一个单独的表达式。
    InitialiseCond(threadPool->threadsAllIdle);
	
    for (   u32 index = 0;
	    index < nThreads;
	    ++index )
    {
	ThreadInit(arena, threadPool, threadPool->threads + index, index);
    }

    while (threadPool->numThreadsAlive != nThreads) {}

    return(threadPool);
}

#define ThreadPoolAddTask(pool, func, args) ThreadPoolAddWork(pool, (void (*)(void *))func, (void *)args)

#ifdef _WIN32
#include <ctime>
#define sleep(x) Sleep(1000 * x)
#endif // _WIN32

global_function
void
ThreadPoolAddWork(thread_pool *threadPool, void (*function)(void*), void *arg)
{
    thread_job *job;
    
    BinarySemaphoreWait(threadPool->jobQueue.hasFree);
    while (!(job = GetFreeThreadJob(&threadPool->jobQueue))) // while no free thread in the thread pool
    {
        printf("Waiting for a free job...\n");
        sleep(1);
        BinarySemaphoreWait(threadPool->jobQueue.hasFree);
    }
    
    job->function = function;
    job->arg = arg;

    JobQueuePush(&threadPool->jobQueue, job); // give the job queue to 
}

global_function
void
ThreadPoolWait(thread_pool *threadPool)
{
    LockMutex(threadPool->threadCountLock);
    
    while (threadPool->jobQueue.len || threadPool->numThreadsWorking)
    {
	WaitOnCond(threadPool->threadsAllIdle, threadPool->threadCountLock);
    }
    
    UnlockMutex(threadPool->threadCountLock);
}

global_function
void
ThreadPoolDestroy(thread_pool *threadPool)
{
    if (threadPool)
    {
	Threads_KeepAlive = 0;

	f64 timeout = 1.0;
	time_t start, end;
	f64 tPassed = 0.0;
	time (&start);
	while (tPassed < timeout && threadPool->numThreadsAlive)
	{
	    BinarySemaphorePostAll(threadPool->jobQueue.hasJobs);
	    time (&end);
	    tPassed = difftime(end, start);
	}

	while (threadPool->numThreadsAlive)
	{
	    BinarySemaphorePostAll(threadPool->jobQueue.hasJobs);
	    sleep(1);
	}

	JobQueueClear(&threadPool->jobQueue);
    }
}

global_function
u32
AreStringsEqual(char *string1, char term1, char *string2, char term2)
{
   u32 result = string1 == string2;

   if (!result)
   {
      do
      {
	 result = *string1++ == *string2++;
      } while (result && !(*string1 == term1 && *string2 == term2));
   }

   return(result);
}

global_function
u32
AreNullTerminatedStringsEqual(u08 *string1, u08 *string2)
{
   u32 result = string1 == string2;
   if (!result)
   {
      do
      {
	 result = (*string1 == *(string2++));
      } while(result && (*(string1++) != '\0'));
   }
   return(result);
}

global_function
u32
AreNullTerminatedStringsEqual(u32 *string1, u32 *string2, u32 nInts) //TODO SIMD array compare
{
   u32 result = string1 == string2; 
   if (!result)
   {
      ForLoop(nInts)
      {
	 result = string1[index] == string2[index];
	 if (!result)
	 {
	    break;
	 }
      }
   }
   return(result);
}

global_function
u32
CopyNullTerminatedString(u08 *source, u08 *dest)
{
	u32 stringLength = 0;

	while(*source != '\0')
	{
		*(dest++) = *(source++);
		++stringLength;
	}
	*dest = '\0';

	return(stringLength);
}

inline
u32
IntPow(u32 base, u32 pow)
{
    	u32 result = 1;
    
    	for(u32 index = 0;
         	index < pow;
         	++index)
    	{
        	result *= base;
    	}
    
    	return(result);
}

inline
u32
StringLength(u08 *string)
{
    	u32 length = 0;
    
    	while(*string++ != '\0') ++length;
    	
    	return(length);
}

struct
string_to_int_result
{
    	u32 integerValue;
    	u32 numDigits;
};

global_function
u32
StringToInt(u08 *stringEnd, u32 length)
{
    u32 result = 0;
    u32 pow = 1;

    while (--length > 0)
    {
	result += (u32)(*--stringEnd - '0') * pow;
	pow *= 10;
    }
    result += (u32)(*--stringEnd - '0') * pow;

    return(result);
}

global_function
u64
StringToInt64(u08 *stringEnd, u32 length)
{
    u64 result = 0;
    u32 pow = 1;

    while (--length > 0)
    {
	result += (u64)(*--stringEnd - '0') * pow;
	pow *= 10;
    }
    result += (u64)(*--stringEnd - '0') * pow;

    return(result);
}

inline
string_to_int_result
StringToInt(char *string)
{
    	string_to_int_result result;
    
    	u32 strLen = 1;
    	while(*++string != '\0') ++strLen;
    
    	result.integerValue = 0;
    	result.numDigits = strLen;
    	u32 pow = 1;
    
    	while(--strLen > 0)
    	{
        	result.integerValue += (u32)(*--string - '0') * pow;
		pow *= 10;
	}
	result.integerValue += (u32)(*--string - '0') * pow;
    
	return(result);
}

global_function
u32
StringToInt_Check(char *string, u32 *result)
{
    u32 goodResult = 1;
    *result = 0;

    u32 strLen = 1;
    while(*++string != '\0') ++strLen;

    u32 pow = 1;

    while(--strLen > 0 && goodResult)
    {
	*result += (u32)(*--string - '0') * pow;	
	goodResult = (*string >= '0' && *string <= '9');
	pow *= 10;
    }
    
    *result += (u32)(*--string - '0') * pow;
    goodResult = (goodResult && *string >= '0' && *string <= '9');
    
    return(goodResult);
}

global_function
u32
StringRGBAHexCodeToU32(u08 *string)
{
   u32 result = 0;

   ForLoop(4)
   {
      u08 character = *string++;
      u08 value1 = (character >= 'a') ? (10 + (character - 'a')) : ((character >= 'A') ? (10 + (character - 'A')) : (character - '0'));
      character = *string++;
      u08 value2 = (character >= 'a') ? (10 + (character - 'a')) : ((character >= 'A') ? (10 + (character - 'A')) : (character - '0'));

      value1 = (16 * value1) + value2;
      result |= ((u32)value1 << (index << 3));
   }

   return(result);
}

global_function
u32
RGBADisplayFormat(u32 rgba)
{
   u32 result = 0;

   result |= ((rgba >> 24) & 0xff);
   result |= ((rgba >> 8) & 0xff00);
   result |= ((rgba << 8) & 0xff0000);
   result |= ((rgba << 24) & 0xff000000);

   return(result);
}

global_function
u08 *
PushStringIntoIntArray(u32 *intArray, u32 arrayLength, u08 *string, u08 stringTerminator = '\0')
{ 
    u08 *stringToInt = (u08 *)intArray;
    u32 stringLength = 0;

    // 将 string 转移到 stringToInt
    while (*string != stringTerminator && stringLength < (arrayLength << 2))
    {
        *(stringToInt++) = *(string++);
        ++stringLength;
    }

    while (stringLength & 3)  // 按位与操作, 确保stringLength的二进制的后两位都是0
    {
        *(stringToInt++) = 0;
        ++stringLength;
    }

    for (   u32 index = (stringLength >> 2);
            index < arrayLength;
            ++index )
    {
        intArray[index] = 0;
    }

    return(string);
}

global_function
u32
IntDivideCeil(u32 x, u32 y)
{
	u32 result = (x + y - 1) / y;
	return(result);
}

//https://github.com/ZilongTan/fast-hash/blob/master/fasthash.c
#ifndef _WIN32
#define HashMix(h) ({					\
			(h) ^= (h) >> 23;		\
			(h) *= 0x2127599bf4325c37ULL;	\
			(h) ^= (h) >> 47; })

global_function
u64
FastHash64(void *buf, u64 len, u64 seed)
{
    u64 m = 0x880355f21e6d1965ULL;
    u64 *pos = (u64 *)buf;
    u64 *end = pos + (len / 8);
    u08 *pos2;
    u64 h = seed ^ (len * m);
    u64 v;

    while (pos != end)
    {
	v  = *pos++;
	h ^= HashMix(v);
	h *= m;
    }

    pos2 = (u08*)pos;
    v = 0;

    switch (len & 7)
    {
        /*
        错误发生在 [[clang::fallthrough]] 上，它似乎被识别为一个表达式，但实际上它是一个注释，用于指示编译器有意执行 switch 语句中的“fall through”。然而，该注释只有在编译器支持 C++11 或以上版本，并且启用了相应的警告时才有效。对于C语言，通常不会使用这种语法。
        要解决此错误，您可以将 [[clang::fallthrough]] 注释替换为标准的  fallthrough  注释，或者删除这个注释，因为在 switch 语句中，如果您没有明确指定 break，则默认会执行“fall through”行为。
        */
	case 7: v ^= (u64)pos2[6] << 48; /*[[clang::fallthrough]];*/
	case 6: v ^= (u64)pos2[5] << 40; /*[[clang::fallthrough]];*/
	case 5: v ^= (u64)pos2[4] << 32; /*[[clang::fallthrough]];*/
	case 4: v ^= (u64)pos2[3] << 24; /*[[clang::fallthrough]];*/
	case 3: v ^= (u64)pos2[2] << 16; /*[[clang::fallthrough]];*/
	case 2: v ^= (u64)pos2[1] << 8;  /*[[clang::fallthrough]];*/
	case 1: v ^= (u64)pos2[0];
		h ^= HashMix(v);
		h *= m;
    }

    return(HashMix(h));
} 
#else
#define HashMix(h) {					\
			(h) ^= (h) >> 23;		\
			(h) *= 0x2127599bf4325c37ULL;	\
			(h) ^= (h) >> 47; }

global_function
u64
FastHash64(void *buf, u64 len, u64 seed)
{
    u64 m = 0x880355f21e6d1965ULL;
    u64 *pos = (u64 *)buf;
    u64 *end = pos + (len / 8);
    u08 *pos2;
    u64 h = seed ^ (len * m);
    u64 v;

    while (pos != end)
    {
	v  = *pos++;
	HashMix(v);
	h ^= v;
	h *= m;
    }

    pos2 = (u08*)pos;
    v = 0;

    switch (len & 7)
    {
	case 7: v ^= (u64)pos2[6] << 48; [[clang::fallthrough]];
	case 6: v ^= (u64)pos2[5] << 40; [[clang::fallthrough]];
	case 5: v ^= (u64)pos2[4] << 32; [[clang::fallthrough]];
	case 4: v ^= (u64)pos2[3] << 24; [[clang::fallthrough]];
	case 3: v ^= (u64)pos2[2] << 16; [[clang::fallthrough]];
	case 2: v ^= (u64)pos2[1] << 8;  [[clang::fallthrough]];
	case 1: v ^= (u64)pos2[0];
		HashMix(v);
		h ^= v;
		h *= m;
    }

    HashMix(h);
    return(h);
} 
#endif // _WIN32

global_function
u32
FastHash32(void *buf, u64 len, u64 seed)
{
    // the following trick converts the 64-bit hashcode to Fermat
    // residue, which shall retain information from both the higher
    // and lower parts of hashcode.
    u64 h = FastHash64(buf, len, seed);
    return((u32)(h - (h >> 32)));
}


global_function u32 IsPrime(u32 n)
{  
   if (n <= 1)	return(0);
   if (n <= 3)	return(1);

   if (n%2 == 0 || n%3 == 0) return(0); 

   for ( u32 i=5;
	 i*i<=n;
	 i+=6 )
   {
      if (n%i == 0 || n%(i+2) == 0) return(0);
   }

   return(1);
}  

global_function u32 NextPrime(u32 N) 
{ 
   if (N <= 1) return(2); 

   while (!IsPrime(N)) ++N;

   return(N); 
} 

#endif // HEADER_H


