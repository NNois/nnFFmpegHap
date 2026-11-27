/*
 * KTX2 (Khronos Texture 2.0) encoder — BC7 + mip levels + Zstandard
 * Alternative Developpement 2026 (AdFlocon NOTE-80, "A bis")
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * One RGBA picture in, one complete .ktx2 file out (like the png encoder):
 *  - level 0 = the picture itself, levels 1..n down to 1x1 (option mipmaps);
 *  - mip levels computed in LINEAR light for sRGB pictures (option srgb), by
 *    the same 2x2 average and the same "Sharpen 0..10" kernel as AdFlocon's
 *    engine (src/engine/media/mipSharpenMath.ts, itself read from Unreal's
 *    TextureCompressorModule.cpp): the chain of 2x2 averages goes down, each
 *    level written is a sharpened version of it, colour kept from the average
 *    and set to the sharpened luminance (no colour shift), clamped edges;
 *  - every level BC7-encoded by basis_universal (the Hap R encoder), in
 *    slices on the codec threads; a level whose size is not a multiple of 4
 *    is padded by repeating its last row / column;
 *  - every level Zstandard-compressed (KTX2 supercompressionScheme 2), or
 *    stored raw (option zstd_level=0);
 *  - key/value data: KTXwriter, adtv.mipmaps, adtv.mipSharpen,
 *    adtv.bc7Quality, adtv.displayAspect (the exact proportions to lay the
 *    image out at, from the sample aspect ratio), plus any adtv.* pair
 *    given by option kv.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
/* Zstandard comes from basis_universal (zstd/zstd.c is in libbasisu_encoder.a, the
 * library of the Hap R encoder): linking the system libzstd as well would define
 * every ZSTD_* symbol twice. The header is the one of that copy (-I$BASISU_ROOT). */
#include "zstd/zstd.h"

#include "libavutil/avstring.h"
#include "libavutil/common.h"
#include "libavutil/dict.h"
#include "libavutil/imgutils.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"
#include "libavutil/ffversion.h"

#include "avcodec.h"
#include "basisu_wrapper.h"
#include "bc7opaque.h"
#include "codec_internal.h"
#include "encode.h"
#include "ktx2.h"
#include "texturedsp.h"

#define KTX2_MAX_LEVELS  16   /* 16384 px -> 15 levels */
#define KTX2_MAX_KV      32
#define KERNEL_SIZE      8

typedef struct KTX2EncContext {
    const AVClass *class;
    int mipmaps;
    int mip_sharpen;
    int srgb;
    int bc7_quality;
    int zstd_level;
    AVDictionary *kv;

    ZSTD_CCtx *zctx;
    float srgb_to_linear[256];
} KTX2EncContext;

/** A level in linear light: straight RGBA floats (AdFlocon's LinearLevel). */
typedef struct LinearLevel {
    int width, height;
    float *data;
} LinearLevel;

/** A level as it goes into the file. */
typedef struct Level {
    int width, height;          /* pixels */
    int bw, bh;                 /* 4x4 blocks */
    uint8_t *rgba;              /* padded RGBA8, (bw * 4) x (bh * 4) */
    uint8_t *bc7;               /* bw * bh * 16 bytes */
    size_t bc7_size;
    uint8_t *packed;            /* zstd frame (owned), or NULL = bc7 stored raw */
    size_t packed_size;
    uint64_t offset;            /* in the file */
} Level;

/* ------------------------------------------------------------------------
 * The mip chain — a line-for-line port of mipSharpenMath.ts. Values are
 * STORED as float (Float32Array) and COMPUTED in double, as JavaScript does.
 * ------------------------------------------------------------------------ */

static void sharpen_kernel_1d(int level, float table[KERNEL_SIZE])
{
    const double sharpen = level * 0.2 * 2;   /* x 2 for the 8 x 8 kernel */
    float positive[KERNEL_SIZE] = { 0 }, negative[KERNEL_SIZE] = { 0 };
    float previous[KERNEL_SIZE];

    positive[KERNEL_SIZE / 2] = positive[KERNEL_SIZE / 2 - 1] = (float)(0.5 * (1 + sharpen));
    negative[KERNEL_SIZE / 2] = negative[KERNEL_SIZE / 2 - 1] = (float)(0.5 * -sharpen);

    /* blur(positive, 1) then blur(negative, 3): 1-2-1 box over 3 taps */
    for (int pass = 0; pass < 4; pass++) {
        float *t = pass == 0 ? positive : negative;
        memcpy(previous, t, sizeof(previous));
        for (int x = 0; x < KERNEL_SIZE; x++) {
            double sum = previous[x];
            if (x)                  sum += previous[x - 1];
            if (x < KERNEL_SIZE - 1) sum += previous[x + 1];
            t[x] = (float)(sum / 3);
        }
    }
    for (int x = 0; x < KERNEL_SIZE; x++)
        table[x] = (float)((double)positive[x] + negative[x]);
}

