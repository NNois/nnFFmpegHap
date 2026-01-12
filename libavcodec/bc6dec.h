/*
 * BC6H (BPTC) block decoder.
 *
 * Ported from Compressonator (MIT license).
 */

#ifndef AVCODEC_BC6DEC_H
#define AVCODEC_BC6DEC_H

#include <stddef.h>
#include <stdint.h>

int ff_bc6dec_block_u(uint8_t *dst, ptrdiff_t stride, const uint8_t *block);
int ff_bc6dec_block_s(uint8_t *dst, ptrdiff_t stride, const uint8_t *block);

#endif /* AVCODEC_BC6DEC_H */
