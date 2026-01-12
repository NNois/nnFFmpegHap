/*
 * BC6H common definitions and tables.
 *
 * Ported from Compressonator (MIT license).
 */

#ifndef AVCODEC_BC6H_COMMON_H
#define AVCODEC_BC6H_COMMON_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t BYTE;
typedef uint32_t DWORD;

enum {
    TRUE = 1,
    FALSE = 0
};

#define BC6H_NUM_BLOCK_TYPES 14
#define BC6H_MAX_PARTITIONS 32
#define BC6H_MAX_SUBSETS 2
#define BC6H_MAX_SUBSET_SIZE 16
#define BC6H_COMPRESSED_BLOCK_SIZE 16
#define BC6H_MAX_PARTITIONS_TABLE (1 + 64 + 64)
#define BC6H_MAX_INDEX_BITS 4
#define NCHANNELS 3
#define NINDICES 16
#define MAXENDPOINTS 2

/* HDR encode shared constants */
#define MAX_SUBSET_SIZE 16
#define MAX_SUBSETS 3
#define MAX_INDEX_BITS 4
#define MAX_PARTITIONS 64
#define NUM_BLOCK_TYPES 8
#define COMPRESSED_BLOCK_SIZE 16
#define MAX_DIMENSION_BIG 4
#define F16HMAX 0x7bff
#define MAX_END_POINTS 2

/* F16 limits */
#define F16_MAX 65504.0f
#define F16_MAX_NEGATIVE -65504.0f
#define F16_MAX_BITS ((float)0x7BFF)
#define F16_MAX_NEGATIVE_BITS (-F16_MAX_BITS)

#define F16S_MASK 0x8000
#define F16EM_MASK 0x7fff
#define U16MAX 0xffff
#define S16MIN (-0x8000)
#define S16MAX 0x7fff
#define INT16_MASK 0xffff
#define BC6H_WEIGHT_MAX 64
#define BC6H_WEIGHT_SHIFT 6
#define BC6H_WEIGHT_ROUND 32
#define BC6H_OFFSET_MAG 31
#define MAX_TWOREGION_MODES 10
#define MAX_BC6H_MODES 14
#define MAX_BC6H_PARTITIONS 32
#define HIGH_INDEX_BIT 4
#define ONE_REGION_INDEX_OFFSET 65
#define TWO_REGION_INDEX_OFFSET 82
#define MIN_MODE_FOR_ONE_REGION 11
#define BC6H_NREGIONS 2

#define R_0(ep) (ep)[0][0][i]
#define R_1(ep) (ep)[0][1][i]
#define R_2(ep) (ep)[1][0][i]
#define R_3(ep) (ep)[1][1][i]

#define MASK(n) ((1 << (n)) - 1)
#define SIGN_EXTEND(w, tbits) \
    (((((int)(w)) & (1 << ((tbits) - 1))) ? ((~0) << (tbits)) : 0) | ((int)(w)))

typedef unsigned int uint;

typedef enum BC6H_COMPONENT {
    BC6H_COMP_RED   = 0,
    BC6H_COMP_GREEN = 1,
    BC6H_COMP_BLUE  = 2,
    BC6H_COMP_ALPHA = 3
} BC6H_COMPONENT;

enum {
    BC6_ONE = 0,
    BC6_TWO
};

enum {
    C_RED = 0,
    C_GREEN,
    C_BLUE
};

enum {
    UNSIGNED_F16 = 1,
    SIGNED_F16   = 2
};

enum EField {
    NA,
    M,
    D,
    RW,
    RX,
    RY,
    RZ,
    GW,
    GX,
    GY,
    GZ,
    BW,
    BX,
    BY,
    BZ,
};

typedef struct EndPointPair {
    int A;
    int B;
} EndPointPair;

typedef struct BC6H_Vec3 {
    int x, y, z;
} BC6H_Vec3;

typedef struct BC6H_Vec3f {
    float x, y, z;
} BC6H_Vec3f;

typedef struct UShortVec3 {
    unsigned short x, y, z;
} UShortVec3;

typedef struct ModePartitions {
    int nbits;
    int prec[3];
    int transformed;
    int modebits;
    int IndexPrec;
    int mode;
    int lowestPrec;
} ModePartitions;

extern const ModePartitions ModePartition[MAX_BC6H_MODES + 1];
extern const int g_Region2FixUp[32];
extern const int g_indexfixups[32];
extern const int g_aWeights3[8];
extern const int g_aWeights4[16];
extern const int PARTITIONS[MAX_SUBSETS][MAX_PARTITIONS][MAX_SUBSET_SIZE];

typedef struct END_Points {
    int A[NCHANNELS];
    int B[NCHANNELS];
} END_Points;

typedef struct AMD_BC6H_Format {
    unsigned short region;
    unsigned short m_mode;
    int            d_shape_index;
    int            rw;
    int            rx;
    int            ry;
    int            rz;
    int            gw;
    int            gx;
    int            gy;
    int            gz;
    int            bw;
    int            bx;
    int            by;
    int            bz;

    union {
        uint8_t indices[4][4];
        uint8_t indices16[16];
    };

    float      din[MAX_SUBSET_SIZE][MAX_DIMENSION_BIG];
    END_Points EC[MAXENDPOINTS];
    END_Points E[MAXENDPOINTS];
    int        issigned;
    int        istransformed;
    short      wBits;
    short      tBits[NCHANNELS];
    int        format;
    BC6H_Vec3  Palete[2][16];
    BC6H_Vec3f Paletef[2][16];

    int   index;
    float fEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];
    float cur_best_fEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];
    int   shape_indices[MAX_SUBSETS][MAX_SUBSET_SIZE];
    int   cur_best_shape_indices[MAX_SUBSETS][MAX_SUBSET_SIZE];
    int   entryCount[MAX_SUBSETS];
    int   cur_best_entryCount[MAX_SUBSETS];
    float partition[MAX_SUBSETS][MAX_SUBSET_SIZE][MAX_DIMENSION_BIG];
    float cur_best_partition[MAX_SUBSETS][MAX_SUBSET_SIZE][MAX_DIMENSION_BIG];
    int   optimized;
} AMD_BC6H_Format;

#endif /* AVCODEC_BC6H_COMMON_H */
