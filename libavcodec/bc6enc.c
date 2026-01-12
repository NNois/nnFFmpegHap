/*
 * BC6H (BPTC) block encoder.
 *
 * Ported from Compressonator (MIT license).
 */

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "libavutil/attributes.h"
#include "libavutil/float2half.h"
#include "libavutil/half2float.h"
#include "libavutil/intfloat.h"

#include "bc6h_common.h"
#include "bc6enc.h"

#define MAX_ENTRIES 64
#define MAX_TRY 4000
#define HDR_FLT_MAX_EXP 128
#define MAX_PARTITIONS_TABLE (1 + 64 + 64)

typedef struct BC6HEncParams {
    uint16_t mode_mask;
    float exposure;
    int is_signed;
    float quality;
    int use_pattern;
} BC6HEncParams;

static BC6HEncParams bc6h_params;
static Float2HalfTables bc6h_f2h;
static Half2FloatTables bc6h_h2f;
static int bc6h_tables_init;

static void bc6h_init_tables(void)
{
    if (!bc6h_tables_init) {
        ff_init_float2half_tables(&bc6h_f2h);
        ff_init_half2float_tables(&bc6h_h2f);
        bc6h_tables_init = 1;
    }
}

static inline uint16_t bc6h_float_to_half(float v)
{
    return float2half(av_float2int(v), &bc6h_f2h);
}

static inline float bc6h_half_to_float(uint16_t h)
{
    return av_int2float(half2float(h, &bc6h_h2f));
}

//===============================================================================
// Copyright (c) 2007-2024  Advanced Micro Devices, Inc. All rights reserved.
// Copyright (c) 2004-2006 ATI Technologies Inc.
//===============================================================================
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//
//  File Name:   HDR_Encode.cpp
//  Description: Reserved utils function for HDR process
//
//////////////////////////////////////////////////////////////////////////////










#define USE_NEWRAMP

#ifdef USE_RAMPS

static int g_init_ramps = 0;
#endif



//==============================================================================================
// return # of bits needed to store n. handle signed or unsigned cases properly
static int NBits(int n, bool bIsSigned) {
    int nb;
    if (n == 0) {
        return 0; // no bits needed for 0, signed or not
    } else if (n > 0) {
        for (nb = 0; n; ++nb, n >>= 1);
        return nb + (bIsSigned ? 1 : 0);
    } else {
        assert(bIsSigned);
        for (nb = 0; n < -1; ++nb, n >>= 1);
        return nb + 1;
    }
}

float lerpf(float a, float b, int i, int denom) {
    assert(denom == 3 || denom == 7 || denom == 15);
    assert(i >= 0 && i <= denom);

    int *weights = NULL;

    switch (denom) {
    case 3:
        denom *= 5;
        i *= 5;    // fall through to case 15
    case 7:
        weights = g_aWeights3;
        break;
    case 15:
        weights = g_aWeights4;
        break;
    default:
        assert(0);
    }
    return (a*weights[denom - i] + b*weights[i]) / 64.0f;
}

int QuantizeToInt(short value, int prec, bool signedfloat16, float exposure) {
    (exposure);

    if (prec <= 1) return 0;
    bool negvalue = false;

    if (signedfloat16) {
        if (value < 0) {
            negvalue = true;
            value = -value;
        }
        prec--;
    } else {
        // clamp -ve
        if (value < 0)
            value = 0;
    }

    // move data to use extra bits for processing
    int ivalue = value;

    int iQuantized;
    int bias = (prec > 10 && prec != 16) ? ((1 << (prec - 11)) - 1) : 0;
    bias = (prec == 16) ? 15 : bias;

    iQuantized = ((ivalue << prec) + bias) / (F16HMAX + 1);

    return (negvalue ? -iQuantized : iQuantized);
}

int Unquantize(int comp, unsigned char uBitsPerComp, bool bSigned) {
    int unq = 0, s = 0;
    if (bSigned) {
        if (uBitsPerComp >= 16) {
            unq = comp;
        } else {
            if (comp < 0) {
                s = 1;
                comp = -comp;
            }

            if (comp == 0) unq = 0;
            else if (comp >= ((1 << (uBitsPerComp - 1)) - 1)) unq = 0x7FFF;
            else unq = ((comp << 15) + 0x4000) >> (uBitsPerComp - 1);

            if (s) unq = -unq;
        }
    } else {
        if (uBitsPerComp >= 15) unq = comp;
        else if (comp == 0) unq = 0;
        else if (comp == ((1 << uBitsPerComp) - 1)) unq = 0xFFFF;
        else unq = ((comp << 16) + 0x8000) >> uBitsPerComp;
    }

    return unq;
}

//==============================================================================================

void    Partition(
    int       shape,
    float    in[][MAX_DIMENSION_BIG],
    float    subsets[MAX_SUBSETS][MAX_SUBSET_SIZE][MAX_DIMENSION_BIG],
    int       count[MAX_SUBSETS],
    int       ShapeTableToUse,
    int       dimension) {
    int   i, j;
    int   *table = NULL;

    // Dont use memset: this is better for now
    for (i = 0; i<MAX_SUBSETS; i++) count[i] = 0;

    switch (ShapeTableToUse) {
    case    0:
    case    1:
        table = &(PARTITIONS[0][0][0]);
        break;
    case    2:
        table = &(PARTITIONS[1][shape][0]);
        break;
    default:
        break;
    }

    // Nothing to do!!: Must indicate an error to user
    if (table == NULL) return;

    for (i = 0; i<MAX_SUBSET_SIZE; i++) {
        int   subset = table[i];
        for (j = 0; j<dimension; j++) {
            subsets[subset][count[subset]][j] = in[i][j];
        }
        if (dimension < MAX_DIMENSION_BIG) {
            subsets[subset][count[subset]][j] = 0.0;
        }
        count[subset]++;
    }
}

//=================================================================================================

void GetEndPoints(float EndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], float outB[MAX_SUBSETS][MAX_SUBSET_SIZE][MAX_DIMENSION_BIG], int max_subsets, int entryCount[MAX_SUBSETS]) {
    // Should have some sort of error notification!
    if (max_subsets > MAX_SUBSETS) return;

    // Save Min and Max OutB points as EndPoints
    for (int subset = 0; subset<max_subsets; subset++) {
        // We now have points on direction vector(s)
        // find the min and max points
        float min = FLT_MAX;
        float max = -FLT_MAX;

        float val;
        int mini = 0;
        int maxi = 0;


        for (int i = 0; i < entryCount[subset]; i++) {
            val = outB[subset][i][0] + outB[subset][i][1] + outB[subset][i][2];
            if (val < min) {
                min = val;
                mini = i;
            }
            if (val > max) {
                max = val;
                maxi = i;
            }
        }

        // Is round best for this !
        for (int c = 0; c < MAX_DIMENSION_BIG; c++) {
            EndPoints[subset][0][c] = outB[subset][mini][c];
        }

        for (int c = 0; c < MAX_DIMENSION_BIG; c++) {
            EndPoints[subset][1][c] = outB[subset][maxi][c];
        }
    }
}

void covariance_d(float data[][MAX_DIMENSION_BIG], int numEntries, float cov[MAX_DIMENSION_BIG][MAX_DIMENSION_BIG], int dimension) {
    int i, j, k;

    for (i = 0; i<dimension; i++)
        for (j = 0; j <= i; j++) {
            cov[i][j] = 0;
            for (k = 0; k<numEntries; k++)
                cov[i][j] += data[k][i] * data[k][j];
        }

    for (i = 0; i<dimension; i++)
        for (j = i + 1; j<dimension; j++)
            cov[i][j] = cov[j][i];
}

void centerInPlace_d(float data[][MAX_DIMENSION_BIG], int numEntries, float mean[MAX_DIMENSION_BIG], int dimension) {
    int i, k;

    for (i = 0; i<dimension; i++) {
        mean[i] = 0;
        for (k = 0; k<numEntries; k++)
            mean[i] += data[k][i];
    }

    if (!numEntries)
        return;

    for (i = 0; i<dimension; i++) {
        mean[i] /= (float)numEntries;
        for (k = 0; k<numEntries; k++)
            data[k][i] -= mean[i];
    }
}

void eigenVector_d(float cov[MAX_DIMENSION_BIG][MAX_DIMENSION_BIG], float vector[MAX_DIMENSION_BIG], int dimension) {
    // calculate an eigenvecto corresponding to a biggest eigenvalue
    // will work for non-zero non-negative matricies only

#define EV_ITERATION_NUMBER 20
#define EV_SLACK            2        /* additive for exp base 2)*/


    int i, j, k, l, m, n, p, q;
    float c[2][MAX_DIMENSION_BIG][MAX_DIMENSION_BIG];
    float maxDiag;

    for (i = 0; i<dimension; i++)
        for (j = 0; j<dimension; j++)
            c[0][i][j] = cov[i][j];

    p = (int)floorf(logf((HDR_FLT_MAX_EXP - EV_SLACK) / ceilf(logf((float)dimension) / logf(2.0f))) / logf(2.0f));

    //assert(p>0);

    p = p >0 ? p : 1;

    q = (EV_ITERATION_NUMBER + p - 1) / p;

    l = 0;

    for (n = 0; n<q; n++) {
        maxDiag = 0;

        for (i = 0; i<dimension; i++)
            maxDiag = c[l][i][i] > maxDiag ? c[l][i][i] : maxDiag;

        if (maxDiag <= 0) {
            return;
        }

        //assert(maxDiag >0);

        for (i = 0; i<dimension; i++)
            for (j = 0; j<dimension; j++)
                c[l][i][j] /= maxDiag;

        for (m = 0; m<p; m++) {
            for (i = 0; i<dimension; i++)
                for (j = 0; j<dimension; j++) {
                    float temp = 0;
                    for (k = 0; k<dimension; k++) {
                        // Notes:
                        // This is the most consuming portion of the code and needs optimizing for perfromance
                        temp += c[l][i][k] * c[l][k][j];
                    }
                    c[1 - l][i][j] = temp;
                }
            l = 1 - l;
        }
    }

    maxDiag = 0;
    k = 0;

    for (i = 0; i<dimension; i++) {
        k = c[l][i][i] > maxDiag ? i : k;
        maxDiag = c[l][i][i] > maxDiag ? c[l][i][i] : maxDiag;
    }
    float t;
    t = 0;
    for (i = 0; i<dimension; i++) {
        t += c[l][k][i] * c[l][k][i];
        vector[i] = c[l][k][i];
    }
    // normalization is really optional
    t = sqrtf(t);
    //assert(t>0);

    if (t <= 0) {
        return;
    }
    for (i = 0; i<dimension; i++)
        vector[i] /= t;
}

void project_d(float data[][MAX_DIMENSION_BIG], int numEntries, float vector[MAX_DIMENSION_BIG], float projection[MAX_ENTRIES], int dimension) {
    // assume that vector is normalized already
    int i, k;

    for (k = 0; k<numEntries; k++) {
        projection[k] = 0;
        for (i = 0; i<dimension; i++) {
            projection[k] += data[k][i] * vector[i];
        }
    }
}

typedef struct {
    float d;
    int i;
} a;

inline int a_compare(const void *arg1, const void *arg2) {
    if (((a*)arg1)->d - ((a*)arg2)->d > 0) return 1;
    if (((a*)arg1)->d - ((a*)arg2)->d < 0) return -1;
    return 0;
};

void sortProjection(float projection[MAX_ENTRIES], int order[MAX_ENTRIES], int numEntries) {
    int i;
    a what[MAX_ENTRIES + MAX_PARTITIONS_TABLE];

    for (i = 0; i < numEntries; i++)
        what[what[i].i = i].d = projection[i];

    qsort((void*)&what, numEntries, sizeof(a), a_compare);

    for (i = 0; i < numEntries; i++)
        order[i] = what[i].i;
};

float totalError_d(float data[MAX_ENTRIES][MAX_DIMENSION_BIG], float data2[MAX_ENTRIES][MAX_DIMENSION_BIG], int numEntries, int dimension) {
    int i, j;
    float t = 0;
    for (i = 0; i<numEntries; i++)
        for (j = 0; j<dimension; j++)
            t += (data[i][j] - data2[i][j])*(data[i][j] - data2[i][j]);

    return t;
};

// input:
//
// v_  points, might be uncentered
// k - number of points in the ramp
// n - number of points in v_
//
// output:
// index, uncentered, in the range 0..k-1
//
void quant_AnD_Shell(float* v_, int k, int n, int *idx) {
#define MAX_BLOCK MAX_ENTRIES
    int i, j;
    float v[MAX_BLOCK];
    float z[MAX_BLOCK];
    a d[MAX_BLOCK];
    float l;
    float mm;
    float r = 0;
    int mi;

    //assert((v_ != NULL) && (n>1) && (k>1));

    float m, M, s, dm = 0.;
    m = M = v_[0];

    for (i = 1; i < n; i++) {
        m = m < v_[i] ? m : v_[i];
        M = M > v_[i] ? M : v_[i];
    }
    if (M == m) {
        for (i = 0; i < n; i++)
            idx[i] = 0;
        return;
    }

    //assert(M - m >0);
    s = (k - 1) / (M - m);
    for (i = 0; i < n; i++) {
        v[i] = v_[i] * s;

        idx[i] = (int)(z[i] = (v[i] + 0.5f /* stabilizer*/ - m *s));  //floorf(v[i] + 0.5f /* stabilizer*/ - m *s));

        d[i].d = v[i] - z[i] - m *s;
        d[i].i = i;
        dm += d[i].d;
        r += d[i].d*d[i].d;
    }
    if (n*r - dm*dm >= (float)(n - 1) / 4 /*slack*/ / 2) {

        dm /= (float)n;

        for (i = 0; i < n; i++)
            d[i].d -= dm;

        qsort((void*)&d, n, sizeof(a), a_compare);

        // got into fundamental simplex
        // move coordinate system origin to its center
        for (i = 0; i < n; i++)
            d[i].d -= (2.0f*(float)i + 1.0f - (float)n) / 2.0f / (float)n;

        mm = l = 0.;
        j = -1;
        for (i = 0; i < n; i++) {
            l += d[i].d;
            if (l < mm) {
                mm = l;
                j = i;
            }
        }

        // position which should be in 0
        j = ++j % n;

        for (i = j; i < n; i++)
            idx[d[i].i]++;
    }
    // get rid of an offset in idx
    mi = idx[0];
    for (i = 1; i < n; i++)
        mi = mi < idx[i] ? mi : idx[i];

    for (i = 0; i < n; i++)
        idx[i] -= mi;
}

float optQuantAnD_d(
    float data[MAX_ENTRIES][MAX_DIMENSION_BIG],
    int numEntries,
    int numClusters,
    int index[MAX_ENTRIES],
    float out[MAX_ENTRIES][MAX_DIMENSION_BIG],
    float direction[MAX_DIMENSION_BIG], float *step,
    int dimension,
    float quality
) {
    int index_[MAX_ENTRIES];

    int maxTry = (int)(MAX_TRY * quality);
    int try_two = 50;

    int i, j, k;
    float t, s;

    float centered[MAX_ENTRIES][MAX_DIMENSION_BIG];

    float mean[MAX_DIMENSION_BIG];

    float cov[MAX_DIMENSION_BIG][MAX_DIMENSION_BIG];

    float projected[MAX_ENTRIES];

    int order_[MAX_ENTRIES];


    for (i = 0; i<numEntries; i++)
        for (j = 0; j<dimension; j++)
            centered[i][j] = data[i][j];

    centerInPlace_d(centered, numEntries, mean, dimension);
    covariance_d(centered, numEntries, cov, dimension);

    // check if they all are the same

    t = 0;
    for (j = 0; j<dimension; j++)
        t += cov[j][j];

    if (numEntries == 0) {
        for (i = 0; i<numEntries; i++) {
            index[i] = 0;
            for (j = 0; j<dimension; j++)
                out[i][j] = mean[j];
        }
        return 0.;
    }

    eigenVector_d(cov, direction, dimension);
    project_d(centered, numEntries, direction, projected, dimension);

    for (i = 0; i<maxTry; i++) {
        int done = 0;

        if (i) {
            do {
                float q;
                q = s = t = 0;

                for (k = 0; k<numEntries; k++) {
                    s += index[k];
                    t += index[k] * index[k];
                }

                for (j = 0; j<dimension; j++) {
                    direction[j] = 0;
                    for (k = 0; k<numEntries; k++)
                        direction[j] += centered[k][j] * index[k];
                    q += direction[j] * direction[j];

                }

                s /= (float)numEntries;
                t = t - s * s * (float)numEntries;
                //assert(t != 0);
                t = (t == 0.0f ? 0.0f : 1.0f / t);
                // We need to requantize

                q = sqrtf(q);
                t *= q;

                if (q != 0)
                    for (j = 0; j<dimension; j++)
                        direction[j] /= q;

                // direction normalized

                project_d(centered, numEntries, direction, projected, dimension);
                sortProjection(projected, order_, numEntries);

                int index__[MAX_ENTRIES];

                // it's projected and centered; cluster centers are (index[i]-s)*t (*dir)
                k = 0;
                for (j = 0; j < numEntries; j++) {
                    while (projected[order_[j]] >(k + 0.5 - s)*t  && k < numClusters - 1)
                        k++;
                    index__[order_[j]] = k;
                }
                done = 1;
                for (j = 0; j < numEntries; j++) {
                    done = (done && (index__[j] == index[j]));
                    index[j] = index__[j];
                }
            } while (!done && try_two--);

            if (i == 1)
                for (j = 0; j < numEntries; j++)
                    index_[j] = index[j];
            else {
                done = 1;
                for (j = 0; j < numEntries; j++) {
                    done = (done && (index_[j] == index[j]));
                    index_[j] = index_[j];
                }
                if (done)
                    break;

            }
        }

        quant_AnD_Shell(projected, numClusters, numEntries, index);
    }
    s = t = 0;

    float q = 0;

    for (k = 0; k<numEntries; k++) {
        s += index[k];
        t += index[k] * index[k];
    }

    for (j = 0; j<dimension; j++) {
        direction[j] = 0;
        for (k = 0; k<numEntries; k++)
            direction[j] += centered[k][j] * index[k];
        q += direction[j] * direction[j];
    }

    s /= (float)numEntries;

    t = t - s * s * (float)numEntries;

    //assert(t != 0);

    t = (t == 0.0 ? 0.0f : 1.0f / t);

    for (i = 0; i<numEntries; i++)
        for (j = 0; j<dimension; j++)
            out[i][j] = mean[j] + direction[j] * t*(index[i] - s);

    // normalize direction for output

    q = sqrtf(q);
    *step = t*q;
    for (j = 0; j<dimension; j++)
        direction[j] /= q;

    return totalError_d(data, out, numEntries, dimension);
}


//=====================================================================================================================
#define LOG_CL_BASE         2
#define BIT_BASE            5
#define LOG_CL_RANGE        5
#define BIT_RANGE           9
#define MAX_CLUSTERS_BIG    16
#define BTT(bits)           (bits-BIT_BASE)
#define CLT(cl)             (cl-LOG_CL_BASE)

const float rampLerpWeights[5][16] = {
    { 0.0 }, // 0 bit index
    { 0.0, 1.0 }, // 1 bit index
    { 0.0, 21.0 / 64.0, 43.0 / 64.0, 1.0 }, // 2 bit index
    { 0.0, 9.0 / 64.0, 18.0 / 64.0, 27.0 / 64.0, 37.0 / 64.0, 46.0 / 64.0, 55.0 / 64.0, 1.0 }, // 3 bit index
    {
        0.0, 4.0 / 64.0, 9.0 / 64.0, 13.0 / 64.0, 17.0 / 64.0, 21.0 / 64.0, 26.0 / 64.0, 30.0 / 64.0,
        34.0 / 64.0, 38.0 / 64.0, 43.0 / 64.0, 47.0 / 64.0, 51.0 / 64.0, 55.0 / 64.0, 60.0 / 64.0, 1.0
    } // 4 bit index
};

