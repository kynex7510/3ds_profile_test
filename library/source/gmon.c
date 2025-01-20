#include "gmon.h"

#include <stdio.h>
#include <string.h>

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

__attribute__((no_instrument_function)) static void writeHeader(FILE* f) {
    static const u8 GMON_HEADER[0x14] = {
        'g', 'm', 'o', 'n',     // Magic
        0x01, 0x00, 0x00, 0x00, // Version
    };

    fwrite(&GMON_HEADER, sizeof(GMON_HEADER), 1, f);
}

__attribute__((no_instrument_function)) static void writeHistogram(FILE* f, GmonCtx* ctx) {
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

__attribute__((no_instrument_function)) bool gmonSave(GmonCtx* ctx) {
    FILE* f = fopen("sdmc:/gmon.out", "wb");
    if (f) {
        writeHeader(f);
        writeHistogram(f, ctx);
        // TODO: CG
        fclose(f);
        return true;
    }

    return false;
}