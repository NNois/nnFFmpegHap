/*
 * KTX2 (Khronos Texture 2.0) decoder — BC7, level 0, Zstandard
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
 * Shows a .ktx2 picture (ffplay, mpv, ffmpeg -i): the largest level (level 0)
 * of a 2D BC7 texture (VK_FORMAT_BC7_SRGB_BLOCK or _UNORM_BLOCK), stored raw
 * or Zstandard-supercompressed, decoded to RGBA by the BC7 decoder of Hap R.
 * The other mip levels, the DFD and the key/value data are not needed to
 * display it; they are listed at verbose level.
 */

/* Zstandard comes from basis_universal (zstd/zstd.c is in libbasisu_encoder.a, the
 * library of the Hap R encoder): linking the system libzstd as well would define
 * every ZSTD_* symbol twice. The header is the one of that copy (-I$BASISU_ROOT). */
#include "zstd/zstd.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libavutil/internal.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"
#include "libavutil/rational.h"

#include "avcodec.h"
#include "bc7dec.h"
#include "codec_internal.h"
#include "decode.h"
#include "ktx2.h"
#include "texturedsp.h"

typedef struct KTX2DecContext {
    const AVClass *class;
    int mip_level;              /* the level shown (option mip_level) */
    uint8_t *blocks;            /* that level once decompressed */
    size_t blocks_size;
} KTX2DecContext;

/* THE KEY/VALUE DATA: logged at verbose level; adtv.displayAspect ("5:3", written by the encoder from the sample
 * aspect ratio) comes back in *display_w / *display_h - a texture scaled to a multiple of 4 is shown at its real
 * proportions. */
static void read_key_values(AVCodecContext *avctx, const uint8_t *data, size_t size,
                            uint32_t offset, uint32_t length, int *display_w, int *display_h)
{
    const uint8_t *q, *end;

    if (!length || offset > size || length > size - offset)
        return;
    q   = data + offset;
    end = q + length;
    while (end - q >= 4) {
        const uint32_t entry = AV_RL32(q);
        const uint8_t *kv = q + 4, *nul;
        if (entry > (size_t)(end - kv))
            break;
        nul = memchr(kv, 0, entry);
        if (nul && nul + 1 < kv + entry) {
            const int value_len = (int)(kv + entry - nul - 1);
            char value[32];
            av_log(avctx, AV_LOG_VERBOSE, "KTX2 %s = %.*s\n", kv, value_len, nul + 1);
            if (!strcmp((const char *)kv, "adtv.displayAspect") && value_len < (int)sizeof(value)) {
                int w = 0, h = 0;
                memcpy(value, nul + 1, value_len);
                value[value_len] = 0;
                if (sscanf(value, "%d:%d", &w, &h) == 2 && w > 0 && h > 0) {
                    *display_w = w;
                    *display_h = h;
                }
            }
        }
        q = kv + FFALIGN(entry, 4);
    }
}

