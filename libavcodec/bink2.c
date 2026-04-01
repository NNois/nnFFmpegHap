/*
 * Bink video 2 decoder — RAD SDK backend
 *
 * Uses the official RAD Game Tools Bink2 SDK (ExpandBink2) for decoding,
 * providing pixel-perfect output identical to Unreal Engine's Bink player.
 *
 * Copyright (c) 2014 Konstantin Shishkov (original reverse-engineered decoder)
 * Copyright (c) 2019 Paul B Mahol (original reverse-engineered decoder)
 * Copyright (c) 2026 Alternative Development (SDK integration)
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

#include "libavutil/imgutils.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/mem.h"
#include "avcodec.h"
#include "codec_internal.h"
#include "decode.h"

/* RAD SDK: must define USING_EGT before including SDK headers so that
 * rrCore.h picks egttypes.h instead of the missing radtypes.h.
 * Also define build flags matching how libbink2sdk.a was compiled. */
#ifndef USING_EGT
#define USING_EGT
#endif
#ifndef INC_BINK2
#define INC_BINK2
#endif
#ifndef __RADINSTATICLIB__
#define __RADINSTATICLIB__
#endif
#ifndef NTELEMETRY
#define NTELEMETRY
#endif
#include "bink.h"
#include "expand2.h"

#define BINK_FLAG_ALPHA 0x00100000

typedef struct Bink2Context {
    AVCodecContext  *avctx;
    int              has_alpha;
    uint32_t         flags;       /* from extradata (OpenFlags) */
    uint32_t         marker;      /* codec_tag = file marker (e.g. BINKMARKERV28) */

    /* RAD SDK frame buffers: 2 frames for current + reference */
    BINKFRAMEBUFFERS fb;
    uint8_t         *plane_buf;   /* single allocation for all plane data */

    /* RAD SDK slice configuration */
    BINKSLICES       slices;
} Bink2Context;

static av_cold int bink2_decode_init(AVCodecContext *avctx)
{
    Bink2Context *c = avctx->priv_data;
    uint32_t ya_w, ya_h, c_w, c_h;
    uint32_t ya_pitch, c_pitch;
    size_t ya_size, c_size, frame_size, total;
    uint8_t *ptr;
    int ret;

    c->avctx = avctx;
    c->marker = avctx->codec_tag;

    if (avctx->extradata_size < 4) {
        av_log(avctx, AV_LOG_ERROR, "Extradata missing or too short\n");
        return AVERROR_INVALIDDATA;
    }
    c->flags = AV_RL32(avctx->extradata);
    c->has_alpha = !!(c->flags & BINK_FLAG_ALPHA);

    if ((ret = av_image_check_size(avctx->width, avctx->height, 0, avctx)) < 0)
        return ret;

    avctx->pix_fmt = c->has_alpha ? AV_PIX_FMT_YUVA420P : AV_PIX_FMT_YUV420P;

    /* Initialize the RAD SDK global tables */
    ExpandBink2GlobalInit();

    /* Set up slice configuration */
    setup_slices(c->marker, c->flags, avctx->width, avctx->height, &c->slices);

    /* Allocate BINKFRAMEBUFFERS: 2 frames (current + reference) */
    ya_w = FFALIGN(avctx->width, 32);
    ya_h = FFALIGN(avctx->height, 32);
    c_w  = ya_w / 2;
    c_h  = ya_h / 2;

    ya_pitch = FFALIGN(ya_w, 16);
    c_pitch  = FFALIGN(c_w, 16);

    ya_size = ya_pitch * ya_h;
    c_size  = c_pitch  * c_h;

    /* Per frame: Y + cR + cB + A (+ H for HDR, but we don't use it) */
    frame_size = ya_size + c_size + c_size + (c->has_alpha ? ya_size : 0);
    total = frame_size * 2; /* 2 frames */

    c->plane_buf = av_mallocz(total);
    if (!c->plane_buf)
        return AVERROR(ENOMEM);

    c->fb.TotalFrames    = 2;
    c->fb.YABufferWidth  = ya_w;
    c->fb.YABufferHeight = ya_h;
    c->fb.cRcBBufferWidth  = c_w;
    c->fb.cRcBBufferHeight = c_h;
    c->fb.FrameNum = 0;

    ptr = c->plane_buf;
    for (int f = 0; f < 2; f++) {
        c->fb.Frames[f].YPlane.Buffer      = ptr; ptr += ya_size;
        c->fb.Frames[f].YPlane.Allocate    = 1;
        c->fb.Frames[f].YPlane.BufferPitch = ya_pitch;

        c->fb.Frames[f].cRPlane.Buffer      = ptr; ptr += c_size;
        c->fb.Frames[f].cRPlane.Allocate    = 1;
        c->fb.Frames[f].cRPlane.BufferPitch = c_pitch;

        c->fb.Frames[f].cBPlane.Buffer      = ptr; ptr += c_size;
        c->fb.Frames[f].cBPlane.Allocate    = 1;
        c->fb.Frames[f].cBPlane.BufferPitch = c_pitch;

        if (c->has_alpha) {
            c->fb.Frames[f].APlane.Buffer      = ptr; ptr += ya_size;
            c->fb.Frames[f].APlane.Allocate    = 1;
            c->fb.Frames[f].APlane.BufferPitch = ya_pitch;
        } else {
            c->fb.Frames[f].APlane.Buffer      = NULL;
            c->fb.Frames[f].APlane.Allocate    = 0;
            c->fb.Frames[f].APlane.BufferPitch = 0;
        }

        c->fb.Frames[f].HPlane.Buffer      = NULL;
        c->fb.Frames[f].HPlane.Allocate    = 0;
        c->fb.Frames[f].HPlane.BufferPitch = 0;
    }

    av_log(avctx, AV_LOG_INFO,
           "bink2: SDK decoder, %dx%d, alpha=%d, marker=0x%08X, slices=%d\n",
           avctx->width, avctx->height, c->has_alpha, c->marker, c->slices.slice_count);

    return 0;
}

