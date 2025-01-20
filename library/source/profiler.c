#include "profiler.h"
#include "hashMap.h"

#define MAX_ADDRS 64

static HashMap g_ThreadPCs;

static Thread g_ProfilerThread;
static LightLock g_ProfilerLock;
static u16* g_Buckets = NULL;
static size_t g_NumBuckets = 0;
static u32 g_Base = 0;
static u64 g_Scale = 0;
static s64 g_Interval = 0;

// Defined in threadHook.c.
void profilerEnableThreadHook(void);
void profilerDisableThreadHook(void);

static void profilerThread(void*) {
    Handle timer;
    svcCreateTimer(&timer, RESET_PULSE);

    LightLock_Lock(&g_ProfilerLock);
    svcSetTimer(timer, 0, g_Interval);

    while (g_Buckets) {
        LightLock_Unlock(&g_ProfilerLock);
        svcWaitSynchronization(timer, U64_MAX);
        LightLock_Lock(&g_ProfilerLock);

        if (!g_Buckets)
            break;

        u32 addrs[MAX_ADDRS];
        size_t numAddrs = 0;
        hashMapGetValues(&g_ThreadPCs, (u8*)addrs, &numAddrs, MAX_ADDRS);
        for (size_t i = 0; i < numAddrs; ++i) {
            const u16 index = ((addrs[i] - g_Base) * g_Scale) >> 16;
            if (index < g_NumBuckets) {
                if (g_Buckets[index] < UINT16_MAX)
                    ++g_Buckets[index];
            }
        }
    }

    LightLock_Unlock(&g_ProfilerLock);
}

static size_t hasher(const u8* key) { return *(u32*)key; }
static bool comparator(const u8* lhs, const u8* rhs) { return *(u32*)lhs == *(u32*)rhs; }

void profilerInit(u16* buckets, size_t numBuckets, u32 base, size_t scale, s64 interval) {
    LightLock_Init(&g_ProfilerLock);
    LightLock_Lock(&g_ProfilerLock);

    HashMapParams hmParams;
    hmParams.numBuckets = 16;
    hmParams.keySize = sizeof(u32);
    hmParams.valueSize = sizeof(u32);
    hmParams.hasher = hasher;
    hmParams.comparator = comparator;
    hashMapInit(&g_ThreadPCs, &hmParams);

    g_Buckets = buckets;
    g_NumBuckets = numBuckets;
    g_Base = base;
    g_Scale = scale;
    g_Interval = interval;

    APT_SetAppCpuTimeLimit(50);
    g_ProfilerThread = threadCreate(profilerThread, NULL, 0x1000, 0x18, 1, true);
    profilerEnableThreadHook();

    LightLock_Unlock(&g_ProfilerLock);
}

void profilerExit(void) {
    LightLock_Lock(&g_ProfilerLock);
    profilerDisableThreadHook();

    g_Buckets = NULL;
    g_NumBuckets = 0;
    g_Base = 0;
    g_Scale = 0;
    g_Interval = 0;

    hashMapFree(&g_ThreadPCs);
    Thread t = g_ProfilerThread;

    LightLock_Unlock(&g_ProfilerLock);
    threadJoin(t, U64_MAX);
}

static u32 thisTid(void) {
    u32 tid;
    svcGetThreadId(&tid, CUR_THREAD_HANDLE);
    return tid;
}

void profilerRegisterThread(u32 ep) {
    const u32 tid = thisTid();
    hashMapInsert(&g_ThreadPCs, (const u8*)&tid, (const u8*)&ep);
}

void profilerUnregisterThread(void) {
    const u32 tid = thisTid();
    hashMapRemove(&g_ThreadPCs, (const u8*)&tid);
}

void profilerUpdateThreadPC(u32 pc) {
    const u32 tid = thisTid();
    hashMapUpdate(&g_ThreadPCs, (const u8*)&tid, (const u8*)&pc);
}