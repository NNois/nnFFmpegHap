/*
 * BC7 (BPTC) block decoder (limited).
 * Based on bc7enc16's bc7decomp.c (ISC license).
 */

#ifndef AVCODEC_BC7DEC_H
#define AVCODEC_BC7DEC_H

#include <stddef.h>
#include <stdint.h>

int ff_bc7dec_block(uint8_t *dst, ptrdiff_t stride, const uint8_t *block);

#endif /* AVCODEC_BC7DEC_H */