static int bink2_decode_frame(AVCodecContext *avctx, AVFrame *frame,
                              int *got_frame, AVPacket *pkt)
{
    Bink2Context *c = avctx->priv_data;
    int ret, w, h;
    uint32_t result;
    int new_frame, ya_pitch, c_pitch;
    uint8_t *y_src, *cr_src, *cb_src, *a_src;
    int is_kf = !!(pkt->flags & AV_PKT_FLAG_KEY);

    w = avctx->width;
    h = avctx->height;
    ret = ff_set_dimensions(avctx, FFALIGN(w, 32), FFALIGN(h, 32));
    if (ret < 0)
        return ret;
    avctx->width  = w;
    avctx->height = h;

    if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
        return ret;

    /* Reset slice work counter for this frame (used by SDK's atomic dispatch) */
    c->slices.slice_inc = 0;

    /* Call the RAD SDK decoder.
     * comp     = pkt->data (compressed frame data, starts with frame_flags U32)
     * compinfo = pkt->data + pkt->size (end of compressed data)
     * alpha    = 1 if alpha plane present in bitstream
     * hdr      = 0 (no HDR support yet)
     * key      = 1 if keyframe
     * control  = startslice=0, numslices=slice_count → (slice_count << 4) | 0
     * seam     = NULL (single-threaded)
     * gpu      = NULL (CPU decode)
     */
    result = ExpandBink2(&c->fb,
                         pkt->data,                  /* compressed data */
                         c->has_alpha ? 1 : 0,       /* alpha */
                         0,                           /* hdr */
                         is_kf ? 1 : 0,              /* key */
                         pkt->data + pkt->size,       /* compinfo / end */
                         &c->slices,
                         (c->slices.slice_count << 4) | 0, /* control: start=0, num=all */
                         NULL,                        /* seam (single-threaded) */
                         NULL);                       /* gpu (CPU decode) */

    if (!result) {
        av_log(avctx, AV_LOG_ERROR, "ExpandBink2 failed\n");
        return AVERROR_INVALIDDATA;
    }

    /* The SDK reads from fb.Frames[FrameNum] (old/reference) and writes to
     * fb.Frames[(FrameNum+1) % TotalFrames] (new/current). */
    new_frame = (c->fb.FrameNum + 1) % c->fb.TotalFrames;

    ya_pitch = c->fb.Frames[new_frame].YPlane.BufferPitch;
    c_pitch  = c->fb.Frames[new_frame].cRPlane.BufferPitch;

    y_src  = (uint8_t *)c->fb.Frames[new_frame].YPlane.Buffer;
    cr_src = (uint8_t *)c->fb.Frames[new_frame].cRPlane.Buffer;
    cb_src = (uint8_t *)c->fb.Frames[new_frame].cBPlane.Buffer;
    a_src  = (uint8_t *)c->fb.Frames[new_frame].APlane.Buffer;

    /* Copy Y plane */
    for (int y = 0; y < h; y++)
        memcpy(frame->data[0] + y * frame->linesize[0],
               y_src + y * ya_pitch, w);

    /* Copy chroma planes (Bink SDK: cR=V, cB=U in standard Bink convention,
     * but FFmpeg YUV420P: data[1]=U(Cb), data[2]=V(Cr)) */
    for (int y = 0; y < (h + 1) / 2; y++) {
        memcpy(frame->data[1] + y * frame->linesize[1],
               cb_src + y * c_pitch, (w + 1) / 2);
        memcpy(frame->data[2] + y * frame->linesize[2],
               cr_src + y * c_pitch, (w + 1) / 2);
    }

    /* Copy alpha plane */
    if (c->has_alpha && a_src && frame->data[3]) {
        for (int y = 0; y < h; y++)
            memcpy(frame->data[3] + y * frame->linesize[3],
                   a_src + y * ya_pitch, w);
    }

    /* Advance the SDK frame counter for the next frame */
    c->fb.FrameNum = (c->fb.FrameNum + 1) % c->fb.TotalFrames;

    if (is_kf)
        frame->flags |= AV_FRAME_FLAG_KEY;
    else
        frame->flags &= ~AV_FRAME_FLAG_KEY;
    frame->pict_type = is_kf ? AV_PICTURE_TYPE_I : AV_PICTURE_TYPE_P;

    *got_frame = 1;
    return pkt->size;
}

