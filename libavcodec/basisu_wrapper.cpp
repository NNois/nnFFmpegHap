/*
 * basis_universal C wrapper for FFmpeg HAP encoder
 * Provides BC7 and BC6H block encoding via basis_universal library.
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

#include <cstring>
#include <cstdint>

/* basis_universal headers */
#include "basisu_enc.h"
#include "basisu_bc7enc.h"
#include "basisu_uastc_hdr_4x4_enc.h"
#include "basisu_astc_hdr_common.h"
#include "basisu_transcoder_internal.h"

#include "basisu_wrapper.h"

/* Current BC7 quality flags, set by basisu_bc7_set_quality() */
static uint32_t g_bc7_flags = basist::bc7f::cPackBC7FlagDefault;

/* Map FFmpeg quality 0-4 to bc7f flag presets */
static uint32_t bc7_quality_to_flags(int quality)
{
    switch (quality) {
    case 0:  return basist::bc7f::cPackBC7FlagDefaultFastest;
    case 1:  return basist::bc7f::cPackBC7FlagDefaultFaster;
    case 2:  return basist::bc7f::cPackBC7FlagDefaultFast;
    case 3:  return basist::bc7f::cPackBC7FlagDefault;
    case 4:  return basist::bc7f::cPackBC7FlagDefaultPartiallyAnalytical;
    default: return basist::bc7f::cPackBC7FlagDefault;
    }
}

extern "C" {

void basisu_init(void)
{
    static bool initialized = false;
    if (!initialized) {
        basisu::basisu_encoder_init(false, false);
        initialized = true;
    }
}

void basisu_bc7_set_quality(int quality)
{
    if (quality < BASISU_BC7_QUALITY_MIN)
        quality = BASISU_BC7_QUALITY_MIN;
    if (quality > BASISU_BC7_QUALITY_MAX)
        quality = BASISU_BC7_QUALITY_MAX;
    g_bc7_flags = bc7_quality_to_flags(quality);
}

int basisu_bc7_encode_block(uint8_t *dst, ptrdiff_t stride, const uint8_t *block)
{
    /*
     * FFmpeg texturedsp passes a pointer to the top-left of the 4x4 block
     * within the full image, with 'stride' as the row pitch.
     * We need to gather 16 RGBA pixels into a contiguous array.
     */
    basist::color_rgba pixels[16];

    for (int y = 0; y < 4; y++) {
        const uint8_t *row = block + y * stride;
        for (int x = 0; x < 4; x++) {
            pixels[y * 4 + x].r = row[x * 4 + 0];
            pixels[y * 4 + x].g = row[x * 4 + 1];
            pixels[y * 4 + x].b = row[x * 4 + 2];
            pixels[y * 4 + x].a = row[x * 4 + 3];
        }
    }

    basist::bc7f::fast_pack_bc7_auto_rgba(dst, pixels, g_bc7_flags);
    return 0;
}

int basisu_bc6h_compress_frame(uint8_t *out, const uint8_t *rgba_u16,
                               int width, int height, int quality)
{
    /*
     * Input: tightly packed RGBA half-float pixels (4 x uint16_t per pixel).
     * basis_universal expects RGB-only half_float arrays (3 x half_float per pixel)
     * for each 4x4 block.
     */
    const uint16_t *src = (const uint16_t *)rgba_u16;
    const int bw = width / 4;
    const int bh = height / 4;

    if (quality < BASISU_BC6H_QUALITY_MIN)
        quality = BASISU_BC6H_QUALITY_MIN;
    if (quality > BASISU_BC6H_QUALITY_MAX)
        quality = BASISU_BC6H_QUALITY_MAX;

    basisu::uastc_hdr_4x4_codec_options coptions;
    coptions.init();
    coptions.set_quality_level(quality);

    for (int by = 0; by < bh; by++) {
        for (int bx = 0; bx < bw; bx++) {
            basist::half_float rgb_half[48]; /* 16 pixels x 3 channels */
            float rgb_float[48];

            /* Extract 4x4 block, convert RGBA4 -> RGB3 */
            for (int py = 0; py < 4; py++) {
                int sy = by * 4 + py;
                for (int px = 0; px < 4; px++) {
                    int sx = bx * 4 + px;
                    int src_idx = (sy * width + sx) * 4;  /* RGBA = 4 uint16_t */
                    int dst_idx = (py * 4 + px) * 3;      /* RGB = 3 floats/halfs */

                    uint16_t r = src[src_idx + 0];
                    uint16_t g = src[src_idx + 1];
                    uint16_t b = src[src_idx + 2];
                    /* alpha channel dropped for BC6H (RGB-only format) */

                    rgb_half[dst_idx + 0] = r;
                    rgb_half[dst_idx + 1] = g;
                    rgb_half[dst_idx + 2] = b;

                    rgb_float[dst_idx + 0] = basist::half_to_float(r);
                    rgb_float[dst_idx + 1] = basist::half_to_float(g);
                    rgb_float[dst_idx + 2] = basist::half_to_float(b);
                }
            }

            basisu::vector<basisu::astc_hdr_4x4_pack_results> results;
            bool ok = basisu::astc_hdr_4x4_enc_block(rgb_float, rgb_half, coptions, results);
            if (!ok || results.empty())
                return -1;

            /* Pick best result and extract BC6H block */
            const basisu::astc_hdr_4x4_pack_results &best = results[0];
            int block_offset = (by * bw + bx) * 16;
            memcpy(out + block_offset, &best.m_bc6h_block, 16);
        }
    }

    return 0;
}

} /* extern "C" */
