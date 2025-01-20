#include "hashMap.h"

#include <stdlib.h>
#include <string.h>

#define IS_POW2(v) !((v) & ((v) - 1))

#define BUCKET_FOR_KEY(hm, key) (&hm->buckets[hm->hasher(key) & (hm->numBuckets - 1)])
#define BUCKET_ENTRY_SIZE(hm) (hm->keySize + hm->valueSize)

bool hashMapAlloc(HashMap* hm) {
    // Num of buckets must be a power of 2.
    if (IS_POW2(hm->numBuckets)) {
        hm->buckets = calloc(hm->numBuckets, sizeof(HashMapBucket));
        return hm->buckets;
    }

    return false;
}

void hashMapFree(HashMap* hm) {
    for (size_t i = 0; i < hm->numBuckets; ++i) {
        HashMapBucket* bucket = &hm->buckets[i];
        free(bucket->storage);
    }

    free(hm->buckets);
}

static bool updateImpl(HashMap* hm, HashMapBucket* bucket, const u8* key, const u8* value) {
    for (size_t i = 0; i < bucket->count; ++i) {
        u8* p = &bucket->storage[i * BUCKET_ENTRY_SIZE(hm)];
        if (hm->comparator(p, key)) {
            memcpy(&p[hm->keySize], value, hm->valueSize);
            return true;
        }
    }

    return false;
}

bool hashMapInsert(HashMap* hm, const u8* key, const u8* value) {
    HashMapBucket* bucket = BUCKET_FOR_KEY(hm, key);
    bool success = false;

    if (!updateImpl(hm, bucket, key, value)) {
        u8* newStorage = realloc(bucket->storage, (bucket->count + 1) * BUCKET_ENTRY_SIZE(hm));
        if (newStorage) {
            bucket->storage = newStorage;
            u8* p = &bucket->storage[bucket->count * BUCKET_ENTRY_SIZE(hm)];
            memcpy(p, key, hm->keySize);
            memcpy(&p[hm->keySize], value, hm->valueSize);
            ++bucket->count;
            success = true;
        }
    } else {
        success = true;
    }

    return success;
}

bool hashMapRemove(HashMap* hm, const u8* key) {
    HashMapBucket* bucket = BUCKET_FOR_KEY(hm, key);
    bool success = false;

    for (size_t i = 0; i < bucket->count; ++i) {
        u8* p = &bucket->storage[i * BUCKET_ENTRY_SIZE(hm)];
        if (hm->comparator(p, key)) {
            u8* newStorage = malloc((bucket->count + 1) * BUCKET_ENTRY_SIZE(hm));
            if (newStorage) {
                memcpy(newStorage, bucket->storage, i * BUCKET_ENTRY_SIZE(hm));
                memcpy(&newStorage[i * BUCKET_ENTRY_SIZE(hm)], &bucket->storage[(i + 1) * BUCKET_ENTRY_SIZE(hm)],
                    (bucket->count - i - 1) * BUCKET_ENTRY_SIZE(hm));
                free(bucket->storage);
                bucket->storage = newStorage;
                --bucket->count;
                success = true;
            }
        }
    }

    return success;
}

bool hashMapGet(HashMap* hm, const u8* key, u8* value) {
    const HashMapBucket* bucket = BUCKET_FOR_KEY(hm, key);
    for (size_t i = 0; i < bucket->count; ++i) {
        const u8* p = &bucket->storage[i * BUCKET_ENTRY_SIZE(hm)];
        if (hm->comparator(p, key)) {
            memcpy(value, &p[hm->keySize], hm->valueSize);
            return true;
        }
    }

    return false;
}

bool hashMapUpdate(HashMap* hm, const u8* key, const u8* value) {
    HashMapBucket* bucket = BUCKET_FOR_KEY(hm, key);
    bool ret = updateImpl(hm, bucket, key, value);
    return ret;
}

size_t hashMapGetNumItems(HashMap* hm) {
    size_t count = 0;
    for (size_t i = 0; i < hm->numBuckets; ++i) {
        HashMapBucket* bucket = &hm->buckets[i];
        count += bucket->count;
    }

    return count;
}

void hashMapGetItems(HashMap* hm, u8* keysBuffer, u8* valuesBuffer, size_t* numItems, size_t maxItems) {
    if (!keysBuffer && !valuesBuffer) {
        *numItems = 0;
        return;
    }

    size_t index = 0;
    for (size_t i = 0; i < hm->numBuckets && index < maxItems; ++i) {
        const HashMapBucket* bucket = &hm->buckets[i];
        for (size_t j = 0; j < bucket->count && index < maxItems; ++j) {
            const u8* p = &bucket->storage[j * BUCKET_ENTRY_SIZE(hm)];

            if (keysBuffer)
                memcpy(&keysBuffer[index * hm->keySize], p, hm->keySize);

            if (valuesBuffer)
                memcpy(&valuesBuffer[index * hm->valueSize], &p[hm->keySize], hm->valueSize);

            ++index;
        }
    }

    *numItems = index;
}