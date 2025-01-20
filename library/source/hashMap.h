#ifndef _CTR_GPROF_HASHMAP_H
#define _CTR_GPROF_HASHMAP_H

#include <3ds.h>

typedef size_t (*HashMapHasher)(const u8* key);
typedef bool (*HashMapComparator)(const u8* lhs, const u8* rhs);

typedef struct {
    u8* storage;
    size_t count;
} HashMapBucket;

typedef struct {
    size_t numBuckets;
    size_t keySize;
    size_t valueSize;
    HashMapHasher hasher;
    HashMapComparator comparator;
} HashMapParams;

typedef struct {
    HashMapParams params;
    LightLock lock;
    HashMapBucket* buckets;
} HashMap;

bool hashMapInit(HashMap* hm, const HashMapParams* params);
void hashMapFree(HashMap* hm);

bool hashMapInsert(HashMap* hm, const u8* key, const u8* value);
bool hashMapRemove(HashMap* hm, const u8* key);
bool hashMapUpdate(HashMap* hm, const u8* key, const u8* value);

void hashMapGetKeys(HashMap* hm, u8* buffer, size_t* numKeys, size_t maxKeys);
void hashMapGetValues(HashMap* hm, u8* buffer, size_t* numValues, size_t maxValues);

#endif /* _CTR_GPROF_HASHMAP_H */