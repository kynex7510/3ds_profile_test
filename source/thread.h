#ifndef _CTR_GPROF_THREAD_H
#define _CTR_GPROF_THREAD_H

#include <3ds.h>

void profilerEnableThreadHooks(void);
void profilerDisableThreadHooks(void);
void profilerUpdateThreadPC(u32 pc);
void profilerGetThreadAddresses(u32* addrsOut, size_t* numAddrs, size_t maxAddrs);

#endif /* _CTR_GPROF_THREAD_H */