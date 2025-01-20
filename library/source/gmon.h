#ifndef _CTR_GMON_H
#define _CTR_GMON_H

#include <3ds.h>
#include "hashMap.h"

typedef struct {
    LightLock lock;
    u32 textBase;
    u32 textSize;
    u16* buckets;
    size_t numBuckets;
    HashMap cgArcs;
} GmonCtx;

void gmonInit(GmonCtx* ctx);
void gmonFree(GmonCtx* ctx);
void gmonAddArc(GmonCtx* ctx, u32 from, u32 to);
bool gmonSave(GmonCtx* ctx, const char* filename);

#endif /* _CTR_GMON_H */