static inline int clamp_index(int v, int size)
{
    return v < 0 ? 0 : v >= size ? size - 1 : v;
}

static int average_down(const LinearLevel *src, LinearLevel *dst)
{
    const int width  = FFMAX(1, src->width  >> 1);
    const int height = FFMAX(1, src->height >> 1);
    const float *s = src->data;
    float *d = av_malloc_array((size_t)width * height, 4 * sizeof(float));

    if (!d)
        return AVERROR(ENOMEM);
    for (int y = 0; y < height; y++) {
        const int y0 = clamp_index(2 * y, src->height), y1 = clamp_index(2 * y + 1, src->height);
        for (int x = 0; x < width; x++) {
            const int x0 = clamp_index(2 * x, src->width), x1 = clamp_index(2 * x + 1, src->width);
            const size_t a = ((size_t)y0 * src->width + x0) * 4, b = ((size_t)y0 * src->width + x1) * 4;
            const size_t c = ((size_t)y1 * src->width + x0) * 4, e = ((size_t)y1 * src->width + x1) * 4;
            const size_t o = ((size_t)y * width + x) * 4;
            for (int k = 0; k < 4; k++)
                d[o + k] = (float)(((double)s[a + k] + s[b + k] + s[c + k] + s[e + k]) * 0.25);
        }
    }
    dst->width  = width;
    dst->height = height;
    dst->data   = d;
    return 0;
}

/** A sharpened level from its parent and the 2x2 average of that parent. */
static int sharpen_down(const LinearLevel *src, const LinearLevel *average,
                        const float kernel[KERNEL_SIZE], LinearLevel *dst)
{
    const int width = average->width, height = average->height;
    const int centre = (KERNEL_SIZE - 1) >> 1;   /* 3: Unreal's KernelCenter */
    const float *s = src->data, *avg = average->data;
    float *tmp = av_malloc_array((size_t)width * src->height, 4 * sizeof(float));
    float *d   = av_malloc_array((size_t)width * height, 4 * sizeof(float));

    if (!tmp || !d) {
        av_free(tmp);
        av_free(d);
        return AVERROR(ENOMEM);
    }
    for (int y = 0; y < src->height; y++) {
        const size_t row = (size_t)y * src->width;
        for (int x = 0; x < width; x++) {
            double r = 0, g = 0, b = 0, a = 0;
            for (int k = 0; k < KERNEL_SIZE; k++) {
                const size_t i = (row + clamp_index(2 * x + k - centre, src->width)) * 4;
                const double w = kernel[k];
                r += w * s[i]; g += w * s[i + 1]; b += w * s[i + 2]; a += w * s[i + 3];
            }
            {
                const size_t o = ((size_t)y * width + x) * 4;
                tmp[o] = (float)r; tmp[o + 1] = (float)g; tmp[o + 2] = (float)b; tmp[o + 3] = (float)a;
            }
        }
    }
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double r = 0, g = 0, b = 0, a = 0;
            for (int k = 0; k < KERNEL_SIZE; k++) {
                const size_t i = ((size_t)clamp_index(2 * y + k - centre, src->height) * width + x) * 4;
                const double w = kernel[k];
                r += w * tmp[i]; g += w * tmp[i + 1]; b += w * tmp[i + 2]; a += w * tmp[i + 3];
            }
            {
                /* no colour shift: the average colour, set to the sharpened luminance */
                const size_t o = ((size_t)y * width + x) * 4;
                const double new_lum = 0.3 * r + 0.59 * g + 0.11 * b;
                const double fr = avg[o], fg = avg[o + 1], fb = avg[o + 2];
                const double old_lum = 0.3 * fr + 0.59 * fg + 0.11 * fb;
                const double k = old_lum > 0.001 ? new_lum / old_lum : 1;
                d[o] = (float)(fr * k); d[o + 1] = (float)(fg * k); d[o + 2] = (float)(fb * k); d[o + 3] = (float)a;
            }
        }
    }
    av_free(tmp);
    dst->width  = width;
    dst->height = height;
    dst->data   = d;
    return 0;
}

