#include "hashMap.h"

#include <stdlib.h>
#include <string.h>

#define IS_POW2(v) !((v) & ((v) - 1))

#define BUCKET_FOR_KEY(hm, key) (&hm->buckets[hm->params.hasher(key) & (hm->params.numBuckets - 1)])
#define BUCKET_ENTRY_SIZE(hm) (hm->params.keySize + hm->params.valueSize)

bool hashMapInit(HashMap* hm, const HashMapParams* params) {
    // Num of buckets must be a power of 2.
    if (IS_POW2(params->numBuckets)) {
        memcpy(&hm->params, params, sizeof(HashMapParams));
        LightLock_Init(&hm->lock);
        hm->buckets = calloc(hm->params.numBuckets, sizeof(HashMapBucket));
        return hm->buckets;
    }

    return false;
}

void hashMapFree(HashMap* hm) {
    LightLock_Lock(&hm->lock);

    for (size_t i = 0; i < hm->params.numBuckets; ++i) {
        HashMapBucket* bucket = &hm->buckets[i];
        free(bucket->storage);
    }

    free(hm->buckets);
    LightLock_Unlock(&hm->lock);
}

static bool updateImpl(HashMap* hm, HashMapBucket* bucket, const u8* key, const u8* value) {
    for (size_t i = 0; i < bucket->count; ++i) {
        u8* p = &bucket->storage[i * BUCKET_ENTRY_SIZE(hm)];
        if (hm->params.comparator(p, key)) {
            memcpy(&p[hm->params.keySize], value, hm->params.valueSize);
            return true;
        }
    }

    return false;
}

bool hashMapInsert(HashMap* hm, const u8* key, const u8* value) {
    LightLock_Lock(&hm->lock);
    HashMapBucket* bucket = BUCKET_FOR_KEY(hm, key);
    bool success = false;

    if (!updateImpl(hm, bucket, key, value)) {
        u8* newStorage = realloc(bucket->storage, (bucket->count + 1) * BUCKET_ENTRY_SIZE(hm));
        if (newStorage) {
            bucket->storage = newStorage;
            u8* p = &bucket->storage[bucket->count * BUCKET_ENTRY_SIZE(hm)];
            memcpy(p, key, hm->params.keySize);
            memcpy(&p[hm->params.keySize], value, hm->params.valueSize);
            ++bucket->count;
            success = true;
        }
    } else {
        success = true;
    }

    LightLock_Unlock(&hm->lock);
    return success;
}

bool hashMapRemove(HashMap* hm, const u8* key) {
    LightLock_Lock(&hm->lock);
    HashMapBucket* bucket = BUCKET_FOR_KEY(hm, key);
    bool success = false;

    for (size_t i = 0; i < bucket->count; ++i) {
        u8* p = &bucket->storage[i * BUCKET_ENTRY_SIZE(hm)];
        if (hm->params.comparator(p, key)) {
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

    LightLock_Unlock(&hm->lock);
    return success;
}

bool hashMapUpdate(HashMap* hm, const u8* key, const u8* value) {
    LightLock_Lock(&hm->lock);
    HashMapBucket* bucket = BUCKET_FOR_KEY(hm, key);
    bool ret = updateImpl(hm, bucket, key, value);
    LightLock_Unlock(&hm->lock);
    return ret;
}

static void getMultiImpl(HashMap* hm, u8* buffer, size_t* numItems, size_t maxItems, size_t itemSize, size_t itemOffset) {
    size_t index = 0;
    for (size_t i = 0; i < hm->params.numBuckets && index < maxItems; ++i) {
        const HashMapBucket* bucket = &hm->buckets[i];
        for (size_t j = 0; j < bucket->count && index < maxItems; ++j) {
            const u8* p = &bucket->storage[j * BUCKET_ENTRY_SIZE(hm)];
            memcpy(&buffer[index * itemSize], &p[itemOffset], itemSize);
            ++index;
        }
    }

    *numItems = index;
}

void hashMapGetKeys(HashMap* hm, u8* buffer, size_t* numKeys, size_t maxKeys) {
    LightLock_Lock(&hm->lock);
    getMultiImpl(hm, buffer, numKeys, maxKeys, hm->params.keySize, 0);
    LightLock_Unlock(&hm->lock);
}

void hashMapGetValues(HashMap* hm, u8* buffer, size_t* numValues, size_t maxValues) {
    LightLock_Lock(&hm->lock);
    getMultiImpl(hm, buffer, numValues, maxValues, hm->params.valueSize, hm->params.keySize);
    LightLock_Unlock(&hm->lock);
}