static void bink2_flush(AVCodecContext *avctx)
{
    Bink2Context *c = avctx->priv_data;

    /* Reset frame counter and zero all plane buffers */
    c->fb.FrameNum = 0;
    if (c->plane_buf) {
        size_t ya_size = c->fb.Frames[0].YPlane.BufferPitch * c->fb.YABufferHeight;
        size_t c_size  = c->fb.Frames[0].cRPlane.BufferPitch * c->fb.cRcBBufferHeight;
        size_t frame_size = ya_size + c_size + c_size + (c->has_alpha ? ya_size : 0);
        memset(c->plane_buf, 0, frame_size * 2);
    }
}

static av_cold int bink2_decode_end(AVCodecContext *avctx)
{
    Bink2Context *c = avctx->priv_data;

    av_freep(&c->plane_buf);

    return 0;
}

const FFCodec ff_bink2_decoder = {
    .p.name         = "binkvideo2",
    .p.long_name    = NULL_IF_CONFIG_SMALL("Bink video 2 (RAD SDK)"),
    .p.type         = AVMEDIA_TYPE_VIDEO,
    .p.id           = AV_CODEC_ID_BINKVIDEO2,
    .priv_data_size = sizeof(Bink2Context),
    .init           = bink2_decode_init,
    .close          = bink2_decode_end,
    FF_CODEC_DECODE_CB(bink2_decode_frame),
    .flush          = bink2_flush,
    .p.capabilities = AV_CODEC_CAP_DR1,
    .caps_internal  = FF_CODEC_CAP_INIT_CLEANUP,
};
