#include "thread.h"
#include "profil.h"

#define MAX_ADDRS 64

static Thread g_ProfilerThread;
static LightLock g_ProfilerLock;
static u16* g_Buckets = NULL;
static size_t g_NumBuckets = 0;
static u32 g_VBase = 0;
static u64 g_Scale = 0;

__attribute__((no_instrument_function)) static void profilerThread(void*) {
    Handle timer;
    svcCreateTimer(&timer, RESET_PULSE);
    svcSetTimer(timer, 0, 10 * 1000 * 1000);

    LightLock_Lock(&g_ProfilerLock);

    while (g_Buckets) {
        LightLock_Unlock(&g_ProfilerLock);
        svcWaitSynchronization(timer, U64_MAX);
        LightLock_Lock(&g_ProfilerLock);

        if (!g_Buckets)
            break;

        u32 addrs[MAX_ADDRS];
        size_t numAddrs = 0;
        profilerGetThreadAddresses(addrs, &numAddrs, MAX_ADDRS);
        for (size_t i = 0; i < numAddrs; ++i) {
            const u16 index = ((addrs[i] - g_VBase) * g_Scale) >> 16;
            if (index < g_NumBuckets) {
                if (g_Buckets[index] < UINT16_MAX)
                    ++g_Buckets[index];
            }
        }
    }

    LightLock_Unlock(&g_ProfilerLock);
}

__attribute__((no_instrument_function)) int profil(unsigned short *buf, size_t bufsiz, size_t offset, unsigned int scale) {
    if (buf) {
        g_Buckets = buf;
        g_NumBuckets = bufsiz >> 1;
        g_VBase = offset;
        g_Scale = scale;
        
        LightLock_Init(&g_ProfilerLock);
        LightLock_Lock(&g_ProfilerLock);

        APT_SetAppCpuTimeLimit(50);
        g_ProfilerThread = threadCreate(profilerThread, NULL, 0x1000, 0x18, 1, true);
        profilerEnableThreadHooks();

        LightLock_Unlock(&g_ProfilerLock);
    } else {
        LightLock_Lock(&g_ProfilerLock);

        profilerDisableThreadHooks();
        g_Buckets = NULL;
        g_NumBuckets = 0;
        g_VBase = 0;
        g_Scale = 0;
        
        LightLock_Unlock(&g_ProfilerLock);
        threadJoin(g_ProfilerThread, U64_MAX);
    }

    return 0;
}