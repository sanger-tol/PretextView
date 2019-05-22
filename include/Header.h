/*
Copyright (c) 2019 Ed Harry, Wellcome Sanger Institute

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

#ifdef _WIN32
#define WINVER 0x0601 // Target Windows 7 as a Minimum Platform
#define _WIN32_WINNT 0x0601
#include <windows.h>
#endif

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include "libdeflate.h"

typedef int8_t s08;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u08;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef size_t memptr;

#define global_function static
#define global_variable static

#define s08_max INT8_MAX
#define s16_max INT16_MAX
#define s32_max INT32_MAX
#define s64_max INT64_MAX

#define s08_min INT8_MIN
#define s16_min INT16_MIN
#define s32_min INT32_MIN
#define s64_min INT64_MIN

#define u08_max UINT8_MAX
#define u16_max UINT16_MAX
#define u32_max UINT32_MAX
#define u64_max UINT64_MAX

#define f32_max MAXFLOAT

#define Min(x, y) (x < y ? x : y)
#define Max(x, y) (x > y ? x : y)

#define u08_n (u08_max + 1)

#define Square(x) (x * x)

#define Pow10(x) (IntPow(10, x))
#define Pow2(N) (1 << N)

#define PI 3.141592653589793238462643383279502884195
#define TwoPI 6.283185307179586476925286766559005768391
#define Sqrt2 1.414213562373095048801688724209698078569
#define SqrtHalf 0.7071067811865475244008443621048490392845

#define ArrayCount(array) (sizeof(array) / sizeof(array[0]))
#define ForLoop(n) for (u32 index = 0; index < (n); ++index)
#define ForLoop2(n) for (u32 index2 = 0; index2 < (n); ++index2)
#define ForLoopN(i, n) for (u32 i = 0; i < (n); ++i)
#define TraverseLinkedList(startNode, type) for (type *(node) = (startNode); node; node = node->next)

#define ArgCount argc
#define ArgBuffer argv
#define Main s32 main()
#define MainArgs s32 main(s32 ArgCount, const char *ArgBuffer[])
#define EndMain return(0)

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
#endif

#ifndef _WIN32
#define ThreadFence __asm__ volatile("" ::: "memory")
#else
#define ThreadFence _mm_mfence()
#endif
#define FenceIn(x) ThreadFence; \
	x; \
	ThreadFence

typedef volatile u32 threadSig;

#ifndef _WIN32
typedef pthread_t thread;
typedef pthread_mutex_t mutex;
typedef pthread_cond_t cond;

#define InitialiseMutex(x) x = PTHREAD_MUTEX_INITIALIZER
#define InitialiseCond(x) x = PTHREAD_COND_INITIALIZER

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
#endif

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
#endif

#define KiloByte(x) 1024*x
#define MegaByte(x) 1024*KiloByte(x)
#define GigaByte(x) 1024*MegaByte(x)

#define Default_Memory_Alignment_Pow2 4

struct
memory_arena
{
	u08 *base;
	u64 currentSize;
	u64 maxSize;
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
void
CreateMemoryArena_(memory_arena *arena, u64 size, u32 alignment_pow2 = Default_Memory_Alignment_Pow2)
{
#ifndef _WIN32
	posix_memalign((void **)&arena->base, Pow2(alignment_pow2), size);
#else
	#include <memoryapi.h>
	(void)alignment_pow2;
	arena->base = (u08 *)VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#endif
	arena->currentSize = 0;
	arena->maxSize = size;
}

#define CreateMemoryArena(arena, size, ...) CreateMemoryArena_(&arena, size, ##__VA_ARGS__)
#define CreateMemoryArenaP(arena, size, ...) CreateMemoryArena_(arena, size, ##__VA_ARGS__)

global_function
void
ResetMemoryArena_(memory_arena *arena)
{
	arena->currentSize = 0;
}

#define ResetMemoryArena(arena) ResetMemoryArena_(&arena)
#define ResetMemoryArenaP(arena) ResetMemoryArena_(arena)

global_function
void
FreeMemoryArena_(memory_arena *arena)
{
	free(arena->base);
}

#define FreeMemoryArena(arena) FreeMemoryArena_(&arena)
#define FreeMemoryArenaP(arena) FreeMemoryArena_(arena)

global_function
u64
GetAlignmentPadding(u64 base, u32 alignment_pow2)
{
	u64 alignment = (u64)Pow2(alignment_pow2);
	u64 result = ((base + alignment - 1) & ~(alignment - 1)) - base;

	return(result);
}

global_function
u32
AlignUp(u32 x, u32 alignment_pow2)
{
	u32 alignment_m1 = Pow2(alignment_pow2) - 1;
	u32 result = (x + alignment_m1) & ~alignment_m1;

	return(result);
}

global_function
void *
PushSize_(memory_arena *arena, u64 size, u32 alignment_pow2 = Default_Memory_Alignment_Pow2)
{
	u64 padding = GetAlignmentPadding((u64)(arena->base + arena->currentSize), alignment_pow2);
	
	void *result;
	if((size + arena->currentSize + padding + sizeof(u64)) > arena->maxSize)
	{
		result = 0;
#if defined(__APPLE__) || defined(_WIN32)
	 	printf("Push of %llu bytes failed, out of memory.\n", size);   
#else
		printf("Push of %lu bytes failed, out of memory.\n", size);
#endif	
		*((volatile u32 *)0) = 0;
	}
	else
	{
		result = arena->base + arena->currentSize + padding;
		arena->currentSize += (size + padding + sizeof(u64));
#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"		
		*((u64 *)(arena->base + arena->currentSize - sizeof(u64))) = (size + padding);
#pragma clang diagnostic pop
	}
	
	return(result);
}

global_function
void
FreeLastPush_(memory_arena *arena)
{
	if (arena->currentSize)
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
    InitialiseMutex(bsem->mut);
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
#endif
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
#endif
}

#define Number_Thread_Jobs 1024
global_function
void
JobQueueInit(memory_arena *arena, job_queue *jobQueue)
{
    jobQueue->hasJobs = PushStructP(arena, binary_semaphore);
    jobQueue->hasFree = PushStructP(arena, binary_semaphore);

    InitialiseMutex(jobQueue->rwMutex);
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
	
    InitialiseMutex(threadPool->threadCountLock);
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
#endif

global_function
void
ThreadPoolAddWork(thread_pool *threadPool, void (*function)(void*), void *arg)
{
    thread_job *job;
    
    BinarySemaphoreWait(threadPool->jobQueue.hasFree);
    while (!(job = GetFreeThreadJob(&threadPool->jobQueue)))
    {
	printf("Waiting for a free job...\n");
	sleep(1);
	BinarySemaphoreWait(threadPool->jobQueue.hasFree);
    }
    
    job->function = function;
    job->arg = arg;

    JobQueuePush(&threadPool->jobQueue, job);
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
AreNullTerminatedStringsEqual(u08 *string1, u08 *string2)
{
	u32 result;
	do
	{
		result = (*string1 == *(string2++));
	} while(result && (*(string1++) != '\0'));

	return(result);
}

global_function
u32
AreNullTerminatedStringsEqual(u32 *string1, u32 *string2, u32 nInts) //TODO SIMD array compare
{
    u32 result = 1;
    ForLoop(nInts)
    {
	result = string1[index] == string2[index];
	if (!result)
	{
	    break;
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
StringToInt_Check(u08 *string, u32 *result)
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
	case 7: v ^= (u64)pos2[6] << 48; [[clang::fallthrough]];
	case 6: v ^= (u64)pos2[5] << 40; [[clang::fallthrough]];
	case 5: v ^= (u64)pos2[4] << 32; [[clang::fallthrough]];
	case 4: v ^= (u64)pos2[3] << 24; [[clang::fallthrough]];
	case 3: v ^= (u64)pos2[2] << 16; [[clang::fallthrough]];
	case 2: v ^= (u64)pos2[1] << 8;  [[clang::fallthrough]];
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
#endif

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

#endif
