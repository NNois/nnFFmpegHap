/*
 * Vidvox Hap encoder
 * Copyright (C) 2015 Vittorio Giovara <vittorio.giovara@gmail.com>
 * Copyright (C) 2015 Tom Butterworth <bangnoise@gmail.com>
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
 * Hap encoder
 *
 * Fourcc: Hap1, Hap5, HapY, HapA, HapM, Hap7
 *
 * https://github.com/Vidvox/hap/blob/master/documentation/HapVideoDRAFT.md
 */

#include <stdint.h>
#include "snappy-c.h"

#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"

#include "avcodec.h"
#include "bytestream.h"
#include "bc7enc16.h"
#include "codec_internal.h"
#include "encode.h"
#include "hap.h"
#include "texturedsp.h"

#define HAP_MAX_CHUNKS 64

enum HapHeaderLength {
    /* Short header: four bytes with a 24 bit size value */
    HAP_HDR_SHORT = 4,
    /* Long header: eight bytes with a 32 bit size value */
    HAP_HDR_LONG = 8,
};

static int compress_texture(AVCodecContext *avctx, uint8_t *out, int out_length, const AVFrame *f)
{
    HapContext *ctx = avctx->priv_data;
    int t;

    if (ctx->texture_count == 2) {
        /* HapM: encode two textures (DXT5-YCoCg + RGTC1 alpha) */
        size_t tex_size_ycocg = avctx->width / TEXTURE_BLOCK_W *
                                avctx->height / TEXTURE_BLOCK_H * ctx->enc[0].tex_ratio;
        size_t tex_size_alpha = avctx->width / TEXTURE_BLOCK_W *
                                avctx->height / TEXTURE_BLOCK_H * ctx->enc[1].tex_ratio;

        if (tex_size_ycocg + tex_size_alpha > out_length)
            return AVERROR_BUFFER_TOO_SMALL;

        /* Encode DXT5-YCoCg texture (RGB data) */
        ctx->enc[0].tex_data.out = out;
        ctx->enc[0].frame_data.in = f->data[0];
        ctx->enc[0].stride = f->linesize[0];
        ctx->enc[0].width  = avctx->width;
        ctx->enc[0].height = avctx->height;
        ff_texturedsp_exec_compress_threads(avctx, &ctx->enc[0]);

        /* Encode RGTC1 alpha texture */
        ctx->enc[1].tex_data.out = out + tex_size_ycocg;
        ctx->enc[1].frame_data.in = f->data[0];
        ctx->enc[1].stride = f->linesize[0];
        ctx->enc[1].width  = avctx->width;
        ctx->enc[1].height = avctx->height;
        ff_texturedsp_exec_compress_threads(avctx, &ctx->enc[1]);

        ctx->tex_size_alpha = tex_size_alpha;
    } else {
        /* Single texture encoding */
        if (ctx->tex_size > out_length)
            return AVERROR_BUFFER_TOO_SMALL;

        ctx->enc[0].tex_data.out = out;
        ctx->enc[0].frame_data.in = f->data[0];
        ctx->enc[0].stride = f->linesize[0];
        ctx->enc[0].width  = avctx->width;
        ctx->enc[0].height = avctx->height;
        ff_texturedsp_exec_compress_threads(avctx, &ctx->enc[0]);
    }

    return 0;
}

/* section_length does not include the header */
static void hap_write_section_header(PutByteContext *pbc,
                                     enum HapHeaderLength header_length,
                                     int section_length,
                                     enum HapSectionType section_type)
{
    /* The first three bytes are the length of the section (not including the
     * header) or zero if using an eight-byte header.
     * For an eight-byte header, the length is in the last four bytes.
     * The fourth byte stores the section type. */
    bytestream2_put_le24(pbc, header_length == HAP_HDR_LONG ? 0 : section_length);
    bytestream2_put_byte(pbc, section_type);

    if (header_length == HAP_HDR_LONG) {
        bytestream2_put_le32(pbc, section_length);
    }
}

