#include "gmon.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define GMON_TAG_TIME_HIST 0
#define GMON_TAG_CG_ARC 1
#define GMON_TAG_BB_COUNT 2

typedef struct {
    u8 magic[4];
    u32 version;
    u32 _reserved[3];
} GmonHeader;

typedef struct {
    u32 lowPC;
    u32 highPC;
    u32 histSize;
    u32 profRate;
    char dimen[15];
    char dimenAbbrev;
} GmonHistHeader;

typedef struct {
    u32 fromPC;
    u32 selfPC;
    u32 count;
} GmonCgArcHeader;

static size_t cgArcHasher(const u8* key) { return *(u32*)key ^ *(u32*)(&key[4]); }
static bool cgArcComparator(const u8* keya, const u8* keyb) { return *(u64*)keya == *(u64*)keyb; }

void gmonInit(GmonCtx* ctx) {
    LightLock_Init(&ctx->lock);
    ctx->cgArcs.numBuckets = 16;
    ctx->cgArcs.keySize = sizeof(u64);
    ctx->cgArcs.valueSize = sizeof(u32);
    ctx->cgArcs.hasher = cgArcHasher;
    ctx->cgArcs.comparator = cgArcComparator;
    hashMapAlloc(&ctx->cgArcs);
}

void gmonFree(GmonCtx* ctx) { hashMapFree(&ctx->cgArcs); }

void gmonAddArc(GmonCtx* ctx, u32 from, u32 to) {
    if (ctx->buckets) {
        LightLock_Lock(&ctx->lock);
        const u64 key = ((u64)(from) << 32) | to;

        u32 count = 0;
        hashMapGet(&ctx->cgArcs, (const u8*)&key, (u8*)&count);

        ++count;
        hashMapInsert(&ctx->cgArcs, (const u8*)&key, (const u8*)&count);

        LightLock_Unlock(&ctx->lock);
    }
}

static void writeHeader(FILE* f) {
    static const u8 GMON_HEADER[0x14] = {
        'g', 'm', 'o', 'n',     // Magic
        0x01, 0x00, 0x00, 0x00, // Version
    };

    fwrite(&GMON_HEADER, sizeof(GMON_HEADER), 1, f);
}

static void writeHistogram(FILE* f, GmonCtx* ctx) {
    GmonHistHeader hdr;
    hdr.lowPC = ctx->textBase;
    hdr.highPC = ctx->textBase + ctx->textSize;
    hdr.histSize = ctx->numBuckets;
    hdr.profRate = 100;
    memcpy(hdr.dimen, "seconds", sizeof("seconds"));
    hdr.dimenAbbrev = 's';

    const u8 tag = GMON_TAG_TIME_HIST;
    fwrite(&tag, 1, 1, f);
    fwrite(&hdr, sizeof(GmonHistHeader), 1, f);
    fwrite(ctx->buckets, sizeof(u16), ctx->numBuckets, f);
}

static void writeCgArcs(FILE* f, GmonCtx* ctx) {
    size_t numArcs = hashMapGetNumItems(&ctx->cgArcs);
    u64* keysBuffer = malloc(numArcs * sizeof(u64));
    u32* valuesBuffer = malloc(numArcs * sizeof(u32));

    hashMapGetItems(&ctx->cgArcs, (u8*)keysBuffer, (u8*)valuesBuffer, &numArcs, numArcs);

    for (size_t i = 0; i < numArcs; ++i) {
        GmonCgArcHeader hdr;
        hdr.fromPC = keysBuffer[i] >> 32;
        hdr.selfPC = keysBuffer[i] & 0xFFFFFFFF;
        hdr.count = valuesBuffer[i];

        const u8 tag = GMON_TAG_CG_ARC;
        fwrite(&tag, 1, 1, f);
        fwrite(&hdr, sizeof(GmonCgArcHeader), 1, f);
    }

    free(keysBuffer);
    free(valuesBuffer);
}

bool gmonSave(GmonCtx* ctx, const char* filename) {
    LightLock_Lock(&ctx->lock);

    bool success = false;
    FILE* f = fopen(filename, "wb");
    if (f) {
        writeHeader(f);
        writeHistogram(f, ctx);
        writeCgArcs(f, ctx);
        fclose(f);
        success = true;
    }

    LightLock_Unlock(&ctx->lock);
    return success;
}