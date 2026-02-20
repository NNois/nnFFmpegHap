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

#ifndef AVCODEC_BASISU_WRAPPER_H
#define AVCODEC_BASISU_WRAPPER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BC7 quality levels (0-4), matching the old bc7enc uber_level */
#define BASISU_BC7_QUALITY_MIN  0
#define BASISU_BC7_QUALITY_MAX  4

/* BC6H quality levels (0-4) */
#define BASISU_BC6H_QUALITY_MIN 0
#define BASISU_BC6H_QUALITY_MAX 4

/**
 * Initialize the basis_universal encoder library.
 * Must be called once before any encoding function.
 */
void basisu_init(void);

/**
 * Set BC7 quality level (0=fastest, 4=best).
 * Affects subsequent calls to basisu_bc7_encode_block().
 */
void basisu_bc7_set_quality(int quality);

/**
 * Encode a single 4x4 RGBA8 block to BC7.
 * Compatible with FFmpeg's texturedsp block function signature.
 *
 * @param dst    Output: 16 bytes of BC7 compressed data
 * @param stride Stride of the source image in bytes
 * @param block  Input: pointer to top-left pixel of 4x4 block (RGBA8, 4 bytes/pixel)
 * @return 0 on success
 */
int basisu_bc7_encode_block(uint8_t *dst, ptrdiff_t stride, const uint8_t *block);

/**
 * Compress an entire frame to BC6H using ASTC HDR 4x4 -> BC6H transcode.
 *
 * @param out       Output buffer (must be at least (w/4)*(h/4)*16 bytes)
 * @param rgba_u16  Input: RGBA half-float pixels (4 x uint16_t per pixel, tightly packed)
 * @param width     Image width (must be multiple of 4)
 * @param height    Image height (must be multiple of 4)
 * @param quality   Quality level 0-4 (0=fastest, 4=best)
 * @return 0 on success, negative on error
 */
int basisu_bc6h_compress_frame(uint8_t *out, const uint8_t *rgba_u16,
                               int width, int height, int quality);

#ifdef __cplusplus
}
#endif

#endif /* AVCODEC_BASISU_WRAPPER_H */
