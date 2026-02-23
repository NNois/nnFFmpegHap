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
#include "basisu_transcoder_internal.h"
#include "basisu_astc_hdr_core.h"

#include "basisu_wrapper.h"

/* Current BC7 quality flags, set by basisu_bc7_set_quality() */
static uint32_t g_bc7_flags = basist::bc7f::cPackBC7FlagDefault;

/* Current BC6H params, set by basisu_bc6h_set_quality() */
static basist::astc_6x6_hdr::fast_bc6h_params g_bc6h_params;

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

int basisu_bc7_encode_block_rgba(uint8_t *dst, ptrdiff_t stride, const uint8_t *block)
{
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

    /*
     * Force RGBA path: never fall back to RGB-only modes.
     * fast_pack_bc7_auto_rgba checks if all alpha==255 and routes to
     * fast_pack_bc7_rgb_analytical, which uses modes that don't store alpha.
     * For content with alpha gradients, we must always use the RGBA encoder.
     */
    if (g_bc7_flags & basist::bc7f::cPackBC7FlagPartiallyAnalyticalRGBA)
        basist::bc7f::fast_pack_bc7_rgba_partial_analytical(dst, pixels, g_bc7_flags);
    else
        basist::bc7f::fast_pack_bc7_rgba_analytical(dst, pixels, g_bc7_flags);

    return 0;
}

void basisu_bc6h_set_quality(int quality)
{
    if (quality < BASISU_BC6H_QUALITY_MIN)
        quality = BASISU_BC6H_QUALITY_MIN;
    if (quality > BASISU_BC6H_QUALITY_MAX)
        quality = BASISU_BC6H_QUALITY_MAX;

    g_bc6h_params.init();
    switch (quality) {
    case 0: /* fastest: 1-subset only, minimal work */
        g_bc6h_params.m_num_diff_endpoint_modes_to_try =
            basist::astc_6x6_hdr::BC6H_NUM_DIFF_ENDPOINT_MODES_TO_TRY_2;
        g_bc6h_params.m_max_2subset_pats_to_try = 0;
        g_bc6h_params.m_hq_ls = false;
        g_bc6h_params.m_brute_force_weight4_assignment = false;
        break;
    case 1: /* fast */
        g_bc6h_params.m_num_diff_endpoint_modes_to_try =
            basist::astc_6x6_hdr::BC6H_NUM_DIFF_ENDPOINT_MODES_TO_TRY_2;
        g_bc6h_params.m_max_2subset_pats_to_try = 1;
        g_bc6h_params.m_hq_ls = false;
        g_bc6h_params.m_brute_force_weight4_assignment = false;
        break;
    case 2: /* normal (default) */
        g_bc6h_params.m_num_diff_endpoint_modes_to_try =
            basist::astc_6x6_hdr::BC6H_NUM_DIFF_ENDPOINT_MODES_TO_TRY_2;
        g_bc6h_params.m_max_2subset_pats_to_try = 1;
        g_bc6h_params.m_hq_ls = true;
        g_bc6h_params.m_brute_force_weight4_assignment = false;
        break;
    case 3: /* slow */
        g_bc6h_params.m_num_diff_endpoint_modes_to_try =
            basist::astc_6x6_hdr::BC6H_NUM_DIFF_ENDPOINT_MODES_TO_TRY_4;
        g_bc6h_params.m_max_2subset_pats_to_try = 1;
        g_bc6h_params.m_hq_ls = true;
        g_bc6h_params.m_brute_force_weight4_assignment = false;
        break;
    case 4: /* best */
        g_bc6h_params.m_num_diff_endpoint_modes_to_try =
            basist::astc_6x6_hdr::BC6H_NUM_DIFF_ENDPOINT_MODES_TO_TRY_9;
        g_bc6h_params.m_max_2subset_pats_to_try = 1;
        g_bc6h_params.m_hq_ls = true;
        g_bc6h_params.m_brute_force_weight4_assignment = true;
        break;
    }
}

int basisu_bc6h_encode_block(uint8_t *dst, ptrdiff_t stride, const uint8_t *block)
{
    /*
     * Input: 4x4 block of RGBA half-float pixels (4 x uint16_t per pixel = 8 bytes/pixel).
     * 'block' points to top-left pixel in the image, 'stride' is row pitch in bytes.
     * fast_encode_bc6h expects 48 RGB half-floats (16 pixels x 3 channels, no alpha).
     */
    basist::half_float rgb_half[48];

    for (int y = 0; y < 4; y++) {
        const uint16_t *row = (const uint16_t *)(block + y * stride);
        for (int x = 0; x < 4; x++) {
            int dst_idx = (y * 4 + x) * 3;
            rgb_half[dst_idx + 0] = row[x * 4 + 0]; /* R */
            rgb_half[dst_idx + 1] = row[x * 4 + 1]; /* G */
            rgb_half[dst_idx + 2] = row[x * 4 + 2]; /* B */
            /* alpha (row[x*4+3]) dropped — BC6H is RGB-only */
        }
    }

    basist::bc6h_block bc6h_blk;
    basist::astc_6x6_hdr::fast_encode_bc6h(rgb_half, &bc6h_blk, g_bc6h_params);
    memcpy(dst, bc6h_blk.m_bytes, 16);

    return 0;
}

} /* extern "C" */
