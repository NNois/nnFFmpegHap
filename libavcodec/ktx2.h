/*
 * KTX2 (Khronos Texture 2.0) — BC7 images with their mip levels
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
 * The parts of the KTX 2.0 specification (KhronosGroup/KTX-Specification)
 * that the ktx2 encoder writes and the ktx2 decoder reads:
 *   identifier (12 bytes), header (9 x uint32), index (4 x uint32 +
 *   2 x uint64), level index (levelCount x 3 x uint64, level 0 first),
 *   Data Format Descriptor, key/value data, then the mip levels stored
 *   smallest first. All integers little-endian.
 */

#ifndef AVCODEC_KTX2_H
#define AVCODEC_KTX2_H

#include <stdint.h>

#define KTX2_IDENTIFIER_SIZE    12
#define KTX2_HEADER_SIZE        (KTX2_IDENTIFIER_SIZE + 9 * 4 + 4 * 4 + 2 * 8)  /* 80 */
#define KTX2_LEVEL_ENTRY_SIZE   24

static const uint8_t ff_ktx2_identifier[KTX2_IDENTIFIER_SIZE] = {
    0xAB, 'K', 'T', 'X', ' ', '2', '0', 0xBB, '\r', '\n', 0x1A, '\n'
};

/* VkFormat values (vulkan_core.h) */
#define KTX2_VK_FORMAT_BC7_UNORM_BLOCK  145
#define KTX2_VK_FORMAT_BC7_SRGB_BLOCK   146

/* supercompressionScheme */
#define KTX2_SUPERCOMPRESSION_NONE      0
#define KTX2_SUPERCOMPRESSION_ZSTD      2

/* Khronos Data Format: basic descriptor block of a BC7 texel block */
#define KTX2_DF_MODEL_BC7               134
#define KTX2_DF_PRIMARIES_BT709         1
#define KTX2_DF_TRANSFER_LINEAR         1
#define KTX2_DF_TRANSFER_SRGB           2
#define KTX2_DF_VERSION_1_3             2
#define KTX2_DFD_BC7_SIZE               44   /* dfdTotalSize + 24-byte block + 1 sample */

/* BC7: 4x4 texels in 16 bytes */
#define KTX2_BC7_BLOCK_BYTES            16

#endif /* AVCODEC_KTX2_H */