/** JavaScript's Math.round for the values met here (>= 0). */
static inline int js_round(double v)
{
    return (int)floor(v + 0.5);
}

static inline uint8_t linear_to_srgb_byte(double v)
{
    const double c = v <= 0 ? 0 : v >= 1 ? 1 : v;
    const double s = c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1 / 2.4) - 0.055;
    return av_clip_uint8(js_round(s * 255));
}

static inline uint8_t unit_to_byte(double v)
{
    return av_clip_uint8(js_round((v <= 0 ? 0 : v >= 1 ? 1 : v) * 255));
}

/* ------------------------------------------------------------------------
 * Levels to blocks
 * ------------------------------------------------------------------------ */

static int level_alloc(Level *lv, int width, int height)
{
    lv->width    = width;
    lv->height   = height;
    lv->bw       = (width  + 3) >> 2;
    lv->bh       = (height + 3) >> 2;
    lv->bc7_size = (size_t)lv->bw * lv->bh * KTX2_BC7_BLOCK_BYTES;
    lv->rgba     = av_malloc((size_t)lv->bw * 4 * lv->bh * 4 * 4);
    lv->bc7      = av_malloc(lv->bc7_size);
    return lv->rgba && lv->bc7 ? 0 : AVERROR(ENOMEM);
}

/** Repeats the last column and row into the 4x4 padding. */
static void level_pad(Level *lv)
{
    const int pw = lv->bw * 4, ph = lv->bh * 4;
    const ptrdiff_t stride = (ptrdiff_t)pw * 4;

    for (int y = 0; y < lv->height; y++) {
        uint8_t *row = lv->rgba + y * stride;
        for (int x = lv->width; x < pw; x++)
            memcpy(row + x * 4, row + (lv->width - 1) * 4, 4);
    }
    for (int y = lv->height; y < ph; y++)
        memcpy(lv->rgba + y * stride, lv->rgba + (lv->height - 1) * stride, stride);
}

/** Level 0: the picture, byte for byte. */
static void level_from_frame(Level *lv, const AVFrame *frame)
{
    const ptrdiff_t stride = (ptrdiff_t)lv->bw * 4 * 4;

    for (int y = 0; y < lv->height; y++)
        memcpy(lv->rgba + y * stride, frame->data[0] + y * frame->linesize[0], (size_t)lv->width * 4);
    level_pad(lv);
}

/** A mip level from its linear values. */
static void level_from_linear(Level *lv, const LinearLevel *src, int srgb)
{
    const ptrdiff_t stride = (ptrdiff_t)lv->bw * 4 * 4;

    for (int y = 0; y < lv->height; y++) {
        uint8_t *row = lv->rgba + y * stride;
        const float *in = src->data + (size_t)y * src->width * 4;
        for (int x = 0; x < lv->width; x++) {
            for (int k = 0; k < 3; k++)
                row[x * 4 + k] = srgb ? linear_to_srgb_byte(in[x * 4 + k]) : unit_to_byte(in[x * 4 + k]);
            row[x * 4 + 3] = unit_to_byte(in[x * 4 + 3]);
        }
    }
    level_pad(lv);
}

static int linear_from_frame(KTX2EncContext *ctx, const AVFrame *frame, int width, int height,
                             LinearLevel *dst)
{
    float *d = av_malloc_array((size_t)width * height, 4 * sizeof(float));

    if (!d)
        return AVERROR(ENOMEM);
    for (int y = 0; y < height; y++) {
        const uint8_t *row = frame->data[0] + y * frame->linesize[0];
        float *o = d + (size_t)y * width * 4;
        for (int x = 0; x < width; x++) {
            for (int k = 0; k < 3; k++)
                o[x * 4 + k] = ctx->srgb ? ctx->srgb_to_linear[row[x * 4 + k]] : (float)(row[x * 4 + k] / 255.0);
            o[x * 4 + 3] = (float)(row[x * 4 + 3] / 255.0);
        }
    }
    dst->width  = width;
    dst->height = height;
    dst->data   = d;
    return 0;
}

