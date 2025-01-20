#ifndef _CTR_GMON_H
#define _CTR_GMON_H

#include <3ds.h>

typedef struct {
    u32 textBase;
    u32 textSize;
    u16* buckets;
    size_t numBuckets;
} GmonCtx;

bool gmonSave(GmonCtx* ctx);

#endif /* _CTR_GMON_H */