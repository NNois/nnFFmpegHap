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
 * Set BC6H quality level (0=fastest, 4=best).
 * Affects subsequent calls to basisu_bc6h_encode_block().
 */
void basisu_bc6h_set_quality(int quality);

/**
 * Encode a single 4x4 RGBA8 block to BC7 (auto RGB/RGBA detection).
 * Uses fast_pack_bc7_auto_rgba: if all 16 pixels have alpha==255,
 * uses the faster RGB-only path (decoded alpha will be 255).
 * Use basisu_bc7_encode_block_rgba for content with alpha gradients.
 * Compatible with FFmpeg's texturedsp block function signature.
 *
 * @param dst    Output: 16 bytes of BC7 compressed data
 * @param stride Stride of the source image in bytes
 * @param block  Input: pointer to top-left pixel of 4x4 block (RGBA8, 4 bytes/pixel)
 * @return 0 on success
 */
int basisu_bc7_encode_block(uint8_t *dst, ptrdiff_t stride, const uint8_t *block);

/**
 * Encode a single 4x4 RGBA8 block to BC7 (forced RGBA path).
 * Always uses the RGBA encoder, preserving alpha channel faithfully.
 * Use this for content with alpha (transparency gradients, etc.).
 * Compatible with FFmpeg's texturedsp block function signature.
 *
 * @param dst    Output: 16 bytes of BC7 compressed data
 * @param stride Stride of the source image in bytes
 * @param block  Input: pointer to top-left pixel of 4x4 block (RGBA8, 4 bytes/pixel)
 * @return 0 on success
 */
int basisu_bc7_encode_block_rgba(uint8_t *dst, ptrdiff_t stride, const uint8_t *block);

/**
 * Encode a single 4x4 RGBA half-float block to BC6H (unsigned).
 * Compatible with FFmpeg's texturedsp block function signature.
 * Uses the bc6hf analytical real-time encoder from basis_universal.
 *
 * @param dst    Output: 16 bytes of BC6H compressed data
 * @param stride Stride of the source image in bytes (half-float RGBA = 8 bytes/pixel)
 * @param block  Input: pointer to top-left pixel of 4x4 block (RGBA16 half-float, 8 bytes/pixel)
 * @return 0 on success
 */
int basisu_bc6h_encode_block(uint8_t *dst, ptrdiff_t stride, const uint8_t *block);

/**
 * Decode a single 16-byte BC7 block to 4x4 RGBA8 pixels.
 * Uses basis_universal's unpack_bc7 (supports all modes 0-7).
 * Compatible with FFmpeg's texturedsp block function signature.
 *
 * @param dst    Output: 4x4 RGBA8 pixels written row by row with stride
 * @param stride Stride of the destination image in bytes
 * @param block  Input: 16 bytes of BC7 compressed data
 * @return 16 (bytes of compressed data consumed)
 */
int basisu_bc7_decode_block(uint8_t *dst, ptrdiff_t stride, const uint8_t *block);

/**
 * Decode a single 16-byte BC6H block to 4x4 RGB half-float pixels.
 * Uses basis_universal's unpack_bc6h (supports all BC6H modes).
 * Compatible with FFmpeg's texturedsp block function signature.
 *
 * @param dst    Output: 4x4 RGB16F pixels written row by row with stride (3 x uint16 per pixel)
 * @param stride Stride of the destination image in bytes
 * @param block  Input: 16 bytes of BC6H compressed data
 * @return 16 (bytes of compressed data consumed)
 */
int basisu_bc6h_decode_block_u(uint8_t *dst, ptrdiff_t stride, const uint8_t *block);
int basisu_bc6h_decode_block_s(uint8_t *dst, ptrdiff_t stride, const uint8_t *block);

#ifdef __cplusplus
}
#endif

#endif /* AVCODEC_BASISU_WRAPPER_H */