static float rampf_linear(int clog, int bits, float p1, float p2, int indexPos)
{
    (bits);
    // (clog+ LOG_CL_BASE) starts from 2 to 4
    return  (float)p1 + rampLerpWeights[clog + LOG_CL_BASE][indexPos] * (p2 - p1);
}

int all_same_d(float d[][MAX_DIMENSION_BIG], int n, int dimension) {
    int i, j;
    int same = 1;
    for (i = 1; i< n; i++)
        for (j = 0; j< dimension; j++)
            same = same && (d[0][j] == d[i][j]);

    return(same);
}

// return the max index from a set of indexes
int max_index(int a[], int n) {
    int i, m = a[0];
    for (i = 0; i< n; i++)
        m = m > a[i] ? m : a[i];
    return (m);
}

int cluster_mean_d_d(float d[MAX_ENTRIES][MAX_DIMENSION_BIG], float mean[MAX_ENTRIES][MAX_DIMENSION_BIG], int index[], int i_comp[], int i_cnt[], int n, int dimension) {
    // unused index values are underfined
    int i, j, k;
    //assert(n!=0);

    for (i = 0; i< n; i++)
        for (j = 0; j< dimension; j++) {
            // assert(index[i]<MAX_CLUSTERS_BIG);
            mean[index[i]][j] = 0;
            i_cnt[index[i]] = 0;
        }
    k = 0;
    for (i = 0; i< n; i++) {
        for (j = 0; j< dimension; j++)
            mean[index[i]][j] += d[i][j];
        if (i_cnt[index[i]] == 0)
            i_comp[k++] = index[i];
        i_cnt[index[i]]++;
    }

    for (i = 0; i< k; i++)
        for (j = 0; j< dimension; j++)
            mean[i_comp[i]][j] /= (float)i_cnt[i_comp[i]];
    return k;
}

void mean_d_d(float d[][MAX_DIMENSION_BIG], float mean[MAX_DIMENSION_BIG], int n, int dimension) {
    int i, j;
    for (j = 0; j< dimension; j++)
        mean[j] = 0;
    for (i = 0; i< n; i++)
        for (j = 0; j< dimension; j++)
            mean[j] += d[i][j];
    for (j = 0; j< dimension; j++)
        mean[j] /= (float)n;
}

void index_collapse_kernel(int index[], int numEntries) {
    int k;
    int d, D;
    int mi;
    int Mi;
    if (numEntries == 0)
        return;

    mi = Mi = index[0];
    for (k = 1; k<numEntries; k++) {
        mi = mi < index[k] ? mi : index[k];
        Mi = Mi > index[k] ? Mi : index[k];
    }
    D = 1;
    for (d = 2; d <= Mi - mi; d++) {

        for (k = 0; k<numEntries; k++)
            if ((index[k] - mi) % d != 0)
                break;
        if (k >= numEntries)
            D = d;
    }
    for (k = 0; k<numEntries; k++)
        index[k] = (index[k] - mi) / D;
}


//========================================================================================================================
//-------------------------------------------------------------------------------------------------------------------------

int max_i(int a[], int n) {
    int i, m = a[0];
    for (i = 0; i< n; i++)
        m = m > a[i] ? m : a[i];
    return (m);
}

int npv_nd[2][2 * MAX_DIMENSION_BIG] = {
    { 1,2,4,8,16,32,0,0 }, //dimension = 3
    { 1,2,4,0,0,0,0,0 }    //dimension = 4
};

short par_vectors_nd[2][8][128][2][MAX_DIMENSION_BIG] = {
    {
        // Dimension = 3
        {
            { { 0,0,0,0 },{ 0,0,0,0 } },
            { { 0,0,0,0 },{ 0,0,0,0 } }
        },

        // 3*n+1    BCC          3*n+1        Cartesian 3*n            //same parity
        {
            // SAME_PAR
            { { 0,0,0 },{ 0,0,0 } },
            { { 1,1,1 },{ 1,1,1 } }
        },
        // 3*n+2    BCC          3*n+1        BCC          3*n+1
        {
            // BCC
            { { 0,0,0 },{ 0,0,0 } },
            { { 0,0,0 },{ 1,1,1 } },
            { { 1,1,1 },{ 0,0,0 } },
            { { 1,1,1 },{ 1,1,1 } }
        },
        // 3*n+3    FCC                    ???                        // ??????
        // BCC with FCC same or inverted, symmetric
        {
            // BCC_SAME_FCC
            { { 0,0,0 },{ 0,0,0 } },
            { { 1,1,0 },{ 1,1,0 } },
            { { 1,0,1 },{ 1,0,1 } },
            { { 0,1,1 },{ 0,1,1 } },

            { { 0,0,0 },{ 1,1,1 } },
            { { 1,1,1 },{ 0,0,0 } },
            { { 0,1,0 },{ 0,1,0 } },  // ??
            { { 1,1,1 },{ 1,1,1 } },

        },
        // 3*n+4    FCC          3*n+2        FCC          3*n+2
        {

            { { 0,0,0 },{ 0,0,0 } },
            { { 1,1,0 },{ 0,0,0 } },
            { { 1,0,1 },{ 0,0,0 } },
            { { 0,1,1 },{ 0,0,0 } },

            { { 0,0,0 },{ 1,1,0 } },
            { { 1,1,0 },{ 1,1,0 } },
            { { 1,0,1 },{ 1,1,0 } },
            { { 0,1,1 },{ 1,1,0 } },

            { { 0,0,0 },{ 1,0,1 } },
            { { 1,1,0 },{ 1,0,1 } },
            { { 1,0,1 },{ 1,0,1 } },
            { { 0,1,1 },{ 1,0,1 } },

            { { 0,0,0 },{ 0,1,1 } },
            { { 1,1,0 },{ 0,1,1 } },
            { { 1,0,1 },{ 0,1,1 } },
            { { 0,1,1 },{ 0,1,1 } }
        },


        // 3*n+5    Cartesian 3*n+3        FCC          3*n+2            //D^*[6]
        {

            { { 0,0,0 },{ 0,0,0 } },
            { { 1,1,0 },{ 0,0,0 } },
            { { 1,0,1 },{ 0,0,0 } },
            { { 0,1,1 },{ 0,0,0 } },

            { { 0,0,0 },{ 1,1,0 } },
            { { 1,1,0 },{ 1,1,0 } },
            { { 1,0,1 },{ 1,1,0 } },
            { { 0,1,1 },{ 1,1,0 } },

            { { 0,0,0 },{ 1,0,1 } },
            { { 1,1,0 },{ 1,0,1 } },
            { { 1,0,1 },{ 1,0,1 } },
            { { 0,1,1 },{ 1,0,1 } },

            { { 0,0,0 },{ 0,1,1 } },
            { { 1,1,0 },{ 0,1,1 } },
            { { 1,0,1 },{ 0,1,1 } },
            { { 0,1,1 },{ 0,1,1 } },


            { { 1,0,0 },{ 1,1,1 } },
            { { 0,1,0 },{ 1,1,1 } },
            { { 0,0,1 },{ 1,1,1 } },
            { { 1,1,1 },{ 1,1,1 } },

            { { 1,0,0 },{ 0,0,1 } },
            { { 0,1,0 },{ 0,0,1 } },
            { { 0,0,1 },{ 0,0,1 } },
            { { 1,1,1 },{ 0,0,1 } },

            { { 1,0,0 },{ 1,0,0 } },
            { { 0,1,0 },{ 1,0,0 } },
            { { 0,0,1 },{ 1,0,0 } },
            { { 1,1,1 },{ 1,0,0 } },

            { { 1,0,0 },{ 0,1,0 } },
            { { 0,1,0 },{ 0,1,0 } },
            { { 0,0,1 },{ 0,1,0 } },
            { { 1,1,1 },{ 0,1,0 } }
        }
    },// Dimension = 3
    {
        // Dimension = 4
        {
            { { 0,0,0,0 },{ 0,0,0,0 } },
            { { 0,0,0,0 },{ 0,0,0,0 } }
        },

        // 3*n+1    BCC          3*n+1        Cartesian 3*n            //same parity
        {
            // SAME_PAR
            { { 0,0,0,0 },{ 0,0,0,0 } },
            { { 1,1,1,1 },{ 1,1,1,1 } }
        },
        // 3*n+2    BCC          3*n+1        BCC          3*n+1
        {
            // BCC
            { { 0,0,0,0 },{ 0,0,0,0 } },
            { { 0,0,0,0 },{ 1,1,1,1 } },
            { { 1,1,1,1 },{ 0,0,0,0 } },
            { { 1,1,1,1 },{ 1,1,1,1 } }
        },
        // 3 PBIT
        {
            { { 0,0,0,0 },{ 0,0,0,0 } },
            { { 0,0,0,0 },{ 0,1,1,1 } },
            { { 0,1,1,1 },{ 0,0,0,0 } },
            { { 0,1,1,1 },{ 0,1,1,1 } },

            { { 1,0,0,0 },{ 1,0,0,0 } },
            { { 1,0,0,0 },{ 1,1,1,1 } },
            { { 1,1,1,1 },{ 1,0,0,0 } },
            { { 1,1,1,1 },{ 1,1,1,1 } }
        },

        // 4 PBIT
        {
            { { 0,0,0,0 },{ 0,0,0,0 } },
            { { 0,0,0,0 },{ 0,1,1,1 } },
            { { 0,1,1,1 },{ 0,0,0,0 } },
            { { 0,1,1,1 },{ 0,1,1,1 } },

            { { 1,0,0,0 },{ 1,0,0,0 } },
            { { 1,0,0,0 },{ 1,1,1,1 } },
            { { 1,1,1,1 },{ 1,0,0,0 } },
            { { 1,1,1,1 },{ 1,1,1,1 } },

            { { 0,0,0,0 },{ 0,0,0,0 } },
            { { 0,0,0,0 },{ 0,0,1,1 } },
            { { 0,0,1,1 },{ 0,0,0,0 } },
            { { 0,1,0,1 },{ 0,1,0,1 } },

            { { 1,0,0,0 },{ 1,0,0,0 } },
            { { 1,0,0,0 },{ 1,0,1,1 } },
            { { 1,0,1,1 },{ 1,0,0,0 } },
            { { 1,1,0,1 },{ 1,1,0,1 } },

        },

    } // Dimension = 4

};

int get_par_vector(int dim1, int dim2, int dim3, int dim4, int dim5) {
    return par_vectors_nd[dim1][dim2][dim3][dim4][dim5];
}



float quant_single_point_d
(
    float data[MAX_ENTRIES][MAX_DIMENSION_BIG],
    int numEntries, int index[MAX_ENTRIES],
    float out[MAX_ENTRIES][MAX_DIMENSION_BIG],
    int epo_1[2][MAX_DIMENSION_BIG],
    int Mi_,                // last cluster
    int bits[3],            // including parity
    int type,
    int dimension           // This should be either 3 or 4
) {
    if (dimension < 3) return FLT_MAX;

    int i, j;

    float err_0 = FLT_MAX;
    float err_1 = FLT_MAX;

    int idx = 0;
    int idx_1 = 0;

    int epo_0[2][MAX_DIMENSION_BIG];

    int use_par = (type != 0);

    int clog = 0;
    i = Mi_ + 1;
    while (i >>= 1)
        clog++;

    //    assert((1<<clog)== Mi_+1);

    int pn;
    for (pn = 0; pn<npv_nd[dimension - 3][type]; pn++) {
        //1

        int dim1 = dimension - 3;
        int dim2 = type;
        int dim3 = pn;


        int o1[2][MAX_DIMENSION_BIG]; // = { 0,2 };
        int o2[2][MAX_DIMENSION_BIG]; // = { 0,2 };

        for (j = 0; j<dimension; j++) {
            //A
            o2[0][j] = o1[0][j] = 0;
            o2[1][j] = o1[1][j] = 2;

            if (use_par) {
                if (get_par_vector(dim1, dim2, dim3, 0, j))
                    o1[0][j] = 1;
                else
                    o1[1][j] = 1;
                if (get_par_vector(dim1, dim2, dim3, 1, j))
                    o2[0][j] = 1;
                else
                    o2[1][j] = 1;
            }
        } //A

        int t1, t2;

        int dr[MAX_DIMENSION_BIG];
        int dr_0[MAX_DIMENSION_BIG];
        //float tr;

        for (i = 0; i< (1 << clog); i++) {
            //E
            float t = 0;
            int t1o[MAX_DIMENSION_BIG], t2o[MAX_DIMENSION_BIG];

            for (j = 0; j<dimension; j++) {
                // D
                float t_ = FLT_MAX;

                for (t1 = o1[0][j]; t1<o1[1][j]; t1++) {
                    // C
                    for (t2 = o2[0][j]; t2<o2[1][j]; t2++)
                        // This is needed for non-integer mean points of "collapsed" sets
                    {
                        // B

#ifdef USE_RAMPS
                        int tf = (int)floorf(data[0][j]);
                        int tc = (int)ceilf(data[0][j]);
                        // if they are not equal, the same representalbe point is used for
                        // both of them, as all representable points are integers in the rage
                        if (sperr(tf, CLT(clog), BTT(bits[j]), t1, t2, i) > sperr(tc, CLT(clog), BTT(bits[j]), t1, t2, i))
                            dr[j] = tc;
                        else if (sperr(tf, CLT(clog), BTT(bits[j]), t1, t2, i) < sperr(tc, CLT(clog), BTT(bits[j]), t1, t2, i))
                            dr[j] = tf;
                        else
#endif
                            dr[j] = (int)floorf(data[0][j] + 0.5f);

#ifdef USE_RAMPS
                        tr = sperr(dr[j], CLT(clog), BTT(bits[j]), t1, t2, i) + 2.0f * sqrtf(sperr(dr[j], CLT(clog), BTT(bits[j]), t1, t2, i)) * fabsf((float)dr[j] - data[0][j]) +
                             (dr[j] - data[0][j])* (dr[j] - data[0][j]);
                        if (tr < t_) {
                            t_ = tr;
#else
                        t_ = 0;
#endif

                            t1o[j] = t1;
                            t2o[j] = t2;
                            dr_0[j] = dr[j];
#ifdef USE_RAMPS
                            if ((dr_0[j] < 0) || (dr_0[j] > 255)) {
                                dr_0[j] = 0; // Error!
                            }
                        }
#endif
                    } // B
                } //C

                t += t_;
            } // D


            if (t < err_0) {

                idx = i;

                for (j = 0; j<dimension; j++) {
#ifdef USE_RAMPS
                    int p1 = CLT(clog);        // < 3
                    int p2 = BTT(bits[j]);     // < 4
                    int in_data = dr_0[j];          // < SP_ERRIDX_MAX
                    int p4 = t1o[j];           // < 2
                    int p5 = t2o[j];           // < 2
                    int p6 = i;                // < 16

                    // New spidx
                    epo_0[0][j] = spidx(in_data, p1, p2, p4, p5, p6, 0);
                    epo_0[1][j] = spidx(in_data, p1, p2, p4, p5, p6, 1);

                    if (epo_0[1][j] >= SP_ERRIDX_MAX) {
                        epo_0[1][j] = 0; // Error!!
                    }
#else
                    epo_0[0][j] = 0;
                    epo_0[1][j] = 0;
#endif
                }
                err_0 = t;
            }
            if (err_0 == 0)
                break;
        } // E

        if (err_0 < err_1) {
            idx_1 = idx;
            for (j = 0; j<dimension; j++) {
                epo_1[0][j] = epo_0[0][j];
                epo_1[1][j] = epo_0[1][j];
            }
            err_1 = err_0;
        }

        if (err_1 == 0)
            break;
    } //1

    for (i = 0; i< numEntries; i++) {
        index[i] = idx_1;
        for (j = 0; j<dimension; j++) {
            int p1 = CLT(clog);        // < 3
            int p2 = BTT(bits[j]);     // < 4
            int p3 = epo_1[0][j];      // < SP_ERRIDX_MAX
            int p4 = epo_1[1][j];      // < SP_ERRIDX_MAX
            int p5 = idx_1;            // < 16
#ifndef USE_NEWRAMP
            out[i][j] = ramp[p1][p2][p3][p4][p5];
#else
            out[i][j] = (int)rampf(p1, p2, p3, p4, p5);
#endif
        }
    }
    return err_1 * numEntries;
}

#define SP_ERRIDX_MAX 256

int expandbits_(int bits, int v) {
    return (v << (8 - bits) | v >> (2 * bits - 8));
}


#ifndef USE_NEWRAMP
float    ep_d[4][SP_ERRIDX_MAX];
float    ramp[3][4][SP_ERRIDX_MAX][SP_ERRIDX_MAX][16];
#else
float ep_df(int bits, int p1) {
    return (float)expandbits_(bits + BIT_BASE, p1);
}

float rampf(int clog, int bits, int p1, int p2, int i) {
    // (clog+ LOG_CL_BASE) starts from 2 to 4
    float ret = floorf((float)ep_df(bits, p1) + rampLerpWeights[clog + LOG_CL_BASE][i] * (float)((ep_df(bits, p2) - ep_df(bits, p1))) + 0.5F);
    if (ret > SP_ERRIDX_MAX) return SP_ERRIDX_MAX - 1;
    return ret;
}
#endif

#ifdef USE_RAMPS

int spidx(int in_data, int in_clog, int in_bits, int in_p2, int in_o1, int in_o2, int in_i) {
    return sp_data[in_data].sp_idx[in_clog][in_bits][in_p2][in_o1][in_o2][in_i];
}

float sperr(int in_data, int clog, int bits, int p2, int o1, int o2) {
    return sp_data[in_data].sp_err[clog][bits][p2][o1][o2];
}
#endif

void init_ramps() {
#ifdef USE_RAMPS
    int clog, bits;
    int in_data; // p1;
    int p2;
    int i;
    int o1, o2;

    if (g_init_ramps > 0) {
        g_init_ramps++;
        return;
    }

    // sp_datap = (SP_DATA **)malloc(SP_ERRIDX_MAX*sizeof(struct SP_DATA));
    // assert(sp_datap);
    // for (int i = 0; i < SP_ERRIDX_MAX; i++)
    // {
    //     sp_datap[i] = (SP_DATA *)malloc(sizeof(struct SP_DATA));
    // }

#ifndef USE_NEWRAMP

    for (bits = BIT_BASE; bits < BIT_RANGE; bits++)
        for (p1 = 0; p1 < (1 << bits); p1++) {
            ep_d[BTT(bits)][p1] = (float)expandbits_(bits, p1);
        }


    for (clog = LOG_CL_BASE; clog < LOG_CL_RANGE; clog++)
        for (bits = BIT_BASE; bits < BIT_RANGE; bits++)
            for (p1 = 0; p1 < (1 << bits); p1++)
                for (p2 = 0; p2 < (1 << bits); p2++) {
                    for (o1 = 0; o1 < (1 << clog); o1++) {
                        ramp[CLT(clog)][BTT(bits)][p1][p2][o1] =
                            floorf((float)ep_d[BTT(bits)][p1] + rampLerpWeights[clog][o1] * (float)((ep_d[BTT(bits)][p2] - ep_d[BTT(bits)][p1])) + 0.5F);
                    }
                }
#endif

    //-----------------------------------------------------------------------------
    // Step 1

    for (clog = LOG_CL_BASE; clog<LOG_CL_RANGE; clog++)
        for (bits = BIT_BASE; bits<BIT_RANGE; bits++)
            for (in_data = 0; in_data<SP_ERRIDX_MAX; in_data++)
                for (o1 = 0; o1<2; o1++)
                    for (o2 = 0; o2<2; o2++)
                        for (i = 0; i<16; i++) {
                            sp_data[in_data].sp_err[CLT(clog)][BTT(bits)][o1][o2][i] = FLT_MAX;
                            sp_data[in_data].sp_idx[CLT(clog)][BTT(bits)][o1][o2][i][0] = -1;
                        }

    // Step 2
    for (clog = LOG_CL_BASE; clog<LOG_CL_RANGE; clog++)
        for (bits = BIT_BASE; bits<BIT_RANGE; bits++)
            for (in_data = 0; in_data<(1 << bits); in_data++)
                for (p2 = 0; p2<(1 << bits); p2++)
                    for (i = 0; i<(1 << clog); i++) {
#ifndef USE_NEWRAMP
                        sp_data[(int)ramp[clog].sp_idx[clog]bits][bits][p1][p2][o1]][p1 & 0x1][p2 & 0x1][o1][0] = p1;
                        sp_data[(int)ramp[clog].sp_idx[clog]bits][bits][p1][p2][o1]][p1 & 0x1][p2 & 0x1][o1][1] = p2;
                        sp_data[(int)ramp[clog].sp_err[clog]bits][bits][p1][p2][o1]][p1 & 0x1][p2 & 0x1][o1] = 0.;
