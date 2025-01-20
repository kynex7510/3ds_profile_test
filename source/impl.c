#include <3ds.h>

#include "profil.h"
#include "thread.h"
#include "gmon.h"

#include <stdlib.h>

#define TEXT_ADDR_TYPE 0x10005
#define TEXT_SIZE_TYPE 0x10002

static GmonCtx g_Ctx;

__attribute__((no_instrument_function)) void __mcount_internal(u32 fromPC, u32 selfPC) { profilerUpdateThreadPC(selfPC); }

__attribute__((no_instrument_function)) static u32 readProcInfo(u32 type) {
    s64 v;
    if (R_FAILED(svcGetProcessInfo(&v, CUR_PROCESS_HANDLE, type)))
        svcBreak(USERBREAK_PANIC);

    return v;
}

__attribute__((no_instrument_function)) void userAppInit(void) {
    g_Ctx.textBase = readProcInfo(TEXT_ADDR_TYPE);
    g_Ctx.textSize = readProcInfo(TEXT_SIZE_TYPE);
    g_Ctx.numBuckets = g_Ctx.textSize >> 2;
    g_Ctx.buckets = calloc(g_Ctx.numBuckets, sizeof(u16));

    profil(g_Ctx.buckets, g_Ctx.numBuckets * sizeof(u16), g_Ctx.textBase, 0x4000);
}

__attribute__((no_instrument_function)) void userAppExit(void) {
    profil(NULL, 0, 0, 0);
    gmonSave(&g_Ctx);
    free(g_Ctx.buckets);
}