/**
 * AN OPAQUE BLOCK DECODES WITH AN ALPHA OF EXACTLY 255 (bc7opaque.h — basis_universal left 254 on 28.6 % of the
 * pixels of an opaque photo at bc7_quality 3): block by block, so the opaque parts of a picture WITH alpha too.
 */
static int ktx2_bc7_block(uint8_t *dst, ptrdiff_t stride, const uint8_t *block)
{
    const int ret = basisu_bc7_encode_block(dst, stride, block);

    if (ff_bc7_source_opaque(block, stride))
        ff_bc7_fix_opaque_alpha(dst);
    return ret;
}

static int ktx2_bc7_block_rgba(uint8_t *dst, ptrdiff_t stride, const uint8_t *block)
{
    const int ret = basisu_bc7_encode_block_rgba(dst, stride, block);

    if (ff_bc7_source_opaque(block, stride))
        ff_bc7_fix_opaque_alpha(dst);
    return ret;
}

static int compress_level(AVCodecContext *avctx, Level *lv,
                          int (*block)(uint8_t *dst, ptrdiff_t stride, const uint8_t *src))
{
    TextureDSPThreadContext t = { 0 };

    t.frame_data.in = lv->rgba;
    t.stride        = (ptrdiff_t)lv->bw * 4 * 4;
    t.width         = lv->bw * 4;
    t.height        = lv->bh * 4;
    t.tex_data.out  = lv->bc7;
    t.tex_ratio     = KTX2_BC7_BLOCK_BYTES;
    t.raw_ratio     = 16;                       /* 4 RGBA8 pixels */
    t.slice_count   = av_clip(avctx->thread_count, 1, lv->bh);
    t.tex_funct     = block;
    return ff_texturedsp_exec_compress_threads(avctx, &t);
}

/* ------------------------------------------------------------------------
 * The file
 * ------------------------------------------------------------------------ */

typedef struct KeyValue {
    const char *key;
    const char *value;
} KeyValue;

static int kv_compare(const void *a, const void *b)
{
    return strcmp(((const KeyValue *)a)->key, ((const KeyValue *)b)->key);
}

static size_t kv_entry_size(const KeyValue *kv)
{
    const size_t length = strlen(kv->key) + 1 + strlen(kv->value) + 1;
    return 4 + FFALIGN(length, 4);
}

static void write_dfd_bc7(uint8_t *p, int srgb)
{
    memset(p, 0, KTX2_DFD_BC7_SIZE);
    AV_WL32(p +  0, KTX2_DFD_BC7_SIZE);                 /* dfdTotalSize */
    AV_WL32(p +  4, 0);                                 /* vendorId Khronos, descriptorType basic */
    AV_WL32(p +  8, KTX2_DF_VERSION_1_3 | (40 << 16));  /* versionNumber, descriptorBlockSize */
    p[12] = KTX2_DF_MODEL_BC7;
    p[13] = KTX2_DF_PRIMARIES_BT709;
    p[14] = srgb ? KTX2_DF_TRANSFER_SRGB : KTX2_DF_TRANSFER_LINEAR;
    p[15] = 0;                                          /* straight alpha */
    p[16] = 3; p[17] = 3; p[18] = 0; p[19] = 0;         /* texel block 4 x 4 x 1 x 1 (minus one) */
    p[20] = KTX2_BC7_BLOCK_BYTES;                       /* bytesPlane0 */
    AV_WL16(p + 28, 0);                                 /* sample: bitOffset */
    p[30] = 127;                                        /*         bitLength - 1 */
    p[31] = 0;                                          /*         channel BC7 colour */
    AV_WL32(p + 32, 0);                                 /*         samplePosition */
    AV_WL32(p + 36, 0);                                 /*         sampleLower */
    AV_WL32(p + 40, 0xFFFFFFFF);                        /*         sampleUpper */
}

