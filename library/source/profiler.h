#ifndef _CTR_GPROF_PROFILER_H
#define _CTR_GPROF_PROFILER_H

#include <3ds.h>

void profilerInit(u16* buckets, size_t numBuckets, u32 base, size_t scale, s64 interval);
void profilerExit(void);

void profilerRegisterThread(u32 ep);
void profilerUnregisterThread(void);
void profilerUpdateThreadPC(u32 pc);

#endif /* _CTR_GPROF_PROFILER_H */