#else
                        int spd_i = (int)rampf(CLT(clog), BTT(bits), in_data, p2, o1);
                        if (spd_i > SP_ERRIDX_MAX) spd_i = SP_ERRIDX_MAX - 1;

                        sp_data[spd_i].sp_idx[CLT(clog)][BTT(bits)][in_data & 0x1][p2 & 0x1][o1][0] = in_data;
                        sp_data[spd_i].sp_idx[CLT(clog)][BTT(bits)][in_data & 0x1][p2 & 0x1][o1][1] = p2;
                        sp_data[spd_i].sp_err[CLT(clog)][BTT(bits)][in_data & 0x1][p2 & 0x1][o1] = 0.;
#endif
                    }

    // Step 3
    for (clog = LOG_CL_BASE; clog<LOG_CL_RANGE; clog++)
        for (bits = BIT_BASE; bits<BIT_RANGE; bits++)
            for (in_data = 0; in_data<SP_ERRIDX_MAX; in_data++)
                for (o1 = 0; o1<2; o1++)
                    for (o2 = 0; o2<2; o2++)
                        for (i = 0; i<(1 << clog); i++)
                            if (sp_data[in_data].sp_idx[CLT(clog)][BTT(bits)][o1][o2][i][0]<0) {
                                int k;
                                for (k = 1; k<SP_ERRIDX_MAX; k++)
                                    if ((in_data - k >= 0 && sp_data[in_data - k].sp_err[CLT(clog)][BTT(bits)][o1][o2][i] == 0) ||
                                            (in_data + k < SP_ERRIDX_MAX && sp_data[in_data + k].sp_err[CLT(clog)][BTT(bits)][o1][o2][i] == 0))
                                        break;
                                {
                                    if ((in_data - k >= 0 && sp_data[in_data - k].sp_err[CLT(clog)][BTT(bits)][o1][o2][i] == 0)) {
                                        sp_data[in_data].sp_idx[CLT(clog)][BTT(bits)][o1][o2][i][0] = sp_data[in_data - k].sp_idx[CLT(clog)][BTT(bits)][o1][o2][i][0];
                                        sp_data[in_data].sp_idx[CLT(clog)][BTT(bits)][o1][o2][i][1] = sp_data[in_data - k].sp_idx[CLT(clog)][BTT(bits)][o1][o2][i][1];
                                        //printf("sp_data[%2d].sp_idx[%2d][%2d][%2d][%2d][%2d][0] = (%d)\n", in_data, CLT(clog), BTT(bits), o1, o2, i, sp_data[in_data].sp_idx[CLT(clog)][BTT(bits)][o1][o2][i][0]);
                                        //printf("sp_data[%2d].sp_idx[%2d][%2d][%2d][%2d][%2d][1] = (%d)\n", in_data, CLT(clog), BTT(bits), o1, o2, i, sp_data[in_data].sp_idx[CLT(clog)][BTT(bits)][o1][o2][i][1]);
                                    } else if ((in_data + k < SP_ERRIDX_MAX && sp_data[in_data + k].sp_err[CLT(clog)][BTT(bits)][o1][o2][i] == 0)) {
                                        sp_data[in_data].sp_idx[CLT(clog)][BTT(bits)][o1][o2][i][0] = sp_data[in_data + k].sp_idx[CLT(clog)][BTT(bits)][o1][o2][i][0];
                                        sp_data[in_data].sp_idx[CLT(clog)][BTT(bits)][o1][o2][i][1] = sp_data[in_data + k].sp_idx[CLT(clog)][BTT(bits)][o1][o2][i][1];
                                        //printf("sp_data[%2d].sp_idx[%2d][%2d][%2d][%2d][%2d][0] = %d\n", in_data, CLT(clog), BTT(bits), o1, o2, i, sp_data[in_data].sp_idx[CLT(clog)][BTT(bits)][o1][o2][i][0]);
                                        //printf("sp_data[%2d].sp_idx[%2d][%2d][%2d][%2d][%2d][1] = %d\n", in_data, CLT(clog), BTT(bits), o1, o2, i, sp_data[in_data].sp_idx[CLT(clog)][BTT(bits)][o1][o2][i][1]);
                                    }

                                    sp_data[in_data].sp_err[CLT(clog)][BTT(bits)][o1][o2][i] = (float)k*k;
                                }
                            }

    //for (clog = LOG_CL_BASE; clog<LOG_CL_RANGE; clog++)
    //    for (bits = BIT_BASE; bits<BIT_RANGE; bits++)
    //        for (in_data = 0; in_data<SP_ERRIDX_MAX; in_data++)
    //            for (o1 = 0; o1<2; o1++)
    //                for (o2 = 0; o2<2; o2++)
    //                    for (i = 0; i<16; i++)
    //                         {
    //                             printf("sp_data[%2d].sp_idx[%2d][%2d][%2d][%2d][%2d][0] = %d\n", in_data,CLT(clog), BTT(bits),o1,o2,i, sp_data[in_data].sp_idx[CLT(clog)][BTT(bits)][o1][o2][i][0]);
    //                             printf("sp_data[%2d].sp_idx[%2d][%2d][%2d][%2d][%2d][1] = %d\n", in_data,CLT(clog), BTT(bits),o1,o2,i, sp_data[in_data].sp_idx[CLT(clog)][BTT(bits)][o1][o2][i][1]);
    //                         }

    g_init_ramps++;

#endif
}

void deinit_ramps() {
#ifdef USE_RAMPS
    if (g_init_ramps > 1)
        g_init_ramps--;
#endif
}

int ep_find_floor(float v, int bits, int use_par, int odd) {
#ifndef USE_NEWRAMP
    float *p = ep_d[BTT(bits)];
#endif
    int i1 = 0;
    int i2 = 1 << (bits - use_par);
    odd = use_par ? odd : 0;
    while (i2 - i1>1) {
        int j = (i1 + i2) / 2;
#ifndef USE_NEWRAMP
        if (v >= p[(j << use_par) + odd])
#else
        if (v >= ep_df(BTT(bits), (j << use_par) + odd))
#endif
            i1 = j;
        else
            i2 = j;
    }
    return (i1 << use_par) + odd;
}



//based on code : ep_shaker_d in BC7 shaker
float ep_shaker_HD(
    float data[MAX_ENTRIES][MAX_DIMENSION_BIG],
    int numEntries,
    int index_[MAX_ENTRIES],
    float out[MAX_ENTRIES][MAX_DIMENSION_BIG],
    int epo_code[2][MAX_DIMENSION_BIG],
    int Mi_,                // last cluster
    int bits[3],            // including parity
    int dimension
) {
    int i, j, k;
    int use_par = 0;
    int clog = 0;

    i = Mi_ + 1;
    while (i >>= 1)
        clog++;

    float mean[MAX_DIMENSION_BIG];
    int index[MAX_ENTRIES];
    int Mi;

    int maxTry = 1;

    for (k = 0; k < numEntries; k++) {
        index[k] = index_[k];
    }

    int done;
    int change;

    int better;

    float   err_o = FLT_MAX;
    float   out_2[MAX_ENTRIES][MAX_DIMENSION_BIG];
    int     idx_2[MAX_ENTRIES];
    int     epo_2[2][MAX_DIMENSION_BIG];

    int max_bits[MAX_DIMENSION_BIG];
    int type = bits[0] % (2 * dimension);

    for (j = 0; j < dimension; j++)
        max_bits[j] = (bits[0] + 2 * dimension - 1) / (2 * dimension);


    // handled below automatically
    int alls = all_same_d(data, numEntries, dimension);

    mean_d_d(data, mean, numEntries, dimension);

    do {
        index_collapse_kernel(index, numEntries);

        Mi = max_index(index, numEntries);  // index can be from requantizer

        int p, q;
        int p0 = -1, q0 = -1;

        float err_2 = FLT_MAX;

        if (Mi == 0) {
            float t;
            int    epo_0[2][MAX_DIMENSION_BIG];
            // either sinle point from the beginning or collapsed index
            if (alls) {
                t = quant_single_point_d(data, numEntries, index, out_2, epo_0, Mi_, bits, type, dimension);
            } else {
                quant_single_point_d(&mean, numEntries, index, out_2, epo_0, Mi_, bits, type, dimension);
                t = totalError_d(data, out_2, numEntries, dimension);
            }

            if (t < err_o) {
                for (k = 0; k<numEntries; k++) {
                    index_[k] = index[k];
                    for (j = 0; j<dimension; j++) {
                        out[k][j] = out_2[k][j];
                        epo_code[0][j] = epo_0[0][j];
                        epo_code[1][j] = epo_0[1][j];
                    }
                };
                err_o = t;
            }
            return err_o;
        }



        for (q = 1; Mi != 0 && q*Mi <= Mi_; q++) { // does not work for single point collapsed index!!!
            for (p = 0; p <= Mi_ - q*Mi; p++) {
                int cidx[MAX_ENTRIES];

                for (k = 0; k<numEntries; k++) {
                    cidx[k] = index[k] * q + p;
                }

                float epa[2][MAX_DIMENSION_BIG];

                //
                // solve RMS problem for center
                //

                float im[2][2] = { { 0,0 },{ 0,0 } };   // matrix /inverse matrix
                float rp[2][MAX_DIMENSION_BIG];            // right part for RMS fit problem

                // get ideal clustr centers
                float cc[MAX_CLUSTERS_BIG][MAX_DIMENSION_BIG];
                int index_cnt[MAX_CLUSTERS_BIG];                        // count of index entries
                int index_comp[MAX_CLUSTERS_BIG];                       // compacted index
                int index_ncl;                                            // number of unique indexes

                index_ncl = cluster_mean_d_d(data, cc, cidx, index_comp, index_cnt, numEntries, dimension); // unrounded

                for (i = 0; i<index_ncl; i++)
                    for (j = 0; j<dimension; j++)
                        cc[index_comp[i]][j] = (float)floorf(cc[index_comp[i]][j] + 0.5f); // more or less ideal location

                for (j = 0; j<dimension; j++) {
                    rp[0][j] = rp[1][j] = 0;
                }

                // weight with cnt if runnning on compacted index
                for (k = 0; k<numEntries; k++) {
                    im[0][0] += (Mi_ - cidx[k])* (Mi_ - cidx[k]);
                    im[0][1] += cidx[k] * (Mi_ - cidx[k]);           // im is symmetric
                    im[1][1] += cidx[k] * cidx[k];

                    for (j = 0; j<dimension; j++) {
                        rp[0][j] += (Mi_ - cidx[k]) * cc[cidx[k]][j];
                        rp[1][j] += cidx[k] * cc[cidx[k]][j];
                    }
                }

                float dd = im[0][0] * im[1][1] - im[0][1] * im[0][1];

                //assert(dd !=0);

                // dd=0 means that cidx[k] and (Mi_-cidx[k]) collinear which implies only one active index;
                // taken care of separately

                im[1][0] = im[0][0];
                im[0][0] = im[1][1] / dd;
                im[1][1] = im[1][0] / dd;
                im[1][0] = im[0][1] = -im[0][1] / dd;

                for (j = 0; j<dimension; j++) {
                    epa[0][j] = (im[0][0] * rp[0][j] + im[0][1] * rp[1][j])*Mi_;
                    epa[1][j] = (im[1][0] * rp[0][j] + im[1][1] * rp[1][j])*Mi_;
                }

                float err_1 = FLT_MAX;
                float out_1[MAX_ENTRIES][MAX_DIMENSION_BIG];
                int idx_1[MAX_ENTRIES];
                int epo_1[2][MAX_DIMENSION_BIG];
                int s1 = 0;
                float epd[2][MAX_DIMENSION_BIG][2];   // first second, coord, begin range end range

                for (j = 0; j<dimension; j++) {
                    for (i = 0; i<2; i++) {
                        // set range
                        epd[i][j][0] = epd[i][j][1] = epa[i][j];
                        epd[i][j][1] += ((1 << bits[j]) - 1 - (int)epd[i][j][1]  < (1 << use_par) ?
                                         (1 << bits[j]) - 1 - (int)epd[i][j][1] : (1 << use_par)) & (~use_par);
                    }
                }

                float ce[MAX_ENTRIES][MAX_CLUSTERS_BIG][MAX_DIMENSION_BIG];
                float err_0 = 0;
                float out_0[MAX_ENTRIES][MAX_DIMENSION_BIG];
                int idx_0[MAX_ENTRIES];

                for (i = 0; i<numEntries; i++) {
                    float d[4];
                    d[0] = data[i][0];
                    d[1] = data[i][1];
                    d[2] = data[i][2];
                    d[3] = data[i][3];
                    for (j = 0; j<(1 << clog); j++)
                        for (k = 0; k < dimension; k++) {
                            ce[i][j][k] = (rampf_linear(CLT(clog), BTT(bits[k]), epd[0][k][0], epd[1][k][0], j) - d[k]) *
                                          (rampf_linear(CLT(clog), BTT(bits[k]), epd[0][k][0], epd[1][k][0], j) - d[k]);
                        }
                }

                int s = 0, p1, g;
                int ei0 = 0, ei1 = 0;

                for (p1 = 0; p1<64; p1++) {
                    int j0 = 0;

                    // Gray code increment
                    g = p1 & (-p1);

                    err_0 = 0;

                    for (j = 0; j<dimension; j++) {
                        if (((g >> (2 * j)) & 0x3) != 0) {
                            j0 = j;
                            // new cords
                            ei0 = (((s^g) >> (2 * j)) & 0x1);
                            ei1 = (((s^g) >> (2 * j + 1)) & 0x1);
                        }
                    }
                    s = s ^ g;
                    err_0 = 0;

                    for (i = 0; i<numEntries; i++) {
                        float d[4];
                        d[0] = data[i][0];
                        d[1] = data[i][1];
                        d[2] = data[i][2];
                        d[3] = data[i][3];
                        int    ci = 0;
                        float cmin = FLT_MAX;

                        for (j = 0; j<(1 << clog); j++) {
                            float t_ = 0.;
                            ce[i][j][j0] = (rampf_linear(CLT(clog), BTT(bits[j0]), epd[0][j0][ei0], epd[1][j0][ei1], j) - d[j0]) *
                                           (rampf_linear(CLT(clog), BTT(bits[j0]), epd[0][j0][ei0], epd[1][j0][ei1], j) - d[j0]);
                            for (k = 0; k<dimension; k++) {
                                t_ += ce[i][j][k];
                            }

                            if (t_< cmin) {
                                cmin = t_;
                                ci = j;
                            }
                        }

                        idx_0[i] = ci;
                        for (k = 0; k<dimension; k++) {
                            out_0[i][k] = rampf_linear(CLT(clog), BTT(bits[k]), epd[0][k][ei0], epd[1][k][ei1], ci);
                        }
                        err_0 += cmin;
                    }

                    if (err_0 < err_1) {
                        // best in the curent ep cube run
                        for (i = 0; i < numEntries; i++) {
                            idx_1[i] = idx_0[i];
                            for (j = 0; j<dimension; j++)
                                out_1[i][j] = out_0[i][j];
                        }
                        err_1 = err_0;

                        s1 = s; // epo coding
                    }
                }

                // reconstruct epo
                for (j = 0; j<dimension; j++) {
                    {
                        // new cords
                        ei0 = ((s1 >> (2 * j)) & 0x1);
                        ei1 = ((s1 >> (2 * j + 1)) & 0x1);
                        epo_1[0][j] = (int)epd[0][j][ei0];
                        epo_1[1][j] = (int)epd[1][j][ei1];
                    }
                }

                if (err_1 < err_2) {
                    // best in the curent ep cube run
                    for (i = 0; i < numEntries; i++) {
                        idx_2[i] = idx_1[i];
                        for (j = 0; j<dimension; j++)
                            out_2[i][j] = out_1[i][j];
                    }
                    err_2 = err_1;
                    for (j = 0; j<dimension; j++) {
                        epo_2[0][j] = epo_1[0][j];
                        epo_2[1][j] = epo_1[1][j];
                    }
                    p0 = p;
                    q0 = q;
                }
            }
        }

        // change/better
        change = 0;
        for (k = 0; k<numEntries; k++)
            change = change || (index[k] * q0 + p0 != idx_2[k]);

        better = err_2 < err_o;

        if (better) {
            for (k = 0; k<numEntries; k++) {
                index_[k] = index[k] = idx_2[k];
                for (j = 0; j<dimension; j++) {
                    out[k][j] = out_2[k][j];
                    epo_code[0][j] = epo_2[0][j];
                    epo_code[1][j] = epo_2[1][j];
                }
            }
            err_o = err_2;
        }

        done = !(change  &&  better);

        if (maxTry > 0) maxTry--;
        else maxTry = 0;

    } while (!done && maxTry);

    return err_o;
}

