#include "profiler.h"

typedef struct {
    ThreadFunc ep;
    void* arg;
    LightLock lock;
    CondVar cv;
    bool canCleanup;
} ThreadArgs;

static u8 g_Enabled = 0;

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
    if (g_Enabled) {
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

    return __real_threadCreate(ep, arg, stackSize, prio, coreId, detached);
}

static void setEnabled(bool enabled) {
    do {
        __ldrexb(&g_Enabled);
    } while (__strexb(&g_Enabled, enabled ? 1 : 0));
}

void profilerEnableThreadHook(void) { setEnabled(true); }
void profilerDisableThreadHook(void) { setEnabled(false); }