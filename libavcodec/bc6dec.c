/*
 * BC6H (BPTC) block decoder.
 *
 * Ported from Compressonator (MIT license).
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bc6h_common.h"
#include "bc6dec.h"

typedef struct BitHeader {
    uint8_t bits[128];
    int sizeinbytes;
} BitHeader;

static void bitheader_init(BitHeader *hdr, const uint8_t *in, int sizeinbytes)
{
    int bitpos = 0;

    memset(hdr->bits, 0, sizeof(hdr->bits));
    hdr->sizeinbytes = sizeinbytes;

    if (!in || sizeinbytes > 16)
        return;

    for (int i = 0; i < sizeinbytes; i++) {
        int bit = 1;
        for (int j = 0; j < 8; j++) {
            hdr->bits[bitpos] = (in[i] & bit) ? 1 : 0;
            bit <<= 1;
            bitpos++;
        }
    }
}

static int bitheader_getvalue(const BitHeader *hdr, int start, int bitsize)
{
    int value = 0;
    int end = start + bitsize - 1;

    for (; end >= start; end--) {
        value |= hdr->bits[end] ? 1 : 0;
        if (end > start)
            value <<= 1;
    }

    return value;
}

static void extract_compressed_endpoints2(AMD_BC6H_Format *bc6h_format)
{
    int t;

    if (bc6h_format->issigned) {
        if (bc6h_format->istransformed) {
            for (int i = 0; i < NCHANNELS; i++) {
                bc6h_format->E[0].A[i] = SIGN_EXTEND(bc6h_format->EC[0].A[i], bc6h_format->wBits);

                t = SIGN_EXTEND(bc6h_format->EC[0].B[i], bc6h_format->tBits[i]);
                t = (t + bc6h_format->EC[0].A[i]) & MASK(bc6h_format->wBits);
                bc6h_format->E[0].B[i] = SIGN_EXTEND(t, bc6h_format->wBits);

                t = SIGN_EXTEND(bc6h_format->EC[1].A[i], bc6h_format->tBits[i]);
                t = (t + bc6h_format->EC[0].A[i]) & MASK(bc6h_format->wBits);
                bc6h_format->E[1].A[i] = SIGN_EXTEND(t, bc6h_format->wBits);

                t = SIGN_EXTEND(bc6h_format->EC[1].B[i], bc6h_format->tBits[i]);
                t = (t + bc6h_format->EC[0].A[i]) & MASK(bc6h_format->wBits);
                bc6h_format->E[1].B[i] = SIGN_EXTEND(t, bc6h_format->wBits);
            }
        } else {
            for (int i = 0; i < NCHANNELS; i++) {
                bc6h_format->E[0].A[i] = SIGN_EXTEND(bc6h_format->EC[0].A[i], bc6h_format->wBits);
                bc6h_format->E[0].B[i] = SIGN_EXTEND(bc6h_format->EC[0].B[i], bc6h_format->tBits[i]);
                bc6h_format->E[1].A[i] = SIGN_EXTEND(bc6h_format->EC[1].A[i], bc6h_format->tBits[i]);
                bc6h_format->E[1].B[i] = SIGN_EXTEND(bc6h_format->EC[1].B[i], bc6h_format->tBits[i]);
            }
        }
    } else {
        if (bc6h_format->istransformed) {
            for (int i = 0; i < NCHANNELS; i++) {
                bc6h_format->E[0].A[i] = bc6h_format->EC[0].A[i];
                t = SIGN_EXTEND(bc6h_format->EC[0].B[i], bc6h_format->tBits[i]);
                bc6h_format->E[0].B[i] = (t + bc6h_format->EC[0].A[i]) & MASK(bc6h_format->wBits);

                t = SIGN_EXTEND(bc6h_format->EC[1].A[i], bc6h_format->tBits[i]);
                bc6h_format->E[1].A[i] = (t + bc6h_format->EC[0].A[i]) & MASK(bc6h_format->wBits);

                t = SIGN_EXTEND(bc6h_format->EC[1].B[i], bc6h_format->tBits[i]);
                bc6h_format->E[1].B[i] = (t + bc6h_format->EC[0].A[i]) & MASK(bc6h_format->wBits);
            }
        } else {
            for (int i = 0; i < NCHANNELS; i++) {
                bc6h_format->E[0].A[i] = bc6h_format->EC[0].A[i];
                bc6h_format->E[0].B[i] = bc6h_format->EC[0].B[i];
                bc6h_format->E[1].A[i] = bc6h_format->EC[1].A[i];
                bc6h_format->E[1].B[i] = bc6h_format->EC[1].B[i];
            }
        }
    }
}

static void extract_compressed_endpoints(AMD_BC6H_Format *bc6h_format)
{
    int t;

    if (bc6h_format->issigned) {
        if (bc6h_format->istransformed) {
            for (int i = 0; i < NCHANNELS; i++) {
                bc6h_format->E[0].A[i] = SIGN_EXTEND(bc6h_format->EC[0].A[i], bc6h_format->wBits);

                t = SIGN_EXTEND(bc6h_format->EC[0].B[i], bc6h_format->tBits[i]);
                t = (t + bc6h_format->EC[0].A[i]) & MASK(bc6h_format->wBits);
                bc6h_format->E[0].B[i] = SIGN_EXTEND(t, bc6h_format->wBits);
            }
        } else {
            for (int i = 0; i < NCHANNELS; i++) {
                bc6h_format->E[0].A[i] = SIGN_EXTEND(bc6h_format->EC[0].A[i], bc6h_format->wBits);
                bc6h_format->E[0].B[i] = SIGN_EXTEND(bc6h_format->EC[0].B[i], bc6h_format->tBits[i]);
            }
        }
    } else {
        if (bc6h_format->istransformed) {
            for (int i = 0; i < NCHANNELS; i++) {
                bc6h_format->E[0].A[i] = bc6h_format->EC[0].A[i];
                t = SIGN_EXTEND(bc6h_format->EC[0].B[i], bc6h_format->tBits[i]);
                bc6h_format->E[0].B[i] = (t + bc6h_format->EC[0].A[i]) & MASK(bc6h_format->wBits);
            }
        } else {
            for (int i = 0; i < NCHANNELS; i++) {
                bc6h_format->E[0].A[i] = bc6h_format->EC[0].A[i];
                bc6h_format->E[0].B[i] = bc6h_format->EC[0].B[i];
            }
        }
    }
}

static int unquantize(const AMD_BC6H_Format *bc6h_format, int q, int prec)
{
    int unq;

    switch (bc6h_format->format) {
    case UNSIGNED_F16:
        if (prec >= 15)
            unq = q;
        else if (q == 0)
            unq = 0;
        else if (q == ((1 << prec) - 1))
            unq = U16MAX;
        else
            unq = (q * (U16MAX + 1) + (U16MAX + 1) / 2) >> prec;
        break;
    case SIGNED_F16:
        if (prec >= 16)
            unq = q;
        else {
            int s = 0;
            if (q < 0) {
                s = 1;
                q = -q;
            }
            if (q == 0)
                unq = 0;
            else if (q >= ((1 << (prec - 1)) - 1))
                unq = S16MAX;
            else
                unq = ((q << 15) + 0x4000) >> (prec - 1);
            if (s)
                unq = -unq;
        }
        break;
    default:
        unq = q;
        break;
    }

    return unq;
}

static int lerp(int a, int b, int i, int denom)
{
    const int *weights = NULL;
    int shift = 0;

    switch (denom) {
    case 3:
        denom *= 5;
        i *= 5;
    case 15:
        weights = g_aWeights4;
        shift = 6;
        break;
    case 7:
        weights = g_aWeights3;
        shift = 6;
        break;
    default:
        return 0;
    }

    return (int)(a * weights[denom - i] + b * weights[i]) / (1 << shift);
}

static int finish_unquantize(const AMD_BC6H_Format *bc6h_format, int q)
{
    if (bc6h_format->format == UNSIGNED_F16)
        return (q * 31) >> 6;
    else if (bc6h_format->format == SIGNED_F16)
        return (q < 0) ? -(((-q) * 31) >> 5) : (q * 31) >> 5;
    return q;
}

static void generate_palette_quantized(int max, AMD_BC6H_Format *bc6h_format, int region)
{
    int a, b, c;

    a = unquantize(bc6h_format, bc6h_format->E[region].A[0], bc6h_format->wBits);
    b = unquantize(bc6h_format, bc6h_format->E[region].B[0], bc6h_format->wBits);
    for (int i = 0; i < max; i++) {
        c = finish_unquantize(bc6h_format, lerp(a, b, i, max - 1));
        bc6h_format->Palete[region][i].x = c;
    }

    a = unquantize(bc6h_format, bc6h_format->E[region].A[1], bc6h_format->wBits);
    b = unquantize(bc6h_format, bc6h_format->E[region].B[1], bc6h_format->wBits);
    for (int i = 0; i < max; i++)
        bc6h_format->Palete[region][i].y = finish_unquantize(bc6h_format, lerp(a, b, i, max - 1));

    a = unquantize(bc6h_format, bc6h_format->E[region].A[2], bc6h_format->wBits);
    b = unquantize(bc6h_format, bc6h_format->E[region].B[2], bc6h_format->wBits);
    for (int i = 0; i < max; i++)
        bc6h_format->Palete[region][i].z = finish_unquantize(bc6h_format, lerp(a, b, i, max - 1));
}

static AMD_BC6H_Format extract_format(const uint8_t in[COMPRESSED_BLOCK_SIZE])
{
    AMD_BC6H_Format bc6h_format;
    unsigned short decvalue;
    uint8_t iData[COMPRESSED_BLOCK_SIZE];
    BitHeader header;

    memcpy(iData, in, COMPRESSED_BLOCK_SIZE);
    memset(&bc6h_format, 0, sizeof(AMD_BC6H_Format));

    if ((in[0] & 0x02) > 0)
        decvalue = (in[0] & 0x1F);
    else
        decvalue = (in[0] & 0x01);

    bitheader_init(&header, in, 16);

    switch (decvalue) {
    case 0x00:
        bc6h_format.m_mode         = 1;
        bc6h_format.wBits          = 10;
        bc6h_format.tBits[C_RED]   = 5;
        bc6h_format.tBits[C_GREEN] = 5;
        bc6h_format.tBits[C_BLUE]  = 5;
        bc6h_format.rw             = bitheader_getvalue(&header, 5, 10);
        bc6h_format.rx             = bitheader_getvalue(&header, 35, 5);
        bc6h_format.ry             = bitheader_getvalue(&header, 65, 5);
        bc6h_format.rz             = bitheader_getvalue(&header, 71, 5);
        bc6h_format.gw             = bitheader_getvalue(&header, 15, 10);
        bc6h_format.gx             = bitheader_getvalue(&header, 45, 5);
        bc6h_format.gy             = bitheader_getvalue(&header, 41, 4) |
                         (bitheader_getvalue(&header, 2, 1) << 4);
        bc6h_format.gz = bitheader_getvalue(&header, 51, 4) |
                         (bitheader_getvalue(&header, 40, 1) << 4);
        bc6h_format.bw = bitheader_getvalue(&header, 25, 10);
        bc6h_format.bx = bitheader_getvalue(&header, 55, 5);
        bc6h_format.by = bitheader_getvalue(&header, 61, 4) |
                         (bitheader_getvalue(&header, 3, 1) << 4);
        bc6h_format.bz = bitheader_getvalue(&header, 50, 1) |
                         (bitheader_getvalue(&header, 60, 1) << 1) |
                         (bitheader_getvalue(&header, 70, 1) << 2) |
                         (bitheader_getvalue(&header, 76, 1) << 3) |
                         (bitheader_getvalue(&header, 4, 1) << 4);
        break;
    case 0x01:
        bc6h_format.m_mode         = 2;
        bc6h_format.wBits          = 7;
        bc6h_format.tBits[C_RED]   = 6;
        bc6h_format.tBits[C_GREEN] = 6;
        bc6h_format.tBits[C_BLUE]  = 6;
        bc6h_format.rw             = bitheader_getvalue(&header, 5, 7);
        bc6h_format.rx             = bitheader_getvalue(&header, 35, 6);
        bc6h_format.ry             = bitheader_getvalue(&header, 65, 6);
        bc6h_format.rz             = bitheader_getvalue(&header, 71, 6);
        bc6h_format.gw             = bitheader_getvalue(&header, 15, 7);
        bc6h_format.gx             = bitheader_getvalue(&header, 45, 6);
        bc6h_format.gy             = bitheader_getvalue(&header, 41, 4) |
                         (bitheader_getvalue(&header, 24, 1) << 4) |
                         (bitheader_getvalue(&header, 2, 1) << 5);
        bc6h_format.gz = bitheader_getvalue(&header, 51, 4) |
                         (bitheader_getvalue(&header, 3, 1) << 4) |
                         (bitheader_getvalue(&header, 4, 1) << 5);
        bc6h_format.bw = bitheader_getvalue(&header, 25, 7);
        bc6h_format.bx = bitheader_getvalue(&header, 55, 6);
        bc6h_format.by = bitheader_getvalue(&header, 61, 4) |
                         (bitheader_getvalue(&header, 14, 1) << 4) |
                         (bitheader_getvalue(&header, 22, 1) << 5);
        bc6h_format.bz = bitheader_getvalue(&header, 12, 1) |
                         (bitheader_getvalue(&header, 13, 1) << 1) |
                         (bitheader_getvalue(&header, 23, 1) << 2) |
                         (bitheader_getvalue(&header, 32, 1) << 3) |
                         (bitheader_getvalue(&header, 34, 1) << 4) |
                         (bitheader_getvalue(&header, 33, 1) << 5);
        break;
    case 0x02:
        bc6h_format.m_mode         = 3;
        bc6h_format.wBits          = 11;
        bc6h_format.tBits[C_RED]   = 5;
        bc6h_format.tBits[C_GREEN] = 4;
        bc6h_format.tBits[C_BLUE]  = 4;
        bc6h_format.rw             = bitheader_getvalue(&header, 5, 10) |
                         (bitheader_getvalue(&header, 40, 1) << 10);
        bc6h_format.rx = bitheader_getvalue(&header, 35, 5);
        bc6h_format.ry = bitheader_getvalue(&header, 65, 5);
        bc6h_format.rz = bitheader_getvalue(&header, 71, 5);
        bc6h_format.gw = bitheader_getvalue(&header, 15, 10) |
                         (bitheader_getvalue(&header, 49, 1) << 10);
        bc6h_format.gx = bitheader_getvalue(&header, 45, 4);
        bc6h_format.gy = bitheader_getvalue(&header, 41, 4);
        bc6h_format.gz = bitheader_getvalue(&header, 51, 4);
        bc6h_format.bw = bitheader_getvalue(&header, 25, 10) |
                         (bitheader_getvalue(&header, 59, 1) << 10);
        bc6h_format.bx = bitheader_getvalue(&header, 55, 4);
        bc6h_format.by = bitheader_getvalue(&header, 61, 4);
        bc6h_format.bz = bitheader_getvalue(&header, 50, 1) |
                         (bitheader_getvalue(&header, 60, 1) << 1) |
                         (bitheader_getvalue(&header, 70, 1) << 2) |
                         (bitheader_getvalue(&header, 76, 1) << 3);
        break;
    case 0x06:
        bc6h_format.m_mode         = 4;
        bc6h_format.wBits          = 11;
        bc6h_format.tBits[C_RED]   = 4;
        bc6h_format.tBits[C_GREEN] = 5;
        bc6h_format.tBits[C_BLUE]  = 4;
        bc6h_format.rw             = bitheader_getvalue(&header, 5, 10) |
                         (bitheader_getvalue(&header, 39, 1) << 10);
        bc6h_format.rx = bitheader_getvalue(&header, 35, 4);
        bc6h_format.ry = bitheader_getvalue(&header, 65, 4);
        bc6h_format.rz = bitheader_getvalue(&header, 71, 4);
        bc6h_format.gw = bitheader_getvalue(&header, 15, 10) |
                         (bitheader_getvalue(&header, 50, 1) << 10);
        bc6h_format.gx = bitheader_getvalue(&header, 45, 5);
        bc6h_format.gy = bitheader_getvalue(&header, 41, 4) |
                         (bitheader_getvalue(&header, 75, 1) << 4);
        bc6h_format.gz = bitheader_getvalue(&header, 51, 4) |
                         (bitheader_getvalue(&header, 40, 1) << 4);
        bc6h_format.bw = bitheader_getvalue(&header, 25, 10) |
                         (bitheader_getvalue(&header, 59, 1) << 10);
        bc6h_format.bx = bitheader_getvalue(&header, 55, 4);
        bc6h_format.by = bitheader_getvalue(&header, 61, 4);
        bc6h_format.bz = bitheader_getvalue(&header, 69, 1) |
                         (bitheader_getvalue(&header, 60, 1) << 1) |
                         (bitheader_getvalue(&header, 70, 1) << 2) |
                         (bitheader_getvalue(&header, 76, 1) << 3);
        break;
    case 0x0A:
        bc6h_format.m_mode         = 5;
        bc6h_format.wBits          = 11;
        bc6h_format.tBits[C_RED]   = 4;
        bc6h_format.tBits[C_GREEN] = 4;
        bc6h_format.tBits[C_BLUE]  = 5;
        bc6h_format.rw             = bitheader_getvalue(&header, 5, 10) |
                         (bitheader_getvalue(&header, 39, 1) << 10);
        bc6h_format.rx = bitheader_getvalue(&header, 35, 4);
        bc6h_format.ry = bitheader_getvalue(&header, 65, 4);
        bc6h_format.rz = bitheader_getvalue(&header, 71, 4);
        bc6h_format.gw = bitheader_getvalue(&header, 15, 10) |
                         (bitheader_getvalue(&header, 49, 1) << 10);
        bc6h_format.gx = bitheader_getvalue(&header, 45, 4);
        bc6h_format.gy = bitheader_getvalue(&header, 41, 4);
        bc6h_format.gz = bitheader_getvalue(&header, 51, 4);
        bc6h_format.bw = bitheader_getvalue(&header, 25, 10) |
                         (bitheader_getvalue(&header, 60, 1) << 10);
        bc6h_format.bx = bitheader_getvalue(&header, 55, 5);
        bc6h_format.by = bitheader_getvalue(&header, 61, 4) |
                         (bitheader_getvalue(&header, 40, 1) << 4);
        bc6h_format.bz = bitheader_getvalue(&header, 50, 1) |
                         (bitheader_getvalue(&header, 69, 1) << 1) |
                         (bitheader_getvalue(&header, 70, 1) << 2) |
                         (bitheader_getvalue(&header, 76, 1) << 3) |
                         (bitheader_getvalue(&header, 75, 1) << 4);
        break;
    case 0x0E:
        bc6h_format.m_mode         = 6;
        bc6h_format.wBits          = 9;
        bc6h_format.tBits[C_RED]   = 5;
        bc6h_format.tBits[C_GREEN] = 5;
        bc6h_format.tBits[C_BLUE]  = 5;
        bc6h_format.rw             = bitheader_getvalue(&header, 5, 9);
        bc6h_format.gw             = bitheader_getvalue(&header, 15, 9);
        bc6h_format.bw             = bitheader_getvalue(&header, 25, 9);
        bc6h_format.rx             = bitheader_getvalue(&header, 35, 5);
        bc6h_format.gx             = bitheader_getvalue(&header, 45, 5);
        bc6h_format.bx             = bitheader_getvalue(&header, 55, 5);
        bc6h_format.ry             = bitheader_getvalue(&header, 65, 5);
        bc6h_format.gy             = bitheader_getvalue(&header, 41, 4) |
                         (bitheader_getvalue(&header, 24, 1) << 4);
        bc6h_format.by = bitheader_getvalue(&header, 61, 4) |
                         (bitheader_getvalue(&header, 14, 1) << 4);
        bc6h_format.rz = bitheader_getvalue(&header, 71, 5);
        bc6h_format.gz = bitheader_getvalue(&header, 51, 4) |
                         (bitheader_getvalue(&header, 40, 1) << 4);
        bc6h_format.bz = bitheader_getvalue(&header, 50, 1) |
                         (bitheader_getvalue(&header, 60, 1) << 1) |
                         (bitheader_getvalue(&header, 70, 1) << 2) |
                         (bitheader_getvalue(&header, 76, 1) << 3) |
                         (bitheader_getvalue(&header, 34, 1) << 4);
        break;
    case 0x12:
        bc6h_format.m_mode         = 7;
        bc6h_format.wBits          = 8;
        bc6h_format.tBits[C_RED]   = 6;
        bc6h_format.tBits[C_GREEN] = 5;
        bc6h_format.tBits[C_BLUE]  = 5;
        bc6h_format.rw             = bitheader_getvalue(&header, 5, 8);
        bc6h_format.gw             = bitheader_getvalue(&header, 15, 8);
        bc6h_format.bw             = bitheader_getvalue(&header, 25, 8);
        bc6h_format.rx             = bitheader_getvalue(&header, 35, 6);
        bc6h_format.gx             = bitheader_getvalue(&header, 45, 5);
        bc6h_format.bx             = bitheader_getvalue(&header, 55, 5);
        bc6h_format.ry             = bitheader_getvalue(&header, 65, 6);
        bc6h_format.gy             = bitheader_getvalue(&header, 41, 4) |
                         (bitheader_getvalue(&header, 24, 1) << 4);
        bc6h_format.by = bitheader_getvalue(&header, 61, 4) |
                         (bitheader_getvalue(&header, 14, 1) << 4);
        bc6h_format.rz = bitheader_getvalue(&header, 71, 6);
        bc6h_format.gz = bitheader_getvalue(&header, 51, 4) |
                         (bitheader_getvalue(&header, 13, 1) << 4);
        bc6h_format.bz = bitheader_getvalue(&header, 50, 1) |
                         (bitheader_getvalue(&header, 60, 1) << 1) |
                         (bitheader_getvalue(&header, 23, 1) << 2) |
                         (bitheader_getvalue(&header, 33, 1) << 3) |
                         (bitheader_getvalue(&header, 34, 1) << 4);
        break;
    case 0x16:
        bc6h_format.m_mode         = 8;
        bc6h_format.wBits          = 8;
        bc6h_format.tBits[C_RED]   = 5;
        bc6h_format.tBits[C_GREEN] = 6;
        bc6h_format.tBits[C_BLUE]  = 5;
        bc6h_format.rw             = bitheader_getvalue(&header, 5, 8);
        bc6h_format.gw             = bitheader_getvalue(&header, 15, 8);
        bc6h_format.bw             = bitheader_getvalue(&header, 25, 8);
        bc6h_format.rx             = bitheader_getvalue(&header, 35, 5);
        bc6h_format.gx             = bitheader_getvalue(&header, 45, 6);
        bc6h_format.bx             = bitheader_getvalue(&header, 55, 5);
        bc6h_format.ry             = bitheader_getvalue(&header, 65, 5);
        bc6h_format.gy             = bitheader_getvalue(&header, 41, 4) |
                         (bitheader_getvalue(&header, 24, 1) << 4) |
                         (bitheader_getvalue(&header, 23, 1) << 5);
        bc6h_format.by = bitheader_getvalue(&header, 61, 4) |
                         (bitheader_getvalue(&header, 14, 1) << 4);
        bc6h_format.rz = bitheader_getvalue(&header, 71, 5);
        bc6h_format.gz = bitheader_getvalue(&header, 51, 4) |
                         (bitheader_getvalue(&header, 40, 1) << 4) |
                         (bitheader_getvalue(&header, 33, 1) << 5);
        bc6h_format.bz = bitheader_getvalue(&header, 13, 1) |
                         (bitheader_getvalue(&header, 60, 1) << 1) |
                         (bitheader_getvalue(&header, 70, 1) << 2) |
                         (bitheader_getvalue(&header, 76, 1) << 3) |
                         (bitheader_getvalue(&header, 34, 1) << 4);
        break;
    case 0x1A:
        bc6h_format.m_mode         = 9;
        bc6h_format.wBits          = 8;
        bc6h_format.tBits[C_RED]   = 5;
        bc6h_format.tBits[C_GREEN] = 5;
        bc6h_format.tBits[C_BLUE]  = 6;
        bc6h_format.rw             = bitheader_getvalue(&header, 5, 8);
        bc6h_format.gw             = bitheader_getvalue(&header, 15, 8);
        bc6h_format.bw             = bitheader_getvalue(&header, 25, 8);
        bc6h_format.rx             = bitheader_getvalue(&header, 35, 5);
        bc6h_format.gx             = bitheader_getvalue(&header, 45, 5);
        bc6h_format.bx             = bitheader_getvalue(&header, 55, 6);
        bc6h_format.ry             = bitheader_getvalue(&header, 65, 5);
        bc6h_format.gy             = bitheader_getvalue(&header, 41, 4) |
                         (bitheader_getvalue(&header, 24, 1) << 4);
        bc6h_format.by = bitheader_getvalue(&header, 61, 4) |
                         (bitheader_getvalue(&header, 14, 1) << 4) |
                         (bitheader_getvalue(&header, 23, 1) << 5);
        bc6h_format.rz = bitheader_getvalue(&header, 71, 5);
        bc6h_format.gz = bitheader_getvalue(&header, 51, 4) |
                         (bitheader_getvalue(&header, 40, 1) << 4);
        bc6h_format.bz = bitheader_getvalue(&header, 50, 1) |
                         (bitheader_getvalue(&header, 13, 1) << 1) |
                         (bitheader_getvalue(&header, 70, 1) << 2) |
                         (bitheader_getvalue(&header, 76, 1) << 3) |
                         (bitheader_getvalue(&header, 34, 1) << 4) |
                         (bitheader_getvalue(&header, 33, 1) << 5);
        break;
    case 0x1E:
        bc6h_format.m_mode         = 10;
        bc6h_format.istransformed  = FALSE;
        bc6h_format.wBits          = 6;
        bc6h_format.tBits[C_RED]   = 6;
        bc6h_format.tBits[C_GREEN] = 6;
        bc6h_format.tBits[C_BLUE]  = 6;
        bc6h_format.rw             = bitheader_getvalue(&header, 5, 6);
        bc6h_format.gw             = bitheader_getvalue(&header, 15, 6);
        bc6h_format.bw             = bitheader_getvalue(&header, 25, 6);
        bc6h_format.rx             = bitheader_getvalue(&header, 35, 6);
        bc6h_format.gx             = bitheader_getvalue(&header, 45, 6);
        bc6h_format.bx             = bitheader_getvalue(&header, 55, 6);
        bc6h_format.ry             = bitheader_getvalue(&header, 65, 6);
        bc6h_format.gy             = bitheader_getvalue(&header, 41, 4) |
                         (bitheader_getvalue(&header, 24, 1) << 4) |
                         (bitheader_getvalue(&header, 21, 1) << 5);
        bc6h_format.by = bitheader_getvalue(&header, 61, 4) |
                         (bitheader_getvalue(&header, 14, 1) << 4) |
                         (bitheader_getvalue(&header, 22, 1) << 5);
        bc6h_format.rz = bitheader_getvalue(&header, 71, 6);
        bc6h_format.gz = bitheader_getvalue(&header, 51, 4) |
                         (bitheader_getvalue(&header, 11, 1) << 4) |
                         (bitheader_getvalue(&header, 31, 1) << 5);
        bc6h_format.bz = bitheader_getvalue(&header, 12, 1) |
                         (bitheader_getvalue(&header, 13, 1) << 1) |
                         (bitheader_getvalue(&header, 23, 1) << 2) |
                         (bitheader_getvalue(&header, 32, 1) << 3) |
                         (bitheader_getvalue(&header, 34, 1) << 4) |
                         (bitheader_getvalue(&header, 33, 1) << 5);
        break;
    case 0x03:
        bc6h_format.m_mode         = 11;
        bc6h_format.wBits          = 10;
        bc6h_format.tBits[C_RED]   = 10;
        bc6h_format.tBits[C_GREEN] = 10;
        bc6h_format.tBits[C_BLUE]  = 10;
        bc6h_format.rw             = bitheader_getvalue(&header, 5, 10);
        bc6h_format.gw             = bitheader_getvalue(&header, 15, 10);
        bc6h_format.bw             = bitheader_getvalue(&header, 25, 10);
        bc6h_format.rx             = bitheader_getvalue(&header, 35, 10);
        bc6h_format.gx             = bitheader_getvalue(&header, 45, 10);
        bc6h_format.bx             = bitheader_getvalue(&header, 55, 10);
        break;
    case 0x07:
        bc6h_format.m_mode         = 12;
        bc6h_format.wBits          = 11;
        bc6h_format.tBits[C_RED]   = 9;
        bc6h_format.tBits[C_GREEN] = 9;
        bc6h_format.tBits[C_BLUE]  = 9;
        bc6h_format.rw             = bitheader_getvalue(&header, 5, 10) |
                         (bitheader_getvalue(&header, 44, 1) << 10);
        bc6h_format.gw = bitheader_getvalue(&header, 15, 10) |
                         (bitheader_getvalue(&header, 54, 1) << 10);
        bc6h_format.bw = bitheader_getvalue(&header, 25, 10) |
                         (bitheader_getvalue(&header, 64, 1) << 10);
        bc6h_format.rx = bitheader_getvalue(&header, 35, 9);
        bc6h_format.gx = bitheader_getvalue(&header, 45, 9);
        bc6h_format.bx = bitheader_getvalue(&header, 55, 9);
        break;
    case 0x0B:
        bc6h_format.m_mode         = 13;
        bc6h_format.wBits          = 12;
        bc6h_format.tBits[C_RED]   = 8;
        bc6h_format.tBits[C_GREEN] = 8;
        bc6h_format.tBits[C_BLUE]  = 8;
        bc6h_format.rw             = bitheader_getvalue(&header, 5, 10) |
                         (bitheader_getvalue(&header, 43, 1) << 11) |
                         (bitheader_getvalue(&header, 44, 1) << 10);
        bc6h_format.gw = bitheader_getvalue(&header, 15, 10) |
                         (bitheader_getvalue(&header, 53, 1) << 11) |
                         (bitheader_getvalue(&header, 54, 1) << 10);
        bc6h_format.bw = bitheader_getvalue(&header, 25, 10) |
                         (bitheader_getvalue(&header, 63, 1) << 11) |
                         (bitheader_getvalue(&header, 64, 1) << 10);
        bc6h_format.rx = bitheader_getvalue(&header, 35, 8);
        bc6h_format.gx = bitheader_getvalue(&header, 45, 8);
        bc6h_format.bx = bitheader_getvalue(&header, 55, 8);
        break;
    case 0x0F:
        bc6h_format.m_mode         = 14;
        bc6h_format.wBits          = 16;
        bc6h_format.tBits[C_RED]   = 4;
        bc6h_format.tBits[C_GREEN] = 4;
        bc6h_format.tBits[C_BLUE]  = 4;
        bc6h_format.rw             = bitheader_getvalue(&header, 5, 10) |
                         (bitheader_getvalue(&header, 39, 1) << 15) |
                         (bitheader_getvalue(&header, 40, 1) << 14) |
                         (bitheader_getvalue(&header, 41, 1) << 13) |
                         (bitheader_getvalue(&header, 42, 1) << 12) |
                         (bitheader_getvalue(&header, 43, 1) << 11) |
                         (bitheader_getvalue(&header, 44, 1) << 10);
        bc6h_format.gw = bitheader_getvalue(&header, 15, 10) |
                         (bitheader_getvalue(&header, 49, 1) << 15) |
                         (bitheader_getvalue(&header, 50, 1) << 14) |
                         (bitheader_getvalue(&header, 51, 1) << 13) |
                         (bitheader_getvalue(&header, 52, 1) << 12) |
                         (bitheader_getvalue(&header, 53, 1) << 11) |
                         (bitheader_getvalue(&header, 54, 1) << 10);
        bc6h_format.bw = bitheader_getvalue(&header, 25, 10) |
                         (bitheader_getvalue(&header, 59, 1) << 15) |
                         (bitheader_getvalue(&header, 60, 1) << 14) |
                         (bitheader_getvalue(&header, 61, 1) << 13) |
                         (bitheader_getvalue(&header, 62, 1) << 12) |
                         (bitheader_getvalue(&header, 63, 1) << 11) |
                         (bitheader_getvalue(&header, 64, 1) << 10);
        bc6h_format.rx = bitheader_getvalue(&header, 35, 4);
        bc6h_format.gx = bitheader_getvalue(&header, 45, 4);
        bc6h_format.bx = bitheader_getvalue(&header, 55, 4);
        break;
    default:
        bc6h_format.m_mode = 0;
        return bc6h_format;
    }

    if (bc6h_format.m_mode <= 10) {
        bc6h_format.region = BC6_TWO;
        bc6h_format.d_shape_index = (unsigned short)bitheader_getvalue(&header, 77, 5);
        bc6h_format.istransformed = (bc6h_format.m_mode < 10) ? TRUE : FALSE;
    } else {
        bc6h_format.region        = BC6_ONE;
        bc6h_format.d_shape_index = 0;
        bc6h_format.istransformed = (bc6h_format.m_mode > 11) ? TRUE : FALSE;
    }

    bc6h_format.EC[0].A[0] = bc6h_format.rw;
    bc6h_format.EC[0].B[0] = bc6h_format.rx;
    bc6h_format.EC[1].A[0] = bc6h_format.ry;
    bc6h_format.EC[1].B[0] = bc6h_format.rz;
    bc6h_format.EC[0].A[1] = bc6h_format.gw;
    bc6h_format.EC[0].B[1] = bc6h_format.gx;
    bc6h_format.EC[1].A[1] = bc6h_format.gy;
    bc6h_format.EC[1].B[1] = bc6h_format.gz;
    bc6h_format.EC[0].A[2] = bc6h_format.bw;
    bc6h_format.EC[0].B[2] = bc6h_format.bx;
    bc6h_format.EC[1].A[2] = bc6h_format.by;
    bc6h_format.EC[1].B[2] = bc6h_format.bz;

    if (bc6h_format.region == BC6_ONE) {
        int startbits = ONE_REGION_INDEX_OFFSET;
        bc6h_format.indices16[0] = (uint8_t)bitheader_getvalue(&header, startbits, 3);
        startbits += 3;
        for (int i = 1; i < 16; i++) {
            bc6h_format.indices16[i] = (uint8_t)bitheader_getvalue(&header, startbits, 4);
            startbits += 4;
        }
    } else {
        int startbit = TWO_REGION_INDEX_OFFSET;
        int nbits = 2;
        bc6h_format.indices16[0] = (uint8_t)bitheader_getvalue(&header, startbit, 2);
        for (int i = 1; i < 16; i++) {
            startbit += nbits;
            nbits = g_indexfixups[bc6h_format.d_shape_index] == i ? 2 : 3;
            bc6h_format.indices16[i] = (uint8_t)bitheader_getvalue(&header, startbit, nbits);
        }
    }

    return bc6h_format;
}

static int bc6h_decode_block(uint8_t *dst, ptrdiff_t stride, const uint8_t *block, int is_signed)
{
    AMD_BC6H_Format bc6h_format = extract_format(block);

    if (bc6h_format.m_mode == 0) {
        for (int y = 0; y < 4; y++)
            memset(dst + y * stride, 0, 24);
        return 16;
    }

    bc6h_format.issigned = is_signed;
    bc6h_format.format = is_signed ? SIGNED_F16 : UNSIGNED_F16;

    if (bc6h_format.region == BC6_ONE) {
        extract_compressed_endpoints(&bc6h_format);
        generate_palette_quantized(16, &bc6h_format, 0);
    } else {
        extract_compressed_endpoints2(&bc6h_format);
        for (int r = 0; r < 2; r++)
            generate_palette_quantized(8, &bc6h_format, r);
    }

    int indexPos = 0;
    for (int block_row = 0; block_row < 4; block_row++) {
        uint16_t *row = (uint16_t *)(dst + block_row * stride);
        for (int block_col = 0; block_col < 4; block_col++) {
            int region = bc6h_format.region == BC6_ONE ? 0 : PARTITIONS[1][bc6h_format.d_shape_index][indexPos];
            int paleteIndex = bc6h_format.indices[block_row][block_col];
            BC6H_Vec3 data = bc6h_format.Palete[region][paleteIndex];
            uint16_t r = (uint16_t)abs(data.x);
            uint16_t g = (uint16_t)abs(data.y);
            uint16_t b = (uint16_t)abs(data.z);

            if (data.x < 0)
                r |= 0x8000;
            if (data.y < 0)
                g |= 0x8000;
            if (data.z < 0)
                b |= 0x8000;

            row[block_col * 3 + 0] = r;
            row[block_col * 3 + 1] = g;
            row[block_col * 3 + 2] = b;

            indexPos++;
        }
    }

    return 16;
}

int ff_bc6dec_block_u(uint8_t *dst, ptrdiff_t stride, const uint8_t *block)
{
    return bc6h_decode_block(dst, stride, block, 0);
}

int ff_bc6dec_block_s(uint8_t *dst, ptrdiff_t stride, const uint8_t *block)
{
    return bc6h_decode_block(dst, stride, block, 1);
}