static av_cold int ktx2_encode_init(AVCodecContext *avctx)
{
    KTX2EncContext *ctx = avctx->priv_data;
    const AVDictionaryEntry *e = NULL;
    int ret = av_image_check_size(avctx->width, avctx->height, 0, avctx);

    if (ret < 0)
        return ret;
    if (avctx->width > 16384 || avctx->height > 16384) {
        av_log(avctx, AV_LOG_ERROR, "KTX2: %dx%d is larger than 16384 px.\n", avctx->width, avctx->height);
        return AVERROR(EINVAL);
    }
    while ((e = av_dict_iterate(ctx->kv, e))) {
        if (!av_strstart(e->key, "adtv.", NULL)) {
            av_log(avctx, AV_LOG_ERROR, "KTX2: key \"%s\" refused - only adtv.* keys may be added "
                   "(KTX* and ktx* are reserved by the specification).\n", e->key);
            return AVERROR(EINVAL);
        }
    }
    for (int i = 0; i < 256; i++) {
        const double c = i / 255.0;
        ctx->srgb_to_linear[i] = (float)(c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4));
    }
    if (ctx->zstd_level > 0 && !(ctx->zctx = ZSTD_createCCtx()))
        return AVERROR(ENOMEM);
    basisu_init();
    basisu_bc7_set_quality(ctx->bc7_quality);
    return 0;
}

static av_cold int ktx2_encode_close(AVCodecContext *avctx)
{
    KTX2EncContext *ctx = avctx->priv_data;

    ZSTD_freeCCtx(ctx->zctx);
    ctx->zctx = NULL;
    return 0;
}