static int hap_compress_frame(AVCodecContext *avctx, uint8_t *dst)
{
    HapContext *ctx = avctx->priv_data;
    int i, final_size = 0;

    for (i = 0; i < ctx->chunk_count; i++) {
        HapChunk *chunk = &ctx->chunks[i];
        uint8_t *chunk_src, *chunk_dst;
        int ret;

        if (i == 0) {
            chunk->compressed_offset = 0;
        } else {
            chunk->compressed_offset = ctx->chunks[i-1].compressed_offset
                                       + ctx->chunks[i-1].compressed_size;
        }
        chunk->uncompressed_size = ctx->tex_size / ctx->chunk_count;
        chunk->uncompressed_offset = i * chunk->uncompressed_size;
        chunk->compressed_size = ctx->max_snappy;
        chunk_src = ctx->tex_buf + chunk->uncompressed_offset;
        chunk_dst = dst + chunk->compressed_offset;

        /* Compress with snappy too, write directly on packet buffer. */
        ret = snappy_compress(chunk_src, chunk->uncompressed_size,
                              chunk_dst, &chunk->compressed_size);
        if (ret != SNAPPY_OK) {
            av_log(avctx, AV_LOG_ERROR, "Snappy compress error.\n");
            return AVERROR_BUG;
        }

        /* If there is no gain from snappy, just use the raw texture. */
        if (chunk->compressed_size >= chunk->uncompressed_size) {
            av_log(avctx, AV_LOG_VERBOSE,
                   "Snappy buffer bigger than uncompressed (%"SIZE_SPECIFIER" >= %"SIZE_SPECIFIER" bytes).\n",
                   chunk->compressed_size, chunk->uncompressed_size);
            memcpy(chunk_dst, chunk_src, chunk->uncompressed_size);
            chunk->compressor = HAP_COMP_NONE;
            chunk->compressed_size = chunk->uncompressed_size;
        } else {
            chunk->compressor = HAP_COMP_SNAPPY;
        }

        final_size += chunk->compressed_size;
    }

    return final_size;
}

static int hap_decode_instructions_length(int chunk_count)
{
    /*    Second-Stage Compressor Table (one byte per entry)
     *  + Chunk Size Table (four bytes per entry)
     *  + headers for both sections (short versions)
     *  = chunk_count + (4 * chunk_count) + 4 + 4 */
    return (5 * chunk_count) + 8;
}

static int hap_header_length(HapContext *ctx)
{
    /* Top section header (long version) */
    int length = HAP_HDR_LONG;

    if (ctx->texture_count == 1 && ctx->chunk_count > 1) {
        /* Decode Instructions header (short) + Decode Instructions Container */
        length += HAP_HDR_SHORT + hap_decode_instructions_length(ctx->chunk_count);
    }

    return length;
}

static int hap_texture_section_header_length(int chunk_count,
                                             enum HapHeaderLength header_length)
{
    int length = header_length;

    if (chunk_count > 1)
        length += HAP_HDR_SHORT + hap_decode_instructions_length(chunk_count);

    return length;
}

static void hap_write_texture_header(HapContext *ctx, uint8_t *dst,
                                     enum HapTextureFormat tex_fmt,
                                     int chunk_count, int frame_length,
                                     enum HapHeaderLength header_length)
{
    PutByteContext pbc;
    int i;

    bytestream2_init_writer(&pbc, dst, frame_length);

    if (chunk_count == 1) {
        /* Write a simple header */
        hap_write_section_header(&pbc, header_length, frame_length - header_length,
                                 ctx->chunks[0].compressor | tex_fmt);
    } else {
        /* Write a complex header with Decode Instructions Container */
        hap_write_section_header(&pbc, header_length, frame_length - header_length,
                                 HAP_COMP_COMPLEX | tex_fmt);
        hap_write_section_header(&pbc, HAP_HDR_SHORT, hap_decode_instructions_length(chunk_count),
                                 HAP_ST_DECODE_INSTRUCTIONS);
        hap_write_section_header(&pbc, HAP_HDR_SHORT, chunk_count,
                                 HAP_ST_COMPRESSOR_TABLE);

        for (i = 0; i < chunk_count; i++) {
            bytestream2_put_byte(&pbc, ctx->chunks[i].compressor >> 4);
        }

        hap_write_section_header(&pbc, HAP_HDR_SHORT, chunk_count * 4,
                                 HAP_ST_SIZE_TABLE);

        for (i = 0; i < chunk_count; i++) {
            bytestream2_put_le32(&pbc, ctx->chunks[i].compressed_size);
        }
    }
}

