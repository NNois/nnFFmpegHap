/*
 * BC7: an opaque block decodes with an alpha of exactly 255
 * Alternative Developpement 2026
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
 * basis_universal's BC7 encoder, at the qualities used by Hap R and ktx2 (bc7_quality 3 and 4:
 * cPackBC7FlagPBitOptMode6, "alpha on fully opaque blocks may be 254 however"), writes blocks of a fully
 * opaque 4x4 source whose alpha endpoint carries a p-bit of 0: mode 6 decodes alpha (127 << 1) | 0 = 254 —
 * on an opaque 1920x1280 photo, 28.6 % of the pixels, in Hap R as in ktx2. Mode 7 (the RGBA two-subset
 * mode) would decode 251 the same way. For such a block, setting those p-bits gives alpha 255; the colour of
 * that endpoint moves by 1/255 (mode 6) or 1/63 (mode 7).
 *
 * Bits, LSB first (BC7 specification):
 *   mode 6: mode 0-6 (0b1000000), R0 R1 G0 G1 B0 B1 7 bits each (7-48), A0 49-55, A1 56-62, P0 63, P1 64.
 *   mode 7: mode 0-7 (0b10000000), partition 8-13, R 14-33, G 34-53, B 54-73 (4 x 5 bits each),
 *           A0..A3 74-93 (5 bits each), P0..P3 94-97.
 */

#ifndef AVCODEC_BC7OPAQUE_H
#define AVCODEC_BC7OPAQUE_H

#include <stddef.h>
#include <stdint.h>

#include "libavutil/intreadwrite.h"

/** Are the 16 RGBA8 pixels of a 4x4 source block all at alpha 255? */
static inline int ff_bc7_source_opaque(const uint8_t *block, ptrdiff_t stride)
{
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (block[y * stride + x * 4 + 3] != 255)
                return 0;
    return 1;
}

static inline unsigned ff_bc7_bits(uint64_t lo, uint64_t hi, int at, int count)
{
    unsigned v = 0;
    for (int i = 0; i < count; i++) {
        const int n = at + i;
        v |= (unsigned)((n < 64 ? lo >> n : hi >> (n - 64)) & 1) << i;
    }
    return v;
}

static inline void ff_bc7_set_bit(uint64_t *lo, uint64_t *hi, int n)
{
    if (n < 64)
        *lo |= 1ULL << n;
    else
        *hi |= 1ULL << (n - 64);
}

/**
 * The BC7 block of a FULLY OPAQUE source: every alpha endpoint at its maximum with a p-bit of 0 gets its
 * p-bit set, so that the block decodes at alpha 255. Returns 1 when the block was changed.
 */
static inline int ff_bc7_fix_opaque_alpha(uint8_t *block)
{
    uint64_t lo = AV_RL64(block), hi = AV_RL64(block + 8);
    int changed = 0;

    if ((lo & 0x7F) == 0x40) {                          /* mode 6 */
        for (int e = 0; e < 2; e++) {
            if (ff_bc7_bits(lo, hi, 49 + 7 * e, 7) == 0x7F && !ff_bc7_bits(lo, hi, 63 + e, 1)) {
                ff_bc7_set_bit(&lo, &hi, 63 + e);
                changed = 1;
            }
        }
    } else if ((lo & 0xFF) == 0x80) {                   /* mode 7 */
        for (int e = 0; e < 4; e++) {
            if (ff_bc7_bits(lo, hi, 74 + 5 * e, 5) == 0x1F && !ff_bc7_bits(lo, hi, 94 + e, 1)) {
                ff_bc7_set_bit(&lo, &hi, 94 + e);
                changed = 1;
            }
        }
    }
    if (changed) {
        AV_WL64(block, lo);
        AV_WL64(block + 8, hi);
    }
    return changed;
}

#endif /* AVCODEC_BC7OPAQUE_H */