static int ktx2_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
                             const AVFrame *frame, int *got_packet)
{
    KTX2EncContext *ctx = avctx->priv_data;
    const int width = avctx->width, height = avctx->height;
    Level levels[KTX2_MAX_LEVELS] = { 0 };
    LinearLevel chain = { 0 };
    KeyValue kv[KTX2_MAX_KV];
    char quality[8], sharpen[8], display[32];
    const char *writer = "AD-TV FFmpeg " FFMPEG_VERSION " ktx2";
    int level_count = 1, kv_count = 0, has_alpha = 0, ret = 0;
    int (*block)(uint8_t *, ptrdiff_t, const uint8_t *);
    size_t kvd_size = 0, total;
    uint64_t dfd_offset, kvd_offset, pos;
    const AVDictionaryEntry *e = NULL;
    uint8_t *p;

    /* LEVEL 0, and whether the picture carries any alpha (BC7 RGBA path). */
    if ((ret = level_alloc(&levels[0], width, height)) < 0)
        goto end;
    level_from_frame(&levels[0], frame);
    for (int y = 0; y < height && !has_alpha; y++) {
        const uint8_t *row = frame->data[0] + y * frame->linesize[0];
        for (int x = 0; x < width; x++)
            if (row[x * 4 + 3] != 255) { has_alpha = 1; break; }
    }

    /* THE MIP CHAIN down to 1 x 1. */
    if (ctx->mipmaps && (width > 1 || height > 1)) {
        float kernel[KERNEL_SIZE];
        sharpen_kernel_1d(ctx->mip_sharpen, kernel);
        if ((ret = linear_from_frame(ctx, frame, width, height, &chain)) < 0)
            goto end;
        while ((chain.width > 1 || chain.height > 1) && level_count < KTX2_MAX_LEVELS) {
            LinearLevel average, out;
            if ((ret = average_down(&chain, &average)) < 0)
                goto end;
            if (ctx->mip_sharpen > 0) {
                if ((ret = sharpen_down(&chain, &average, kernel, &out)) < 0) {
                    av_free(average.data);
                    goto end;
                }
            } else {
                out = average;
            }
            if ((ret = level_alloc(&levels[level_count], out.width, out.height)) < 0) {
                if (out.data != average.data)
                    av_free(out.data);
                av_free(average.data);
                goto end;
            }
            level_from_linear(&levels[level_count], &out, ctx->srgb);
            if (out.data != average.data)
                av_free(out.data);
            av_free(chain.data);
            chain = average;
            level_count++;
        }
    }

    /* BC7, then Zstandard, level by level. */
    block = has_alpha ? ktx2_bc7_block_rgba : ktx2_bc7_block;
    for (int i = 0; i < level_count; i++) {
        Level *lv = &levels[i];
        if ((ret = compress_level(avctx, lv, block)) < 0)
            goto end;
        if (ctx->zstd_level > 0) {
            const size_t bound = ZSTD_compressBound(lv->bc7_size);
            size_t size;
            if (!(lv->packed = av_malloc(bound))) {
                ret = AVERROR(ENOMEM);
                goto end;
            }
            size = ZSTD_compressCCtx(ctx->zctx, lv->packed, bound, lv->bc7, lv->bc7_size, ctx->zstd_level);
            if (ZSTD_isError(size)) {
                av_log(avctx, AV_LOG_ERROR, "KTX2: zstd failed on level %d: %s\n", i, ZSTD_getErrorName(size));
                ret = AVERROR_EXTERNAL;
                goto end;
            }
            lv->packed_size = size;
        }
    }

    /* KEY / VALUE DATA, sorted by key as the specification requires. */
    snprintf(quality, sizeof(quality), "%d", ctx->bc7_quality);
    snprintf(sharpen, sizeof(sharpen), "%d", ctx->mip_sharpen);
    kv[kv_count++] = (KeyValue){ "KTXwriter", writer };
    kv[kv_count++] = (KeyValue){ "adtv.bc7Quality", quality };
    kv[kv_count++] = (KeyValue){ "adtv.mipmaps", level_count > 1 ? "1" : "0" };
    if (level_count > 1)
        kv[kv_count++] = (KeyValue){ "adtv.mipSharpen", sharpen };
    /* THE DISPLAY ASPECT: a BC texture is scaled to a multiple of 4 (WebGPU), which bends the aspect of a small image
     * (30 x 18 -> 32 x 20). A scale in front keeps the display aspect in the sample aspect ratio (setsar=1 then scale);
     * written here exactly ("5:3"), the reader lays the image out at its real proportions. Square pixels: W:H. */
    {
        const AVRational sar = avctx->sample_aspect_ratio;
        int num, den;
        av_reduce(&num, &den, (int64_t)avctx->width * (sar.num > 0 && sar.den > 0 ? sar.num : 1),
                  (int64_t)avctx->height * (sar.num > 0 && sar.den > 0 ? sar.den : 1), INT_MAX);
        snprintf(display, sizeof(display), "%d:%d", num, den);
        kv[kv_count++] = (KeyValue){ "adtv.displayAspect", display };
    }
    while ((e = av_dict_iterate(ctx->kv, e)) && kv_count < KTX2_MAX_KV) {
        int known = 0;
        for (int i = 0; i < kv_count; i++)
            if (!strcmp(kv[i].key, e->key)) { kv[i].value = e->value; known = 1; }
        if (!known)
            kv[kv_count++] = (KeyValue){ e->key, e->value };
    }
    qsort(kv, kv_count, sizeof(*kv), kv_compare);
    for (int i = 0; i < kv_count; i++)
        kvd_size += kv_entry_size(&kv[i]);

    /* LAYOUT: header, level index, DFD, KVD, then the levels smallest first. */
    dfd_offset = KTX2_HEADER_SIZE + (uint64_t)level_count * KTX2_LEVEL_ENTRY_SIZE;
    kvd_offset = dfd_offset + KTX2_DFD_BC7_SIZE;
    pos = kvd_offset + kvd_size;
    for (int i = level_count - 1; i >= 0; i--) {
        Level *lv = &levels[i];
        /* Unsupercompressed levels align on lcm(texel block size, 4) = 16. */
        if (!lv->packed)
            pos = FFALIGN(pos, KTX2_BC7_BLOCK_BYTES);
        lv->offset = pos;
        pos += lv->packed ? lv->packed_size : lv->bc7_size;
    }
    total = pos;

    if ((ret = ff_get_encode_buffer(avctx, pkt, total, 0)) < 0)
        goto end;
    p = pkt->data;
    memset(p, 0, total);
    memcpy(p, ff_ktx2_identifier, KTX2_IDENTIFIER_SIZE);
    AV_WL32(p + 12, ctx->srgb ? KTX2_VK_FORMAT_BC7_SRGB_BLOCK : KTX2_VK_FORMAT_BC7_UNORM_BLOCK);
    AV_WL32(p + 16, 1);                 /* typeSize */
    AV_WL32(p + 20, width);
    AV_WL32(p + 24, height);
    AV_WL32(p + 28, 0);                 /* pixelDepth */
    AV_WL32(p + 32, 0);                 /* layerCount */
    AV_WL32(p + 36, 1);                 /* faceCount */
    AV_WL32(p + 40, level_count);
    AV_WL32(p + 44, ctx->zstd_level > 0 ? KTX2_SUPERCOMPRESSION_ZSTD : KTX2_SUPERCOMPRESSION_NONE);
    AV_WL32(p + 48, dfd_offset);
    AV_WL32(p + 52, KTX2_DFD_BC7_SIZE);
    AV_WL32(p + 56, kvd_offset);
    AV_WL32(p + 60, kvd_size);
    AV_WL64(p + 64, 0);                 /* sgdByteOffset */
    AV_WL64(p + 72, 0);                 /* sgdByteLength */
    for (int i = 0; i < level_count; i++) {
        uint8_t *entry = p + KTX2_HEADER_SIZE + i * KTX2_LEVEL_ENTRY_SIZE;
        const Level *lv = &levels[i];
        AV_WL64(entry +  0, lv->offset);
        AV_WL64(entry +  8, lv->packed ? lv->packed_size : lv->bc7_size);
        AV_WL64(entry + 16, lv->bc7_size);
        memcpy(p + lv->offset, lv->packed ? lv->packed : lv->bc7, lv->packed ? lv->packed_size : lv->bc7_size);
    }
    write_dfd_bc7(p + dfd_offset, ctx->srgb);
    {
        uint8_t *q = p + kvd_offset;
        for (int i = 0; i < kv_count; i++) {
            const size_t key = strlen(kv[i].key) + 1, value = strlen(kv[i].value) + 1;
            AV_WL32(q, key + value);
            memcpy(q + 4, kv[i].key, key);
            memcpy(q + 4 + key, kv[i].value, value);
            q += kv_entry_size(&kv[i]);
        }
    }

    av_log(avctx, AV_LOG_VERBOSE, "KTX2: %dx%d, %d level(s), BC7%s quality %d, sharpen %d, %s, %zu bytes\n",
           width, height, level_count, has_alpha ? " RGBA" : "", ctx->bc7_quality, ctx->mip_sharpen,
           ctx->zstd_level > 0 ? "zstd" : "raw", total);
    *got_packet = 1;

end:
    av_free(chain.data);
    for (int i = 0; i < KTX2_MAX_LEVELS; i++) {
        av_free(levels[i].rgba);
        av_free(levels[i].bc7);
        av_free(levels[i].packed);
    }
    return ret;
}