static int ktx2_decode_frame(AVCodecContext *avctx, AVFrame *frame,
                             int *got_frame, AVPacket *avpkt)
{
    KTX2DecContext *ctx = avctx->priv_data;
    const uint8_t *data = avpkt->data;
    const size_t size = avpkt->size;
    TextureDSPThreadContext t = { 0 };
    uint32_t format, width, height, depth, layers, faces, levels, scheme;
    uint64_t offset, length, raw_length;
    const uint8_t *blocks, *entry;
    size_t bw, bh;
    int level, ret, display_w = 0, display_h = 0;
    uint32_t width0, height0;

    if (size < KTX2_HEADER_SIZE + KTX2_LEVEL_ENTRY_SIZE ||
        memcmp(data, ff_ktx2_identifier, KTX2_IDENTIFIER_SIZE)) {
        av_log(avctx, AV_LOG_ERROR, "Not a KTX2 file.\n");
        return AVERROR_INVALIDDATA;
    }
    format = AV_RL32(data + 12);
    width  = AV_RL32(data + 20);
    height = AV_RL32(data + 24);
    depth  = AV_RL32(data + 28);
    layers = AV_RL32(data + 32);
    faces  = AV_RL32(data + 36);
    levels = AV_RL32(data + 40);
    scheme = AV_RL32(data + 44);

    if (format != KTX2_VK_FORMAT_BC7_SRGB_BLOCK && format != KTX2_VK_FORMAT_BC7_UNORM_BLOCK) {
        avpriv_request_sample(avctx, "KTX2 vkFormat %u (only BC7 is read)", format);
        return AVERROR_PATCHWELCOME;
    }
    if (depth > 1 || layers > 1 || faces != 1) {
        avpriv_request_sample(avctx, "KTX2 depth %u, %u layer(s), %u face(s) (2D only)", depth, layers, faces);
        return AVERROR_PATCHWELCOME;
    }
    if (scheme != KTX2_SUPERCOMPRESSION_NONE && scheme != KTX2_SUPERCOMPRESSION_ZSTD) {
        avpriv_request_sample(avctx, "KTX2 supercompression scheme %u", scheme);
        return AVERROR_PATCHWELCOME;
    }
    if ((ret = av_image_check_size(width, height, 0, avctx)) < 0)
        return ret;
    if (!levels)
        levels = 1;   /* 0 = "generate the mips at load time": level 0 is still there */
    width0  = width;
    height0 = height;
    if (size < KTX2_HEADER_SIZE + (uint64_t)levels * KTX2_LEVEL_ENTRY_SIZE)
        return AVERROR_INVALIDDATA;

    /* THE LEVEL SHOWN: 0 (the picture) unless option mip_level asks for a smaller one. The level index lists
     * level 0 first; level n is max(1, size >> n). */
    level = FFMIN(ctx->mip_level, (int)levels - 1);
    if (level < ctx->mip_level)
        av_log(avctx, AV_LOG_WARNING, "KTX2: mip_level %d asked, the file has %u level(s): showing level %d.\n",
               ctx->mip_level, levels, level);
    entry      = data + KTX2_HEADER_SIZE + level * KTX2_LEVEL_ENTRY_SIZE;
    offset     = AV_RL64(entry + 0);
    length     = AV_RL64(entry + 8);
    raw_length = AV_RL64(entry + 16);
    width      = FFMAX(1, width  >> level);
    height     = FFMAX(1, height >> level);
    bw = (width  + 3) >> 2;
    bh = (height + 3) >> 2;
    if (offset > size || length > size - offset) {
        av_log(avctx, AV_LOG_ERROR, "KTX2 level %d outside the file.\n", level);
        return AVERROR_INVALIDDATA;
    }
    if (raw_length != bw * bh * KTX2_BC7_BLOCK_BYTES) {
        av_log(avctx, AV_LOG_ERROR, "KTX2 level %d: %"PRIu64" bytes, %zu expected for %ux%u BC7.\n",
               level, raw_length, bw * bh * KTX2_BC7_BLOCK_BYTES, width, height);
        return AVERROR_INVALIDDATA;
    }
    read_key_values(avctx, data, size, AV_RL32(data + 56), AV_RL32(data + 60), &display_w, &display_h);
    av_log(avctx, AV_LOG_VERBOSE, "KTX2: level %d of %u, %ux%u BC7 %s, %s\n", level, levels, width, height,
           format == KTX2_VK_FORMAT_BC7_SRGB_BLOCK ? "sRGB" : "UNORM",
           scheme == KTX2_SUPERCOMPRESSION_ZSTD ? "zstd" : "raw");

    if (scheme == KTX2_SUPERCOMPRESSION_ZSTD) {
        size_t got;
        if (ctx->blocks_size < raw_length) {
            av_freep(&ctx->blocks);
            ctx->blocks_size = 0;
            if (!(ctx->blocks = av_malloc(raw_length)))
                return AVERROR(ENOMEM);
            ctx->blocks_size = raw_length;
        }
        got = ZSTD_decompress(ctx->blocks, raw_length, data + offset, length);
        if (ZSTD_isError(got) || got != raw_length) {
            av_log(avctx, AV_LOG_ERROR, "KTX2 level %d: zstd %s\n", level,
                   ZSTD_isError(got) ? ZSTD_getErrorName(got) : "short");
            return AVERROR_INVALIDDATA;
        }
        blocks = ctx->blocks;
    } else {
        if (length != raw_length)
            return AVERROR_INVALIDDATA;
        blocks = data + offset;
    }

    if ((ret = ff_set_dimensions(avctx, width, height)) < 0)
        return ret;
    /* The pixel aspect that brings the texture back to its display aspect (the same at every level). */
    if (display_w > 0 && display_h > 0)
        av_reduce(&avctx->sample_aspect_ratio.num, &avctx->sample_aspect_ratio.den,
                  (int64_t)display_w * height0, (int64_t)width0 * display_h, INT_MAX);
    /* Blocks are 4 x 4: the frame is allocated at the aligned size, cropped on output. */
    avctx->coded_width     = bw * 4;
    avctx->coded_height    = bh * 4;
    avctx->pix_fmt         = AV_PIX_FMT_RGBA;
    avctx->color_primaries = AVCOL_PRI_BT709;
    avctx->color_trc       = format == KTX2_VK_FORMAT_BC7_SRGB_BLOCK ? AVCOL_TRC_IEC61966_2_1 : AVCOL_TRC_LINEAR;
    if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
        return ret;

    t.frame_data.out = frame->data[0];
    t.stride         = frame->linesize[0];
    t.width          = avctx->coded_width;
    t.height         = avctx->coded_height;
    t.tex_data.in    = blocks;
    t.tex_ratio      = KTX2_BC7_BLOCK_BYTES;
    t.raw_ratio      = 16;
    t.slice_count    = av_clip(avctx->thread_count, 1, avctx->coded_height / 4);
    t.tex_funct      = ff_bc7dec_block;
    ff_texturedsp_exec_decompress_threads(avctx, &t);

    frame->pict_type = AV_PICTURE_TYPE_I;
    frame->flags    |= AV_FRAME_FLAG_KEY;
    *got_frame = 1;
    return avpkt->size;
}

static av_cold int ktx2_decode_close(AVCodecContext *avctx)
{
    KTX2DecContext *ctx = avctx->priv_data;

    av_freep(&ctx->blocks);
    ctx->blocks_size = 0;
    return 0;
}

#define OFFSET(x) offsetof(KTX2DecContext, x)
#define FLAGS     AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_DECODING_PARAM
static const AVOption options[] = {
    { "mip_level", "mip level shown (0: the picture; n: max(1, size >> n), the last one if beyond)", OFFSET(mip_level), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, 15, FLAGS },
    { NULL },
};

static const AVClass ktx2dec_class = {
    .class_name = "ktx2 decoder",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

const FFCodec ff_ktx2_decoder = {
    .p.name         = "ktx2",
    CODEC_LONG_NAME("Khronos KTX2 texture (BC7, mip levels, zstd)"),
    .p.type         = AVMEDIA_TYPE_VIDEO,
    .p.id           = AV_CODEC_ID_KTX2,
    .priv_data_size = sizeof(KTX2DecContext),
    .p.priv_class   = &ktx2dec_class,
    FF_CODEC_DECODE_CB(ktx2_decode_frame),
    .close          = ktx2_decode_close,
    .p.capabilities = AV_CODEC_CAP_DR1 | AV_CODEC_CAP_SLICE_THREADS,
    .caps_internal  = FF_CODEC_CAP_INIT_CLEANUP,
};
