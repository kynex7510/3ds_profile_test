#include "profiler.h"
#include "gmon.h"

#include <stdlib.h>

#define TEXT_ADDR_TYPE 0x10005
#define TEXT_SIZE_TYPE 0x10002

#define DEFAULT_INTERVAL 10 * 1000 * 1000 // 10ms

static GmonCtx g_Ctx;

void __mcount_internal(u32 fromPC, u32 selfPC) {
    profilerUpdateThreadPC(selfPC);
    gmonAddArc(&g_Ctx, fromPC, selfPC);
}

static u32 readProcInfo(u32 type) {
    s64 v;
    if (R_FAILED(svcGetProcessInfo(&v, CUR_PROCESS_HANDLE, type)))
        svcBreak(USERBREAK_PANIC);

    return v;
}

void userAppInit(void) {
    g_Ctx.textBase = readProcInfo(TEXT_ADDR_TYPE);
    g_Ctx.textSize = readProcInfo(TEXT_SIZE_TYPE);
    g_Ctx.numBuckets = g_Ctx.textSize >> 2;
    g_Ctx.buckets = calloc(g_Ctx.numBuckets, sizeof(u16));
    gmonInit(&g_Ctx);

    profilerInit(g_Ctx.buckets, g_Ctx.numBuckets, g_Ctx.textBase, 0x4000, DEFAULT_INTERVAL);
    profilerRegisterThread(0);
}

void userAppExit(void) {
    profilerExit();
    gmonSave(&g_Ctx, "sdmc:/gmon.out");
    free(g_Ctx.buckets);
}