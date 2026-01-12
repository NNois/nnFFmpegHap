/*
 * BC6H (BPTC) block encoder.
 *
 * Ported from Compressonator (MIT license).
 */

#ifndef AVCODEC_BC6ENC_H
#define AVCODEC_BC6ENC_H

#include <stddef.h>
#include <stdint.h>

typedef struct BC6EncContext {
    int (*bc6enc_block)(uint8_t *dst, ptrdiff_t stride, const uint8_t *block);
} BC6EncContext;

typedef enum BC6EncInputFormat {
    BC6ENC_INPUT_RGBF16 = 0,
    BC6ENC_INPUT_RGBA64LE,
    BC6ENC_INPUT_RGBA64BE,
} BC6EncInputFormat;

void ff_bc6enc_init(BC6EncContext *c, int is_signed, uint16_t mode_mask,
                    float exposure, float quality, int use_pattern);
void ff_bc6enc_init_input(BC6EncContext *c, int is_signed, uint16_t mode_mask,
                          float exposure, float quality, int use_pattern,
                          BC6EncInputFormat input_fmt, int max_try,
                          int partitions_to_try);

#endif /* AVCODEC_BC6ENC_H */