static int hap_encode(AVCodecContext *avctx, AVPacket *pkt,
                      const AVFrame *frame, int *got_packet)
{
    HapContext *ctx = avctx->priv_data;
    int ret;

    if (ctx->texture_count == 1) {
        int header_length = hap_header_length(ctx);
        int final_data_size;
        int pktsize = FFMAX(ctx->tex_size, ctx->max_snappy * ctx->chunk_count) + header_length;

        /* Allocate maximum size packet, shrink later. */
        ret = ff_alloc_packet(avctx, pkt, pktsize);
        if (ret < 0)
            return ret;

        if (ctx->opt_compressor == HAP_COMP_NONE) {
            /* DXTC compression directly to the packet buffer. */
            ret = compress_texture(avctx, pkt->data + header_length, pkt->size - header_length, frame);
            if (ret < 0)
                return ret;

            ctx->chunks[0].compressor = HAP_COMP_NONE;
            ctx->chunks[0].compressed_offset = 0;
            ctx->chunks[0].compressed_size = ctx->tex_size;
            final_data_size = ctx->tex_size;
        } else {
            /* DXTC compression. */
            ret = compress_texture(avctx, ctx->tex_buf, ctx->tex_size, frame);
            if (ret < 0)
                return ret;

            /* Compress (using Snappy) the frame */
            final_data_size = hap_compress_frame(avctx, pkt->data + header_length);
            if (final_data_size < 0)
                return final_data_size;
        }

        /* Write header at the start. */
        hap_write_texture_header(ctx, pkt->data, ctx->opt_tex_fmt, ctx->chunk_count,
                                 final_data_size + header_length, HAP_HDR_LONG);

        av_shrink_packet(pkt, final_data_size + header_length);
        *got_packet = 1;
        return 0;
    } else {
        int chunk_count = ctx->chunk_count;
        int tex_header_len = hap_texture_section_header_length(chunk_count, HAP_HDR_SHORT);
        int top_header_len = HAP_HDR_SHORT;
        int max_payload[2];
        int compressed_size[2];
        int header_total = top_header_len + tex_header_len * ctx->texture_count;
        int offset = top_header_len;
        size_t tex_size_main = ctx->tex_size;
        size_t tex_size_alpha = ctx->tex_size_alpha;
        size_t max_snappy_main = ctx->max_snappy;
        size_t max_snappy_alpha = ctx->max_snappy_alpha;
        uint8_t *tex_buf_main = ctx->tex_buf;
        PutByteContext pbc;
        enum HapTextureFormat tex_formats[2] = { HAP_FMT_YCOCGDXT5, HAP_FMT_RGTC1 };

        if (ctx->opt_compressor == HAP_COMP_SNAPPY) {
            max_payload[0] = (int)(ctx->max_snappy * chunk_count);
            max_payload[1] = (int)(ctx->max_snappy_alpha * chunk_count);
        } else {
            max_payload[0] = (int)ctx->tex_size;
            max_payload[1] = (int)ctx->tex_size_alpha;
        }

        ret = ff_alloc_packet(avctx, pkt, header_total + max_payload[0] + max_payload[1]);
        if (ret < 0)
            return ret;

        for (int t = 0; t < ctx->texture_count; t++) {
            TextureDSPThreadContext *enc = &ctx->enc[t];
            int header_offset = offset;
            int data_offset = header_offset + tex_header_len;
            uint8_t *texture_dst = pkt->data + data_offset;
            size_t tex_size = t == 0 ? tex_size_main : tex_size_alpha;

            enc->tex_data.out = (ctx->opt_compressor == HAP_COMP_NONE) ?
                                texture_dst :
                                (t == 0 ? tex_buf_main : ctx->tex_buf_alpha);
            enc->frame_data.in = frame->data[0];
            enc->stride = frame->linesize[0];
            enc->width  = avctx->width;
            enc->height = avctx->height;
            ff_texturedsp_exec_compress_threads(avctx, enc);

            if (ctx->opt_compressor == HAP_COMP_NONE) {
                ctx->chunks[0].compressor = HAP_COMP_NONE;
                ctx->chunks[0].compressed_offset = 0;
                ctx->chunks[0].compressed_size = tex_size;
                compressed_size[t] = (int)tex_size;
            } else {
                ctx->tex_size = tex_size;
                ctx->max_snappy = t == 0 ? max_snappy_main : max_snappy_alpha;
                ctx->tex_buf = (t == 0) ? tex_buf_main : ctx->tex_buf_alpha;

                compressed_size[t] = hap_compress_frame(avctx, texture_dst);
                if (compressed_size[t] < 0)
                    return compressed_size[t];
            }

            if ((size_t)compressed_size[t] > 0xFFFFFFU) {
                av_log(avctx, AV_LOG_ERROR, "HapM texture section too large for short header.\n");
                return AVERROR_INVALIDDATA;
            }

            hap_write_texture_header(ctx,
                                     pkt->data + header_offset,
                                     tex_formats[t],
                                     chunk_count,
                                     compressed_size[t] + tex_header_len,
                                     HAP_HDR_SHORT);

            offset = data_offset + compressed_size[t];
        }

        ctx->tex_size = tex_size_main;
        ctx->max_snappy = max_snappy_main;
        ctx->tex_buf = tex_buf_main;

        if ((size_t)offset - top_header_len > 0xFFFFFFU) {
            av_log(avctx, AV_LOG_ERROR, "HapM frame too large for short header.\n");
            return AVERROR_INVALIDDATA;
        }

        bytestream2_init_writer(&pbc, pkt->data, offset);
        hap_write_section_header(&pbc, HAP_HDR_SHORT,
                                 offset - top_header_len,
                                 HAP_FMT_HAPM);

        av_shrink_packet(pkt, offset);
        *got_packet = 1;
        return 0;
    }
}

