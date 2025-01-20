#include "thread.h"

#include <stdlib.h>
#include <string.h>

#define NUM_THREAD_BUCKETS 16

typedef struct {
    u32 tid;
    u32 lastPC;
} ThreadInfo;

typedef struct {
    ThreadInfo* storage;
    size_t size;
} ThreadBucket;

typedef struct {
    ThreadBucket* buckets;
    LightLock lock;
} ThreadMap;

typedef struct {
    ThreadFunc ep;
    void* arg;
    LightLock lock;
    CondVar cv;
    bool canCleanup;
} ThreadArgs;

static ThreadMap g_TM = {};
static u8 g_Enabled = false;

__attribute__((no_instrument_function)) static void threadMapInit(ThreadMap* tm) {
    tm->buckets = calloc(NUM_THREAD_BUCKETS, sizeof(ThreadBucket));
    LightLock_Init(&tm->lock);
}

__attribute__((no_instrument_function)) static void threadMapFree(ThreadMap* tm) {
    LightLock_Lock(&tm->lock);

    for (size_t i = 0u; i < NUM_THREAD_BUCKETS; ++i)
        free(tm->buckets[i].storage);

    free(tm->buckets);

    LightLock_Unlock(&tm->lock);
}

__attribute__((no_instrument_function)) static bool threadMapUpdateImpl(ThreadBucket* bucket, u32 tid, u32 pc) {
    for (size_t i = 0; i < bucket->size; ++i) {
        ThreadInfo* tinfo = &bucket->storage[i];
        if (tinfo->tid == tid) {
            tinfo->lastPC = pc;
            return true;
        }
    }

    return false;
}

__attribute__((no_instrument_function)) static void threadMapInsert(ThreadMap* tm, u32 tid, u32 ep) {
    LightLock_Lock(&tm->lock);
    ThreadBucket* bucket = &tm->buckets[tid & 0x0F];

    if (!threadMapUpdateImpl(bucket, tid, ep)) {
        bucket->storage = realloc(bucket->storage, (bucket->size + 1) * sizeof(ThreadInfo));
        ThreadInfo* tinfo = &bucket->storage[bucket->size];
        tinfo->tid = tid;
        tinfo->lastPC = ep;
        ++bucket->size;
    }

    LightLock_Unlock(&tm->lock);
}

__attribute__((no_instrument_function)) static void threadMapRemove(ThreadMap* tm, u32 tid) {
    LightLock_Lock(&tm->lock);
    ThreadBucket* bucket = &tm->buckets[tid & 0x0F];

    for (size_t i = 0; i < bucket->size; ++i) {
        ThreadInfo* tinfo = &bucket->storage[i];
        if (tinfo->tid == tid) {
            ThreadInfo* newStorage = malloc((bucket->size - 1) * sizeof(ThreadInfo));
            memcpy(newStorage, bucket->storage, i * sizeof(ThreadInfo));
            memcpy(&newStorage[i], &bucket->storage[i + 1], (bucket->size - i - 1) * sizeof(ThreadInfo));
            free(bucket->storage);
            bucket->storage = newStorage;
            --bucket->size;
        }
    }

    LightLock_Unlock(&tm->lock);
}

__attribute__((no_instrument_function)) static void threadMapUpdate(ThreadMap* tm, u32 tid, u32 pc) {
    LightLock_Lock(&tm->lock);
    ThreadBucket* bucket = &tm->buckets[tid & 0x0F];
    threadMapUpdateImpl(bucket, tid, pc);
    LightLock_Unlock(&tm->lock);
}

__attribute__((no_instrument_function)) static void threadMapGetAddresses(ThreadMap* tm, u32* addrsOut, size_t* numAddrs, size_t maxAddrs) {
    LightLock_Lock(&tm->lock);

    size_t index = 0;
    for (size_t i = 0; i < NUM_THREAD_BUCKETS && index < maxAddrs; ++i) {
        const ThreadBucket* bucket = &tm->buckets[i];
        for (size_t j = 0; j < bucket->size && index < maxAddrs; ++j) {
            const ThreadInfo* tinfo = &bucket->storage[j];
            addrsOut[index++] = tinfo->lastPC;
        }
    }

    *numAddrs = index;
    LightLock_Unlock(&tm->lock);
}

__attribute__((no_instrument_function)) static u32 thisTid(void) {
    u32 tid;
    svcGetThreadId(&tid, CUR_THREAD_HANDLE);
    return tid;
}

__attribute__((no_instrument_function)) static void onThreadCreate(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;

    ThreadFunc ep = args->ep;
    arg = args->arg;

    args->canCleanup = true;
    CondVar_WakeUp(&args->cv, 1);

    const u32 tid = thisTid();
    threadMapInsert(&g_TM, tid, (u32)ep);
    ep(arg);
    threadMapRemove(&g_TM, tid);

    svcExitThread();
}

extern Result __real_svcCreateThread(Handle* thread, ThreadFunc ep, u32 arg, u32* stackTop, s32 threadPrio, s32 procId);

__attribute__((no_instrument_function)) Result __wrap_svcCreateThread(Handle* thread, ThreadFunc ep, u32 arg, u32* stackTop, s32 threadPrio, s32 procId) {
    if (g_Enabled) {
        ThreadArgs args;
        args.ep = ep;
        args.arg = (void*)arg;
        args.canCleanup = false;

        LightLock_Init(&args.lock);
        CondVar_Init(&args.cv);

        Result ret = __real_svcCreateThread(thread, onThreadCreate, (u32)&args, stackTop, threadPrio, procId);

        if (R_SUCCEEDED(ret)) {
            LightLock_Lock(&args.lock);

            while (!args.canCleanup)
                CondVar_Wait(&args.cv, &args.lock);

            LightLock_Unlock(&args.lock);
        }

        return ret;
    }

    return __real_svcCreateThread(thread, ep, arg, stackTop, threadPrio, procId);
}

__attribute__((no_instrument_function)) static void setEnabled(bool enabled) {
    do {
        __ldrexb(&g_Enabled);
    } while (__strexb(&g_Enabled, enabled ? 1 : 0));
}

__attribute__((no_instrument_function)) void profilerEnableThreadHooks(void) {
    threadMapInit(&g_TM);
    threadMapInsert(&g_TM, thisTid(), 0);

    // Will not work, C functions need thread vars to be setup, but our entrypoint runs before that.
    //setEnabled(true);
}

__attribute__((no_instrument_function)) void profilerDisableThreadHooks(void) {
    //setEnabled(false);
    threadMapFree(&g_TM);
}

__attribute__((no_instrument_function)) void profilerUpdateThreadPC(u32 pc) { threadMapUpdate(&g_TM, thisTid(), pc); }

__attribute__((no_instrument_function)) void profilerGetThreadAddresses(u32* addrsOut, size_t* numAddrs, size_t maxAddrs) {
    threadMapGetAddresses(&g_TM, addrsOut, numAddrs, maxAddrs);
}