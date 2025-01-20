#include "profiler.h"

typedef struct {
    ThreadFunc ep;
    void* arg;
    LightLock lock;
    CondVar cv;
    bool canCleanup;
} ThreadArgs;

static void onThreadCreate(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    ThreadFunc ep = args->ep;
    arg = args->arg;
    args->canCleanup = true;
    CondVar_WakeUp(&args->cv, 1);

    profilerRegisterThread((u32)ep);
    ep(arg);
    profilerUnregisterThread();
}

extern Thread __real_threadCreate(ThreadFunc ep, void* arg, size_t stackSize, int prio, int coreId, bool detached);

Thread __wrap_threadCreate(ThreadFunc ep, void* arg, size_t stackSize, int prio, int coreId, bool detached) {
    ThreadArgs args;
    args.ep = ep;
    args.arg = arg;
    args.canCleanup = false;

    LightLock_Init(&args.lock);
    CondVar_Init(&args.cv);

    Thread t = __real_threadCreate(onThreadCreate, &args, stackSize, prio, coreId, detached);
    if (t) {
        LightLock_Lock(&args.lock);

        while (!args.canCleanup)
            CondVar_Wait(&args.cv, &args.lock);

        LightLock_Unlock(&args.lock);
    }

    return t;
}