static av_cold int hap_init(AVCodecContext *avctx)
{
    HapContext *ctx = avctx->priv_data;
    TextureDSPEncContext dxtc;
    int corrected_chunk_count;
    int block_count;
    int ret = av_image_check_size(avctx->width, avctx->height, 0, avctx);

    if (ret < 0) {
        av_log(avctx, AV_LOG_ERROR, "Invalid video size %dx%d.\n",
               avctx->width, avctx->height);
        return ret;
    }

    if (avctx->width % 4 || avctx->height % 4) {
        av_log(avctx, AV_LOG_ERROR, "Video size %dx%d is not multiple of 4.\n",
               avctx->width, avctx->height);
        return AVERROR_INVALIDDATA;
    }

    ff_texturedspenc_init(&dxtc);

    ctx->texture_count = 1;  /* Default to single texture */

    switch (ctx->opt_tex_fmt) {
    case HAP_FMT_RGBDXT1:
        ctx->enc[0].tex_ratio = 8;
        avctx->codec_tag = MKTAG('H', 'a', 'p', '1');
        avctx->bits_per_coded_sample = 24;
        ctx->enc[0].tex_funct = dxtc.dxt1_block;
        break;
    case HAP_FMT_BPTC: {
        BC7Enc16Context bc7;
        ff_bc7enc16_init(&bc7, BC7ENC16_TRUE, BC7ENC16_MAX_PARTITIONS1, 0);
        ctx->enc[0].tex_ratio = 16;
        avctx->codec_tag = MKTAG('H', 'a', 'p', '7');
        avctx->bits_per_coded_sample = 32;
        ctx->enc[0].tex_funct = bc7.bc7enc16_block;
        break;
    }
    case HAP_FMT_RGBADXT5:
        ctx->enc[0].tex_ratio = 16;
        avctx->codec_tag = MKTAG('H', 'a', 'p', '5');
        avctx->bits_per_coded_sample = 32;
        ctx->enc[0].tex_funct = dxtc.dxt5_block;
        break;
    case HAP_FMT_YCOCGDXT5:
        ctx->enc[0].tex_ratio = 16;
        avctx->codec_tag = MKTAG('H', 'a', 'p', 'Y');
        avctx->bits_per_coded_sample = 24;
        ctx->enc[0].tex_funct = dxtc.dxt5ys_block;
        break;
    case HAP_FMT_RGTC1:
        ctx->enc[0].tex_ratio = 8;
        avctx->codec_tag = MKTAG('H', 'a', 'p', 'A');
        avctx->bits_per_coded_sample = 8;
        ctx->enc[0].tex_funct = dxtc.rgtc1u_gray_block;
        break;
    case HAP_FMT_HAPM:
        /* HapM uses two textures: DXT5-YCoCg (16 bytes) + RGTC1 alpha (8 bytes) */
        ctx->texture_count = 2;
        ctx->enc[0].tex_ratio = 16;
        ctx->enc[1].tex_ratio = 8;
        avctx->codec_tag = MKTAG('H', 'a', 'p', 'M');
        avctx->bits_per_coded_sample = 32;
        ctx->enc[0].tex_funct = dxtc.dxt5ys_block;
        ctx->enc[1].tex_funct = dxtc.rgtc1u_alpha_block;
        ctx->enc[1].raw_ratio = 16;
        ctx->enc[1].slice_count = av_clip(avctx->thread_count, 1, avctx->height / TEXTURE_BLOCK_H);
        break;
    default:
        av_log(avctx, AV_LOG_ERROR, "Invalid format %02X\n", ctx->opt_tex_fmt);
        return AVERROR_INVALIDDATA;
    }
    ctx->enc[0].raw_ratio = 16;
    ctx->enc[0].slice_count = av_clip(avctx->thread_count, 1, avctx->height / TEXTURE_BLOCK_H);

    block_count = (avctx->width  / TEXTURE_BLOCK_W) *
                  (avctx->height / TEXTURE_BLOCK_H);

    /* Texture compression ratio is constant, so can we computer
     * beforehand the final size of the uncompressed buffer. */
    ctx->tex_size = block_count * ctx->enc[0].tex_ratio;
    if (ctx->texture_count == 2)
        ctx->tex_size_alpha = block_count * ctx->enc[1].tex_ratio;
    else
        ctx->tex_size_alpha = 0;

    switch (ctx->opt_compressor) {
    case HAP_COMP_NONE:
        /* No benefit chunking uncompressed data */
        corrected_chunk_count = 1;

        ctx->max_snappy = ctx->tex_size;
        ctx->max_snappy_alpha = ctx->tex_size_alpha;
        ctx->tex_buf = NULL;
        ctx->tex_buf_alpha = NULL;
        break;
    case HAP_COMP_SNAPPY:
        /* Round the chunk count to divide evenly on DXT block edges */
        corrected_chunk_count = av_clip(ctx->opt_chunk_count, 1, HAP_MAX_CHUNKS);
        while (block_count % corrected_chunk_count != 0) {
            corrected_chunk_count--;
        }

        ctx->max_snappy = snappy_max_compressed_length(ctx->tex_size / corrected_chunk_count);
        ctx->tex_buf = av_malloc(ctx->tex_size);
        if (!ctx->tex_buf) {
            return AVERROR(ENOMEM);
        }
        if (ctx->texture_count == 2) {
            ctx->max_snappy_alpha = snappy_max_compressed_length(ctx->tex_size_alpha / corrected_chunk_count);
            ctx->tex_buf_alpha = av_malloc(ctx->tex_size_alpha);
            if (!ctx->tex_buf_alpha) {
                av_freep(&ctx->tex_buf);
                return AVERROR(ENOMEM);
            }
        }
        break;
    default:
        av_log(avctx, AV_LOG_ERROR, "Invalid compressor %02X\n", ctx->opt_compressor);
        return AVERROR_INVALIDDATA;
    }
    if (corrected_chunk_count != ctx->opt_chunk_count) {
        av_log(avctx, AV_LOG_INFO, "%d chunks requested but %d used.\n",
                                    ctx->opt_chunk_count, corrected_chunk_count);
    }
    ret = ff_hap_set_chunk_count(ctx, corrected_chunk_count, 1);
    if (ret != 0)
        return ret;

    return 0;
}