#define OFFSET(x) offsetof(KTX2EncContext, x)
#define FLAGS     AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM
static const AVOption options[] = {
    { "mipmaps",     "write the mip levels down to 1x1 (0: level 0 only)", OFFSET(mipmaps),     AV_OPT_TYPE_BOOL, { .i64 = 1 },  0, 1, FLAGS },
    { "mip_sharpen", "mip sharpening, Unreal Sharpen0..10 (0: 2x2 average)", OFFSET(mip_sharpen), AV_OPT_TYPE_INT, { .i64 = 0 },  0, 10, FLAGS },
    { "srgb",        "sRGB picture: BC7_SRGB, mips in linear light (0: data map, BC7_UNORM)", OFFSET(srgb), AV_OPT_TYPE_BOOL, { .i64 = 1 }, 0, 1, FLAGS },
    { "bc7_quality", "BC7 quality level (0 fastest .. 4 best)", OFFSET(bc7_quality), AV_OPT_TYPE_INT, { .i64 = 3 }, BASISU_BC7_QUALITY_MIN, BASISU_BC7_QUALITY_MAX, FLAGS },
    { "zstd_level",  "Zstandard level of every mip level (0: stored raw)", OFFSET(zstd_level), AV_OPT_TYPE_INT, { .i64 = 15 }, 0, 22, FLAGS },
    { "kv",          "extra adtv.* key/value pairs (key=value:key=value)", OFFSET(kv), AV_OPT_TYPE_DICT, { .str = NULL }, 0, 0, FLAGS },
    { NULL },
};

static const AVClass ktx2enc_class = {
    .class_name = "ktx2",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

const FFCodec ff_ktx2_encoder = {
    .p.name         = "ktx2",
    CODEC_LONG_NAME("Khronos KTX2 texture (BC7, mip levels, zstd)"),
    .p.type         = AVMEDIA_TYPE_VIDEO,
    .p.id           = AV_CODEC_ID_KTX2,
    .p.capabilities = AV_CODEC_CAP_DR1 | AV_CODEC_CAP_SLICE_THREADS |
                      AV_CODEC_CAP_ENCODER_REORDERED_OPAQUE,
    .priv_data_size = sizeof(KTX2EncContext),
    .p.priv_class   = &ktx2enc_class,
    .init           = ktx2_encode_init,
    FF_CODEC_ENCODE_CB(ktx2_encode_frame),
    .close          = ktx2_encode_close,
    CODEC_PIXFMTS(AV_PIX_FMT_RGBA),
    .caps_internal  = FF_CODEC_CAP_INIT_CLEANUP,
};