float ep_shaker_2_d(
    float data[MAX_ENTRIES][MAX_DIMENSION_BIG],
    int numEntries,
    int index_[MAX_ENTRIES],
    float out[MAX_ENTRIES][MAX_DIMENSION_BIG],
    int epo_code[2][MAX_DIMENSION_BIG],
    int size,
    int Mi_,             // last cluster
    int bits,            // total for all channels
    // defined by total numbe of bits and dimensioin
    int dimension,
    float epo[2][MAX_DIMENSION_BIG]
) {
    if (dimension < 3) return FLT_MAX;

    int i, j, k;
    int max_bits[MAX_DIMENSION_BIG];
    int type = bits % (2 * dimension);
    int use_par = (type != 0);

    for (j = 0; j < dimension; j++)
        max_bits[j] = (bits + 2 * dimension - 1) / (2 * dimension);

    int clog = 0;
    i = Mi_ + 1;
    while (i >>= 1)
        clog++;

    if (CLT(clog) > 3)
        return FLT_MAX;

    float mean[MAX_DIMENSION_BIG];
    int index[MAX_ENTRIES];
    int Mi;

    int maxTry = 8;

    for (k = 0; k < numEntries; k++) {
        index[k] = index_[k];
    }

    int done;
    int change;

    int better;

    float err_o = FLT_MAX;

    int epo_0[2][MAX_DIMENSION_BIG];

    float outg[MAX_ENTRIES][MAX_DIMENSION_BIG];

    // handled below automatically
    int alls = all_same_d(data, numEntries, dimension);
    mean_d_d(data, mean, numEntries, dimension);

    do {
        index_collapse_kernel(index, numEntries);

        Mi = max_i(index, numEntries);  // index can be from requantizer

        int p, q;
        int p0 = -1, q0 = -1;

        float err_0 = FLT_MAX;

        if (Mi == 0) {
            float t;
            // either single point from the beginning or collapsed index
            if (alls) {
                t = quant_single_point_d(data, numEntries, index, outg, epo_0, Mi_, max_bits, type, dimension);
            } else {
                quant_single_point_d(&mean, numEntries, index, outg, epo_0, Mi_, max_bits, type, dimension);
                t = totalError_d(data, outg, numEntries, dimension);
            }

            if (t < err_o) {
                for (k = 0; k<numEntries; k++) {
                    index_[k] = index[k];
                    for (j = 0; j<dimension; j++) {
                        out[k][j] = outg[k][j];
                        epo_code[0][j] = epo_0[0][j];
                        epo_code[1][j] = epo_0[1][j];
                    }
                };
                err_o = t;
            }

            for (j = 0; j<dimension; j++) {
#ifndef USE_NEWRAMP
                epo[0][j] = ramp[CLT(clog)][BTT(max_bits[j])][epo_code[0][j]][epo_code[1][j]][0];
                epo[1][j] = ramp[CLT(clog)][BTT(max_bits[j])][epo_code[0][j]][epo_code[1][j]][(1 << clog) - 1];
#else
                epo[0][j] = rampf(CLT(clog), BTT(max_bits[j]), epo_code[0][j], epo_code[1][j], 0);
                epo[1][j] = rampf(CLT(clog), BTT(max_bits[j]), epo_code[0][j], epo_code[1][j], (1 << clog) - 1);
#endif
            }

            return err_o;
        }

        for (q = 1; Mi != 0 && q*Mi <= Mi_; q++) // does not work for single point collapsed index!!!
            for (p = 0; p <= Mi_ - q*Mi; p++) {
                int cidx[MAX_ENTRIES];

                for (k = 0; k<numEntries; k++)
                    cidx[k] = index[k] * q + p;

                float epa[2][MAX_DIMENSION_BIG];

                //
                // solve RMS problem for center
                //

                float im[2][2] = { { 0,0 },{ 0,0 } };   // matrix /inverse matrix
                float rp[2][MAX_DIMENSION_BIG];            // right part for RMS fit problem

                // get ideal clustr centers
                float cc[MAX_CLUSTERS_BIG][MAX_DIMENSION_BIG];
                int i_cnt[MAX_CLUSTERS_BIG]; // count of index entries
                int i_comp[MAX_CLUSTERS_BIG];   // compacted index
                int ncl;                        // number of unique indexes

                ncl = cluster_mean_d_d(data, cc, cidx, i_comp, i_cnt, numEntries, dimension); // unrounded

                // round
                for (i = 0; i<ncl; i++)
                    for (j = 0; j<dimension; j++)
                        cc[i_comp[i]][j] = (float)floorf(cc[i_comp[i]][j] + 0.5F); // more or less ideal location

                for (j = 0; j<dimension; j++)
                    rp[0][j] = rp[1][j] = 0;

                // weight with cnt if runnning on compacted index
                for (k = 0; k<numEntries; k++) {
                    im[0][0] += (Mi_ - cidx[k])* (Mi_ - cidx[k]);
                    im[0][1] += cidx[k] * (Mi_ - cidx[k]);           // im is symmetric
                    im[1][1] += cidx[k] * cidx[k];

                    for (j = 0; j<dimension; j++) {
                        rp[0][j] += (Mi_ - cidx[k]) * cc[cidx[k]][j];
                        rp[1][j] += cidx[k] * cc[cidx[k]][j];
                    }
                }

                float dd = im[0][0] * im[1][1] - im[0][1] * im[0][1];

                // dd=0 means that cidx[k] and (Mi_-cidx[k]) collinear which implies only one active index;
                // taken care of separately

                im[1][0] = im[0][0];
                im[0][0] = im[1][1] / dd;
                im[1][1] = im[1][0] / dd;
                im[1][0] = im[0][1] = -im[0][1] / dd;

                for (j = 0; j<dimension; j++) {
                    epa[0][j] = (im[0][0] * rp[0][j] + im[0][1] * rp[1][j])*Mi_;
                    epa[1][j] = (im[1][0] * rp[0][j] + im[1][1] * rp[1][j])*Mi_;
                }

                // shake single or                                   - cartesian
                // shake odd/odd and even/even or                    - same parity
                // shake odd/odd odd/even , even/odd and even/even   - bcc

                float err_1 = FLT_MAX;
                int epo_1[2][MAX_DIMENSION_BIG];

                float ed[2][2][MAX_DIMENSION_BIG];
                int epo_2_[2][2][2][MAX_DIMENSION_BIG];

                for (j = 0; j<dimension; j++) {

#ifndef USE_NEWRAMP
                    float(*rb)[SP_ERRIDX_MAX][16] = ramp[CLT(clog)][BTT(max_bits[j])];
#endif

                    int pp[2] = { 0,0 };
                    int rr = (use_par ? 2 : 1);

                    int epi[2][2];  // first/second, coord, begin rage end range


                    for (pp[0] = 0; pp[0]<rr; pp[0]++) {
                        for (pp[1] = 0; pp[1]<rr; pp[1]++) {

                            for (i = 0; i<2; i++) {     // set range
                                epi[i][0] = epi[i][1] = ep_find_floor(epa[i][j], max_bits[j], use_par, pp[i]);

                                epi[i][0] -= ((epi[i][0]  < (size >> 1) - 1 ? epi[i][0] : (size >> 1) - 1))  & (~use_par);

                                epi[i][1] += ((1 << max_bits[j]) - 1 - epi[i][1]  < (size >> 1) ?
                                              (1 << max_bits[j]) - 1 - epi[i][1] : (size >> 1)) & (~use_par);
                            }

                            int p1, p2, step = (1 << use_par);

                            ed[pp[0]][pp[1]][j] = FLT_MAX;

                            for (p1 = epi[0][0]; p1 <= epi[0][1]; p1 += step)
                                for (p2 = epi[1][0]; p2 <= epi[1][1]; p2 += step) {
#ifndef USE_NEWRAMP
                                    float *rbp = rb[p1][p2];
#endif
                                    float t = 0;
                                    int    *ci = cidx;
                                    int    m = numEntries;
                                    int _mc = m;

                                    while (_mc > 0) {
#ifndef USE_NEWRAMP
                                        t += (rbp[ci[_mc - 1]] - data[_mc - 1][j])
                                             *(rbp[ci[_mc - 1]] - data[_mc - 1][j]);
#else
                                        t += (rampf(CLT(clog), BTT(max_bits[j]), p1, p2, ci[_mc - 1]) - data[_mc - 1][j])
                                             *(rampf(CLT(clog), BTT(max_bits[j]), p1, p2, ci[_mc - 1]) - data[_mc - 1][j]);
#endif
                                        _mc--;
                                    }

                                    if (t<ed[pp[0]][pp[1]][j]) {
                                        ed[pp[0]][pp[1]][j] = t;
                                        epo_2_[pp[0]][pp[1]][0][j] = p1;
                                        epo_2_[pp[0]][pp[1]][1][j] = p2;
                                    }
                                }
                        } // pp[1]
                    } // pp[0]
                } // j

                int pn;

                for (pn = 0; pn<npv_nd[dimension - 3][type]; pn++) {
                    int dim1 = dimension - 3;
                    int dim2 = type;
                    int dim3 = pn;
                    int j1;
                    float err_2 = 0;

                    for (j1 = 0; j1<dimension; j1++)
                        err_2 += ed[
                                     get_par_vector(dim1, dim2, dim3, 0, j1)][get_par_vector(dim1, dim2, dim3, 1, j1)][j1];
                    if (err_2 < err_1) {
                        err_1 = err_2;
                        for (j1 = 0; j1<dimension; j1++) {
                            epo_1[0][j1] = epo_2_[get_par_vector(dim1, dim2, dim3, 0, j1)][get_par_vector(dim1, dim2, dim3, 1, j1)][0][j1];
                            epo_1[1][j1] = epo_2_[get_par_vector(dim1, dim2, dim3, 0, j1)][get_par_vector(dim1, dim2, dim3, 1, j1)][1][j1];
                        }
                    }
                }

                if (err_1 <= err_0) { // we'd want to get expanded index;
                    err_0 = err_1;
                    p0 = p;
                    q0 = q;
                    for (j = 0; j<dimension; j++) {
                        epo_0[0][j] = epo_1[0][j];
                        epo_0[1][j] = epo_1[1][j];
                    }
                }
            }
        // requantize
#ifndef USE_NEWRAMP
        float *r[MAX_DIMENSION_BIG];
#endif
        int idg[MAX_ENTRIES];

        float err_r = 0;

        if (CLT(clog) > (LOG_CL_RANGE - LOG_CL_BASE))
            return FLT_MAX;

        for (int jj = 0; jj<dimension; jj++) {
            if (BTT(max_bits[jj]) >(BIT_RANGE - BIT_BASE))
                return FLT_MAX;
            if ((epo_0[0][jj] > 255) || (epo_0[0][jj] < 0))
                return FLT_MAX;
            if ((epo_0[1][jj] > 255) || (epo_0[0][jj] < 0))
                return FLT_MAX;
#ifndef USE_NEWRAMP
            r[jj] = ramp[CLT(clog)][BTT(max_bits[jj])][epo_0[0][jj]][epo_0[1][jj]];
#endif
        }

        for (i = 0; i<numEntries; i++) {
            float  cmin = FLT_MAX;
            int        ci = 0;
            float   d[4];
            d[0] = data[i][0];
            d[1] = data[i][1];
            d[2] = data[i][2];
            d[3] = data[i][3];

            for (j = 0; j < (1 << clog); j++) {
                float t_ = 0.;

                for (k = 0; k<dimension; k++) {
#ifndef USE_NEWRAMP
                    t_ += (r[k][j] - d[k])*
                          (r[k][j] - d[k]);
#else
                    t_ += (rampf(CLT(clog), BTT(max_bits[k]), epo_0[0][k], epo_0[1][k], j) - d[k])*
                          (rampf(CLT(clog), BTT(max_bits[k]), epo_0[0][k], epo_0[1][k], j) - d[k]);
#endif
                }

                if (t_<cmin) {
                    cmin = t_;
                    ci = j;
                }
            }

            idg[i] = ci;
            for (k = 0; k<dimension; k++) {
#ifndef USE_NEWRAMP
                outg[i][k] = r[k][ci];
#else
                outg[i][k] = rampf(CLT(clog), BTT(max_bits[k]), epo_0[0][k], epo_0[1][k], ci);
#endif
            }

            err_r += cmin;
        }

        change = 0;
        for (k = 0; k<numEntries; k++)
            change = change || (index[k] * q0 + p0 != idg[k]);

        better = err_r < err_o;

        if (better) {
            for (k = 0; k<numEntries; k++) {
                index_[k] = index[k] = idg[k];
                for (j = 0; j<dimension; j++) {
                    out[k][j] = outg[k][j];
                    epo_code[0][j] = epo_0[0][j];
                    epo_code[1][j] = epo_0[1][j];
                }
            }
            err_o = err_r;
        }

        done = !(change  &&  better);
        if (maxTry > 0) maxTry--;
        else maxTry = 0;
    } while (!done && maxTry);

    for (j = 0; j<dimension; j++) {
#ifndef USE_NEWRAMP
        epo[0][j] = ramp[CLT(clog)][BTT(max_bits[j])][epo_code[0][j]][epo_code[1][j]][0];
        epo[1][j] = ramp[CLT(clog)][BTT(max_bits[j])][epo_code[0][j]][epo_code[1][j]][(1 << clog) - 1];
#else
        epo[0][j] = rampf(CLT(clog), BTT(max_bits[j]), epo_code[0][j], epo_code[1][j], 0);
        epo[1][j] = rampf(CLT(clog), BTT(max_bits[j]), epo_code[0][j], epo_code[1][j], (1 << clog) - 1);
#endif
    }

    return err_o;
}

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

static void bitheader_transferbits(const BitHeader *hdr, uint8_t *out, int sizeinbytes)
{
    if (!out || sizeinbytes > hdr->sizeinbytes)
        return;

    memset(out, 0, sizeinbytes);
    int bitpos = 0;
    for (int i = 0; i < sizeinbytes; i++) {
        int bit = 1;
        for (int j = 0; j < 8; j++) {
            if (hdr->bits[bitpos])
                out[i] |= bit;
            bit <<= 1;
            bitpos++;
        }
    }
}

static void bitheader_setvalue_shift(BitHeader *hdr, int start, int bitsize,
                                      int value, int maskshift)
{
    int end = start + bitsize - 1;
    int mask = 0x1 << maskshift;
    for (; start <= end; start++) {
        hdr->bits[start] = (value & mask) ? 1 : 0;
        mask <<= 1;
    }
}

static inline void bitheader_setvalue(BitHeader *hdr, int start, int bitsize, int value)
{
    bitheader_setvalue_shift(hdr, start, bitsize, value, 0);
}

static inline void bc6h_swap_int(int *a, int *b)
{
    int t = *a;
    *a = *b;
    *b = t;
}

typedef struct BC6HBlockEncoder {
    int m_sortedModes[MAX_PARTITIONS];
    float m_blockMin[MAX_DIMENSION_BIG];
    float m_blockMax[MAX_DIMENSION_BIG];
    float m_blockRange[MAX_DIMENSION_BIG];
    float m_blockMaxRange;
    float m_quality;
    DWORD m_ModeMask;
    int m_isSigned;
    float m_Exposure;
    int m_bAverageEndPoint;
    float m_DiffLevel;
    int m_useMonoShapePatterns;
} BC6HBlockEncoder;

static void bc6h_encoder_init(BC6HBlockEncoder *enc)
{
    enc->m_quality = bc6h_params.quality;
    enc->m_useMonoShapePatterns = bc6h_params.use_pattern;
    enc->m_isSigned = bc6h_params.is_signed;
    enc->m_ModeMask = bc6h_params.mode_mask;
    enc->m_Exposure = bc6h_params.exposure;
    enc->m_bAverageEndPoint = 1;
    enc->m_DiffLevel = 0.01f;
}

//===============================================================================
// Copyright (c) 2014-2024  Advanced Micro Devices, Inc. All rights reserved.
//===============================================================================
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// BC6H_Encode.cpp : Encoder for BC6H
//
// Revision
// 0.1    First implementation
// 0.2    Removed unused code and disabeled optimization
//














#define USE_SHAKERHD  // reserved for future use!





#ifdef DEBUG_PATTERNS
// random pixel noise range
float bc6h_encoder_DoPixelNoise()
{
    float ret = (rand() % RANDOM_NOISE_LEVEL) / 100.0;
    return (ret);
}
#endif

/*
Reserved Feature MONOSHAPE_PATTERNS
int bc6h_encoder_FindPattern();
*/

