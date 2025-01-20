#ifndef _CTR_GPROF_HASHMAP_H
#define _CTR_GPROF_HASHMAP_H

#include <3ds.h>

typedef size_t (*HashMapHasher)(const u8* key);
typedef bool (*HashMapComparator)(const u8* keya, const u8* keyb);

typedef struct {
    u8* storage;
    size_t count;
} HashMapBucket;

typedef struct {
    HashMapBucket* buckets;
    size_t numBuckets;
    size_t keySize;
    size_t valueSize;
    HashMapHasher hasher;
    HashMapComparator comparator;
} HashMap;

bool hashMapAlloc(HashMap* hm);
void hashMapFree(HashMap* hm);

bool hashMapInsert(HashMap* hm, const u8* key, const u8* value);
bool hashMapRemove(HashMap* hm, const u8* key);
bool hashMapGet(HashMap* hm, const u8* key, u8* value);
bool hashMapUpdate(HashMap* hm, const u8* key, const u8* value);
size_t hashMapGetNumItems(HashMap* hm);

void hashMapGetItems(HashMap* hm, u8* keysBuffer, u8* valuesBuffer, size_t* numItems, size_t maxItems);

inline void hashMapGetKeys(HashMap* hm, u8* buffer, size_t* numKeys, size_t maxKeys) {
    hashMapGetItems(hm, buffer, NULL, numKeys, maxKeys);
}

inline void hashMapGetValues(HashMap* hm, u8* buffer, size_t* numValues, size_t maxValues) {
    hashMapGetItems(hm, NULL, buffer, numValues, maxValues);
}

#endif /* _CTR_GPROF_HASHMAP_H */