static av_cold int hap_close(AVCodecContext *avctx)
{
    HapContext *ctx = avctx->priv_data;

    ff_hap_free_context(ctx);

    return 0;
}

#define OFFSET(x) offsetof(HapContext, x)
#define FLAGS     AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM
static const AVOption options[] = {
    { "format", NULL, OFFSET(opt_tex_fmt), AV_OPT_TYPE_INT, { .i64 = HAP_FMT_RGBDXT1 }, HAP_FMT_RGTC1, HAP_FMT_YCOCGDXT5, FLAGS, .unit = "format" },
        { "hap",       "Hap 1 (DXT1 textures)", 0, AV_OPT_TYPE_CONST, { .i64 = HAP_FMT_RGBDXT1   }, 0, 0, FLAGS, .unit = "format" },
        { "hap_r",     "Hap R (BC7 textures)", 0, AV_OPT_TYPE_CONST, { .i64 = HAP_FMT_BPTC }, 0, 0, FLAGS, .unit = "format" },
        { "hap_alpha", "Hap Alpha (DXT5 textures)", 0, AV_OPT_TYPE_CONST, { .i64 = HAP_FMT_RGBADXT5  }, 0, 0, FLAGS, .unit = "format" },
        { "hap_q",     "Hap Q (DXT5-YCoCg textures)", 0, AV_OPT_TYPE_CONST, { .i64 = HAP_FMT_YCOCGDXT5 }, 0, 0, FLAGS, .unit = "format" },
        { "hap_a",     "Hap Alpha-Only (RGTC1 textures)", 0, AV_OPT_TYPE_CONST, { .i64 = HAP_FMT_RGTC1 }, 0, 0, FLAGS, .unit = "format" },
        { "hap_m",     "Hap M (DXT5-YCoCg + RGTC1 alpha)", 0, AV_OPT_TYPE_CONST, { .i64 = HAP_FMT_HAPM }, 0, 0, FLAGS, .unit = "format" },
    { "chunks", "chunk count", OFFSET(opt_chunk_count), AV_OPT_TYPE_INT, {.i64 = 1 }, 1, HAP_MAX_CHUNKS, FLAGS, },
    { "compressor", "second-stage compressor", OFFSET(opt_compressor), AV_OPT_TYPE_INT, { .i64 = HAP_COMP_SNAPPY }, HAP_COMP_NONE, HAP_COMP_SNAPPY, FLAGS, .unit = "compressor" },
        { "none",       "None", 0, AV_OPT_TYPE_CONST, { .i64 = HAP_COMP_NONE }, 0, 0, FLAGS, .unit = "compressor" },
        { "snappy",     "Snappy", 0, AV_OPT_TYPE_CONST, { .i64 = HAP_COMP_SNAPPY }, 0, 0, FLAGS, .unit = "compressor" },
    { NULL },
};

static const AVClass hapenc_class = {
    .class_name = "Hap encoder",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

const FFCodec ff_hap_encoder = {
    .p.name         = "hap",
    CODEC_LONG_NAME("Vidvox Hap"),
    .p.type         = AVMEDIA_TYPE_VIDEO,
    .p.id           = AV_CODEC_ID_HAP,
    .p.capabilities = AV_CODEC_CAP_DR1 | AV_CODEC_CAP_SLICE_THREADS |
                      AV_CODEC_CAP_ENCODER_REORDERED_OPAQUE,
    .priv_data_size = sizeof(HapContext),
    .p.priv_class   = &hapenc_class,
    .init           = hap_init,
    FF_CODEC_ENCODE_CB(hap_encode),
    .close          = hap_close,
    CODEC_PIXFMTS(AV_PIX_FMT_RGBA, AV_PIX_FMT_GRAY8),
    .caps_internal  = FF_CODEC_CAP_INIT_CLEANUP,
};