static void SaveDataBlock(const AMD_BC6H_Format *bc6h_format,
                          BYTE out[BC6H_COMPRESSED_BLOCK_SIZE])
{
    BitHeader header;
    bitheader_init(&header, NULL, BC6H_COMPRESSED_BLOCK_SIZE);

    // Save the RGB end point values
    switch (bc6h_format->m_mode)
    {
    case 1:  //0x00
        bitheader_setvalue(&header, 0, 2, 0x00);
        bitheader_setvalue_shift(&header, 2, 1, bc6h_format->gy, 4);   //        gy[4]
        bitheader_setvalue_shift(&header, 3, 1, bc6h_format->by, 4);   //        by[4]
        bitheader_setvalue_shift(&header, 4, 1, bc6h_format->bz, 4);   //        bz[4]
        bitheader_setvalue(&header, 5, 10, bc6h_format->rw);     // 10:    rw[9:0]
        bitheader_setvalue(&header, 15, 10, bc6h_format->gw);    // 10:    gw[9:0]
        bitheader_setvalue(&header, 25, 10, bc6h_format->bw);    // 10:    bw[9:0]
        bitheader_setvalue(&header, 35, 5, bc6h_format->rx);     // 5:     rx[4:0]
        bitheader_setvalue_shift(&header, 40, 1, bc6h_format->gz, 4);  //        gz[4]
        bitheader_setvalue(&header, 41, 4, bc6h_format->gy);     // 5:     gy[3:0]
        bitheader_setvalue(&header, 45, 5, bc6h_format->gx);     // 5:     gx[4:0]
        bitheader_setvalue(&header, 50, 1, bc6h_format->bz);     // 5:     bz[0]
        bitheader_setvalue(&header, 51, 4, bc6h_format->gz);     // 5:     gz[3:0]
        bitheader_setvalue(&header, 55, 5, bc6h_format->bx);     // 5:     bx[4:0]
        bitheader_setvalue_shift(&header, 60, 1, bc6h_format->bz, 1);  //        bz[1]
        bitheader_setvalue(&header, 61, 4, bc6h_format->by);     // 5:     by[3:0]
        bitheader_setvalue(&header, 65, 5, bc6h_format->ry);     // 5:     ry[4:0]
        bitheader_setvalue_shift(&header, 70, 1, bc6h_format->bz, 2);  //        bz[2]
        bitheader_setvalue(&header, 71, 5, bc6h_format->rz);     // 5:     rz[4:0]
        bitheader_setvalue_shift(&header, 76, 1, bc6h_format->bz, 3);  //        bz[3]
        break;
    case 2:  // 0x01
        bitheader_setvalue(&header, 0, 2, 0x01);
        bitheader_setvalue_shift(&header, 2, 1, bc6h_format->gy, 5);   //        gy[5]
        bitheader_setvalue_shift(&header, 3, 1, bc6h_format->gz, 4);   //        gz[4]
        bitheader_setvalue_shift(&header, 4, 1, bc6h_format->gz, 5);   //        gz[5]
        bitheader_setvalue(&header, 5, 7, bc6h_format->rw);      //        rw[6:0]
        bitheader_setvalue(&header, 12, 1, bc6h_format->bz);     //        bz[0]
        bitheader_setvalue_shift(&header, 13, 1, bc6h_format->bz, 1);  //        bz[1]
        bitheader_setvalue_shift(&header, 14, 1, bc6h_format->by, 4);  //        by[4]
        bitheader_setvalue(&header, 15, 7, bc6h_format->gw);     //        gw[6:0]
        bitheader_setvalue_shift(&header, 22, 1, bc6h_format->by, 5);  //        by[5]
        bitheader_setvalue_shift(&header, 23, 1, bc6h_format->bz, 2);  //        bz[2]
        bitheader_setvalue_shift(&header, 24, 1, bc6h_format->gy, 4);  //        gy[4]
        bitheader_setvalue(&header, 25, 7, bc6h_format->bw);     // 7:     bw[6:0]
        bitheader_setvalue_shift(&header, 32, 1, bc6h_format->bz, 3);  //        bz[3]
        bitheader_setvalue_shift(&header, 33, 1, bc6h_format->bz, 5);  //        bz[5]
        bitheader_setvalue_shift(&header, 34, 1, bc6h_format->bz, 4);  //        bz[4]
        bitheader_setvalue(&header, 35, 6, bc6h_format->rx);     // 6:     rx[5:0]
        bitheader_setvalue(&header, 41, 4, bc6h_format->gy);     // 6:     gy[3:0]
        bitheader_setvalue(&header, 45, 6, bc6h_format->gx);     // 6:     gx[5:0]
        bitheader_setvalue(&header, 51, 4, bc6h_format->gz);     // 6:     gz[3:0]
        bitheader_setvalue(&header, 55, 6, bc6h_format->bx);     // 6:     bx[5:0]
        bitheader_setvalue(&header, 61, 4, bc6h_format->by);     // 6:     by[3:0]
        bitheader_setvalue(&header, 65, 6, bc6h_format->ry);     // 6:     ry[5:0]
        bitheader_setvalue(&header, 71, 6, bc6h_format->rz);     // 6:     rz[5:0]
        break;
    case 3:  // 0x02
        bitheader_setvalue(&header, 0, 5, 0x02);
        bitheader_setvalue(&header, 5, 10, bc6h_format->rw);      // 11:    rw[9:0]
        bitheader_setvalue(&header, 15, 10, bc6h_format->gw);     // 11:    gw[9:0]
        bitheader_setvalue(&header, 25, 10, bc6h_format->bw);     // 11:    bw[9:0]
        bitheader_setvalue(&header, 35, 5, bc6h_format->rx);      // 5:     rx[4:0]
        bitheader_setvalue_shift(&header, 40, 1, bc6h_format->rw, 10);  //        rw[10]
        bitheader_setvalue(&header, 41, 4, bc6h_format->gy);      // 4:     gy[3:0]
        bitheader_setvalue(&header, 45, 4, bc6h_format->gx);      // 4:     gx[3:0]
        bitheader_setvalue_shift(&header, 49, 1, bc6h_format->gw, 10);  //        gw[10]
        bitheader_setvalue(&header, 50, 1, bc6h_format->bz);      // 4:     bz[0]
        bitheader_setvalue(&header, 51, 4, bc6h_format->gz);      // 4:     gz[3:0]
        bitheader_setvalue(&header, 55, 4, bc6h_format->bx);      // 4:     bx[3:0]
        bitheader_setvalue_shift(&header, 59, 1, bc6h_format->bw, 10);  //        bw[10]
        bitheader_setvalue_shift(&header, 60, 1, bc6h_format->bz, 1);   //        bz[1]
        bitheader_setvalue(&header, 61, 4, bc6h_format->by);      // 4:     by[3:0]
        bitheader_setvalue(&header, 65, 5, bc6h_format->ry);      // 5:     ry[4:0]
        bitheader_setvalue_shift(&header, 70, 1, bc6h_format->bz, 2);   //        bz[2]
        bitheader_setvalue(&header, 71, 5, bc6h_format->rz);      // 5:     rz[4:0]
        bitheader_setvalue_shift(&header, 76, 1, bc6h_format->bz, 3);   //        bz[3]
        break;
    case 4:  // 0x06
        bitheader_setvalue(&header, 0, 5, 0x06);
        bitheader_setvalue(&header, 5, 10, bc6h_format->rw);      // 11:    rw[9:0]
        bitheader_setvalue(&header, 15, 10, bc6h_format->gw);     // 11:    gw[9:0]
        bitheader_setvalue(&header, 25, 10, bc6h_format->bw);     // 11:    bw[9:0]
        bitheader_setvalue(&header, 35, 4, bc6h_format->rx);      //        rx[3:0]
        bitheader_setvalue_shift(&header, 39, 1, bc6h_format->rw, 10);  //        rw[10]
        bitheader_setvalue_shift(&header, 40, 1, bc6h_format->gz, 4);   //        gz[4]
        bitheader_setvalue(&header, 41, 4, bc6h_format->gy);      // 5:     gy[3:0]
        bitheader_setvalue(&header, 45, 5, bc6h_format->gx);      //        gx[4:0]
        bitheader_setvalue_shift(&header, 50, 1, bc6h_format->gw, 10);  // 5:     gw[10]
        bitheader_setvalue(&header, 51, 4, bc6h_format->gz);      // 5:     gz[3:0]
        bitheader_setvalue(&header, 55, 4, bc6h_format->bx);      // 4:     bx[3:0]
        bitheader_setvalue_shift(&header, 59, 1, bc6h_format->bw, 10);  //        bw[10]
        bitheader_setvalue_shift(&header, 60, 1, bc6h_format->bz, 1);   //        bz[1]
        bitheader_setvalue(&header, 61, 4, bc6h_format->by);      // 4:     by[3:0]
        bitheader_setvalue(&header, 65, 4, bc6h_format->ry);      // 4:     ry[3:0]
        bitheader_setvalue(&header, 69, 1, bc6h_format->bz);      // 4:     bz[0]
        bitheader_setvalue_shift(&header, 70, 1, bc6h_format->bz, 2);   //        bz[2]
        bitheader_setvalue(&header, 71, 4, bc6h_format->rz);      // 4:     rz[3:0]
        bitheader_setvalue_shift(&header, 75, 1, bc6h_format->gy, 4);   //        gy[4]
        bitheader_setvalue_shift(&header, 76, 1, bc6h_format->bz, 3);   //        bz[3]
        break;
    case 5:  // 0x0A
        bitheader_setvalue(&header, 0, 5, 0x0A);
        bitheader_setvalue(&header, 5, 10, bc6h_format->rw);      // 11:   rw[9:0]
        bitheader_setvalue(&header, 15, 10, bc6h_format->gw);     // 11:   gw[9:0]
        bitheader_setvalue(&header, 25, 10, bc6h_format->bw);     // 11:   bw[9:0]
        bitheader_setvalue(&header, 35, 4, bc6h_format->rx);      // 4:    rx[3:0]
        bitheader_setvalue_shift(&header, 39, 1, bc6h_format->rw, 10);  //       rw[10]
        bitheader_setvalue_shift(&header, 40, 1, bc6h_format->by, 4);   //       by[4]
        bitheader_setvalue(&header, 41, 4, bc6h_format->gy);      // 4:    gy[3:0]
        bitheader_setvalue(&header, 45, 4, bc6h_format->gx);      // 4:    gx[3:0]
        bitheader_setvalue_shift(&header, 49, 1, bc6h_format->gw, 10);  //       gw[10]
        bitheader_setvalue(&header, 50, 1, bc6h_format->bz);      // 5:    bz[0]
        bitheader_setvalue(&header, 51, 4, bc6h_format->gz);      // 4:    gz[3:0]
        bitheader_setvalue(&header, 55, 5, bc6h_format->bx);      // 5:    bx[4:0]
        bitheader_setvalue_shift(&header, 60, 1, bc6h_format->bw, 10);  //       bw[10]
        bitheader_setvalue(&header, 61, 4, bc6h_format->by);      // 5:    by[3:0]
        bitheader_setvalue(&header, 65, 4, bc6h_format->ry);      // 4:    ry[3:0]
        bitheader_setvalue_shift(&header, 69, 1, bc6h_format->bz, 1);   //       bz[1]
        bitheader_setvalue_shift(&header, 70, 1, bc6h_format->bz, 2);   //       bz[2]
        bitheader_setvalue(&header, 71, 4, bc6h_format->rz);      // 4:    rz[3:0]
        bitheader_setvalue_shift(&header, 75, 1, bc6h_format->bz, 4);   //       bz[4]
        bitheader_setvalue_shift(&header, 76, 1, bc6h_format->bz, 3);   //       bz[3]
        break;
    case 6:  // 0x0E
        bitheader_setvalue(&header, 0, 5, 0x0E);
        bitheader_setvalue(&header, 5, 9, bc6h_format->rw);      // 9:    rw[8:0]
        bitheader_setvalue_shift(&header, 14, 1, bc6h_format->by, 4);  //       by[4]
        bitheader_setvalue(&header, 15, 9, bc6h_format->gw);     // 9:    gw[8:0]
        bitheader_setvalue_shift(&header, 24, 1, bc6h_format->gy, 4);  //       gy[4]
        bitheader_setvalue(&header, 25, 9, bc6h_format->bw);     // 9:    bw[8:0]
        bitheader_setvalue_shift(&header, 34, 1, bc6h_format->bz, 4);  //       bz[4]
        bitheader_setvalue(&header, 35, 5, bc6h_format->rx);     // 5:    rx[4:0]
        bitheader_setvalue_shift(&header, 40, 1, bc6h_format->gz, 4);  //       gz[4]
        bitheader_setvalue(&header, 41, 4, bc6h_format->gy);     // 5:    gy[3:0]
        bitheader_setvalue(&header, 45, 5, bc6h_format->gx);     // 5:    gx[4:0]
        bitheader_setvalue(&header, 50, 1, bc6h_format->bz);     // 5:    bz[0]
        bitheader_setvalue(&header, 51, 4, bc6h_format->gz);     // 5:    gz[3:0]
        bitheader_setvalue(&header, 55, 5, bc6h_format->bx);     // 5:    bx[4:0]
        bitheader_setvalue_shift(&header, 60, 1, bc6h_format->bz, 1);  //       bz[1]
        bitheader_setvalue(&header, 61, 4, bc6h_format->by);     // 5:    by[3:0]
        bitheader_setvalue(&header, 65, 5, bc6h_format->ry);     // 5:    ry[4:0]
        bitheader_setvalue_shift(&header, 70, 1, bc6h_format->bz, 2);  //       bz[2]
        bitheader_setvalue(&header, 71, 5, bc6h_format->rz);     // 5:    rz[4:0]
        bitheader_setvalue_shift(&header, 76, 1, bc6h_format->bz, 3);  //       bz[3]
        break;
    case 7:  // 0x12
        bitheader_setvalue(&header, 0, 5, 0x12);
        bitheader_setvalue(&header, 5, 8, bc6h_format->rw);      // 8:    rw[7:0]
        bitheader_setvalue_shift(&header, 13, 1, bc6h_format->gz, 4);  //       gz[4]
        bitheader_setvalue_shift(&header, 14, 1, bc6h_format->by, 4);  //       by[4]
        bitheader_setvalue(&header, 15, 8, bc6h_format->gw);     // 8:    gw[7:0]
        bitheader_setvalue_shift(&header, 23, 1, bc6h_format->bz, 2);  //       bz[2]
        bitheader_setvalue_shift(&header, 24, 1, bc6h_format->gy, 4);  //       gy[4]
        bitheader_setvalue(&header, 25, 8, bc6h_format->bw);     // 8:    bw[7:0]
        bitheader_setvalue_shift(&header, 33, 1, bc6h_format->bz, 3);  //       bz[3]
        bitheader_setvalue_shift(&header, 34, 1, bc6h_format->bz, 4);  //       bz[4]
        bitheader_setvalue(&header, 35, 6, bc6h_format->rx);     // 6:    rx[5:0]
        bitheader_setvalue(&header, 41, 4, bc6h_format->gy);     // 5:    gy[3:0]
        bitheader_setvalue(&header, 45, 5, bc6h_format->gx);     // 5:    gx[4:0]
        bitheader_setvalue(&header, 50, 1, bc6h_format->bz);     // 5:    bz[0]
        bitheader_setvalue(&header, 51, 4, bc6h_format->gz);     // 5:    gz[3:0]
        bitheader_setvalue(&header, 55, 5, bc6h_format->bx);     // 5:    bx[4:0]
        bitheader_setvalue_shift(&header, 60, 1, bc6h_format->bz, 1);  //       bz[1]
        bitheader_setvalue(&header, 61, 4, bc6h_format->by);     // 5:    by[3:0]
        bitheader_setvalue(&header, 65, 6, bc6h_format->ry);     // 6:    ry[5:0]
        bitheader_setvalue(&header, 71, 6, bc6h_format->rz);     // 6:    rz[5:0]
        break;
    case 8:  // 0x16
        bitheader_setvalue(&header, 0, 5, 0x16);
        bitheader_setvalue(&header, 5, 8, bc6h_format->rw);      // 8:   rw[7:0]
        bitheader_setvalue(&header, 13, 1, bc6h_format->bz);     // 5:   bz[0]
        bitheader_setvalue_shift(&header, 14, 1, bc6h_format->by, 4);  //      by[4]
        bitheader_setvalue(&header, 15, 8, bc6h_format->gw);     // 8:   gw[7:0]
        bitheader_setvalue_shift(&header, 23, 1, bc6h_format->gy, 5);  //      gy[5]
        bitheader_setvalue_shift(&header, 24, 1, bc6h_format->gy, 4);  //      gy[4]
        bitheader_setvalue(&header, 25, 8, bc6h_format->bw);     // 8:   bw[7:0]
        bitheader_setvalue_shift(&header, 33, 1, bc6h_format->gz, 5);  //      gz[5]
        bitheader_setvalue_shift(&header, 34, 1, bc6h_format->bz, 4);  //      bz[4]
        bitheader_setvalue(&header, 35, 5, bc6h_format->rx);     // 5:   rx[4:0]
        bitheader_setvalue_shift(&header, 40, 1, bc6h_format->gz, 4);  //      gz[4]
        bitheader_setvalue(&header, 41, 4, bc6h_format->gy);     // 6:   gy[3:0]
        bitheader_setvalue(&header, 45, 6, bc6h_format->gx);     // 6:   gx[5:0]
        bitheader_setvalue(&header, 51, 4, bc6h_format->gz);     // 6:   gz[3:0]
        bitheader_setvalue(&header, 55, 5, bc6h_format->bx);     // 5:   bx[4:0]
        bitheader_setvalue_shift(&header, 60, 1, bc6h_format->bz, 1);  //      bz[1]
        bitheader_setvalue(&header, 61, 4, bc6h_format->by);     // 5:   by[3:0]
        bitheader_setvalue(&header, 65, 5, bc6h_format->ry);     // 5:   ry[4:0]
        bitheader_setvalue_shift(&header, 70, 1, bc6h_format->bz, 2);  //      bz[2]
        bitheader_setvalue(&header, 71, 5, bc6h_format->rz);     // 5:   rz[4:0]
        bitheader_setvalue_shift(&header, 76, 1, bc6h_format->bz, 3);  //      bz[3]
        break;
    case 9:  // 0x1A
        bitheader_setvalue(&header, 0, 5, 0x1A);
        bitheader_setvalue(&header, 5, 8, bc6h_format->rw);      // 8:   rw[7:0]
        bitheader_setvalue_shift(&header, 13, 1, bc6h_format->bz, 1);  //      bz[1]
        bitheader_setvalue_shift(&header, 14, 1, bc6h_format->by, 4);  //      by[4]
        bitheader_setvalue(&header, 15, 8, bc6h_format->gw);     // 8:   gw[7:0]
        bitheader_setvalue_shift(&header, 23, 1, bc6h_format->by, 5);  //      by[5]
        bitheader_setvalue_shift(&header, 24, 1, bc6h_format->gy, 4);  //      gy[4]
        bitheader_setvalue(&header, 25, 8, bc6h_format->bw);     // 8:   bw[7:0]
        bitheader_setvalue_shift(&header, 33, 1, bc6h_format->bz, 5);  //      bz[5]
        bitheader_setvalue_shift(&header, 34, 1, bc6h_format->bz, 4);  //      bz[4]
        bitheader_setvalue(&header, 35, 5, bc6h_format->rx);     // 5:   rx[4:0]
        bitheader_setvalue_shift(&header, 40, 1, bc6h_format->gz, 4);  //      gz[4]
        bitheader_setvalue(&header, 41, 4, bc6h_format->gy);     // 5:   gy[3:0]
        bitheader_setvalue(&header, 45, 5, bc6h_format->gx);     // 5:   gx[4:0]
        bitheader_setvalue(&header, 50, 1, bc6h_format->bz);     // 6:   bz[0]
        bitheader_setvalue(&header, 51, 4, bc6h_format->gz);     // 5:   gz[3:0]
        bitheader_setvalue(&header, 55, 6, bc6h_format->bx);     // 6:   bx[5:0]
        bitheader_setvalue(&header, 61, 4, bc6h_format->by);     // 6:   by[3:0]
        bitheader_setvalue(&header, 65, 5, bc6h_format->ry);     // 5:   ry[4:0]
        bitheader_setvalue_shift(&header, 70, 1, bc6h_format->bz, 2);  //      bz[2]
        bitheader_setvalue(&header, 71, 5, bc6h_format->rz);     // 5:   rz[4:0]
        bitheader_setvalue_shift(&header, 76, 1, bc6h_format->bz, 3);  //      bz[3]
        break;
    case 10:  // 0x1E
        bitheader_setvalue(&header, 0, 5, 0x1E);
        bitheader_setvalue(&header, 5, 6, bc6h_format->rw);      // 6:   rw[5:0]
        bitheader_setvalue_shift(&header, 11, 1, bc6h_format->gz, 4);  //      gz[4]
        bitheader_setvalue(&header, 12, 1, bc6h_format->bz);     // 6:   bz[0]
        bitheader_setvalue_shift(&header, 13, 1, bc6h_format->bz, 1);  //      bz[1]
        bitheader_setvalue_shift(&header, 14, 1, bc6h_format->by, 4);  //      by[4]
        bitheader_setvalue(&header, 15, 6, bc6h_format->gw);     // 6:   gw[5:0]
        bitheader_setvalue_shift(&header, 21, 1, bc6h_format->gy, 5);  //      gy[5]
        bitheader_setvalue_shift(&header, 22, 1, bc6h_format->by, 5);  //      by[5]
        bitheader_setvalue_shift(&header, 23, 1, bc6h_format->bz, 2);  //      bz[2]
        bitheader_setvalue_shift(&header, 24, 1, bc6h_format->gy, 4);  //      gy[4]
        bitheader_setvalue(&header, 25, 6, bc6h_format->bw);     // 6:   bw[5:0]
        bitheader_setvalue_shift(&header, 31, 1, bc6h_format->gz, 5);  //      gz[5]
        bitheader_setvalue_shift(&header, 32, 1, bc6h_format->bz, 3);  //      bz[3]
        bitheader_setvalue_shift(&header, 33, 1, bc6h_format->bz, 5);  //      bz[5]
        bitheader_setvalue_shift(&header, 34, 1, bc6h_format->bz, 4);  //      bz[4]
        bitheader_setvalue(&header, 35, 6, bc6h_format->rx);     // 6:   rx[5:0]
        bitheader_setvalue(&header, 41, 4, bc6h_format->gy);     // 6:   gy[3:0]
        bitheader_setvalue(&header, 45, 6, bc6h_format->gx);     // 6:   gx[5:0]
        bitheader_setvalue(&header, 51, 4, bc6h_format->gz);     // 6:   gz[3:0]
        bitheader_setvalue(&header, 55, 6, bc6h_format->bx);     // 6:   bx[5:0]
        bitheader_setvalue(&header, 61, 4, bc6h_format->by);     // 6:   by[3:0]
        bitheader_setvalue(&header, 65, 6, bc6h_format->ry);     // 6:   ry[5:0]
        bitheader_setvalue(&header, 71, 6, bc6h_format->rz);     // 6:   rz[5:0]
        break;

    // Single regions Modes
    case 11:  // 0x03
        bitheader_setvalue(&header, 0, 5, 0x03);
        bitheader_setvalue(&header, 5, 10, bc6h_format->rw);   // 10:   rw[9:0]
        bitheader_setvalue(&header, 15, 10, bc6h_format->gw);  // 10:   gw[9:0]
        bitheader_setvalue(&header, 25, 10, bc6h_format->bw);  // 10:   bw[9:0]
        bitheader_setvalue(&header, 35, 10, bc6h_format->rx);  // 10:   rx[9:0]
        bitheader_setvalue(&header, 45, 10, bc6h_format->gx);  // 10:   gx[9:0]
        bitheader_setvalue(&header, 55, 10, bc6h_format->bx);  // 10:   bx[9:0]
        break;
    case 12:  // 0x07
        bitheader_setvalue(&header, 0, 5, 0x07);
        bitheader_setvalue(&header, 5, 10, bc6h_format->rw);      // 11:   rw[9:0]
        bitheader_setvalue(&header, 15, 10, bc6h_format->gw);     // 11:   gw[9:0]
        bitheader_setvalue(&header, 25, 10, bc6h_format->bw);     // 11:   bw[9:0]
        bitheader_setvalue(&header, 35, 9, bc6h_format->rx);      // 9:    rx[8:0]
        bitheader_setvalue_shift(&header, 44, 1, bc6h_format->rw, 10);  //       rw[10]
        bitheader_setvalue(&header, 45, 9, bc6h_format->gx);      // 9:    gx[8:0]
        bitheader_setvalue_shift(&header, 54, 1, bc6h_format->gw, 10);  //       gw[10]
        bitheader_setvalue(&header, 55, 9, bc6h_format->bx);      // 9:    bx[8:0]
        bitheader_setvalue_shift(&header, 64, 1, bc6h_format->bw, 10);  //       bw[10]
        break;
    case 13:  // 0x0B
        bitheader_setvalue(&header, 0, 5, 0x0B);
        bitheader_setvalue(&header, 5, 10, bc6h_format->rw);      // 12:   rw[9:0]
        bitheader_setvalue(&header, 15, 10, bc6h_format->gw);     // 12:   gw[9:0]
        bitheader_setvalue(&header, 25, 10, bc6h_format->bw);     // 12:   bw[9:0]
        bitheader_setvalue(&header, 35, 8, bc6h_format->rx);      // 8:    rx[7:0]
        bitheader_setvalue_shift(&header, 43, 1, bc6h_format->rw, 11);  //       rw[11]
        bitheader_setvalue_shift(&header, 44, 1, bc6h_format->rw, 10);  //       rw[10]
        bitheader_setvalue(&header, 45, 8, bc6h_format->gx);      // 8:    gx[7:0]
        bitheader_setvalue_shift(&header, 53, 1, bc6h_format->gw, 11);  //       gw[11]
        bitheader_setvalue_shift(&header, 54, 1, bc6h_format->gw, 10);  //       gw[10]
        bitheader_setvalue(&header, 55, 8, bc6h_format->bx);      // 8:    bx[7:0]
        bitheader_setvalue_shift(&header, 63, 1, bc6h_format->bw, 11);  //       bw[11]
        bitheader_setvalue_shift(&header, 64, 1, bc6h_format->bw, 10);  //       bw[10]
        break;
    case 14:  // 0x0F
        bitheader_setvalue(&header, 0, 5, 0x0F);
        bitheader_setvalue(&header, 5, 10, bc6h_format->rw);      // 16:   rw[9:0]
        bitheader_setvalue(&header, 15, 10, bc6h_format->gw);     // 16:   gw[9:0]
        bitheader_setvalue(&header, 25, 10, bc6h_format->bw);     // 16:   bw[9:0]
        bitheader_setvalue(&header, 35, 4, bc6h_format->rx);      //  4:   rx[3:0]
        bitheader_setvalue_shift(&header, 39, 6, bc6h_format->rw, 10);  //       rw[15:10]
        bitheader_setvalue(&header, 45, 4, bc6h_format->gx);      //  4:   gx[3:0]
        bitheader_setvalue_shift(&header, 49, 6, bc6h_format->gw, 10);  //       gw[15:10]
        bitheader_setvalue(&header, 55, 4, bc6h_format->bx);      //  4:   bx[3:0]
        bitheader_setvalue_shift(&header, 59, 6, bc6h_format->bw, 10);  //       bw[15:10]
        break;
    default:  // Need to indicate error!
        return;
    }

    // Each format in the mode table can be uniquely identified by the mode bits.
    // The first ten modes are used for two-region tiles, and the mode bit field
    // can be either two or five bits long. These blocks also have fields for
    // the compressed color endpoints (72 or 75 bits), the partition (5 bits),
    // and the partition indices (46 bits).

    if (bc6h_format->m_mode >= MIN_MODE_FOR_ONE_REGION)
    {
        int startbit = ONE_REGION_INDEX_OFFSET;
        bitheader_setvalue(&header, startbit, 3, bc6h_format->indices16[0]);
        startbit += 3;
        for (int i = 1; i < 16; i++)
        {
            bitheader_setvalue(&header, startbit, 4, bc6h_format->indices16[i]);
            startbit += 4;
        }
    }
    else
    {
        bitheader_setvalue(&header, 77, 5, bc6h_format->d_shape_index);  // Shape Index
        int startbit = TWO_REGION_INDEX_OFFSET, nbits = 2;
        bitheader_setvalue(&header, startbit, nbits, bc6h_format->indices16[0]);
        for (int i = 1; i < 16; i++)
        {
            startbit += nbits;                                              // offset start bit for next index using prior nbits used
            nbits = g_indexfixups[bc6h_format->d_shape_index] == i ? 2 : 3;  // get new number of bit to save index with
            bitheader_setvalue(&header, startbit, nbits, bc6h_format->indices16[i]);
        }
    }

    // save to output buffer our new bit values
    // this can be optimized if header is part of bc6h_format struct
    bitheader_transferbits(&header, out, 16);
}

// decompress endpoints
static void decompress_endpts(const int in[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                              int       out[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                              const int mode,
                              bool      issigned)
{
    if (ModePartition[mode].transformed)
    {
        for (int i = 0; i < 3; ++i)
        {
            R_0(out) = issigned ? SIGN_EXTEND(R_0(in), ModePartition[mode].nbits) : R_0(in);
            int t;
            t        = SIGN_EXTEND(R_1(in), ModePartition[mode].prec[i]);
            t        = (t + R_0(in)) & MASK(ModePartition[mode].nbits);
            R_1(out) = issigned ? SIGN_EXTEND(t, ModePartition[mode].nbits) : t;

            t        = SIGN_EXTEND(R_2(in), ModePartition[mode].prec[i]);
            t        = (t + R_0(in)) & MASK(ModePartition[mode].nbits);
            R_2(out) = issigned ? SIGN_EXTEND(t, ModePartition[mode].nbits) : t;

            t        = SIGN_EXTEND(R_3(in), ModePartition[mode].prec[i]);
            t        = (t + R_0(in)) & MASK(ModePartition[mode].nbits);
            R_3(out) = issigned ? SIGN_EXTEND(t, ModePartition[mode].nbits) : t;
        }
    }
    else
    {
        for (int i = 0; i < 3; ++i)
        {
            R_0(out) = issigned ? SIGN_EXTEND(R_0(in), ModePartition[mode].nbits) : R_0(in);
            R_1(out) = issigned ? SIGN_EXTEND(R_1(in), ModePartition[mode].prec[i]) : R_1(in);
            R_2(out) = issigned ? SIGN_EXTEND(R_2(in), ModePartition[mode].prec[i]) : R_2(in);
            R_3(out) = issigned ? SIGN_EXTEND(R_3(in), ModePartition[mode].prec[i]) : R_3(in);
        }
    }
}

// endpoints fit only if the compression was lossless
static bool endpts_fit(const int orig[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                       const int compressed[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                       const int mode,
                       int       max_subsets,
                       bool      issigned)
{
    int uncompressed[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];

    decompress_endpts(compressed, uncompressed, mode, issigned);

    for (int j = 0; j < max_subsets; ++j)
        for (int i = 0; i < 3; ++i)
        {
            if (orig[j][0][i] != uncompressed[j][0][i])
                return false;
            if (orig[j][1][i] != uncompressed[j][1][i])
                return false;
        }

    return true;
}

// Dont know exact limits : for now just say is -2.0 to +2.0
static void bc6h_encoder_clampF16Max(const BC6HBlockEncoder *enc,
                                     float EndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG])
{
    for (int region = 0; region < 2; region++)
        for (int ab = 0; ab < 2; ab++)
            for (int rgb = 0; rgb < 3; rgb++)
            {
                if (enc->m_isSigned)
                {
                    if (EndPoints[region][ab][rgb] < F16_MAX_NEGATIVE_BITS)
                        EndPoints[region][ab][rgb] = F16_MAX_NEGATIVE_BITS;
                    else if (EndPoints[region][ab][rgb] > F16_MAX_BITS)
                        EndPoints[region][ab][rgb] = F16_MAX_BITS;
                }
                else
                {
                    if (EndPoints[region][ab][rgb] < 0.0)
                        EndPoints[region][ab][rgb] = 0;
                    else if (EndPoints[region][ab][rgb] > F16_MAX_BITS)
                        EndPoints[region][ab][rgb] = F16_MAX_BITS;
                }
                // Zero region
                // if ((EndPoints[region][ab][rgb] > -0.01) && ((EndPoints[region][ab][rgb] < 0.01))) EndPoints[region][ab][rgb] = 0.0;
            }
}

/*=================================================================
    Quantize Endpoints
    for a given mode
==================================================================*/

static void bc6h_encoder_QuantizeEndPointToF16Prec(const BC6HBlockEncoder *enc,
                                                   float EndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                                                   int   iEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                                                   int   max_subsets,
                                                   int   prec)
{
    for (int subset = 0; subset < max_subsets; ++subset)
    {
        iEndPoints[subset][0][0] = QuantizeToInt((short)EndPoints[subset][0][0], prec, enc->m_isSigned, enc->m_Exposure);  // A.Red
        iEndPoints[subset][0][1] = QuantizeToInt((short)EndPoints[subset][0][1], prec, enc->m_isSigned, enc->m_Exposure);  // A.Green
        iEndPoints[subset][0][2] = QuantizeToInt((short)EndPoints[subset][0][2], prec, enc->m_isSigned, enc->m_Exposure);  // A.Blue
        iEndPoints[subset][1][0] = QuantizeToInt((short)EndPoints[subset][1][0], prec, enc->m_isSigned, enc->m_Exposure);  // B.Red
        iEndPoints[subset][1][1] = QuantizeToInt((short)EndPoints[subset][1][1], prec, enc->m_isSigned, enc->m_Exposure);  // B.Green
        iEndPoints[subset][1][2] = QuantizeToInt((short)EndPoints[subset][1][2], prec, enc->m_isSigned, enc->m_Exposure);  // B.Blue
    }
}

/*=================================================================
    Swap Indices
    so that indices at fix up points have higher order bit set to 0
==================================================================*/

void bc6h_encoder_SwapIndices(int iEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                                   int iIndices[3][BC6H_MAX_SUBSET_SIZE],
                                   int entryCount[BC6H_MAX_SUBSETS],
                                   int max_subsets,
                                   int mode,
                                   int shape_pattern)
{
    unsigned int uNumIndices   = 1 << ModePartition[mode].IndexPrec;
    unsigned int uHighIndexBit = uNumIndices >> 1;

    for (int subset = 0; subset < max_subsets; ++subset)
    {
        // region 0 (subset = 0) The fix-up index for this subset is allways index 0
        // region 1 (subset = 1) The fix-up index for this subset varies based on the shape
        size_t i = subset ? g_Region2FixUp[shape_pattern] : 0;

        if (iIndices[subset][i] & uHighIndexBit)
        {
            // high bit is set, swap the aEndPts and indices for this region
            bc6h_swap_int(&(iEndPoints[subset][0][0]), &(iEndPoints[subset][1][0]));
            bc6h_swap_int(&(iEndPoints[subset][0][1]), &(iEndPoints[subset][1][1]));
            bc6h_swap_int(&(iEndPoints[subset][0][2]), &(iEndPoints[subset][1][2]));
            for (size_t j = 0; j < (size_t)entryCount[subset]; ++j)
            {
                iIndices[subset][j] = uNumIndices - 1 - iIndices[subset][j];
            }
        }
    }
}

/*=================================================================
    Tranforms according to shape precission
==================================================================*/
// helper function to check transform overflow
bool isOverflow(int endpoint, int nbit, bool bIsSigned)
{
    if (bIsSigned)
    {
        int nbRequired;  //bits required for the encode
        int nb;
        if (endpoint == 0)
        {
            return false;  // no overflow
        }
        else if (endpoint > 0)
        {
            for (nb = 0; endpoint; ++nb, endpoint >>= 1)
                ;
            nbRequired = nb + (bIsSigned ? 1 : 0);
            if (nbRequired > nbit)  //overflow
                return true;
        }
        else
        {  //negative endpoints
            if (!bIsSigned)
                return true;

            for (nb = 0; endpoint < -1; ++nb, endpoint >>= 1)
                ;
            nbRequired = nb + 1;
            if (nbRequired > nbit)  //overflow
                return true;
        }

        return false;
    }
    else
    {
        int maxRange = (int)pow(2, nbit - 1) - 1;
        int minRange = (int)-(pow(2, nbit - 1));

        //no overflow
        if ((endpoint >= minRange) && (endpoint <= maxRange))
            return false;
        else  //overflow
            return true;
    }
}

// Bug in this code : Need to add signed bit to values
bool bc6h_encoder_TransformEndPoints(AMD_BC6H_Format * BC6H_data,
                                          int              iEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                                          int              oEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                                          int              max_subsets,
                                          int              mode)
{
    int Mask;
    if (ModePartition[mode].transformed)
    {
        BC6H_data->istransformed = true;
        for (int i = 0; i < 3; ++i)
        {
            Mask                = MASK(ModePartition[mode].nbits);
            oEndPoints[0][0][i] = iEndPoints[0][0][i] & Mask;  // [0][A]

            // for negative numbers we add back the sign bits
            if (iEndPoints[0][0][i] < 0)
            {
                Mask = ~Mask;
                oEndPoints[0][0][i] |= Mask;
            }

            Mask                = MASK(ModePartition[mode].prec[i]);
            oEndPoints[0][1][i] = iEndPoints[0][1][i] - iEndPoints[0][0][i];  // [0][B] - [0][A]

            if (isOverflow(oEndPoints[0][1][i], ModePartition[mode].prec[i], BC6H_data->issigned))
                return false;

            oEndPoints[0][1][i] = (oEndPoints[0][1][i] & Mask);

            //redo the check for sign overflow for one region case
            if (max_subsets <= 1)
            {
                if (isOverflow(oEndPoints[0][1][i], ModePartition[mode].prec[i], BC6H_data->issigned))
                    return false;
            }

            if (max_subsets > 1)
            {
                oEndPoints[1][0][i] = iEndPoints[1][0][i] - iEndPoints[0][0][i];  // [1][A] - [0][A]
                if (isOverflow(oEndPoints[1][0][i], ModePartition[mode].prec[i], BC6H_data->issigned))
                    return false;

                oEndPoints[1][0][i] = (oEndPoints[1][0][i] & Mask);

                oEndPoints[1][1][i] = iEndPoints[1][1][i] - iEndPoints[0][0][i];  // [1][B] - [0][A]
                if (isOverflow(oEndPoints[1][1][i], ModePartition[mode].prec[i], BC6H_data->issigned))
                    return false;

                oEndPoints[1][1][i] = (oEndPoints[1][1][i] & Mask);
            }
        }
    }
    else
    {
        BC6H_data->istransformed = false;
        for (int i = 0; i < 3; ++i)
        {
            Mask                = MASK(ModePartition[mode].nbits);
            oEndPoints[0][0][i] = iEndPoints[0][0][i] & Mask;

            Mask                = MASK(ModePartition[mode].prec[i]);
            oEndPoints[0][1][i] = iEndPoints[0][1][i] & Mask;

            if (max_subsets > 1)
            {
                oEndPoints[1][0][i] = iEndPoints[1][0][i] & Mask;
                oEndPoints[1][1][i] = iEndPoints[1][1][i] & Mask;
            }
        }
    }

    return true;
}

void bc6h_encoder_SaveCompressedBlockData(AMD_BC6H_Format * BC6H_data,
                                               int              oEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                                               int              iIndices[2][BC6H_MAX_SUBSET_SIZE],
                                               int              max_subsets,
                                               int              mode)
{
    BC6H_data->m_mode = (unsigned short)mode;
    BC6H_data->index++;

    // Save the data to output
    BC6H_data->rw = oEndPoints[0][0][0];  // rw
    BC6H_data->gw = oEndPoints[0][0][1];  // gw
    BC6H_data->bw = oEndPoints[0][0][2];  // bw
    BC6H_data->rx = oEndPoints[0][1][0];  // rx
    BC6H_data->gx = oEndPoints[0][1][1];  // gx
    BC6H_data->bx = oEndPoints[0][1][2];  // bx

    if (max_subsets > 1)
    {
        // Save the data to output
        BC6H_data->ry = oEndPoints[1][0][0];  // ry
        BC6H_data->gy = oEndPoints[1][0][1];  // gy
        BC6H_data->by = oEndPoints[1][0][2];  // by
        BC6H_data->rz = oEndPoints[1][1][0];  // rz
        BC6H_data->gz = oEndPoints[1][1][1];  // gz
        BC6H_data->bz = oEndPoints[1][1][2];  // bz
    }

    // Map our two subset Indices for the shape to output 4x4 block
    int pos[2] = {0, 0};
    int asubset;
    for (int i = 0; i < BC6H_MAX_SUBSET_SIZE; i++)
    {
        if (max_subsets > 1)
            asubset = PARTITIONS[1][BC6H_data->d_shape_index][i];  // Two region shapes
        else
            asubset = PARTITIONS[0][BC6H_data->d_shape_index][i];  // One region shapes
        BC6H_data->indices16[i] = (uint8_t)iIndices[asubset][pos[asubset]];
        pos[asubset]++;
    }
}

void palitizeEndPointsF(AMD_BC6H_Format * BC6H_data, float fEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG])
{
    // scale endpoints
    float Ar, Ag, Ab, Br, Bg, Bb;

    // Compose index colors from end points
    if (BC6H_data->region == 1)
    {
        Ar = fEndPoints[0][0][0];
        Ag = fEndPoints[0][0][1];
        Ab = fEndPoints[0][0][2];
        Br = fEndPoints[0][1][0];
        Bg = fEndPoints[0][1][1];
        Bb = fEndPoints[0][1][2];

        for (int i = 0; i < 16; i++)
        {
            // Red
            BC6H_data->Paletef[0][i].x = lerpf(Ar, Br, i, 15);
            // Green
            BC6H_data->Paletef[0][i].y = lerpf(Ag, Bg, i, 15);
            // Blue
            BC6H_data->Paletef[0][i].z = lerpf(Ab, Bb, i, 15);
        }
    }
    else
    {  //mode.type == BC6_TWO
        for (int region = 0; region < 2; region++)
        {
            Ar = fEndPoints[region][0][0];
            Ag = fEndPoints[region][0][1];
            Ab = fEndPoints[region][0][2];
            Br = fEndPoints[region][1][0];
            Bg = fEndPoints[region][1][1];
            Bb = fEndPoints[region][1][2];
            for (int i = 0; i < 8; i++)
            {
                // Red
                BC6H_data->Paletef[region][i].x = lerpf(Ar, Br, i, 7);
                // Greed
                BC6H_data->Paletef[region][i].y = lerpf(Ag, Bg, i, 7);
                // Blue
                BC6H_data->Paletef[region][i].z = lerpf(Ab, Bb, i, 7);
            }
        }
    }
}

float CalcOneRegionEndPtsError(AMD_BC6H_Format * BC6H_data,
                               float            fEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                               int              shape_indices[MAX_SUBSETS][MAX_SUBSET_SIZE])
{
    float error = 0;

    for (int i = 0; i < BC6H_MAX_SUBSET_SIZE; i++)
    {
        for (int m = 0; m < MAX_END_POINTS; m++)
        {
            for (int n = 0; n < NCHANNELS; n++)
            {
                float calencpts = fEndPoints[0][m][n] + (fabsf(fEndPoints[0][m][n] - fEndPoints[0][m][n]) * (shape_indices[0][i] / 15));
                error += fabsf(BC6H_data->din[i][n] - calencpts);
            }
        }
    }

    return error;
}

float CalcShapeError(AMD_BC6H_Format * BC6H_data, float fEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG], bool SkipPallet)
{
    int   maxPallet;
    int   subset     = 0;
    float totalError = 0.0f;
    int   region     = (BC6H_data->region - 1);

    if (region == 0)
        maxPallet = 16;
    else
        maxPallet = 8;

    if (!SkipPallet)
        palitizeEndPointsF(BC6H_data, fEndPoints);

    for (int i = 0; i < MAX_SUBSET_SIZE; i++)
    {
        float error     = 0.0f;
        float bestError = 0.0f;

        if (region == 0)
        {
            subset = 0;
        }
        else
        {
            //subset 0 or subset 1
            subset = PARTITIONS[region][BC6H_data->d_shape_index][i];
        }

        // initialize bestError to the difference for first data
        bestError = fabsf(BC6H_data->din[i][0] - BC6H_data->Paletef[subset][0].x) + fabsf(BC6H_data->din[i][1] - BC6H_data->Paletef[subset][0].y) +
                    fabsf(BC6H_data->din[i][2] - BC6H_data->Paletef[subset][0].z);

        // loop through the rest of the data until find the best error
        for (int j = 1; j < maxPallet && bestError > 0; j++)
        {
            error = fabsf(BC6H_data->din[i][0] - BC6H_data->Paletef[subset][j].x) + fabsf(BC6H_data->din[i][1] - BC6H_data->Paletef[subset][j].y) +
                    fabsf(BC6H_data->din[i][2] - BC6H_data->Paletef[subset][j].z);

            if (error <= bestError)
                bestError = error;
            else
                break;
        }
        totalError += bestError;
    }

    return totalError;
}

void ReIndexShapef(AMD_BC6H_Format * BC6H_data, int shape_indices[BC6H_MAX_SUBSETS][MAX_SUBSET_SIZE])
{
    float error = 0;
    float bestError;
    int   bestIndex = 0;
    int   sub0index = 0;
    int   sub1index = 0;
    int   MaxPallet;
    int   region = (BC6H_data->region - 1);

    if (region == 0)
        MaxPallet = 16;
    else
        MaxPallet = 8;

    for (int i = 0; i < BC6H_MAX_SUBSET_SIZE; i++)
    {
        // subset 0 or subset 1
        if (PARTITIONS[region][BC6H_data->d_shape_index][i])
        {
            bestError = FLT_MAX;
            bestIndex = 0;

            // For two shape regions max Pallet is 8
            for (int j = 0; j < MaxPallet; j++)
            {
                // Calculate error from original
                error = fabsf(BC6H_data->din[i][0] - BC6H_data->Paletef[1][j].x) + fabsf(BC6H_data->din[i][1] - BC6H_data->Paletef[1][j].y) +
                        fabsf(BC6H_data->din[i][2] - BC6H_data->Paletef[1][j].z);
                if (error < bestError)
                {
                    bestError = error;
                    bestIndex = j;
                }
            }

            shape_indices[1][sub1index] = bestIndex;
            sub1index++;
        }
        else
        {
            // This is shared for one or two shape regions max Pallet either 16 or 8
            bestError = FLT_MAX;
            bestIndex = 0;

            for (int j = 0; j < MaxPallet; j++)
            {
                // Calculate error from original
                error = fabsf(BC6H_data->din[i][0] - BC6H_data->Paletef[0][j].x) + fabsf(BC6H_data->din[i][1] - BC6H_data->Paletef[0][j].y) +
                        fabsf(BC6H_data->din[i][2] - BC6H_data->Paletef[0][j].z);
                if (error < bestError)
                {
                    bestError = error;
                    bestIndex = j;
                }
            }

            shape_indices[0][sub0index] = bestIndex;
            sub0index++;
        }
    }
}

static float bc6h_encoder_FindBestPattern(BC6HBlockEncoder *enc,
                                          AMD_BC6H_Format *BC6H_data,
                                          bool TwoRegionShapes,
                                          int shape_pattern)
{
    // Index bit size for the patterns been used.
    // All two zone shapes have 3 bits per color, max index value < 8
    // All one zone shapes gave 4 bits per color, max index value < 16
    int   Index_BitSize = TwoRegionShapes ? 8 : 16;
    int   max_subsets   = TwoRegionShapes ? 2 : 1;
    float direction[NCHANNELS];
    float step;

    BC6H_data->region        = (unsigned short)max_subsets;
    BC6H_data->index         = 0;
    BC6H_data->d_shape_index = (unsigned short)shape_pattern;
    memset(BC6H_data->partition, 0, sizeof(BC6H_data->partition));
    memset(BC6H_data->shape_indices, 0, sizeof(BC6H_data->shape_indices));

    // Get the pattern to encode with
    Partition(shape_pattern,         // Shape pattern we want to get
              BC6H_data->din,         // Input data
              BC6H_data->partition,   // Returns the patterned shape data
              BC6H_data->entryCount,  // counts the number of pixel used in each subset region num of 0's amd 1's
              max_subsets,           // Table Shapes to use eithe one regions 1 or two regions 2
              3);                    // rgb no alpha always = 3

    float error[MAX_SUBSETS] = {0.0, FLT_MAX, FLT_MAX};
    int   BestOutB           = 0;
    float BestError;         //the lowest error from vector direction quantization
    float BestError_endpts;  //the lowest error from endpoints extracted from the vector direction quantization

    float outB[2][2][MAX_SUBSET_SIZE][MAX_DIMENSION_BIG];
    int   shape_indicesB[2][MAX_SUBSETS][MAX_SUBSET_SIZE];

    for (int subset = 0; subset < max_subsets; subset++)
    {
        error[0] += optQuantAnD_d(BC6H_data->partition[subset],   // input data
                                  BC6H_data->entryCount[subset],  // number of input points above (not clear about 1, better to avoid)
                                  Index_BitSize,                 // number of clusters on the ramp, 8  or 16
                                  shape_indicesB[0][subset],     // output index, if not all points of the ramp used, 0 may not be assigned
                                  outB[0][subset],               // resulting quantization
                                  direction,                     // direction vector of the ramp (check normalization)
                                  &step,                         // step size (check normalization)
                                  3,                             // number of channels (always 3 = RGB for BC6H)
                                  enc->m_quality                      // Quality set number of retry to get good end points
                                                                 // Max retries = MAX_TRY = 4000 when Quality is 1.0
                                                                 // Min = 0 and default with quality 0.05 is 200 times
        );
    }

    BestError = error[0];
    BestOutB  = 0;

    // The following code is almost complete - runs very slow and not sure if % of improvement is justified..
#ifdef USE_SHAKERHD
    // Valid only for 2 region shapes
    if ((max_subsets > 1) && (enc->m_quality > 0.80))
    {
        int tempIndices[MAX_SUBSET_SIZE];
        // int     temp_epo_code[2][2][MAX_DIMENSION_BIG];
        int bits[3] = {8, 8, 8};  // Channel index bit size

        // float   epo[2][MAX_DIMENSION_BIG];
        int epo_code[MAX_SUBSETS][2][MAX_DIMENSION_BIG];
        // int     shakeSize = 8;

        error[1] = 0.0;
        for (int subset = 0; subset < max_subsets; subset++)
        {
            for (int k = 0; k < BC6H_data->entryCount[subset]; k++)
            {
                tempIndices[k] = shape_indicesB[0][subset][k];
            }

            error[1] += ep_shaker_HD(BC6H_data->partition[subset],
                                     BC6H_data->entryCount[subset],
                                     tempIndices,      // output index, if not all points of the ramp used, 0 may not be assigned
                                     outB[1][subset],  // resulting quantization
                                     epo_code[subset],
                                     BC6H_data->entryCount[subset] - 1,
                                     bits,
                                     3);

            // error[1] += ep_shaker_2_d(
            //      BC6H_data->partition[subset],
            //      BC6H_data->entryCount[subset],
            //      tempIndices,                    // output index, if not all points of the ramp used, 0 may not be assigned
            //      outB[1][subset],                // resulting quantization
            //      epo_code[subset],
            //      shakeSize,
            //      BC6H_data->entryCount[subset] - 1,
            //      bits[0],
            //      3,
            //      epo
            //      );

            for (int k = 0; k < BC6H_data->entryCount[subset]; k++)
            {
                shape_indicesB[1][subset][k] = tempIndices[k];
            }

        }  // subsets

        if (BestError > error[1])
        {
            BestError = error[1];
            BestOutB  = 1;
            for (int subset = 0; subset < max_subsets; subset++)
            {
                for (int k = 0; k < MAX_DIMENSION_BIG; k++)
                {
                    BC6H_data->fEndPoints[subset][0][k] = (float)epo_code[subset][0][k];
                    BC6H_data->fEndPoints[subset][1][k] = (float)epo_code[subset][1][k];
                }
            }
        }
    }
#endif

    // Save the best for BC6H data processing later
    if (BestOutB == 0)
        GetEndPoints(BC6H_data->fEndPoints, outB[BestOutB], max_subsets, BC6H_data->entryCount);

    memcpy(BC6H_data->shape_indices, shape_indicesB[BestOutB], sizeof(BC6H_data->shape_indices));
    bc6h_encoder_clampF16Max(enc, BC6H_data->fEndPoints);

    BestError_endpts = CalcShapeError(BC6H_data, BC6H_data->fEndPoints, false);
    return BestError_endpts;
}

int finish_unquantizeF16(int q, bool isSigned)
{
    // Is it F16 Signed else F16 Unsigned
    if (isSigned)
        return (q < 0) ? -(((-q) * 31) >> 5) : (q * 31) >> 5;  // scale the magnitude by 31/32
    else
        return (q * 31) >> 6;  // scale the magnitude by 31/64

    // Note for Undefined we should return q as is
}

void decompress_endpoints1(AMD_BC6H_Format * bc6h_format,
                           int              oEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                           float            outf[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                           int              mode)
{
    int i;
    int t;
    int out[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];

    if (bc6h_format->issigned)
    {
        if (bc6h_format->istransformed)
        {
            for (i = 0; i < NCHANNELS; i++)
            {
                out[0][0][i] = SIGN_EXTEND(oEndPoints[0][0][i], ModePartition[mode].nbits);

                t            = SIGN_EXTEND(oEndPoints[0][1][i], ModePartition[mode].prec[i]);  //C_RED
                t            = (t + oEndPoints[0][0][i]) & MASK(ModePartition[mode].nbits);
                out[0][1][i] = SIGN_EXTEND(t, ModePartition[mode].nbits);

                // Unquantize all points to nbits
                out[0][0][i] = Unquantize(out[0][0][i], (unsigned char)ModePartition[mode].nbits, true);
                out[0][1][i] = Unquantize(out[0][1][i], (unsigned char)ModePartition[mode].nbits, true);

                // F16 format
                outf[0][0][i] = (float)finish_unquantizeF16(out[0][0][i], true);
                outf[0][1][i] = (float)finish_unquantizeF16(out[0][1][i], true);
            }
        }
        else
        {
            for (i = 0; i < NCHANNELS; i++)
            {
                out[0][0][i] = SIGN_EXTEND(oEndPoints[0][0][i], ModePartition[mode].nbits);
                out[0][1][i] = SIGN_EXTEND(oEndPoints[0][1][i], ModePartition[mode].prec[i]);

                // Unquantize all points to nbits
                out[0][0][i] = Unquantize(out[0][0][i], (unsigned char)ModePartition[mode].nbits, true);
                out[0][1][i] = Unquantize(out[0][1][i], (unsigned char)ModePartition[mode].nbits, true);

                // F16 format
                outf[0][0][i] = (float)finish_unquantizeF16(out[0][0][i], true);
                outf[0][1][i] = (float)finish_unquantizeF16(out[0][1][i], true);
            }
        }
    }
    else
    {
        if (bc6h_format->istransformed)
        {
            for (i = 0; i < NCHANNELS; i++)
            {
                out[0][0][i] = oEndPoints[0][0][i];
                t            = SIGN_EXTEND(oEndPoints[0][1][i], ModePartition[mode].prec[i]);
                out[0][1][i] = (t + oEndPoints[0][0][i]) & MASK(ModePartition[mode].nbits);

                // Unquantize all points to nbits
                out[0][0][i] = Unquantize(out[0][0][i], (unsigned char)ModePartition[mode].nbits, false);
                out[0][1][i] = Unquantize(out[0][1][i], (unsigned char)ModePartition[mode].nbits, false);

                // F16 format
                outf[0][0][i] = (float)finish_unquantizeF16(out[0][0][i], false);
                outf[0][1][i] = (float)finish_unquantizeF16(out[0][1][i], false);
            }
        }
        else
        {
            for (i = 0; i < NCHANNELS; i++)
            {
                out[0][0][i] = oEndPoints[0][0][i];
                out[0][1][i] = oEndPoints[0][1][i];

                // Unquantize all points to nbits
                out[0][0][i] = Unquantize(out[0][0][i], (unsigned char)ModePartition[mode].nbits, false);
                out[0][1][i] = Unquantize(out[0][1][i], (unsigned char)ModePartition[mode].nbits, false);

                // F16 format
                outf[0][0][i] = (float)finish_unquantizeF16(out[0][0][i], false);
                outf[0][1][i] = (float)finish_unquantizeF16(out[0][1][i], false);
            }
        }
    }
}

void decompress_endpoints2(AMD_BC6H_Format * bc6h_format,
                           int              oEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                           float            outf[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                           int              mode)
{
    int i;
    int t;
    int out[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];

    if (bc6h_format->issigned)
    {
        if (bc6h_format->istransformed)
        {
            for (i = 0; i < NCHANNELS; i++)
            {
                // get the quantized values
                out[0][0][i] = SIGN_EXTEND(oEndPoints[0][0][i], ModePartition[mode].nbits);

                t            = SIGN_EXTEND(oEndPoints[0][1][i], ModePartition[mode].prec[i]);
                t            = (t + oEndPoints[0][0][i]) & MASK(ModePartition[mode].nbits);
                out[0][1][i] = SIGN_EXTEND(t, ModePartition[mode].nbits);

                t            = SIGN_EXTEND(oEndPoints[1][0][i], ModePartition[mode].prec[i]);
                t            = (t + oEndPoints[0][0][i]) & MASK(ModePartition[mode].nbits);
                out[1][0][i] = SIGN_EXTEND(t, ModePartition[mode].nbits);

                t            = SIGN_EXTEND(oEndPoints[1][1][i], ModePartition[mode].prec[i]);
                t            = (t + oEndPoints[0][0][i]) & MASK(ModePartition[mode].nbits);
                out[1][1][i] = SIGN_EXTEND(t, ModePartition[mode].nbits);

                // Unquantize all points to nbits
                out[0][0][i] = Unquantize(out[0][0][i], (unsigned char)ModePartition[mode].nbits, true);
                out[0][1][i] = Unquantize(out[0][1][i], (unsigned char)ModePartition[mode].nbits, true);
                out[1][0][i] = Unquantize(out[1][0][i], (unsigned char)ModePartition[mode].nbits, true);
                out[1][1][i] = Unquantize(out[1][1][i], (unsigned char)ModePartition[mode].nbits, true);

                // F16 format
                outf[0][0][i] = (float)finish_unquantizeF16(out[0][0][i], true);
                outf[0][1][i] = (float)finish_unquantizeF16(out[0][1][i], true);
                outf[1][0][i] = (float)finish_unquantizeF16(out[1][0][i], true);
                outf[1][1][i] = (float)finish_unquantizeF16(out[1][1][i], true);
            }
        }
        else
        {
            for (i = 0; i < NCHANNELS; i++)
            {
                out[0][0][i] = SIGN_EXTEND(oEndPoints[0][0][i], ModePartition[mode].nbits);
                out[0][1][i] = SIGN_EXTEND(oEndPoints[0][1][i], ModePartition[mode].prec[i]);
                out[1][0][i] = SIGN_EXTEND(oEndPoints[1][0][i], ModePartition[mode].prec[i]);
                out[1][1][i] = SIGN_EXTEND(oEndPoints[1][1][i], ModePartition[mode].prec[i]);

                // Unquantize all points to nbits
                out[0][0][i] = Unquantize(out[0][0][i], (unsigned char)ModePartition[mode].nbits, true);
                out[0][1][i] = Unquantize(out[0][1][i], (unsigned char)ModePartition[mode].nbits, true);
                out[1][0][i] = Unquantize(out[1][0][i], (unsigned char)ModePartition[mode].nbits, true);
                out[1][1][i] = Unquantize(out[1][1][i], (unsigned char)ModePartition[mode].nbits, true);

                // nbits to F16 format
                outf[0][0][i] = (float)finish_unquantizeF16(out[0][0][i], true);
                outf[0][1][i] = (float)finish_unquantizeF16(out[0][1][i], true);
                outf[1][0][i] = (float)finish_unquantizeF16(out[1][0][i], true);
                outf[1][1][i] = (float)finish_unquantizeF16(out[1][1][i], true);
            }
        }
    }
    else
    {
        if (bc6h_format->istransformed)
        {
            for (i = 0; i < NCHANNELS; i++)
            {
                out[0][0][i] = oEndPoints[0][0][i];
                t            = SIGN_EXTEND(oEndPoints[0][1][i], ModePartition[mode].prec[i]);
                out[0][1][i] = (t + oEndPoints[0][0][i]) & MASK(ModePartition[mode].nbits);

                t            = SIGN_EXTEND(oEndPoints[1][0][i], ModePartition[mode].prec[i]);
                out[1][0][i] = (t + oEndPoints[0][0][i]) & MASK(ModePartition[mode].nbits);

                t            = SIGN_EXTEND(oEndPoints[1][1][i], ModePartition[mode].prec[i]);
                out[1][1][i] = (t + oEndPoints[0][0][i]) & MASK(ModePartition[mode].nbits);

                // Unquantize all points to nbits
                out[0][0][i] = Unquantize(out[0][0][i], (unsigned char)ModePartition[mode].nbits, false);
                out[0][1][i] = Unquantize(out[0][1][i], (unsigned char)ModePartition[mode].nbits, false);
                out[1][0][i] = Unquantize(out[1][0][i], (unsigned char)ModePartition[mode].nbits, false);
                out[1][1][i] = Unquantize(out[1][1][i], (unsigned char)ModePartition[mode].nbits, false);

                // nbits to F16 format
                outf[0][0][i] = (float)finish_unquantizeF16(out[0][0][i], false);
                outf[0][1][i] = (float)finish_unquantizeF16(out[0][1][i], false);
                outf[1][0][i] = (float)finish_unquantizeF16(out[1][0][i], false);
                outf[1][1][i] = (float)finish_unquantizeF16(out[1][1][i], false);
            }
        }
        else
        {
            for (i = 0; i < NCHANNELS; i++)
            {
                out[0][0][i] = oEndPoints[0][0][i];
                out[0][1][i] = oEndPoints[0][1][i];
                out[1][0][i] = oEndPoints[1][0][i];
                out[1][1][i] = oEndPoints[1][1][i];

                // Unquantize all points to nbits
                out[0][0][i] = Unquantize(out[0][0][i], (unsigned char)ModePartition[mode].nbits, false);
                out[0][1][i] = Unquantize(out[0][1][i], (unsigned char)ModePartition[mode].nbits, false);
                out[1][0][i] = Unquantize(out[1][0][i], (unsigned char)ModePartition[mode].nbits, false);
                out[1][1][i] = Unquantize(out[1][1][i], (unsigned char)ModePartition[mode].nbits, false);

                // nbits to F16 format
                outf[0][0][i] = (float)finish_unquantizeF16(out[0][0][i], false);
                outf[0][1][i] = (float)finish_unquantizeF16(out[0][1][i], false);
                outf[1][0][i] = (float)finish_unquantizeF16(out[1][0][i], false);
                outf[1][1][i] = (float)finish_unquantizeF16(out[1][1][i], false);
            }
        }
    }
}

static av_unused void bc6h_encoder_AverageEndPoint(BC6HBlockEncoder *enc,
                                                   float EndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                                                   float fEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG],
                                                   int   max_subsets,
                                                   int   mode)
{
    if (ModePartition[mode].nbits > 7)
    {
        for (int subset = 0; subset < max_subsets; ++subset)
        {
            fEndPoints[subset][0][0] = EndPoints[subset][0][0];  // A.Red
            fEndPoints[subset][0][1] = EndPoints[subset][0][1];  // A.Green
            fEndPoints[subset][0][2] = EndPoints[subset][0][2];  // A.Blue
            fEndPoints[subset][1][0] = EndPoints[subset][1][0];  // A.Red
            fEndPoints[subset][1][1] = EndPoints[subset][1][1];  // A.Green
            fEndPoints[subset][1][2] = EndPoints[subset][1][2];  // A.Blue
        }

        return;
    }

    float diff;
    float avr;

    // determin differance level based on lowest precision of the mode
    enc->m_DiffLevel = (float)ModePartition[mode].lowestPrec;

    for (int subset = 0; subset < max_subsets; ++subset)
    {
        avr = (EndPoints[subset][0][0] + EndPoints[subset][0][1] + EndPoints[subset][0][2]) / 3.0f;

        // determine average diff
        diff = (fabsf(EndPoints[subset][0][0] - avr) + fabsf(EndPoints[subset][0][1] - avr) + fabsf(EndPoints[subset][0][2] - avr)) / 3;

        if ((diff < enc->m_DiffLevel) && (avr > enc->m_DiffLevel))
        {
            fEndPoints[subset][0][0] = avr;  // A.Red
            fEndPoints[subset][0][1] = avr;  // A.Green
            fEndPoints[subset][0][2] = avr;  // A.Blue
        }
        else
        {
            fEndPoints[subset][0][0] = EndPoints[subset][0][0];  // A.Red
            fEndPoints[subset][0][1] = EndPoints[subset][0][1];  // A.Green
            fEndPoints[subset][0][2] = EndPoints[subset][0][2];  // A.Blue
        }

        avr = (EndPoints[subset][1][0] + EndPoints[subset][1][1] + EndPoints[subset][1][2]) / 3.0f;

        diff = (fabsf(EndPoints[subset][1][0] - avr) + fabsf(EndPoints[subset][1][1] - avr) + fabsf(EndPoints[subset][1][2] - avr)) / 3;

        if ((diff < enc->m_DiffLevel) && (avr > enc->m_DiffLevel))
        {
            fEndPoints[subset][1][0] = avr;  // B.Red
            fEndPoints[subset][1][1] = avr;  // B.Green
            fEndPoints[subset][1][2] = avr;  // B.Blue
        }
        else
        {
            fEndPoints[subset][1][0] = EndPoints[subset][1][0];  // A.Red
            fEndPoints[subset][1][1] = EndPoints[subset][1][1];  // A.Green
            fEndPoints[subset][1][2] = EndPoints[subset][1][2];  // A.Blue
        }
    }
}

//================================================
// Mode Pathern order to try on endpoints
// The order can be rearranged to set which modes gets processed first
// for now it is set in order.
//================================================
static int ModeFitOrder[MAX_BC6H_MODES + 1] = {
    0,  //0: N/A
    // ----  2 region lower bits ---
    1,   // 10 5 5 5
    2,   // 7  6 6 6
    3,   // 11 5 4 5
    4,   // 11 4 5 4
    5,   // 11 4 4 5
    6,   // 9  5 5 5
    7,   // 8  6 5 5
    8,   // 8  5 6 5
    9,   // 8  5 5 6
    10,  // 6  6 6 6
    //------ 1 region high bits ---
    11,  // 10 10 10 10
    12,  // 11 9  9  9
    13,  // 12 8  8  8
    14   // 16 4  4  4
};

static float bc6h_encoder_EncodePattern(BC6HBlockEncoder *enc,
                                        AMD_BC6H_Format *BC6H_data,
                                        float error)
{
    int max_subsets = BC6H_data->region;

    // now we have input colors (in), output colors (outB) mapped to a line of ends (EndPoints)
    // and a set of colors on the line equally spaced (indexedcolors)
    // Lets assign indices

    //float SrcEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];                  // temp endpoints used during calculations

    // Quantize the EndPoints
    int F16EndPoints[MAX_BC6H_MODES + 1][MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];    // temp endpoints used during calculations
    int quantEndPoints[MAX_BC6H_MODES + 1][MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];  // endpoints to save for a given mode

    // ModePartition[] starts from 1 to 14
    // If we have a shape pattern set the loop to check modes from 1 to 10 else from 11 to 14
    // of the ModePartition table
    int min_mode = (BC6H_data->region == 2) ? 1 : 11;
    int max_mode = (BC6H_data->region == 2) ? MAX_TWOREGION_MODES : MAX_BC6H_MODES;

    bool fits[15];
    memset(fits, 0, sizeof(fits));

    int   bestFit          = 0;
    int   bestEndpointMode = 0;
    float bestError        = FLT_MAX;
    float bestEndpointsErr = FLT_MAX;
    float endPointErr      = 0;

    // Try Optimization for the Mode
    float best_EndPoints[MAX_BC6H_MODES + 1][MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];
    int   best_Indices[MAX_BC6H_MODES + 1][MAX_SUBSETS][MAX_SUBSET_SIZE];
    float opt_toterr[MAX_BC6H_MODES + 1];

    // for debugging
    memset(opt_toterr, 0, sizeof(opt_toterr));

    int numfits = 0;
    //
    // Notes;  Only the endpoints are varying; the indices stay fixed in values!
    // so to optimize which mode we need only check the endpoints error against our original to pick the mode to save
    //
    for (int modes = min_mode; modes <= max_mode; ++modes)
    {
        memcpy(best_EndPoints[modes], BC6H_data->fEndPoints, sizeof(BC6H_data->fEndPoints));
        memcpy(best_Indices[modes], BC6H_data->shape_indices, sizeof(BC6H_data->shape_indices));

        // For some modes the differances between channels can be quite small
        // typically for 6 bits 0..32 an increment of 1 in a channel can cause
        // unwanted color artifacts.
        // Check if computed channel endpoint have a wide spread between channels if not
        // scale all the channels to a avarage so that the variance is not noticed at lower bit values
        //if (enc->m_bAverageEndPoint)
        //{
        //    AverageEndPoint(best_EndPoints[modes], SrcEndPoints, max_subsets, ModeFitOrder[modes]);
        //    QuantizeEndPointToF16Prec(SrcEndPoints, F16EndPoints[modes], max_subsets, ModePartition[ModeFitOrder[modes]].nbits);
        //}
        //else
        {
            bc6h_encoder_QuantizeEndPointToF16Prec(enc, best_EndPoints[modes], F16EndPoints[modes], max_subsets,
                                                   ModePartition[ModeFitOrder[modes]].nbits);
        }

        // Indices data to save for given mode
        bc6h_encoder_SwapIndices(F16EndPoints[modes], best_Indices[modes], BC6H_data->entryCount, max_subsets,
                                 ModeFitOrder[modes], BC6H_data->d_shape_index);
        bool transformfit = bc6h_encoder_TransformEndPoints(BC6H_data, F16EndPoints[modes], quantEndPoints[modes],
                                                            max_subsets, ModeFitOrder[modes]);
        fits[modes]       = endpts_fit(F16EndPoints[modes], quantEndPoints[modes], ModeFitOrder[modes], max_subsets, enc->m_isSigned);
        if (fits[modes] && transformfit)
        {
            numfits++;

            // The new compressed end points fit the mode
            // recalculate the error for this mode with a new set of indices
            // since we have shifted the end points from what we origially calc
            // from the find_bestpattern
            float uncompressed[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];
            if (BC6H_data->region == 1)
                decompress_endpoints1(BC6H_data, quantEndPoints[modes], uncompressed, ModeFitOrder[modes]);
            else
                decompress_endpoints2(BC6H_data, quantEndPoints[modes], uncompressed, ModeFitOrder[modes]);
            // Takes the end points and creates a pallet of colors
            // based on preset weights along a vector formed by the two end points
            palitizeEndPointsF(BC6H_data, uncompressed);

            // Once we have the pallet - recalculate the optimal indices using the pallet
            // and the original image data stored in BC6H_data->din[]
            if (!enc->m_isSigned)
                ReIndexShapef(BC6H_data, best_Indices[modes]);

            // Calculate the error of the new tile vs the old tile data
            opt_toterr[modes] = CalcShapeError(BC6H_data, uncompressed, true);

            if (BC6H_data->region == 1)
            {
                endPointErr = CalcOneRegionEndPtsError(BC6H_data, uncompressed, best_Indices[modes]);
                if (endPointErr < bestEndpointsErr)
                {
                    bestEndpointsErr = endPointErr;
                    bestEndpointMode = modes;
                }
            }

            bool transformFit = true;
            // Save hold this mode fit data if its better than the last one checked.
            if (opt_toterr[modes] < bestError)
            {
                if (!enc->m_isSigned)
                {
                    bc6h_encoder_QuantizeEndPointToF16Prec(enc, uncompressed, F16EndPoints[modes], max_subsets,
                                                           ModePartition[ModeFitOrder[modes]].nbits);
                    bc6h_encoder_SwapIndices(F16EndPoints[modes], best_Indices[modes], BC6H_data->entryCount, max_subsets,
                                             ModeFitOrder[modes], BC6H_data->d_shape_index);
                    transformFit = bc6h_encoder_TransformEndPoints(BC6H_data, F16EndPoints[modes],
                                                                   quantEndPoints[modes], max_subsets, ModeFitOrder[modes]);
                }
                if (transformFit)
                {
                    if (BC6H_data->region == 1)
                    {
                        bestFit = (modes == bestEndpointMode) ? modes : ((modes < bestEndpointMode) ? modes : bestEndpointMode);
                    }
                    else
                    {
                        bestFit = modes;
                    }
                    bestError = opt_toterr[bestFit];
                    error     = bestError;
                }
            }
        }
    }

    if (numfits > 0)
    {
        bc6h_encoder_SaveCompressedBlockData(BC6H_data, quantEndPoints[bestFit], best_Indices[bestFit],
                                             max_subsets, ModeFitOrder[bestFit]);
        return error;
    }

    // Should not get here!
    return error;
}

//==================================================================================
// CompressBlock
// in[]  is half float32 data  [0..1] for unsigned and [-1..+1] for signed
// it will be converted to 16 bit half CMP_HALFSHORT (short with signed component) for processing
//
// out is 128 bits BC6H Encoded data
//==================================================================================

//#define DEBUG_A_BLOCK
#ifdef DEBUG_A_BLOCK
float Testdin[MAX_SUBSET_SIZE][MAX_DIMENSION_BIG] = {
    {29440.0000, 29440.0000, 30255.0000, 0.000000000},
    {29440.0000, 29440.0000, 30123.0000, 0.000000000},
    {29440.0000, 29440.0000, 29440.0000, 0.000000000},
    {29440.0000, 29440.0000, 29440.0000, 0.000000000},
    {29440.0000, 29440.0000, 30251.0000, 0.000000000},
    {29440.0000, 29440.0000, 30105.0000, 0.000000000},
    {29440.0000, 29440.0000, 29440.0000, 0.000000000},
    {29440.0000, 29440.0000, 29440.0000, 0.000000000},
    {29440.0000, 29440.0000, 30246.0000, 0.000000000},
    {29440.0000, 29440.0000, 30086.0000, 0.000000000},
    {29440.0000, 29440.0000, 29440.0000, 0.000000000},
    {29440.0000, 29440.0000, 29440.0000, 0.000000000},
    {29440.0000, 29440.0000, 30240.0000, 0.000000000},
    {29440.0000, 29440.0000, 30047.0000, 0.000000000},
    {29440.0000, 29440.0000, 29440.0000, 0.000000000},
    {29440.0000, 29440.0000, 29440.0000, 0.000000000},
};
#endif

static const BYTE Cmp_Red_Block[16] = {
    0xc2, 0x7b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xe0, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00
};

static float MapToF16(float f, bool isSigned)
{
    const float normalization = 1.0;  // For future use

    float result = 0.0f;

    // negative infinity for signed formats
    if (isSigned && isinf(f) && f < 0.0f)
    {
        result = F16_MAX_NEGATIVE_BITS;
    }
    // positive infinity
    else if (isinf(f) && f > 0.0f)
    {
        result = F16_MAX_BITS;
    }
    // NaN, or negative infinity for non-signed formats
    else if (isnan(f) || isinf(f))
    {
        result = 0.0f;
    }
    // signed number is very close to zero
    else if (isSigned && f > -0.00001f && f < 0.00001f)
    {
        result = 0.0f;
    }
    // non-signed number is negative or very close to zero
    else if (!isSigned && f < 0.00001f)
    {
        result = 0.0f;
    }
    else if (isSigned && f < 0.0f)
    {
        result = -(float)bc6h_float_to_half(fabsf(f / normalization));
    }
    else
    {
        result = (float)bc6h_float_to_half(f / normalization);
    }

    return result;
}

static float bc6h_encoder_CompressBlock(BC6HBlockEncoder *enc,
                                        float in[MAX_SUBSET_SIZE][MAX_DIMENSION_BIG],
                                        BYTE out[COMPRESSED_BLOCK_SIZE])
{
    /* Reserved feature:
    float smono[16];
    float R,G,B;
    float smin = FLT_MAX;
    */

#ifdef DEBUG_PATTERNS
    srand(100);
    // Save delta image to file
    fi = fopen("deltaImages.txt", "w");
#endif

    float bestError = FLT_MAX;
    float error     = FLT_MAX;
    int   bestShape = 0;

    AMD_BC6H_Format bc6h_data;
    AMD_BC6H_Format *BC6H_data = &bc6h_data;

    memset(BC6H_data, 0, sizeof(*BC6H_data));

    for (int i = 0; i < BC6H_MAX_SUBSET_SIZE; i++)
    {
        BC6H_data->din[i][0] = MapToF16(in[i][0], enc->m_isSigned);
        BC6H_data->din[i][1] = MapToF16(in[i][1], enc->m_isSigned);
        BC6H_data->din[i][2] = MapToF16(in[i][2], enc->m_isSigned);
        BC6H_data->din[i][3] = 0.0f;
    }

    BC6H_data->issigned = enc->m_isSigned;
#ifdef DEBUG_A_BLOCK
    // Used for debugging blocks!
    for (int i = 0; i < BC6H_MAX_SUBSET_SIZE; i++)
    {
        BC6H_data->din[i][0] = Testdin[i][0];
        BC6H_data->din[i][1] = Testdin[i][1];
        BC6H_data->din[i][2] = Testdin[i][2];
        BC6H_data->din[i][3] = Testdin[i][3];
    }
#endif

    if (enc->m_useMonoShapePatterns)
    {
        /*
        Reserved Feature MONOSHAPE_PATTERNS
        */
    }

    // run through no partition first
    error = bc6h_encoder_FindBestPattern(enc, BC6H_data, false, 0);
    if (error < bestError)
    {
        bestError = error;
        bestShape = -1;
        memcpy(BC6H_data->cur_best_shape_indices, BC6H_data->shape_indices, sizeof(BC6H_data->shape_indices));
        memcpy(BC6H_data->cur_best_partition, BC6H_data->partition, sizeof(BC6H_data->partition));
        memcpy(BC6H_data->cur_best_fEndPoints, BC6H_data->fEndPoints, sizeof(BC6H_data->fEndPoints));
        memcpy(BC6H_data->cur_best_entryCount, BC6H_data->entryCount, sizeof(BC6H_data->entryCount));
        BC6H_data->d_shape_index = bestShape;
    }

    // now run through all two regions shapes to find the best pattern
    for (int shape = 0; shape < MAX_BC6H_PARTITIONS; shape++)
    {
        error = bc6h_encoder_FindBestPattern(enc, BC6H_data, true, shape);
        if (error < bestError)
        {
            bestError = error;
            bestShape = shape;

            memcpy(BC6H_data->cur_best_shape_indices, BC6H_data->shape_indices, sizeof(BC6H_data->shape_indices));
            memcpy(BC6H_data->cur_best_partition, BC6H_data->partition, sizeof(BC6H_data->partition));
            memcpy(BC6H_data->cur_best_fEndPoints, BC6H_data->fEndPoints, sizeof(BC6H_data->fEndPoints));
            memcpy(BC6H_data->cur_best_entryCount, BC6H_data->entryCount, sizeof(BC6H_data->entryCount));
            BC6H_data->d_shape_index = bestShape;
        }
        else
        {
            if (bestShape != -1)
            {
                BC6H_data->d_shape_index = bestShape;
                memcpy(BC6H_data->shape_indices, BC6H_data->cur_best_shape_indices, sizeof(BC6H_data->shape_indices));
                memcpy(BC6H_data->partition, BC6H_data->cur_best_partition, sizeof(BC6H_data->partition));
                memcpy(BC6H_data->fEndPoints, BC6H_data->cur_best_fEndPoints, sizeof(BC6H_data->fEndPoints));
                memcpy(BC6H_data->entryCount, BC6H_data->cur_best_entryCount, sizeof(BC6H_data->entryCount));
            }
        }
    }

    // Optimize the result for encoding
    bestError = bc6h_encoder_EncodePattern(enc, BC6H_data, bestError);

    // used for debugging modes, set the value you want to debug with
    if (BC6H_data->m_mode != 0) {
        // do final encoding and save to output block
        SaveDataBlock(BC6H_data, out);
    } else {
        memcpy(out, Cmp_Red_Block, 16);
    }

        // do final encoding and save to output block
        // SaveDataBlock(best_BC6H_data,out);

#ifdef DEBUG_PATTERNS
    if (fi)
        fclose(fi);
#endif

    return (float)bestError;
}

/* Input block is packed RGBF16 (RGBRGB...), stride in bytes. */
static int bc6enc_block(uint8_t *dst, ptrdiff_t stride, const uint8_t *block)
{
    BC6HBlockEncoder enc;
    float in[MAX_SUBSET_SIZE][MAX_DIMENSION_BIG];

    if (!bc6h_tables_init)
        bc6h_init_tables();

    memset(&enc, 0, sizeof(enc));
    bc6h_encoder_init(&enc);

    for (int y = 0; y < 4; y++) {
        const uint8_t *row = block + y * stride;

        for (int x = 0; x < 4; x++) {
            const uint8_t *px = row + x * 6;
            uint16_t r, g, b;
            int idx = y * 4 + x;

            memcpy(&r, px, 2);
            memcpy(&g, px + 2, 2);
            memcpy(&b, px + 4, 2);

            in[idx][0] = bc6h_half_to_float(r);
            in[idx][1] = bc6h_half_to_float(g);
            in[idx][2] = bc6h_half_to_float(b);
            in[idx][3] = 0.0f;
        }
    }

    bc6h_encoder_CompressBlock(&enc, in, dst);
    return 16;
}

void ff_bc6enc_init(BC6EncContext *c, int is_signed, uint16_t mode_mask,
                    float exposure, float quality, int use_pattern)
{
    bc6h_params.is_signed = is_signed;
    bc6h_params.mode_mask = mode_mask;
    bc6h_params.exposure = exposure;
    bc6h_params.quality = quality;
    bc6h_params.use_pattern = use_pattern;

    bc6h_init_tables();
    init_ramps();

    c->bc6enc_block = bc6enc_block;
}
