/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/common/x86/av1_inv_txfm_avx2.h"

#include <assert.h>

#include "config/aom_config.h"

#include "config/av1_rtcd.h"

#include "av1/common/av1_inv_txfm1d_cfg.h"
#include "av1/common/av1_inv_txfm2d.h"
#include "av1/common/x86/av1_txfm_sse2.h"
#include "av1/common/x86/av1_inv_txfm_ssse3.h"

// TODO(venkatsanampudi@ittiam.com): move this to header file

// Sqrt2, Sqrt2^2, Sqrt2^3, Sqrt2^4, Sqrt2^5
static const int32_t NewSqrt2list[TX_SIZES] = { 5793, 2 * 4096, 2 * 5793,
                                                4 * 4096, 4 * 5793 };

static void idct16_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));
  __m256i x1[16];

  // Odd half x1[8..15]: stages 1..6 (only 8 state regs + 3 temp regs live)
  x1[8] = input[1];
  x1[9] = input[9];
  x1[10] = input[5];
  x1[11] = input[13];
  x1[12] = input[3];
  x1[13] = input[11];
  x1[14] = input[7];
  x1[15] = input[15];
  {
    const __m256i cospi_p60_m04 = pair_set_w16_epi16(cospi[60], -cospi[4]);
    const __m256i cospi_p04_p60 = pair_set_w16_epi16(cospi[4], cospi[60]);
    btf_16_w16_avx2(cospi_p60_m04, cospi_p04_p60, &x1[8], &x1[15], _r,
                    INV_COS_BIT);
  }
  {
    const __m256i cospi_p28_m36 = pair_set_w16_epi16(cospi[28], -cospi[36]);
    const __m256i cospi_p36_p28 = pair_set_w16_epi16(cospi[36], cospi[28]);
    btf_16_w16_avx2(cospi_p28_m36, cospi_p36_p28, &x1[9], &x1[14], _r,
                    INV_COS_BIT);
  }
  {
    const __m256i cospi_p44_m20 = pair_set_w16_epi16(cospi[44], -cospi[20]);
    const __m256i cospi_p20_p44 = pair_set_w16_epi16(cospi[20], cospi[44]);
    btf_16_w16_avx2(cospi_p44_m20, cospi_p20_p44, &x1[10], &x1[13], _r,
                    INV_COS_BIT);
  }
  {
    const __m256i cospi_p12_m52 = pair_set_w16_epi16(cospi[12], -cospi[52]);
    const __m256i cospi_p52_p12 = pair_set_w16_epi16(cospi[52], cospi[12]);
    btf_16_w16_avx2(cospi_p12_m52, cospi_p52_p12, &x1[11], &x1[12], _r,
                    INV_COS_BIT);
  }
  btf_16_adds_subs_avx2(&x1[8], &x1[9]);
  btf_16_adds_subs_avx2(&x1[11], &x1[10]);
  btf_16_adds_subs_avx2(&x1[12], &x1[13]);
  btf_16_adds_subs_avx2(&x1[15], &x1[14]);
  {
    const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
    const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
    const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
    btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x1[9], &x1[14], _r,
                    INV_COS_BIT);
    btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x1[10], &x1[13], _r,
                    INV_COS_BIT);
  }
  btf_16_adds_subs_avx2(&x1[8], &x1[11]);
  btf_16_adds_subs_avx2(&x1[9], &x1[10]);
  btf_16_adds_subs_avx2(&x1[15], &x1[12]);
  btf_16_adds_subs_avx2(&x1[14], &x1[13]);
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[10], &x1[13], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[11], &x1[12], _r,
                  INV_COS_BIT);
  output[9] = x1[8];
  output[11] = x1[9];
  output[13] = x1[14];
  output[15] = x1[15];

  // Even half x1[4..7]: stages 1..5
  x1[4] = input[2];
  x1[5] = input[10];
  x1[6] = input[6];
  x1[7] = input[14];
  {
    const __m256i cospi_p56_m08 = pair_set_w16_epi16(cospi[56], -cospi[8]);
    const __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
    btf_16_w16_avx2(cospi_p56_m08, cospi_p08_p56, &x1[4], &x1[7], _r,
                    INV_COS_BIT);
  }
  {
    const __m256i cospi_p24_m40 = pair_set_w16_epi16(cospi[24], -cospi[40]);
    const __m256i cospi_p40_p24 = pair_set_w16_epi16(cospi[40], cospi[24]);
    btf_16_w16_avx2(cospi_p24_m40, cospi_p40_p24, &x1[5], &x1[6], _r,
                    INV_COS_BIT);
  }
  btf_16_adds_subs_avx2(&x1[4], &x1[5]);
  btf_16_adds_subs_avx2(&x1[7], &x1[6]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[5], &x1[6], _r,
                  INV_COS_BIT);

  // Even half x1[0..3]: stages 1..5, then combine 0..7 and 0..15
  x1[0] = input[0];
  x1[1] = input[8];
  {
    const __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x1[0], &x1[1], _r,
                    INV_COS_BIT);
  }
  x1[2] = input[4];
  x1[3] = input[12];
  {
    const __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
    const __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
    btf_16_w16_avx2(cospi_p48_m16, cospi_p16_p48, &x1[2], &x1[3], _r,
                    INV_COS_BIT);
  }
  btf_16_adds_subs_avx2(&x1[0], &x1[3]);
  btf_16_adds_subs_avx2(&x1[1], &x1[2]);
  btf_16_adds_subs_avx2(&x1[0], &x1[7]);
  btf_16_adds_subs_avx2(&x1[1], &x1[6]);
  btf_16_adds_subs_avx2(&x1[2], &x1[5]);
  btf_16_adds_subs_avx2(&x1[3], &x1[4]);
  btf_16_adds_subs_out_avx2(&output[0], &output[15], x1[0], output[15]);
  btf_16_adds_subs_out_avx2(&output[1], &output[14], x1[1], output[13]);
  btf_16_adds_subs_out_avx2(&output[7], &output[8], x1[7], output[9]);
  btf_16_adds_subs_out_avx2(&output[6], &output[9], x1[6], output[11]);
  btf_16_adds_subs_out_avx2(&output[2], &output[13], x1[2], x1[13]);
  btf_16_adds_subs_out_avx2(&output[3], &output[12], x1[3], x1[12]);
  btf_16_adds_subs_out_avx2(&output[4], &output[11], x1[4], x1[11]);
  btf_16_adds_subs_out_avx2(&output[5], &output[10], x1[5], x1[10]);
}

static void idct16_low8_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));
  __m256i x1[16];

  // Odd half x1[8..15]: stages 1..6 (only 8 state regs live)
  x1[8] = input[1];
  x1[10] = input[5];
  x1[12] = input[3];
  x1[14] = input[7];
  btf_16_w16_0_avx2(cospi[60], cospi[4], x1[8], x1[8], x1[15]);
  btf_16_w16_0_avx2(-cospi[36], cospi[28], x1[14], x1[9], x1[14]);
  btf_16_w16_0_avx2(cospi[44], cospi[20], x1[10], x1[10], x1[13]);
  btf_16_w16_0_avx2(-cospi[52], cospi[12], x1[12], x1[11], x1[12]);
  btf_16_adds_subs_avx2(&x1[8], &x1[9]);
  btf_16_adds_subs_avx2(&x1[11], &x1[10]);
  btf_16_adds_subs_avx2(&x1[12], &x1[13]);
  btf_16_adds_subs_avx2(&x1[15], &x1[14]);
  {
    const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
    const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
    const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
    btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x1[9], &x1[14], _r,
                    INV_COS_BIT);
    btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x1[10], &x1[13], _r,
                    INV_COS_BIT);
  }
  btf_16_adds_subs_avx2(&x1[8], &x1[11]);
  btf_16_adds_subs_avx2(&x1[9], &x1[10]);
  btf_16_adds_subs_avx2(&x1[15], &x1[12]);
  btf_16_adds_subs_avx2(&x1[14], &x1[13]);
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[10], &x1[13], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[11], &x1[12], _r,
                  INV_COS_BIT);
  output[9] = x1[8];

  // Even half x1[4..7]: stages 1..5
  x1[4] = input[2];
  x1[6] = input[6];
  btf_16_w16_0_avx2(cospi[56], cospi[8], x1[4], x1[4], x1[7]);
  btf_16_w16_0_avx2(-cospi[40], cospi[24], x1[6], x1[5], x1[6]);
  btf_16_adds_subs_avx2(&x1[4], &x1[5]);
  btf_16_adds_subs_avx2(&x1[7], &x1[6]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[5], &x1[6], _r,
                  INV_COS_BIT);

  // Even half x1[0..3]: stages 1..5, then combine 0..7 and 0..15
  x1[0] = input[0];
  x1[2] = input[4];
  btf_16_w16_0_avx2(cospi[32], cospi[32], x1[0], x1[0], x1[1]);
  btf_16_w16_0_avx2(cospi[48], cospi[16], x1[2], x1[2], x1[3]);
  btf_16_adds_subs_avx2(&x1[0], &x1[3]);
  btf_16_adds_subs_avx2(&x1[1], &x1[2]);
  btf_16_adds_subs_avx2(&x1[0], &x1[7]);
  btf_16_adds_subs_avx2(&x1[1], &x1[6]);
  btf_16_adds_subs_avx2(&x1[2], &x1[5]);
  btf_16_adds_subs_avx2(&x1[3], &x1[4]);
  btf_16_adds_subs_out_avx2(&output[7], &output[8], x1[7], output[9]);
  btf_16_adds_subs_out_avx2(&output[0], &output[15], x1[0], x1[15]);
  btf_16_adds_subs_out_avx2(&output[1], &output[14], x1[1], x1[14]);
  btf_16_adds_subs_out_avx2(&output[2], &output[13], x1[2], x1[13]);
  btf_16_adds_subs_out_avx2(&output[3], &output[12], x1[3], x1[12]);
  btf_16_adds_subs_out_avx2(&output[4], &output[11], x1[4], x1[11]);
  btf_16_adds_subs_out_avx2(&output[5], &output[10], x1[5], x1[10]);
  btf_16_adds_subs_out_avx2(&output[6], &output[9], x1[6], x1[9]);
}

static void idct16_low1_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i w = _mm256_set1_epi16(cospi[32] * 8);
  const __m256i x = _mm256_mulhrs_epi16(input[0], w);
  for (int i = 0; i < 16; ++i) {
    output[i] = x;
  }
}

static inline void iadst16_stage4_avx2(__m256i *x, const int32_t *cospi,
                                       const __m256i _r, int8_t cos_bit) {
  {
    const __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
    const __m256i cospi_p56_m08 = pair_set_w16_epi16(cospi[56], -cospi[8]);
    btf_16_w16_avx2(cospi_p08_p56, cospi_p56_m08, &x[8], &x[9], _r, cos_bit);
    const __m256i cospi_m56_p08 = pair_set_w16_epi16(-cospi[56], cospi[8]);
    btf_16_w16_avx2(cospi_m56_p08, cospi_p08_p56, &x[12], &x[13], _r, cos_bit);
  }
  {
    const __m256i cospi_p40_p24 = pair_set_w16_epi16(cospi[40], cospi[24]);
    const __m256i cospi_p24_m40 = pair_set_w16_epi16(cospi[24], -cospi[40]);
    btf_16_w16_avx2(cospi_p40_p24, cospi_p24_m40, &x[10], &x[11], _r, cos_bit);
    const __m256i cospi_m24_p40 = pair_set_w16_epi16(-cospi[24], cospi[40]);
    btf_16_w16_avx2(cospi_m24_p40, cospi_p40_p24, &x[14], &x[15], _r, cos_bit);
  }
}

static inline void iadst16_stage8_avx2(__m256i *x1, const int32_t *cospi,
                                       const __m256i _r, int8_t cos_bit) {
  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  const __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x1[2], &x1[3], _r, cos_bit);
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x1[6], &x1[7], _r, cos_bit);
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x1[10], &x1[11], _r, cos_bit);
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x1[14], &x1[15], _r, cos_bit);
}

static inline void iadst16_stage9_avx2(__m256i *output, __m256i *x1) {
  const __m256i __zero = _mm256_setzero_si256();
  output[0] = x1[0];
  output[1] = _mm256_subs_epi16(__zero, x1[8]);
  output[2] = x1[12];
  output[3] = _mm256_subs_epi16(__zero, x1[4]);
  output[4] = x1[6];
  output[5] = _mm256_subs_epi16(__zero, x1[14]);
  output[6] = x1[10];
  output[7] = _mm256_subs_epi16(__zero, x1[2]);
  output[8] = x1[3];
  output[9] = _mm256_subs_epi16(__zero, x1[11]);
  output[10] = x1[15];
  output[11] = _mm256_subs_epi16(__zero, x1[7]);
  output[12] = x1[5];
  output[13] = _mm256_subs_epi16(__zero, x1[13]);
  output[14] = x1[9];
  output[15] = _mm256_subs_epi16(__zero, x1[1]);
}

// Finishes stages 4..9 of iadst16 by splitting the 16 state vectors after
// stage 3 into two independent 8-register halves (x[8..15] and x[0..7]),
// parking x[0..7] in output[0,15,7,8,3,12,4,11] so 0 stack bytes are used.
static AOM_FORCE_INLINE void iadst16_stages4_to_9_split_avx2(
    __m256i *x1, __m256i *output, const int32_t *cospi, const __m256i _r) {
  // Upper half x1[8..15]: stages 4, 5, 6, 7, 8, 9
  iadst16_stage4_avx2(x1, cospi, _r, INV_COS_BIT);
  btf_16_adds_subs_avx2(&x1[8], &x1[12]);
  btf_16_adds_subs_avx2(&x1[9], &x1[13]);
  btf_16_adds_subs_avx2(&x1[10], &x1[14]);
  btf_16_adds_subs_avx2(&x1[11], &x1[15]);
  {
    const __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
    const __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
    btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, &x1[12], &x1[13], _r,
                    INV_COS_BIT);
    const __m256i cospi_m48_p16 = pair_set_w16_epi16(-cospi[48], cospi[16]);
    btf_16_w16_avx2(cospi_m48_p16, cospi_p16_p48, &x1[14], &x1[15], _r,
                    INV_COS_BIT);
  }
  btf_16_adds_subs_avx2(&x1[8], &x1[10]);
  btf_16_adds_subs_avx2(&x1[9], &x1[11]);
  btf_16_adds_subs_avx2(&x1[12], &x1[14]);
  btf_16_adds_subs_avx2(&x1[13], &x1[15]);
  {
    const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
    const __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x1[10], &x1[11], _r,
                    INV_COS_BIT);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x1[14], &x1[15], _r,
                    INV_COS_BIT);
  }
  {
    const __m256i __zero = _mm256_setzero_si256();
    output[1] = _mm256_subs_epi16(__zero, x1[8]);
    output[2] = x1[12];
    output[5] = _mm256_subs_epi16(__zero, x1[14]);
    output[6] = x1[10];
    output[9] = _mm256_subs_epi16(__zero, x1[11]);
    output[10] = x1[15];
    output[13] = _mm256_subs_epi16(__zero, x1[13]);
    output[14] = x1[9];
  }

  // Lower half y0..y7: stages 5, 6, 7, 8, 9
  __m256i y0 = output[0], y1 = output[15], y2 = output[7], y3 = output[8];
  __m256i y4 = output[3], y5 = output[12], y6 = output[4], y7 = output[11];
  btf_16_adds_subs_avx2(&y0, &y4);
  btf_16_adds_subs_avx2(&y1, &y5);
  btf_16_adds_subs_avx2(&y2, &y6);
  btf_16_adds_subs_avx2(&y3, &y7);
  {
    const __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
    const __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
    btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, &y4, &y5, _r, INV_COS_BIT);
    const __m256i cospi_m48_p16 = pair_set_w16_epi16(-cospi[48], cospi[16]);
    btf_16_w16_avx2(cospi_m48_p16, cospi_p16_p48, &y6, &y7, _r, INV_COS_BIT);
  }
  btf_16_adds_subs_avx2(&y0, &y2);
  btf_16_adds_subs_avx2(&y1, &y3);
  btf_16_adds_subs_avx2(&y4, &y6);
  btf_16_adds_subs_avx2(&y5, &y7);
  {
    const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
    const __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &y2, &y3, _r, INV_COS_BIT);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &y6, &y7, _r, INV_COS_BIT);
  }
  const __m256i __zero = _mm256_setzero_si256();
  output[0] = y0;
  output[3] = _mm256_subs_epi16(__zero, y4);
  output[4] = y6;
  output[7] = _mm256_subs_epi16(__zero, y2);
  output[8] = y3;
  output[11] = _mm256_subs_epi16(__zero, y7);
  output[12] = y5;
  output[15] = _mm256_subs_epi16(__zero, y1);
}

static void iadst16_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  // stages 1, 2, & 3: compute each (k, k+8) pair, park x_lo in output,
  // and keep only x1[8..15] in registers.
  __m256i x1[16];
  {
    __m256i x0 = input[15], x1_0 = input[0];
    const __m256i cospi_p02_p62 = pair_set_w16_epi16(cospi[2], cospi[62]);
    const __m256i cospi_p62_m02 = pair_set_w16_epi16(cospi[62], -cospi[2]);
    btf_16_w16_avx2(cospi_p02_p62, cospi_p62_m02, &x0, &x1_0, _r, INV_COS_BIT);
    x1[8] = input[7];
    x1[9] = input[8];
    const __m256i cospi_p34_p30 = pair_set_w16_epi16(cospi[34], cospi[30]);
    const __m256i cospi_p30_m34 = pair_set_w16_epi16(cospi[30], -cospi[34]);
    btf_16_w16_avx2(cospi_p34_p30, cospi_p30_m34, &x1[8], &x1[9], _r,
                    INV_COS_BIT);
    btf_16_adds_subs_avx2(&x0, &x1[8]);
    btf_16_adds_subs_avx2(&x1_0, &x1[9]);
    output[0] = x0;
    output[15] = x1_0;
  }
  {
    __m256i x2 = input[13], x3 = input[2];
    const __m256i cospi_p10_p54 = pair_set_w16_epi16(cospi[10], cospi[54]);
    const __m256i cospi_p54_m10 = pair_set_w16_epi16(cospi[54], -cospi[10]);
    btf_16_w16_avx2(cospi_p10_p54, cospi_p54_m10, &x2, &x3, _r, INV_COS_BIT);
    x1[10] = input[5];
    x1[11] = input[10];
    const __m256i cospi_p42_p22 = pair_set_w16_epi16(cospi[42], cospi[22]);
    const __m256i cospi_p22_m42 = pair_set_w16_epi16(cospi[22], -cospi[42]);
    btf_16_w16_avx2(cospi_p42_p22, cospi_p22_m42, &x1[10], &x1[11], _r,
                    INV_COS_BIT);
    btf_16_adds_subs_avx2(&x2, &x1[10]);
    btf_16_adds_subs_avx2(&x3, &x1[11]);
    output[7] = x2;
    output[8] = x3;
  }
  {
    __m256i x4 = input[11], x5 = input[4];
    const __m256i cospi_p18_p46 = pair_set_w16_epi16(cospi[18], cospi[46]);
    const __m256i cospi_p46_m18 = pair_set_w16_epi16(cospi[46], -cospi[18]);
    btf_16_w16_avx2(cospi_p18_p46, cospi_p46_m18, &x4, &x5, _r, INV_COS_BIT);
    x1[12] = input[3];
    x1[13] = input[12];
    const __m256i cospi_p50_p14 = pair_set_w16_epi16(cospi[50], cospi[14]);
    const __m256i cospi_p14_m50 = pair_set_w16_epi16(cospi[14], -cospi[50]);
    btf_16_w16_avx2(cospi_p50_p14, cospi_p14_m50, &x1[12], &x1[13], _r,
                    INV_COS_BIT);
    btf_16_adds_subs_avx2(&x4, &x1[12]);
    btf_16_adds_subs_avx2(&x5, &x1[13]);
    output[3] = x4;
    output[12] = x5;
  }
  {
    __m256i x6 = input[9], x7 = input[6];
    const __m256i cospi_p26_p38 = pair_set_w16_epi16(cospi[26], cospi[38]);
    const __m256i cospi_p38_m26 = pair_set_w16_epi16(cospi[38], -cospi[26]);
    btf_16_w16_avx2(cospi_p26_p38, cospi_p38_m26, &x6, &x7, _r, INV_COS_BIT);
    x1[14] = input[1];
    x1[15] = input[14];
    const __m256i cospi_p58_p06 = pair_set_w16_epi16(cospi[58], cospi[6]);
    const __m256i cospi_p06_m58 = pair_set_w16_epi16(cospi[6], -cospi[58]);
    btf_16_w16_avx2(cospi_p58_p06, cospi_p06_m58, &x1[14], &x1[15], _r,
                    INV_COS_BIT);
    btf_16_adds_subs_avx2(&x6, &x1[14]);
    btf_16_adds_subs_avx2(&x7, &x1[15]);
    output[4] = x6;
    output[11] = x7;
  }
  iadst16_stages4_to_9_split_avx2(x1, output, cospi, _r);
}

static void iadst16_low8_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  __m256i x1[16];
  {
    __m256i x0, x1_0 = input[0];
    btf_16_w16_0_avx2(cospi[62], -cospi[2], x1_0, x0, x1_0);
    x1[8] = input[7];
    btf_16_w16_0_avx2(cospi[34], cospi[30], x1[8], x1[8], x1[9]);
    btf_16_adds_subs_avx2(&x0, &x1[8]);
    btf_16_adds_subs_avx2(&x1_0, &x1[9]);
    output[0] = x0;
    output[15] = x1_0;
  }
  {
    __m256i x2, x3 = input[2];
    btf_16_w16_0_avx2(cospi[54], -cospi[10], x3, x2, x3);
    x1[10] = input[5];
    btf_16_w16_0_avx2(cospi[42], cospi[22], x1[10], x1[10], x1[11]);
    btf_16_adds_subs_avx2(&x2, &x1[10]);
    btf_16_adds_subs_avx2(&x3, &x1[11]);
    output[7] = x2;
    output[8] = x3;
  }
  {
    __m256i x4, x5 = input[4];
    btf_16_w16_0_avx2(cospi[46], -cospi[18], x5, x4, x5);
    x1[12] = input[3];
    btf_16_w16_0_avx2(cospi[50], cospi[14], x1[12], x1[12], x1[13]);
    btf_16_adds_subs_avx2(&x4, &x1[12]);
    btf_16_adds_subs_avx2(&x5, &x1[13]);
    output[3] = x4;
    output[12] = x5;
  }
  {
    __m256i x6, x7 = input[6];
    btf_16_w16_0_avx2(cospi[38], -cospi[26], x7, x6, x7);
    x1[14] = input[1];
    btf_16_w16_0_avx2(cospi[58], cospi[06], x1[14], x1[14], x1[15]);
    btf_16_adds_subs_avx2(&x6, &x1[14]);
    btf_16_adds_subs_avx2(&x7, &x1[15]);
    output[4] = x6;
    output[11] = x7;
  }
  iadst16_stages4_to_9_split_avx2(x1, output, cospi, _r);
}

static void iadst16_low1_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
  const __m256i cospi_p56_m08 = pair_set_w16_epi16(cospi[56], -cospi[8]);
  const __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
  const __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);

  // stage 1
  __m256i x1[16];
  x1[1] = input[0];

  // stage 2
  btf_16_w16_0_avx2(cospi[62], -cospi[2], x1[1], x1[0], x1[1]);

  // stage 3
  x1[8] = x1[0];
  x1[9] = x1[1];

  // stage 4
  btf_16_w16_avx2(cospi_p08_p56, cospi_p56_m08, &x1[8], &x1[9], _r,
                  INV_COS_BIT);

  // stage 5
  x1[4] = x1[0];
  x1[5] = x1[1];

  x1[12] = x1[8];
  x1[13] = x1[9];

  // stage 6
  btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, &x1[4], &x1[5], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, &x1[12], &x1[13], _r,
                  INV_COS_BIT);

  // stage 7
  x1[2] = x1[0];
  x1[3] = x1[1];
  x1[6] = x1[4];
  x1[7] = x1[5];
  x1[10] = x1[8];
  x1[11] = x1[9];
  x1[14] = x1[12];
  x1[15] = x1[13];

  iadst16_stage8_avx2(x1, cospi, _r, INV_COS_BIT);
  iadst16_stage9_avx2(output, x1);
}

static void idct32_low1_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i w = _mm256_set1_epi16(cospi[32] * 8);
  const __m256i x = _mm256_mulhrs_epi16(input[0], w);
  for (int i = 0; i < 32; ++i) {
    output[i] = x;
  }
}

static inline void idct32_odd_stage7_to_9_avx2(
    __m256i x16, __m256i x17, __m256i x18, __m256i x19, __m256i x28,
    __m256i x29, __m256i x30, __m256i x31, const __m256i *x_g2,
    const __m256i *x_even, __m256i *output, const int32_t *cospi,
    const __m256i _r) {
  __m256i x20 = x_g2[0], x21 = x_g2[1], x22 = x_g2[2], x23 = x_g2[3];
  btf_16_adds_subs_avx2(&x16, &x23);
  btf_16_adds_subs_avx2(&x17, &x22);
  btf_16_adds_subs_avx2(&x18, &x21);
  btf_16_adds_subs_avx2(&x19, &x20);
  btf_16_adds_subs_out_avx2(&output[15], &output[16], x_even[15], x16);
  btf_16_adds_subs_out_avx2(&output[14], &output[17], x_even[14], x17);
  btf_16_adds_subs_out_avx2(&output[13], &output[18], x_even[13], x18);
  btf_16_adds_subs_out_avx2(&output[12], &output[19], x_even[12], x19);

  __m256i x24 = x_g2[4], x25 = x_g2[5], x26 = x_g2[6], x27 = x_g2[7];
  btf_16_adds_subs_avx2(&x31, &x24);
  btf_16_adds_subs_avx2(&x30, &x25);
  btf_16_adds_subs_avx2(&x29, &x26);
  btf_16_adds_subs_avx2(&x28, &x27);
  btf_16_adds_subs_out_avx2(&output[0], &output[31], x_even[0], x31);
  btf_16_adds_subs_out_avx2(&output[1], &output[30], x_even[1], x30);
  btf_16_adds_subs_out_avx2(&output[2], &output[29], x_even[2], x29);
  btf_16_adds_subs_out_avx2(&output[3], &output[28], x_even[3], x28);

  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x20, &x27, _r, INV_COS_BIT);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x21, &x26, _r, INV_COS_BIT);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x22, &x25, _r, INV_COS_BIT);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x23, &x24, _r, INV_COS_BIT);

  btf_16_adds_subs_out_avx2(&output[4], &output[27], x_even[4], x27);
  btf_16_adds_subs_out_avx2(&output[5], &output[26], x_even[5], x26);
  btf_16_adds_subs_out_avx2(&output[6], &output[25], x_even[6], x25);
  btf_16_adds_subs_out_avx2(&output[7], &output[24], x_even[7], x24);
  btf_16_adds_subs_out_avx2(&output[8], &output[23], x_even[8], x23);
  btf_16_adds_subs_out_avx2(&output[9], &output[22], x_even[9], x22);
  btf_16_adds_subs_out_avx2(&output[10], &output[21], x_even[10], x21);
  btf_16_adds_subs_out_avx2(&output[11], &output[20], x_even[11], x20);
}

static void idct32_low8_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));
  __m256i x_even[16];
  __m256i x_g2[8];

  // Even half (stages 1..8 on input[0, 2, 4, 6])
  {
    __m256i x8 = input[2];
    __m256i x12 = input[6];
    __m256i x11, x15;
    btf_16_w16_0_avx2(cospi[60], cospi[4], x8, x8, x15);
    btf_16_w16_0_avx2(-cospi[52], cospi[12], x12, x11, x12);
    __m256i x9 = x8, x10 = x11, x13 = x12, x14 = x15;
    {
      const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
      const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
      const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
      btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x9, &x14, _r, INV_COS_BIT);
      btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x10, &x13, _r,
                      INV_COS_BIT);
    }
    btf_16_adds_subs_avx2(&x8, &x11);
    btf_16_adds_subs_avx2(&x9, &x10);
    btf_16_adds_subs_avx2(&x15, &x12);
    btf_16_adds_subs_avx2(&x14, &x13);

    const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
    const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x10, &x13, _r, INV_COS_BIT);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x11, &x12, _r, INV_COS_BIT);

    __m256i x0 = input[0];
    __m256i x4 = input[4];
    __m256i x1, x7;
    btf_16_w16_0_avx2(cospi[56], cospi[8], x4, x4, x7);
    btf_16_w16_0_avx2(cospi[32], cospi[32], x0, x0, x1);
    __m256i x5 = x4, x6 = x7;
    __m256i x3 = x0, x2 = x1;
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x5, &x6, _r, INV_COS_BIT);
    btf_16_adds_subs_avx2(&x0, &x7);
    btf_16_adds_subs_avx2(&x1, &x6);
    btf_16_adds_subs_avx2(&x2, &x5);
    btf_16_adds_subs_avx2(&x3, &x4);

    btf_16_adds_subs_out_avx2(&x_even[0], &x_even[15], x0, x15);
    btf_16_adds_subs_out_avx2(&x_even[1], &x_even[14], x1, x14);
    btf_16_adds_subs_out_avx2(&x_even[2], &x_even[13], x2, x13);
    btf_16_adds_subs_out_avx2(&x_even[3], &x_even[12], x3, x12);
    btf_16_adds_subs_out_avx2(&x_even[4], &x_even[11], x4, x11);
    btf_16_adds_subs_out_avx2(&x_even[5], &x_even[10], x5, x10);
    btf_16_adds_subs_out_avx2(&x_even[6], &x_even[9], x6, x9);
    btf_16_adds_subs_out_avx2(&x_even[7], &x_even[8], x7, x8);
  }

  // Odd half group B: x20..x27 (stages 1..6 on input[3, 5])
  {
    __m256i x20 = input[5];
    __m256i x24 = input[3];
    __m256i x23, x27;
    btf_16_w16_0_avx2(cospi[54], cospi[10], x20, x20, x27);
    btf_16_w16_0_avx2(-cospi[58], cospi[6], x24, x23, x24);
    __m256i x21 = x20, x22 = x23, x25 = x24, x26 = x27;
    {
      const __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
      const __m256i cospi_p24_p40 = pair_set_w16_epi16(cospi[24], cospi[40]);
      const __m256i cospi_m24_m40 = pair_set_w16_epi16(-cospi[24], -cospi[40]);
      btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, &x21, &x26, _r,
                      INV_COS_BIT);
      btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, &x22, &x25, _r,
                      INV_COS_BIT);
    }
    btf_16_adds_subs_avx2(&x23, &x20);
    btf_16_adds_subs_avx2(&x22, &x21);
    btf_16_adds_subs_avx2(&x24, &x27);
    btf_16_adds_subs_avx2(&x25, &x26);
    {
      const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
      const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
      btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x20, &x27, _r,
                      INV_COS_BIT);
      btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x21, &x26, _r,
                      INV_COS_BIT);
    }
    x_g2[0] = x20;
    x_g2[1] = x21;
    x_g2[2] = x22;
    x_g2[3] = x23;
    x_g2[4] = x24;
    x_g2[5] = x25;
    x_g2[6] = x26;
    x_g2[7] = x27;
  }

  // Odd half group A: x16..x19, x28..x31 (stages 1..6 on input[1, 7])
  __m256i x16 = input[1];
  __m256i x28 = input[7];
  __m256i x19, x31;
  btf_16_w16_0_avx2(cospi[62], cospi[2], x16, x16, x31);
  btf_16_w16_0_avx2(-cospi[50], cospi[14], x28, x19, x28);
  __m256i x17 = x16, x18 = x19, x29 = x28, x30 = x31;
  {
    const __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
    const __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
    const __m256i cospi_m56_m08 = pair_set_w16_epi16(-cospi[56], -cospi[8]);
    btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, &x17, &x30, _r, INV_COS_BIT);
    btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, &x18, &x29, _r, INV_COS_BIT);
  }
  btf_16_adds_subs_avx2(&x16, &x19);
  btf_16_adds_subs_avx2(&x17, &x18);
  btf_16_adds_subs_avx2(&x31, &x28);
  btf_16_adds_subs_avx2(&x30, &x29);
  {
    const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
    const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
    btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x18, &x29, _r, INV_COS_BIT);
    btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x19, &x28, _r, INV_COS_BIT);
  }

  idct32_odd_stage7_to_9_avx2(x16, x17, x18, x19, x28, x29, x30, x31, x_g2,
                              x_even, output, cospi, _r);
}

static void idct32_low16_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));
  __m256i x_even[16];
  __m256i x_g2[8];

  // Even half (stages 1..8 on input[0, 2, 4, 6, 8, 10, 12, 14])
  {
    __m256i x8 = input[2], x10 = input[10], x12 = input[6], x14 = input[14];
    __m256i x9, x11, x13, x15;
    btf_16_w16_0_avx2(cospi[60], cospi[4], x8, x8, x15);
    btf_16_w16_0_avx2(-cospi[36], cospi[28], x14, x9, x14);
    btf_16_w16_0_avx2(cospi[44], cospi[20], x10, x10, x13);
    btf_16_w16_0_avx2(-cospi[52], cospi[12], x12, x11, x12);
    btf_16_adds_subs_avx2(&x8, &x9);
    btf_16_adds_subs_avx2(&x11, &x10);
    btf_16_adds_subs_avx2(&x12, &x13);
    btf_16_adds_subs_avx2(&x15, &x14);
    {
      const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
      const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
      const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
      btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x9, &x14, _r, INV_COS_BIT);
      btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x10, &x13, _r,
                      INV_COS_BIT);
    }
    btf_16_adds_subs_avx2(&x8, &x11);
    btf_16_adds_subs_avx2(&x9, &x10);
    btf_16_adds_subs_avx2(&x15, &x12);
    btf_16_adds_subs_avx2(&x14, &x13);

    const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
    const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x10, &x13, _r, INV_COS_BIT);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x11, &x12, _r, INV_COS_BIT);

    __m256i x0 = input[0], x2 = input[8], x4 = input[4], x6 = input[12];
    __m256i x1, x3, x5, x7;
    btf_16_w16_0_avx2(cospi[56], cospi[8], x4, x4, x7);
    btf_16_w16_0_avx2(-cospi[40], cospi[24], x6, x5, x6);
    btf_16_w16_0_avx2(cospi[32], cospi[32], x0, x0, x1);
    btf_16_w16_0_avx2(cospi[48], cospi[16], x2, x2, x3);
    btf_16_adds_subs_avx2(&x4, &x5);
    btf_16_adds_subs_avx2(&x7, &x6);
    btf_16_adds_subs_avx2(&x0, &x3);
    btf_16_adds_subs_avx2(&x1, &x2);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x5, &x6, _r, INV_COS_BIT);
    btf_16_adds_subs_avx2(&x0, &x7);
    btf_16_adds_subs_avx2(&x1, &x6);
    btf_16_adds_subs_avx2(&x2, &x5);
    btf_16_adds_subs_avx2(&x3, &x4);

    btf_16_adds_subs_out_avx2(&x_even[0], &x_even[15], x0, x15);
    btf_16_adds_subs_out_avx2(&x_even[1], &x_even[14], x1, x14);
    btf_16_adds_subs_out_avx2(&x_even[2], &x_even[13], x2, x13);
    btf_16_adds_subs_out_avx2(&x_even[3], &x_even[12], x3, x12);
    btf_16_adds_subs_out_avx2(&x_even[4], &x_even[11], x4, x11);
    btf_16_adds_subs_out_avx2(&x_even[5], &x_even[10], x5, x10);
    btf_16_adds_subs_out_avx2(&x_even[6], &x_even[9], x6, x9);
    btf_16_adds_subs_out_avx2(&x_even[7], &x_even[8], x7, x8);
  }

  // Odd half group B: x20..x27 (stages 1..6 on input[5, 11, 13, 3])
  {
    __m256i x20 = input[5], x22 = input[13], x24 = input[3], x26 = input[11];
    __m256i x21, x23, x25, x27;
    btf_16_w16_0_avx2(cospi[54], cospi[10], x20, x20, x27);
    btf_16_w16_0_avx2(-cospi[42], cospi[22], x26, x21, x26);
    btf_16_w16_0_avx2(cospi[38], cospi[26], x22, x22, x25);
    btf_16_w16_0_avx2(-cospi[58], cospi[6], x24, x23, x24);
    btf_16_adds_subs_avx2(&x20, &x21);
    btf_16_adds_subs_avx2(&x23, &x22);
    btf_16_adds_subs_avx2(&x24, &x25);
    btf_16_adds_subs_avx2(&x27, &x26);
    {
      const __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
      const __m256i cospi_p24_p40 = pair_set_w16_epi16(cospi[24], cospi[40]);
      const __m256i cospi_m24_m40 = pair_set_w16_epi16(-cospi[24], -cospi[40]);
      btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, &x21, &x26, _r,
                      INV_COS_BIT);
      btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, &x22, &x25, _r,
                      INV_COS_BIT);
    }
    btf_16_adds_subs_avx2(&x23, &x20);
    btf_16_adds_subs_avx2(&x22, &x21);
    btf_16_adds_subs_avx2(&x24, &x27);
    btf_16_adds_subs_avx2(&x25, &x26);
    {
      const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
      const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
      btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x20, &x27, _r,
                      INV_COS_BIT);
      btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x21, &x26, _r,
                      INV_COS_BIT);
    }
    x_g2[0] = x20;
    x_g2[1] = x21;
    x_g2[2] = x22;
    x_g2[3] = x23;
    x_g2[4] = x24;
    x_g2[5] = x25;
    x_g2[6] = x26;
    x_g2[7] = x27;
  }

  // Odd half group A: x16..x19, x28..x31 (stages 1..6 on input[1, 9, 7, 15])
  __m256i x16 = input[1], x18 = input[9], x28 = input[7], x30 = input[15];
  __m256i x17, x19, x29, x31;
  btf_16_w16_0_avx2(cospi[62], cospi[2], x16, x16, x31);
  btf_16_w16_0_avx2(-cospi[34], cospi[30], x30, x17, x30);
  btf_16_w16_0_avx2(cospi[46], cospi[18], x18, x18, x29);
  btf_16_w16_0_avx2(-cospi[50], cospi[14], x28, x19, x28);
  btf_16_adds_subs_avx2(&x16, &x17);
  btf_16_adds_subs_avx2(&x19, &x18);
  btf_16_adds_subs_avx2(&x28, &x29);
  btf_16_adds_subs_avx2(&x31, &x30);
  {
    const __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
    const __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
    const __m256i cospi_m56_m08 = pair_set_w16_epi16(-cospi[56], -cospi[8]);
    btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, &x17, &x30, _r, INV_COS_BIT);
    btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, &x18, &x29, _r, INV_COS_BIT);
  }
  btf_16_adds_subs_avx2(&x16, &x19);
  btf_16_adds_subs_avx2(&x17, &x18);
  btf_16_adds_subs_avx2(&x31, &x28);
  btf_16_adds_subs_avx2(&x30, &x29);
  {
    const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
    const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
    btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x18, &x29, _r, INV_COS_BIT);
    btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x19, &x28, _r, INV_COS_BIT);
  }

  idct32_odd_stage7_to_9_avx2(x16, x17, x18, x19, x28, x29, x30, x31, x_g2,
                              x_even, output, cospi, _r);
}

static void idct32_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));
  __m256i x_even[16];
  __m256i x_g2[8];

  // Even half (stages 1..8 on input[0, 2, 4, ..., 30])
  {
    __m256i x8 = input[2], x9 = input[18], x10 = input[10], x11 = input[26];
    __m256i x12 = input[6], x13 = input[22], x14 = input[14], x15 = input[30];
    {
      const __m256i cospi_p60_m04 = pair_set_w16_epi16(cospi[60], -cospi[4]);
      const __m256i cospi_p04_p60 = pair_set_w16_epi16(cospi[4], cospi[60]);
      btf_16_w16_avx2(cospi_p60_m04, cospi_p04_p60, &x8, &x15, _r, INV_COS_BIT);
    }
    {
      const __m256i cospi_p28_m36 = pair_set_w16_epi16(cospi[28], -cospi[36]);
      const __m256i cospi_p36_p28 = pair_set_w16_epi16(cospi[36], cospi[28]);
      btf_16_w16_avx2(cospi_p28_m36, cospi_p36_p28, &x9, &x14, _r, INV_COS_BIT);
    }
    {
      const __m256i cospi_p44_m20 = pair_set_w16_epi16(cospi[44], -cospi[20]);
      const __m256i cospi_p20_p44 = pair_set_w16_epi16(cospi[20], cospi[44]);
      btf_16_w16_avx2(cospi_p44_m20, cospi_p20_p44, &x10, &x13, _r,
                      INV_COS_BIT);
    }
    {
      const __m256i cospi_p12_m52 = pair_set_w16_epi16(cospi[12], -cospi[52]);
      const __m256i cospi_p52_p12 = pair_set_w16_epi16(cospi[52], cospi[12]);
      btf_16_w16_avx2(cospi_p12_m52, cospi_p52_p12, &x11, &x12, _r,
                      INV_COS_BIT);
    }
    btf_16_adds_subs_avx2(&x8, &x9);
    btf_16_adds_subs_avx2(&x11, &x10);
    btf_16_adds_subs_avx2(&x12, &x13);
    btf_16_adds_subs_avx2(&x15, &x14);
    {
      const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
      const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
      const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
      btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x9, &x14, _r, INV_COS_BIT);
      btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x10, &x13, _r,
                      INV_COS_BIT);
    }
    btf_16_adds_subs_avx2(&x8, &x11);
    btf_16_adds_subs_avx2(&x9, &x10);
    btf_16_adds_subs_avx2(&x15, &x12);
    btf_16_adds_subs_avx2(&x14, &x13);

    const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
    const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x10, &x13, _r, INV_COS_BIT);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x11, &x12, _r, INV_COS_BIT);

    __m256i x0 = input[0], x1 = input[16], x2 = input[8], x3 = input[24];
    __m256i x4 = input[4], x5 = input[20], x6 = input[12], x7 = input[28];
    {
      const __m256i cospi_p56_m08 = pair_set_w16_epi16(cospi[56], -cospi[8]);
      const __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
      btf_16_w16_avx2(cospi_p56_m08, cospi_p08_p56, &x4, &x7, _r, INV_COS_BIT);
    }
    {
      const __m256i cospi_p24_m40 = pair_set_w16_epi16(cospi[24], -cospi[40]);
      const __m256i cospi_p40_p24 = pair_set_w16_epi16(cospi[40], cospi[24]);
      btf_16_w16_avx2(cospi_p24_m40, cospi_p40_p24, &x5, &x6, _r, INV_COS_BIT);
    }
    {
      const __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
      btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x0, &x1, _r, INV_COS_BIT);
    }
    {
      const __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
      const __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
      btf_16_w16_avx2(cospi_p48_m16, cospi_p16_p48, &x2, &x3, _r, INV_COS_BIT);
    }
    btf_16_adds_subs_avx2(&x4, &x5);
    btf_16_adds_subs_avx2(&x7, &x6);
    btf_16_adds_subs_avx2(&x0, &x3);
    btf_16_adds_subs_avx2(&x1, &x2);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x5, &x6, _r, INV_COS_BIT);
    btf_16_adds_subs_avx2(&x0, &x7);
    btf_16_adds_subs_avx2(&x1, &x6);
    btf_16_adds_subs_avx2(&x2, &x5);
    btf_16_adds_subs_avx2(&x3, &x4);

    btf_16_adds_subs_out_avx2(&x_even[0], &x_even[15], x0, x15);
    btf_16_adds_subs_out_avx2(&x_even[1], &x_even[14], x1, x14);
    btf_16_adds_subs_out_avx2(&x_even[2], &x_even[13], x2, x13);
    btf_16_adds_subs_out_avx2(&x_even[3], &x_even[12], x3, x12);
    btf_16_adds_subs_out_avx2(&x_even[4], &x_even[11], x4, x11);
    btf_16_adds_subs_out_avx2(&x_even[5], &x_even[10], x5, x10);
    btf_16_adds_subs_out_avx2(&x_even[6], &x_even[9], x6, x9);
    btf_16_adds_subs_out_avx2(&x_even[7], &x_even[8], x7, x8);
  }

  // Odd half group B: x20..x27 (stages 1..6)
  {
    __m256i x20 = input[5], x21 = input[21], x22 = input[13], x23 = input[29];
    __m256i x24 = input[3], x25 = input[19], x26 = input[11], x27 = input[27];
    {
      const __m256i cospi_p54_m10 = pair_set_w16_epi16(cospi[54], -cospi[10]);
      const __m256i cospi_p10_p54 = pair_set_w16_epi16(cospi[10], cospi[54]);
      btf_16_w16_avx2(cospi_p54_m10, cospi_p10_p54, &x20, &x27, _r,
                      INV_COS_BIT);
    }
    {
      const __m256i cospi_p22_m42 = pair_set_w16_epi16(cospi[22], -cospi[42]);
      const __m256i cospi_p42_p22 = pair_set_w16_epi16(cospi[42], cospi[22]);
      btf_16_w16_avx2(cospi_p22_m42, cospi_p42_p22, &x21, &x26, _r,
                      INV_COS_BIT);
    }
    {
      const __m256i cospi_p38_m26 = pair_set_w16_epi16(cospi[38], -cospi[26]);
      const __m256i cospi_p26_p38 = pair_set_w16_epi16(cospi[26], cospi[38]);
      btf_16_w16_avx2(cospi_p38_m26, cospi_p26_p38, &x22, &x25, _r,
                      INV_COS_BIT);
    }
    {
      const __m256i cospi_p06_m58 = pair_set_w16_epi16(cospi[6], -cospi[58]);
      const __m256i cospi_p58_p06 = pair_set_w16_epi16(cospi[58], cospi[6]);
      btf_16_w16_avx2(cospi_p06_m58, cospi_p58_p06, &x23, &x24, _r,
                      INV_COS_BIT);
    }
    btf_16_adds_subs_avx2(&x20, &x21);
    btf_16_adds_subs_avx2(&x23, &x22);
    btf_16_adds_subs_avx2(&x24, &x25);
    btf_16_adds_subs_avx2(&x27, &x26);
    {
      const __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
      const __m256i cospi_p24_p40 = pair_set_w16_epi16(cospi[24], cospi[40]);
      const __m256i cospi_m24_m40 = pair_set_w16_epi16(-cospi[24], -cospi[40]);
      btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, &x21, &x26, _r,
                      INV_COS_BIT);
      btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, &x22, &x25, _r,
                      INV_COS_BIT);
    }
    btf_16_adds_subs_avx2(&x23, &x20);
    btf_16_adds_subs_avx2(&x22, &x21);
    btf_16_adds_subs_avx2(&x24, &x27);
    btf_16_adds_subs_avx2(&x25, &x26);
    {
      const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
      const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
      btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x20, &x27, _r,
                      INV_COS_BIT);
      btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x21, &x26, _r,
                      INV_COS_BIT);
    }
    x_g2[0] = x20;
    x_g2[1] = x21;
    x_g2[2] = x22;
    x_g2[3] = x23;
    x_g2[4] = x24;
    x_g2[5] = x25;
    x_g2[6] = x26;
    x_g2[7] = x27;
  }

  // Odd half group A: x16..x19, x28..x31 (stages 1..6)
  __m256i x16 = input[1], x17 = input[17], x18 = input[9], x19 = input[25];
  __m256i x28 = input[7], x29 = input[23], x30 = input[15], x31 = input[31];
  {
    const __m256i cospi_p62_m02 = pair_set_w16_epi16(cospi[62], -cospi[2]);
    const __m256i cospi_p02_p62 = pair_set_w16_epi16(cospi[2], cospi[62]);
    btf_16_w16_avx2(cospi_p62_m02, cospi_p02_p62, &x16, &x31, _r, INV_COS_BIT);
  }
  {
    const __m256i cospi_p30_m34 = pair_set_w16_epi16(cospi[30], -cospi[34]);
    const __m256i cospi_p34_p30 = pair_set_w16_epi16(cospi[34], cospi[30]);
    btf_16_w16_avx2(cospi_p30_m34, cospi_p34_p30, &x17, &x30, _r, INV_COS_BIT);
  }
  {
    const __m256i cospi_p46_m18 = pair_set_w16_epi16(cospi[46], -cospi[18]);
    const __m256i cospi_p18_p46 = pair_set_w16_epi16(cospi[18], cospi[46]);
    btf_16_w16_avx2(cospi_p46_m18, cospi_p18_p46, &x18, &x29, _r, INV_COS_BIT);
  }
  {
    const __m256i cospi_p14_m50 = pair_set_w16_epi16(cospi[14], -cospi[50]);
    const __m256i cospi_p50_p14 = pair_set_w16_epi16(cospi[50], cospi[14]);
    btf_16_w16_avx2(cospi_p14_m50, cospi_p50_p14, &x19, &x28, _r, INV_COS_BIT);
  }
  btf_16_adds_subs_avx2(&x16, &x17);
  btf_16_adds_subs_avx2(&x19, &x18);
  btf_16_adds_subs_avx2(&x28, &x29);
  btf_16_adds_subs_avx2(&x31, &x30);
  {
    const __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
    const __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
    const __m256i cospi_m56_m08 = pair_set_w16_epi16(-cospi[56], -cospi[8]);
    btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, &x17, &x30, _r, INV_COS_BIT);
    btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, &x18, &x29, _r, INV_COS_BIT);
  }
  btf_16_adds_subs_avx2(&x16, &x19);
  btf_16_adds_subs_avx2(&x17, &x18);
  btf_16_adds_subs_avx2(&x31, &x28);
  btf_16_adds_subs_avx2(&x30, &x29);
  {
    const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
    const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
    btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x18, &x29, _r, INV_COS_BIT);
    btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x19, &x28, _r, INV_COS_BIT);
  }

  idct32_odd_stage7_to_9_avx2(x16, x17, x18, x19, x28, x29, x30, x31, x_g2,
                              x_even, output, cospi, _r);
}

static inline void idct64_stage4_high32_avx2(__m256i *x, const int32_t *cospi,
                                             const __m256i _r, int8_t cos_bit) {
  (void)cos_bit;
  const __m256i cospi_m04_p60 = pair_set_w16_epi16(-cospi[4], cospi[60]);
  const __m256i cospi_p60_p04 = pair_set_w16_epi16(cospi[60], cospi[4]);
  const __m256i cospi_m60_m04 = pair_set_w16_epi16(-cospi[60], -cospi[4]);
  const __m256i cospi_m36_p28 = pair_set_w16_epi16(-cospi[36], cospi[28]);
  const __m256i cospi_p28_p36 = pair_set_w16_epi16(cospi[28], cospi[36]);
  const __m256i cospi_m28_m36 = pair_set_w16_epi16(-cospi[28], -cospi[36]);
  const __m256i cospi_m20_p44 = pair_set_w16_epi16(-cospi[20], cospi[44]);
  const __m256i cospi_p44_p20 = pair_set_w16_epi16(cospi[44], cospi[20]);
  const __m256i cospi_m44_m20 = pair_set_w16_epi16(-cospi[44], -cospi[20]);
  const __m256i cospi_m52_p12 = pair_set_w16_epi16(-cospi[52], cospi[12]);
  const __m256i cospi_p12_p52 = pair_set_w16_epi16(cospi[12], cospi[52]);
  const __m256i cospi_m12_m52 = pair_set_w16_epi16(-cospi[12], -cospi[52]);
  btf_16_w16_avx2(cospi_m04_p60, cospi_p60_p04, &x[33], &x[62], _r, cos_bit);
  btf_16_w16_avx2(cospi_m60_m04, cospi_m04_p60, &x[34], &x[61], _r, cos_bit);
  btf_16_w16_avx2(cospi_m36_p28, cospi_p28_p36, &x[37], &x[58], _r, cos_bit);
  btf_16_w16_avx2(cospi_m28_m36, cospi_m36_p28, &x[38], &x[57], _r, cos_bit);
  btf_16_w16_avx2(cospi_m20_p44, cospi_p44_p20, &x[41], &x[54], _r, cos_bit);
  btf_16_w16_avx2(cospi_m44_m20, cospi_m20_p44, &x[42], &x[53], _r, cos_bit);
  btf_16_w16_avx2(cospi_m52_p12, cospi_p12_p52, &x[45], &x[50], _r, cos_bit);
  btf_16_w16_avx2(cospi_m12_m52, cospi_m52_p12, &x[46], &x[49], _r, cos_bit);
}

static inline void idct64_stage5_high48_avx2(__m256i *x, const int32_t *cospi,
                                             const __m256i _r, int8_t cos_bit) {
  (void)cos_bit;
  const __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
  const __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
  const __m256i cospi_m56_m08 = pair_set_w16_epi16(-cospi[56], -cospi[8]);
  const __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
  const __m256i cospi_p24_p40 = pair_set_w16_epi16(cospi[24], cospi[40]);
  const __m256i cospi_m24_m40 = pair_set_w16_epi16(-cospi[24], -cospi[40]);
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, &x[17], &x[30], _r, cos_bit);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, &x[18], &x[29], _r, cos_bit);
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, &x[21], &x[26], _r, cos_bit);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, &x[22], &x[25], _r, cos_bit);
  btf_16_adds_subs_avx2(&x[32], &x[35]);
  btf_16_adds_subs_avx2(&x[33], &x[34]);
  btf_16_adds_subs_avx2(&x[39], &x[36]);
  btf_16_adds_subs_avx2(&x[38], &x[37]);
  btf_16_adds_subs_avx2(&x[40], &x[43]);
  btf_16_adds_subs_avx2(&x[41], &x[42]);
  btf_16_adds_subs_avx2(&x[47], &x[44]);
  btf_16_adds_subs_avx2(&x[46], &x[45]);
  btf_16_adds_subs_avx2(&x[48], &x[51]);
  btf_16_adds_subs_avx2(&x[49], &x[50]);
  btf_16_adds_subs_avx2(&x[55], &x[52]);
  btf_16_adds_subs_avx2(&x[54], &x[53]);
  btf_16_adds_subs_avx2(&x[56], &x[59]);
  btf_16_adds_subs_avx2(&x[57], &x[58]);
  btf_16_adds_subs_avx2(&x[63], &x[60]);
  btf_16_adds_subs_avx2(&x[62], &x[61]);
}

static inline void idct64_stage6_high32_avx2(__m256i *x, const int32_t *cospi,
                                             const __m256i _r, int8_t cos_bit) {
  (void)cos_bit;
  const __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
  const __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
  const __m256i cospi_m56_m08 = pair_set_w16_epi16(-cospi[56], -cospi[8]);
  const __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
  const __m256i cospi_p24_p40 = pair_set_w16_epi16(cospi[24], cospi[40]);
  const __m256i cospi_m24_m40 = pair_set_w16_epi16(-cospi[24], -cospi[40]);
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, &x[34], &x[61], _r, cos_bit);
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, &x[35], &x[60], _r, cos_bit);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, &x[36], &x[59], _r, cos_bit);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, &x[37], &x[58], _r, cos_bit);
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, &x[42], &x[53], _r, cos_bit);
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, &x[43], &x[52], _r, cos_bit);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, &x[44], &x[51], _r, cos_bit);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, &x[45], &x[50], _r, cos_bit);
}

static inline void idct64_stage6_high48_avx2(__m256i *x, const int32_t *cospi,
                                             const __m256i _r, int8_t cos_bit) {
  btf_16_adds_subs_avx2(&x[16], &x[19]);
  btf_16_adds_subs_avx2(&x[17], &x[18]);
  btf_16_adds_subs_avx2(&x[23], &x[20]);
  btf_16_adds_subs_avx2(&x[22], &x[21]);
  btf_16_adds_subs_avx2(&x[24], &x[27]);
  btf_16_adds_subs_avx2(&x[25], &x[26]);
  btf_16_adds_subs_avx2(&x[31], &x[28]);
  btf_16_adds_subs_avx2(&x[30], &x[29]);
  idct64_stage6_high32_avx2(x, cospi, _r, cos_bit);
}

static inline void idct64_stage7_high48_avx2(__m256i *x, const int32_t *cospi,
                                             const __m256i _r, int8_t cos_bit) {
  (void)cos_bit;
  const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[18], &x[29], _r, cos_bit);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[19], &x[28], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[20], &x[27], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[21], &x[26], _r, cos_bit);
  btf_16_adds_subs_avx2(&x[32], &x[39]);
  btf_16_adds_subs_avx2(&x[33], &x[38]);
  btf_16_adds_subs_avx2(&x[34], &x[37]);
  btf_16_adds_subs_avx2(&x[35], &x[36]);
  btf_16_adds_subs_avx2(&x[47], &x[40]);
  btf_16_adds_subs_avx2(&x[46], &x[41]);
  btf_16_adds_subs_avx2(&x[45], &x[42]);
  btf_16_adds_subs_avx2(&x[44], &x[43]);
  btf_16_adds_subs_avx2(&x[48], &x[55]);
  btf_16_adds_subs_avx2(&x[49], &x[54]);
  btf_16_adds_subs_avx2(&x[50], &x[53]);
  btf_16_adds_subs_avx2(&x[51], &x[52]);
  btf_16_adds_subs_avx2(&x[63], &x[56]);
  btf_16_adds_subs_avx2(&x[62], &x[57]);
  btf_16_adds_subs_avx2(&x[61], &x[58]);
  btf_16_adds_subs_avx2(&x[60], &x[59]);
}

static inline void idct64_stage8_high48_avx2(__m256i *x, const int32_t *cospi,
                                             const __m256i _r, int8_t cos_bit) {
  (void)cos_bit;
  const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  btf_16_adds_subs_avx2(&x[16], &x[23]);
  btf_16_adds_subs_avx2(&x[17], &x[22]);
  btf_16_adds_subs_avx2(&x[18], &x[21]);
  btf_16_adds_subs_avx2(&x[19], &x[20]);
  btf_16_adds_subs_avx2(&x[31], &x[24]);
  btf_16_adds_subs_avx2(&x[30], &x[25]);
  btf_16_adds_subs_avx2(&x[29], &x[26]);
  btf_16_adds_subs_avx2(&x[28], &x[27]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[36], &x[59], _r, cos_bit);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[37], &x[58], _r, cos_bit);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[38], &x[57], _r, cos_bit);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[39], &x[56], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[40], &x[55], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[41], &x[54], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[42], &x[53], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[43], &x[52], _r, cos_bit);
}

static inline void idct64_stage9_avx2(__m256i *x, const int32_t *cospi,
                                      const __m256i _r, int8_t cos_bit) {
  (void)cos_bit;
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  btf_16_adds_subs_avx2(&x[0], &x[15]);
  btf_16_adds_subs_avx2(&x[1], &x[14]);
  btf_16_adds_subs_avx2(&x[2], &x[13]);
  btf_16_adds_subs_avx2(&x[3], &x[12]);
  btf_16_adds_subs_avx2(&x[4], &x[11]);
  btf_16_adds_subs_avx2(&x[5], &x[10]);
  btf_16_adds_subs_avx2(&x[6], &x[9]);
  btf_16_adds_subs_avx2(&x[7], &x[8]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[20], &x[27], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[21], &x[26], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[22], &x[25], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[23], &x[24], _r, cos_bit);
  btf_16_adds_subs_avx2(&x[32], &x[47]);
  btf_16_adds_subs_avx2(&x[33], &x[46]);
  btf_16_adds_subs_avx2(&x[34], &x[45]);
  btf_16_adds_subs_avx2(&x[35], &x[44]);
  btf_16_adds_subs_avx2(&x[36], &x[43]);
  btf_16_adds_subs_avx2(&x[37], &x[42]);
  btf_16_adds_subs_avx2(&x[38], &x[41]);
  btf_16_adds_subs_avx2(&x[39], &x[40]);
  btf_16_adds_subs_avx2(&x[63], &x[48]);
  btf_16_adds_subs_avx2(&x[62], &x[49]);
  btf_16_adds_subs_avx2(&x[61], &x[50]);
  btf_16_adds_subs_avx2(&x[60], &x[51]);
  btf_16_adds_subs_avx2(&x[59], &x[52]);
  btf_16_adds_subs_avx2(&x[58], &x[53]);
  btf_16_adds_subs_avx2(&x[57], &x[54]);
  btf_16_adds_subs_avx2(&x[56], &x[55]);
}

static inline void idct64_stage10_avx2(__m256i *x, const int32_t *cospi,
                                       const __m256i _r, int8_t cos_bit) {
  (void)cos_bit;
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  btf_16_adds_subs_avx2(&x[0], &x[31]);
  btf_16_adds_subs_avx2(&x[1], &x[30]);
  btf_16_adds_subs_avx2(&x[2], &x[29]);
  btf_16_adds_subs_avx2(&x[3], &x[28]);
  btf_16_adds_subs_avx2(&x[4], &x[27]);
  btf_16_adds_subs_avx2(&x[5], &x[26]);
  btf_16_adds_subs_avx2(&x[6], &x[25]);
  btf_16_adds_subs_avx2(&x[7], &x[24]);
  btf_16_adds_subs_avx2(&x[8], &x[23]);
  btf_16_adds_subs_avx2(&x[9], &x[22]);
  btf_16_adds_subs_avx2(&x[10], &x[21]);
  btf_16_adds_subs_avx2(&x[11], &x[20]);
  btf_16_adds_subs_avx2(&x[12], &x[19]);
  btf_16_adds_subs_avx2(&x[13], &x[18]);
  btf_16_adds_subs_avx2(&x[14], &x[17]);
  btf_16_adds_subs_avx2(&x[15], &x[16]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[40], &x[55], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[41], &x[54], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[42], &x[53], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[43], &x[52], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[44], &x[51], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[45], &x[50], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[46], &x[49], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[47], &x[48], _r, cos_bit);
}

static inline void idct64_stage11_avx2(__m256i *output, __m256i *x) {
  btf_16_adds_subs_out_avx2(&output[0], &output[63], x[0], x[63]);
  btf_16_adds_subs_out_avx2(&output[1], &output[62], x[1], x[62]);
  btf_16_adds_subs_out_avx2(&output[2], &output[61], x[2], x[61]);
  btf_16_adds_subs_out_avx2(&output[3], &output[60], x[3], x[60]);
  btf_16_adds_subs_out_avx2(&output[4], &output[59], x[4], x[59]);
  btf_16_adds_subs_out_avx2(&output[5], &output[58], x[5], x[58]);
  btf_16_adds_subs_out_avx2(&output[6], &output[57], x[6], x[57]);
  btf_16_adds_subs_out_avx2(&output[7], &output[56], x[7], x[56]);
  btf_16_adds_subs_out_avx2(&output[8], &output[55], x[8], x[55]);
  btf_16_adds_subs_out_avx2(&output[9], &output[54], x[9], x[54]);
  btf_16_adds_subs_out_avx2(&output[10], &output[53], x[10], x[53]);
  btf_16_adds_subs_out_avx2(&output[11], &output[52], x[11], x[52]);
  btf_16_adds_subs_out_avx2(&output[12], &output[51], x[12], x[51]);
  btf_16_adds_subs_out_avx2(&output[13], &output[50], x[13], x[50]);
  btf_16_adds_subs_out_avx2(&output[14], &output[49], x[14], x[49]);
  btf_16_adds_subs_out_avx2(&output[15], &output[48], x[15], x[48]);
  btf_16_adds_subs_out_avx2(&output[16], &output[47], x[16], x[47]);
  btf_16_adds_subs_out_avx2(&output[17], &output[46], x[17], x[46]);
  btf_16_adds_subs_out_avx2(&output[18], &output[45], x[18], x[45]);
  btf_16_adds_subs_out_avx2(&output[19], &output[44], x[19], x[44]);
  btf_16_adds_subs_out_avx2(&output[20], &output[43], x[20], x[43]);
  btf_16_adds_subs_out_avx2(&output[21], &output[42], x[21], x[42]);
  btf_16_adds_subs_out_avx2(&output[22], &output[41], x[22], x[41]);
  btf_16_adds_subs_out_avx2(&output[23], &output[40], x[23], x[40]);
  btf_16_adds_subs_out_avx2(&output[24], &output[39], x[24], x[39]);
  btf_16_adds_subs_out_avx2(&output[25], &output[38], x[25], x[38]);
  btf_16_adds_subs_out_avx2(&output[26], &output[37], x[26], x[37]);
  btf_16_adds_subs_out_avx2(&output[27], &output[36], x[27], x[36]);
  btf_16_adds_subs_out_avx2(&output[28], &output[35], x[28], x[35]);
  btf_16_adds_subs_out_avx2(&output[29], &output[34], x[29], x[34]);
  btf_16_adds_subs_out_avx2(&output[30], &output[33], x[30], x[33]);
  btf_16_adds_subs_out_avx2(&output[31], &output[32], x[31], x[32]);
}

static void idct64_low1_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i w = _mm256_set1_epi16(cospi[32] * 8);
  const __m256i x = _mm256_mulhrs_epi16(input[0], w);
  for (int i = 0; i < 64; ++i) {
    output[i] = x;
  }
}

static void idct64_low8_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));
  const __m256i cospi_m04_p60 = pair_set_w16_epi16(-cospi[4], cospi[60]);
  const __m256i cospi_p60_p04 = pair_set_w16_epi16(cospi[60], cospi[4]);
  const __m256i cospi_m36_p28 = pair_set_w16_epi16(-cospi[36], cospi[28]);
  const __m256i cospi_m28_m36 = pair_set_w16_epi16(-cospi[28], -cospi[36]);
  const __m256i cospi_m20_p44 = pair_set_w16_epi16(-cospi[20], cospi[44]);
  const __m256i cospi_p44_p20 = pair_set_w16_epi16(cospi[44], cospi[20]);
  const __m256i cospi_m52_p12 = pair_set_w16_epi16(-cospi[52], cospi[12]);
  const __m256i cospi_m12_m52 = pair_set_w16_epi16(-cospi[12], -cospi[52]);
  const __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
  const __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
  const __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
  const __m256i cospi_m24_m40 = pair_set_w16_epi16(-cospi[24], -cospi[40]);
  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m256i x[64];
  x[0] = input[0];
  x[8] = input[4];
  x[16] = input[2];
  x[24] = input[6];
  x[32] = input[1];
  x[40] = input[5];
  x[48] = input[3];
  x[56] = input[7];

  // stage 2
  btf_16_w16_0_avx2(cospi[63], cospi[1], x[32], x[32], x[63]);
  btf_16_w16_0_avx2(-cospi[57], cospi[7], x[56], x[39], x[56]);
  btf_16_w16_0_avx2(cospi[59], cospi[5], x[40], x[40], x[55]);
  btf_16_w16_0_avx2(-cospi[61], cospi[3], x[48], x[47], x[48]);

  // stage 3
  btf_16_w16_0_avx2(cospi[62], cospi[2], x[16], x[16], x[31]);
  btf_16_w16_0_avx2(-cospi[58], cospi[6], x[24], x[23], x[24]);
  x[33] = x[32];
  x[38] = x[39];
  x[41] = x[40];
  x[46] = x[47];
  x[49] = x[48];
  x[54] = x[55];
  x[57] = x[56];
  x[62] = x[63];

  // stage 4
  btf_16_w16_0_avx2(cospi[60], cospi[4], x[8], x[8], x[15]);
  x[17] = x[16];
  x[22] = x[23];
  x[25] = x[24];
  x[30] = x[31];
  btf_16_w16_avx2(cospi_m04_p60, cospi_p60_p04, &x[33], &x[62], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m28_m36, cospi_m36_p28, &x[38], &x[57], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m20_p44, cospi_p44_p20, &x[41], &x[54], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m12_m52, cospi_m52_p12, &x[46], &x[49], _r,
                  INV_COS_BIT);

  // stage 5
  x[9] = x[8];
  x[14] = x[15];
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, &x[17], &x[30], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, &x[22], &x[25], _r,
                  INV_COS_BIT);
  x[35] = x[32];
  x[34] = x[33];
  x[36] = x[39];
  x[37] = x[38];
  x[43] = x[40];
  x[42] = x[41];
  x[44] = x[47];
  x[45] = x[46];
  x[51] = x[48];
  x[50] = x[49];
  x[52] = x[55];
  x[53] = x[54];
  x[59] = x[56];
  x[58] = x[57];
  x[60] = x[63];
  x[61] = x[62];

  // stage 6
  btf_16_w16_0_avx2(cospi[32], cospi[32], x[0], x[0], x[1]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[9], &x[14], _r, INV_COS_BIT);
  x[19] = x[16];
  x[18] = x[17];
  x[20] = x[23];
  x[21] = x[22];
  x[27] = x[24];
  x[26] = x[25];
  x[28] = x[31];
  x[29] = x[30];
  idct64_stage6_high32_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 7
  x[3] = x[0];
  x[2] = x[1];
  x[11] = x[8];
  x[10] = x[9];
  x[12] = x[15];
  x[13] = x[14];
  idct64_stage7_high48_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 8
  x[7] = x[0];
  x[6] = x[1];
  x[5] = x[2];
  x[4] = x[3];
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[10], &x[13], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[11], &x[12], _r,
                  INV_COS_BIT);
  idct64_stage8_high48_avx2(x, cospi, _r, INV_COS_BIT);

  idct64_stage9_avx2(x, cospi, _r, INV_COS_BIT);
  idct64_stage10_avx2(x, cospi, _r, INV_COS_BIT);
  idct64_stage11_avx2(output, x);
}

static void idct64_low16_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m256i x[64];
  x[0] = input[0];
  x[4] = input[8];
  x[8] = input[4];
  x[12] = input[12];
  x[16] = input[2];
  x[20] = input[10];
  x[24] = input[6];
  x[28] = input[14];
  x[32] = input[1];
  x[36] = input[9];
  x[40] = input[5];
  x[44] = input[13];
  x[48] = input[3];
  x[52] = input[11];
  x[56] = input[7];
  x[60] = input[15];

  // stage 2
  btf_16_w16_0_avx2(cospi[63], cospi[1], x[32], x[32], x[63]);
  btf_16_w16_0_avx2(-cospi[49], cospi[15], x[60], x[35], x[60]);
  btf_16_w16_0_avx2(cospi[55], cospi[9], x[36], x[36], x[59]);
  btf_16_w16_0_avx2(-cospi[57], cospi[7], x[56], x[39], x[56]);
  btf_16_w16_0_avx2(cospi[59], cospi[5], x[40], x[40], x[55]);
  btf_16_w16_0_avx2(-cospi[53], cospi[11], x[52], x[43], x[52]);
  btf_16_w16_0_avx2(cospi[51], cospi[13], x[44], x[44], x[51]);
  btf_16_w16_0_avx2(-cospi[61], cospi[3], x[48], x[47], x[48]);

  // stage 3
  btf_16_w16_0_avx2(cospi[62], cospi[2], x[16], x[16], x[31]);
  btf_16_w16_0_avx2(-cospi[50], cospi[14], x[28], x[19], x[28]);
  btf_16_w16_0_avx2(cospi[54], cospi[10], x[20], x[20], x[27]);
  btf_16_w16_0_avx2(-cospi[58], cospi[6], x[24], x[23], x[24]);
  x[33] = x[32];
  x[34] = x[35];
  x[37] = x[36];
  x[38] = x[39];
  x[41] = x[40];
  x[42] = x[43];
  x[45] = x[44];
  x[46] = x[47];
  x[49] = x[48];
  x[50] = x[51];
  x[53] = x[52];
  x[54] = x[55];
  x[57] = x[56];
  x[58] = x[59];
  x[61] = x[60];
  x[62] = x[63];

  // stage 4
  btf_16_w16_0_avx2(cospi[60], cospi[4], x[8], x[8], x[15]);
  btf_16_w16_0_avx2(-cospi[52], cospi[12], x[12], x[11], x[12]);
  x[17] = x[16];
  x[18] = x[19];
  x[21] = x[20];
  x[22] = x[23];
  x[25] = x[24];
  x[26] = x[27];
  x[29] = x[28];
  x[30] = x[31];
  idct64_stage4_high32_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 5
  btf_16_w16_0_avx2(cospi[56], cospi[8], x[4], x[4], x[7]);
  x[9] = x[8];
  x[10] = x[11];
  x[13] = x[12];
  x[14] = x[15];
  idct64_stage5_high48_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 6
  btf_16_w16_0_avx2(cospi[32], cospi[32], x[0], x[0], x[1]);
  x[5] = x[4];
  x[6] = x[7];
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[9], &x[14], _r, INV_COS_BIT);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[10], &x[13], _r,
                  INV_COS_BIT);
  idct64_stage6_high48_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 7
  x[3] = x[0];
  x[2] = x[1];
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[5], &x[6], _r, INV_COS_BIT);
  btf_16_adds_subs_avx2(&x[8], &x[11]);
  btf_16_adds_subs_avx2(&x[9], &x[10]);
  btf_16_adds_subs_avx2(&x[15], &x[12]);
  btf_16_adds_subs_avx2(&x[14], &x[13]);
  idct64_stage7_high48_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 8
  btf_16_adds_subs_avx2(&x[0], &x[7]);
  btf_16_adds_subs_avx2(&x[1], &x[6]);
  btf_16_adds_subs_avx2(&x[2], &x[5]);
  btf_16_adds_subs_avx2(&x[3], &x[4]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[10], &x[13], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[11], &x[12], _r,
                  INV_COS_BIT);
  idct64_stage8_high48_avx2(x, cospi, _r, INV_COS_BIT);

  idct64_stage9_avx2(x, cospi, _r, INV_COS_BIT);
  idct64_stage10_avx2(x, cospi, _r, INV_COS_BIT);
  idct64_stage11_avx2(output, x);
}

static void idct64_low32_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m256i x[64];
  x[0] = input[0];
  x[2] = input[16];
  x[4] = input[8];
  x[6] = input[24];
  x[8] = input[4];
  x[10] = input[20];
  x[12] = input[12];
  x[14] = input[28];
  x[16] = input[2];
  x[18] = input[18];
  x[20] = input[10];
  x[22] = input[26];
  x[24] = input[6];
  x[26] = input[22];
  x[28] = input[14];
  x[30] = input[30];
  x[32] = input[1];
  x[34] = input[17];
  x[36] = input[9];
  x[38] = input[25];
  x[40] = input[5];
  x[42] = input[21];
  x[44] = input[13];
  x[46] = input[29];
  x[48] = input[3];
  x[50] = input[19];
  x[52] = input[11];
  x[54] = input[27];
  x[56] = input[7];
  x[58] = input[23];
  x[60] = input[15];
  x[62] = input[31];

  // stage 2
  btf_16_w16_0_avx2(cospi[63], cospi[1], x[32], x[32], x[63]);
  btf_16_w16_0_avx2(-cospi[33], cospi[31], x[62], x[33], x[62]);
  btf_16_w16_0_avx2(cospi[47], cospi[17], x[34], x[34], x[61]);
  btf_16_w16_0_avx2(-cospi[49], cospi[15], x[60], x[35], x[60]);
  btf_16_w16_0_avx2(cospi[55], cospi[9], x[36], x[36], x[59]);
  btf_16_w16_0_avx2(-cospi[41], cospi[23], x[58], x[37], x[58]);
  btf_16_w16_0_avx2(cospi[39], cospi[25], x[38], x[38], x[57]);
  btf_16_w16_0_avx2(-cospi[57], cospi[7], x[56], x[39], x[56]);
  btf_16_w16_0_avx2(cospi[59], cospi[5], x[40], x[40], x[55]);
  btf_16_w16_0_avx2(-cospi[37], cospi[27], x[54], x[41], x[54]);
  btf_16_w16_0_avx2(cospi[43], cospi[21], x[42], x[42], x[53]);
  btf_16_w16_0_avx2(-cospi[53], cospi[11], x[52], x[43], x[52]);
  btf_16_w16_0_avx2(cospi[51], cospi[13], x[44], x[44], x[51]);
  btf_16_w16_0_avx2(-cospi[45], cospi[19], x[50], x[45], x[50]);
  btf_16_w16_0_avx2(cospi[35], cospi[29], x[46], x[46], x[49]);
  btf_16_w16_0_avx2(-cospi[61], cospi[3], x[48], x[47], x[48]);

  // stage 3
  btf_16_w16_0_avx2(cospi[62], cospi[2], x[16], x[16], x[31]);
  btf_16_w16_0_avx2(-cospi[34], cospi[30], x[30], x[17], x[30]);
  btf_16_w16_0_avx2(cospi[46], cospi[18], x[18], x[18], x[29]);
  btf_16_w16_0_avx2(-cospi[50], cospi[14], x[28], x[19], x[28]);
  btf_16_w16_0_avx2(cospi[54], cospi[10], x[20], x[20], x[27]);
  btf_16_w16_0_avx2(-cospi[42], cospi[22], x[26], x[21], x[26]);
  btf_16_w16_0_avx2(cospi[38], cospi[26], x[22], x[22], x[25]);
  btf_16_w16_0_avx2(-cospi[58], cospi[6], x[24], x[23], x[24]);
  btf_16_adds_subs_avx2(&x[32], &x[33]);
  btf_16_adds_subs_avx2(&x[35], &x[34]);
  btf_16_adds_subs_avx2(&x[36], &x[37]);
  btf_16_adds_subs_avx2(&x[39], &x[38]);
  btf_16_adds_subs_avx2(&x[40], &x[41]);
  btf_16_adds_subs_avx2(&x[43], &x[42]);
  btf_16_adds_subs_avx2(&x[44], &x[45]);
  btf_16_adds_subs_avx2(&x[47], &x[46]);
  btf_16_adds_subs_avx2(&x[48], &x[49]);
  btf_16_adds_subs_avx2(&x[51], &x[50]);
  btf_16_adds_subs_avx2(&x[52], &x[53]);
  btf_16_adds_subs_avx2(&x[55], &x[54]);
  btf_16_adds_subs_avx2(&x[56], &x[57]);
  btf_16_adds_subs_avx2(&x[59], &x[58]);
  btf_16_adds_subs_avx2(&x[60], &x[61]);
  btf_16_adds_subs_avx2(&x[63], &x[62]);

  // stage 4
  btf_16_w16_0_avx2(cospi[60], cospi[4], x[8], x[8], x[15]);
  btf_16_w16_0_avx2(-cospi[36], cospi[28], x[14], x[9], x[14]);
  btf_16_w16_0_avx2(cospi[44], cospi[20], x[10], x[10], x[13]);
  btf_16_w16_0_avx2(-cospi[52], cospi[12], x[12], x[11], x[12]);
  btf_16_adds_subs_avx2(&x[16], &x[17]);
  btf_16_adds_subs_avx2(&x[19], &x[18]);
  btf_16_adds_subs_avx2(&x[20], &x[21]);
  btf_16_adds_subs_avx2(&x[23], &x[22]);
  btf_16_adds_subs_avx2(&x[24], &x[25]);
  btf_16_adds_subs_avx2(&x[27], &x[26]);
  btf_16_adds_subs_avx2(&x[28], &x[29]);
  btf_16_adds_subs_avx2(&x[31], &x[30]);
  idct64_stage4_high32_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 5
  btf_16_w16_0_avx2(cospi[56], cospi[8], x[4], x[4], x[7]);
  btf_16_w16_0_avx2(-cospi[40], cospi[24], x[6], x[5], x[6]);
  btf_16_adds_subs_avx2(&x[8], &x[9]);
  btf_16_adds_subs_avx2(&x[11], &x[10]);
  btf_16_adds_subs_avx2(&x[12], &x[13]);
  btf_16_adds_subs_avx2(&x[15], &x[14]);
  idct64_stage5_high48_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 6
  btf_16_w16_0_avx2(cospi[32], cospi[32], x[0], x[0], x[1]);
  btf_16_w16_0_avx2(cospi[48], cospi[16], x[2], x[2], x[3]);
  btf_16_adds_subs_avx2(&x[4], &x[5]);
  btf_16_adds_subs_avx2(&x[7], &x[6]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[9], &x[14], _r, INV_COS_BIT);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[10], &x[13], _r,
                  INV_COS_BIT);
  idct64_stage6_high48_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 7
  btf_16_adds_subs_avx2(&x[0], &x[3]);
  btf_16_adds_subs_avx2(&x[1], &x[2]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[5], &x[6], _r, INV_COS_BIT);
  btf_16_adds_subs_avx2(&x[8], &x[11]);
  btf_16_adds_subs_avx2(&x[9], &x[10]);
  btf_16_adds_subs_avx2(&x[15], &x[12]);
  btf_16_adds_subs_avx2(&x[14], &x[13]);
  idct64_stage7_high48_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 8
  btf_16_adds_subs_avx2(&x[0], &x[7]);
  btf_16_adds_subs_avx2(&x[1], &x[6]);
  btf_16_adds_subs_avx2(&x[2], &x[5]);
  btf_16_adds_subs_avx2(&x[3], &x[4]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[10], &x[13], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[11], &x[12], _r,
                  INV_COS_BIT);
  idct64_stage8_high48_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 9~11
  idct64_stage9_avx2(x, cospi, _r, INV_COS_BIT);
  idct64_stage10_avx2(x, cospi, _r, INV_COS_BIT);
  idct64_stage11_avx2(output, x);
}

typedef void (*transform_1d_avx2)(const __m256i *input, __m256i *output);

// 1D functions process 16 pixels at one time.
static const transform_1d_avx2
    lowbd_txfm_all_1d_zeros_w16_arr[TX_SIZES][ITX_TYPES_1D][4] = {
      {
          { NULL, NULL, NULL, NULL },
          { NULL, NULL, NULL, NULL },
          { NULL, NULL, NULL, NULL },
      },
      { { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } },
      {
          { idct16_low1_avx2, idct16_low8_avx2, idct16_avx2, NULL },
          { iadst16_low1_avx2, iadst16_low8_avx2, iadst16_avx2, NULL },
          { NULL, NULL, NULL, NULL },
      },
      { { idct32_low1_avx2, idct32_low8_avx2, idct32_low16_avx2, idct32_avx2 },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } },
      { { idct64_low1_avx2, idct64_low8_avx2, idct64_low16_avx2,
          idct64_low32_avx2 },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } }
    };

// Transposes only the lower 8 rows (t[0..7] -> out[0..7]) of a 16x16 16-bit
// matrix when the upper 8 rows are guaranteed to be zero (eoby <= 7) and the
// column transform (*_low1_avx2 or *_low8_avx2) only reads out[0..7].
static inline void transpose_16bit_16x16_low8_scale_avx2(
    const __m256i *const in, __m256i *const out, const __m256i scale) {
  __m256i t[8];
#define LOADL_SCALE(idx)                                                   \
  do {                                                                     \
    __m256i v =                                                            \
        _mm256_castsi128_si256(_mm_load_si128((__m128i const *)&in[idx])); \
    v = _mm256_inserti128_si256(                                           \
        v, _mm_load_si128((__m128i const *)&in[idx + 8]), 1);              \
    t[idx] = _mm256_mulhrs_epi16(v, scale);                                \
  } while (0)

  LOADL_SCALE(0);
  LOADL_SCALE(1);
  LOADL_SCALE(2);
  LOADL_SCALE(3);
  LOADL_SCALE(4);
  LOADL_SCALE(5);
  LOADL_SCALE(6);
  LOADL_SCALE(7);
  transpose2_8x8_avx2(t, out);
#undef LOADL_SCALE
}

static inline void transpose_16bit_16x16_scale_avx2(const __m256i *const in,
                                                    __m256i *const out,
                                                    const __m256i scale) {
  __m256i t[8];
#define LOADL_SCALE(idx)                                                   \
  do {                                                                     \
    __m256i v =                                                            \
        _mm256_castsi128_si256(_mm_load_si128((__m128i const *)&in[idx])); \
    v = _mm256_inserti128_si256(                                           \
        v, _mm_load_si128((__m128i const *)&in[idx + 8]), 1);              \
    t[idx] = _mm256_mulhrs_epi16(v, scale);                                \
  } while (0)

#define LOADR_SCALE(idx)                                                       \
  do {                                                                         \
    __m256i v =                                                                \
        _mm256_castsi128_si256(_mm_load_si128((__m128i const *)&in[idx] + 1)); \
    v = _mm256_inserti128_si256(                                               \
        v, _mm_load_si128((__m128i const *)&in[idx + 8] + 1), 1);              \
    t[idx] = _mm256_mulhrs_epi16(v, scale);                                    \
  } while (0)

  LOADL_SCALE(0);
  LOADL_SCALE(1);
  LOADL_SCALE(2);
  LOADL_SCALE(3);
  LOADL_SCALE(4);
  LOADL_SCALE(5);
  LOADL_SCALE(6);
  LOADL_SCALE(7);
  transpose2_8x8_avx2(t, out);

  LOADR_SCALE(0);
  LOADR_SCALE(1);
  LOADR_SCALE(2);
  LOADR_SCALE(3);
  LOADR_SCALE(4);
  LOADR_SCALE(5);
  LOADR_SCALE(6);
  LOADR_SCALE(7);
  transpose2_8x8_avx2(t, out + 8);

#undef LOADL_SCALE
#undef LOADR_SCALE
}

static AOM_NOINLINE void lowbd_inv_txfm2d_16x16_dct_avx2(const int32_t *input,
                                                         uint8_t *output,
                                                         int stride, int eob) {
  __m256i buf[16];
  const __m256i scale0 = _mm256_set1_epi16(1 << (15 - 2));
  const __m256i scale1 = _mm256_set1_epi16(1 << (15 - 4));
  if (eob <= 36) {
    load_buffer_32bit_to_16bit_w8_avx2(input, 16, buf, 8);
    idct16_low8_avx2(buf, buf);
    transpose_16bit_16x16_low8_scale_avx2(buf, buf, scale0);
    idct16_low8_avx2(buf, buf);
  } else {
    __m256i buf1[16];
    load_buffer_32bit_to_16bit_w16_avx2(input, 16, buf, 16);
    idct16_avx2(buf, buf);
    transpose_16bit_16x16_scale_avx2(buf, buf1, scale0);
    idct16_avx2(buf1, buf);
  }
  for (int r = 0; r < 16; r += 2) {
    const __m256i res0 = _mm256_mulhrs_epi16(buf[r], scale1);
    const __m256i res1 = _mm256_mulhrs_epi16(buf[r + 1], scale1);
    write_recon_w16x2_avx2(res0, res1, output + r * stride,
                           output + (r + 1) * stride);
  }
}

static AOM_NOINLINE void lowbd_inv_txfm2d_16x16_non_dct_avx2(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    int eob) {
  __m256i buf0[16], buf1[16];
  int eobx, eoby;
  get_eobx_eoby_scan_default(&eobx, &eoby, TX_16X16, eob);
  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_avx2 row_txfm =
      lowbd_txfm_all_1d_zeros_w16_arr[2][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_1d_avx2 col_txfm =
      lowbd_txfm_all_1d_zeros_w16_arr[2][vitx_1d_tab[tx_type]][fun_idx_y];
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const __m256i scale0 = _mm256_set1_epi16(1 << (15 - 2));
  const __m256i scale1 = _mm256_set1_epi16(1 << (15 - 4));
  if (eoby <= 7) {
    load_buffer_32bit_to_16bit_w8_avx2(input, 16, buf0, eobx + 1);
    row_txfm(buf0, buf0);
    if (UNLIKELY(lr_flip)) {
      for (int i = 0; i < 8; ++i) {
        const __m256i t = buf0[i];
        buf0[i] = buf0[15 - i];
        buf0[15 - i] = t;
      }
    }
    transpose_16bit_16x16_low8_scale_avx2(buf0, buf1, scale0);
  } else {
    load_buffer_32bit_to_16bit_w16_avx2(input, 16, buf0, eobx + 1);
    row_txfm(buf0, buf0);
    if (UNLIKELY(lr_flip)) {
      for (int i = 0; i < 8; ++i) {
        const __m256i t = buf0[i];
        buf0[i] = buf0[15 - i];
        buf0[15 - i] = t;
      }
    }
    transpose_16bit_16x16_scale_avx2(buf0, buf1, scale0);
  }
  col_txfm(buf1, buf1);
  if (UNLIKELY(ud_flip)) {
    for (int r = 0; r < 16; r += 2) {
      const __m256i res0 = _mm256_mulhrs_epi16(buf1[15 - r], scale1);
      const __m256i res1 = _mm256_mulhrs_epi16(buf1[14 - r], scale1);
      write_recon_w16x2_avx2(res0, res1, output + r * stride,
                             output + (r + 1) * stride);
    }
  } else {
    for (int r = 0; r < 16; r += 2) {
      const __m256i res0 = _mm256_mulhrs_epi16(buf1[r], scale1);
      const __m256i res1 = _mm256_mulhrs_epi16(buf1[r + 1], scale1);
      write_recon_w16x2_avx2(res0, res1, output + r * stride,
                             output + (r + 1) * stride);
    }
  }
}

// only process w >= 16 h >= 16
static AOM_NOINLINE void lowbd_inv_txfm2d_add_no_identity_general_avx2(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div16 = txfm_size_col >> 4;
  const int is_rect = (unsigned)(tx_size - TX_4X8) <= (TX_64X32 - TX_4X8);

  __m256i buf1[64 * 4];
  int eobx, eoby;
  get_eobx_eoby_scan_default(&eobx, &eoby, tx_size, eob);
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int buf_size_nonzero_w = eobx + 1;
  const int buf_size_nonzero_h_div16 = (eoby + 16) >> 4;
  const int input_stride = AOMMIN(32, txfm_size_row);

  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_avx2 row_txfm =
      lowbd_txfm_all_1d_zeros_w16_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_1d_avx2 col_txfm =
      lowbd_txfm_all_1d_zeros_w16_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const __m256i scale0 = _mm256_set1_epi16(1 << (15 + shift[0]));
  for (int i = 0; i < buf_size_nonzero_h_div16; i++) {
    __m256i buf0[64];
    const int32_t *in_ptr = input + 16 * i;
    if (is_rect) {
      const __m256i rect_scale = _mm256_set1_epi16(NewInvSqrt2 * 8);
      if (eoby <= 7) {
        for (int j = 0; j < buf_size_nonzero_w; ++j) {
          buf0[j] = _mm256_mulhrs_epi16(
              load_32bit_to_16bit_w8_avx2(in_ptr + j * input_stride),
              rect_scale);
        }
      } else {
        for (int j = 0; j < buf_size_nonzero_w; ++j) {
          buf0[j] = _mm256_mulhrs_epi16(
              load_32bit_to_16bit_w16_avx2(in_ptr + j * input_stride),
              rect_scale);
        }
      }
    } else if (eoby <= 7) {
      load_buffer_32bit_to_16bit_w8_avx2(in_ptr, input_stride, buf0,
                                         buf_size_nonzero_w);
    } else {
      load_buffer_32bit_to_16bit_w16_avx2(in_ptr, input_stride, buf0,
                                          buf_size_nonzero_w);
    }
    row_txfm(buf0, buf0);

    __m256i *buf1_cur = buf1 + (i << 4);
    if (lr_flip) {
      for (int j = 0; j < buf_size_w_div16; ++j) {
        __m256i temp[16];
        flip_buf_avx2(buf0 + 16 * j, temp, 16);
        int offset = txfm_size_row * (buf_size_w_div16 - 1 - j);
        if (eoby <= 7) {
          transpose_16bit_16x16_low8_scale_avx2(temp, buf1_cur + offset,
                                                scale0);
        } else {
          transpose_16bit_16x16_scale_avx2(temp, buf1_cur + offset, scale0);
        }
      }
    } else if (eoby <= 7) {
      for (int j = 0; j < buf_size_w_div16; ++j) {
        transpose_16bit_16x16_low8_scale_avx2(
            buf0 + 16 * j, buf1_cur + txfm_size_row * j, scale0);
      }
    } else {
      for (int j = 0; j < buf_size_w_div16; ++j) {
        transpose_16bit_16x16_scale_avx2(buf0 + 16 * j,
                                         buf1_cur + txfm_size_row * j, scale0);
      }
    }
  }
  const __m256i scale1 = _mm256_set1_epi16(1 << (15 - 4));
  for (int i = 0; i < buf_size_w_div16; i++) {
    __m256i *buf1_cur = buf1 + i * txfm_size_row;
    col_txfm(buf1_cur, buf1_cur);
    uint8_t *out = output + 16 * i;
    int j = ud_flip ? (txfm_size_row - 1) : 0;
    const int step = ud_flip ? -1 : 1;
    for (int r = 0; r < txfm_size_row; r += 2, j += 2 * step) {
      const __m256i res0 = _mm256_mulhrs_epi16(buf1_cur[j], scale1);
      const __m256i res1 = _mm256_mulhrs_epi16(buf1_cur[j + step], scale1);
      write_recon_w16x2_avx2(res0, res1, out + r * stride,
                             out + (r + 1) * stride);
    }
  }
}

static inline void lowbd_inv_txfm2d_add_no_identity_avx2(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  if (eob == 1 && LIKELY(tx_type == DCT_DCT)) {
    const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
    const int txfm_size_col = tx_size_wide[tx_size];
    const int txfm_size_row = tx_size_high[tx_size];
    const int buf_size_w_div16 = txfm_size_col >> 4;
    const int is_rect = (unsigned)(tx_size - TX_4X8) <= (TX_64X32 - TX_4X8);
    const int32_t *cospi = cospi_arr(INV_COS_BIT);
    const __m128i w = _mm_set1_epi16(cospi[32] * 8);
    const __m128i s0 = _mm_set1_epi16(1 << (15 + shift[0]));
    const __m128i s1 = _mm_set1_epi16(1 << (15 - 4));
    __m128i dc = _mm_cvtsi32_si128(input[0]);
    dc = _mm_packs_epi32(dc, dc);
    if (is_rect) {
      dc = _mm_mulhrs_epi16(dc, w);
    }
    dc = _mm_mulhrs_epi16(dc, w);
    dc = _mm_mulhrs_epi16(dc, s0);
    dc = _mm_mulhrs_epi16(dc, w);
    dc = _mm_mulhrs_epi16(dc, s1);
    const __m256i res = _mm256_broadcastw_epi16(dc);
    for (int i = 0; i < buf_size_w_div16; i++) {
      uint8_t *out_col = output + 16 * i;
      for (int r = 0; r < txfm_size_row; r += 2) {
        write_recon_w16x2_avx2(res, res, out_col + r * stride,
                               out_col + (r + 1) * stride);
      }
    }
    return;
  }
  if (tx_size == TX_16X16) {
    if (LIKELY(tx_type == DCT_DCT)) {
      lowbd_inv_txfm2d_16x16_dct_avx2(input, output, stride, eob);
    } else {
      lowbd_inv_txfm2d_16x16_non_dct_avx2(input, output, stride, tx_type, eob);
    }
    return;
  }
  lowbd_inv_txfm2d_add_no_identity_general_avx2(input, output, stride, tx_type,
                                                tx_size, eob);
}

static inline void iidentity_row_16xn_avx2(__m256i *out, const int32_t *input,
                                           int stride, int shift, int height,
                                           int txw_idx, int rect_type) {
  const int32_t *input_row = input;
  const __m256i scale = _mm256_set1_epi16(NewSqrt2list[txw_idx]);
  const __m256i _r = _mm256_set1_epi16((1 << (NewSqrt2Bits - 1)) +
                                       (1 << (NewSqrt2Bits - shift - 1)));
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i scale__r = _mm256_unpacklo_epi16(scale, _r);
  if (rect_type != 1 && rect_type != -1) {
    for (int i = 0; i < height; ++i) {
      const __m256i src = load_32bit_to_16bit_w16_avx2(input_row);
      input_row += stride;
      __m256i lo = _mm256_unpacklo_epi16(src, one);
      __m256i hi = _mm256_unpackhi_epi16(src, one);
      lo = _mm256_madd_epi16(lo, scale__r);
      hi = _mm256_madd_epi16(hi, scale__r);
      lo = _mm256_srai_epi32(lo, NewSqrt2Bits - shift);
      hi = _mm256_srai_epi32(hi, NewSqrt2Bits - shift);
      out[i] = _mm256_packs_epi32(lo, hi);
    }
  } else {
    const __m256i rect_scale =
        _mm256_set1_epi16(NewInvSqrt2 << (15 - NewSqrt2Bits));
    for (int i = 0; i < height; ++i) {
      __m256i src = load_32bit_to_16bit_w16_avx2(input_row);
      src = _mm256_mulhrs_epi16(src, rect_scale);
      input_row += stride;
      __m256i lo = _mm256_unpacklo_epi16(src, one);
      __m256i hi = _mm256_unpackhi_epi16(src, one);
      lo = _mm256_madd_epi16(lo, scale__r);
      hi = _mm256_madd_epi16(hi, scale__r);
      lo = _mm256_srai_epi32(lo, NewSqrt2Bits - shift);
      hi = _mm256_srai_epi32(hi, NewSqrt2Bits - shift);
      out[i] = _mm256_packs_epi32(lo, hi);
    }
  }
}

static inline void iidentity_col_16xn_avx2(uint8_t *output, int stride,
                                           __m256i *buf, int shift, int height,
                                           int txh_idx) {
  const __m256i scale = _mm256_set1_epi16(NewSqrt2list[txh_idx]);
  const __m256i scale__r = _mm256_set1_epi16(1 << (NewSqrt2Bits - 1));
  const __m256i shift__r = _mm256_set1_epi32(1 << (-shift - 1));
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i scale_coeff = _mm256_unpacklo_epi16(scale, scale__r);
  for (int h = 0; h < height; ++h) {
    __m256i lo = _mm256_unpacklo_epi16(buf[h], one);
    __m256i hi = _mm256_unpackhi_epi16(buf[h], one);
    lo = _mm256_madd_epi16(lo, scale_coeff);
    hi = _mm256_madd_epi16(hi, scale_coeff);
    lo = _mm256_srai_epi32(lo, NewSqrt2Bits);
    hi = _mm256_srai_epi32(hi, NewSqrt2Bits);
    lo = _mm256_add_epi32(lo, shift__r);
    hi = _mm256_add_epi32(hi, shift__r);
    lo = _mm256_srai_epi32(lo, -shift);
    hi = _mm256_srai_epi32(hi, -shift);
    const __m256i x = _mm256_packs_epi32(lo, hi);
    write_recon_w16_avx2(x, output);
    output += stride;
  }
}

static inline void lowbd_inv_txfm2d_add_idtx_avx2(const int32_t *input,
                                                  uint8_t *output, int stride,
                                                  TX_SIZE tx_size,
                                                  int32_t eob) {
  (void)eob;
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int col_max = AOMMIN(32, txfm_size_col);
  const int row_max = AOMMIN(32, txfm_size_row);
  const int input_stride = row_max;
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  __m256i buf[32];

  for (int i = 0; i < (col_max >> 4); ++i) {
    for (int j = 0; j < (row_max >> 4); j++) {
      iidentity_row_16xn_avx2(buf, input + j * 16 + i * 16 * input_stride,
                              row_max, shift[0], 16, txw_idx, rect_type);
      transpose_16bit_16x16_avx2(buf, buf);
      iidentity_col_16xn_avx2(output + i * 16 + j * 16 * stride, stride, buf,
                              shift[1], 16, txh_idx);
    }
  }
}

static inline void lowbd_inv_txfm2d_add_h_identity_avx2(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  int eobx, eoby;
  get_eobx_eoby_scan_h_identity(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int txfm_size_row_notzero = AOMMIN(32, txfm_size_row);
  const int input_stride = txfm_size_row_notzero;
  const int buf_size_w_div16 = (eobx + 16) >> 4;
  const int buf_size_h_div16 = (eoby + 16) >> 4;
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);

  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_avx2 col_txfm =
      lowbd_txfm_all_1d_zeros_w16_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  for (int i = 0; i < buf_size_w_div16; i++) {
    __m256i buf0[64];
    for (int j = 0; j < buf_size_h_div16; j++) {
      __m256i *buf0_cur = buf0 + j * 16;
      const int32_t *input_cur = input + i * 16 * input_stride + j * 16;
      iidentity_row_16xn_avx2(buf0_cur, input_cur, input_stride, shift[0], 16,
                              txw_idx, rect_type);
      transpose_16bit_16x16_avx2(buf0_cur, buf0_cur);
    }
    col_txfm(buf0, buf0);
    __m256i mshift = _mm256_set1_epi16(1 << (15 + shift[1]));
    int k = ud_flip ? (txfm_size_row - 1) : 0;
    const int step = ud_flip ? -1 : 1;
    for (int j = 0; j < txfm_size_row; ++j, k += step) {
      __m256i res = _mm256_mulhrs_epi16(buf0[k], mshift);
      write_recon_w16_avx2(res, output + (i << 4) + j * stride);
    }
  }
}

static inline void lowbd_inv_txfm2d_add_v_identity_avx2(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  __m256i buf1[64];
  int eobx, eoby;
  get_eobx_eoby_scan_v_identity(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div16 = txfm_size_col >> 4;
  const int buf_size_h_div16 = (eoby + 16) >> 4;
  const int buf_size_nonzero_w = eobx + 1;
  const int input_stride = AOMMIN(32, txfm_size_row);
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);

  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const transform_1d_avx2 row_txfm =
      lowbd_txfm_all_1d_zeros_w16_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];

  assert(row_txfm != NULL);

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  for (int i = 0; i < buf_size_h_div16; i++) {
    __m256i buf0[64];
    load_buffer_32bit_to_16bit_w16_avx2(input + i * 16, input_stride, buf0,
                                        buf_size_nonzero_w);
    if (rect_type == 1 || rect_type == -1) {
      round_shift_avx2(buf0, buf0, buf_size_nonzero_w);  // rect special code
    }
    row_txfm(buf0, buf0);
    round_shift_16bit_w16_avx2(buf0, txfm_size_col, shift[0]);
    __m256i *_buf1 = buf1;
    if (lr_flip) {
      for (int j = 0; j < buf_size_w_div16; ++j) {
        __m256i temp[16];
        flip_buf_avx2(buf0 + 16 * j, temp, 16);
        transpose_16bit_16x16_avx2(temp,
                                   _buf1 + 16 * (buf_size_w_div16 - 1 - j));
      }
    } else {
      for (int j = 0; j < buf_size_w_div16; ++j) {
        transpose_16bit_16x16_avx2(buf0 + 16 * j, _buf1 + 16 * j);
      }
    }
    for (int j = 0; j < buf_size_w_div16; ++j) {
      iidentity_col_16xn_avx2(output + i * 16 * stride + j * 16, stride,
                              buf1 + j * 16, shift[1], 16, txh_idx);
    }
  }
}

static inline void btf_16_dual256_avx2(const __m256i w0, const __m256i w1,
                                       const __m256i r, const __m256i in0,
                                       const __m256i in1, __m256i *out0,
                                       __m256i *out1) {
  const __m256i t0 = _mm256_unpacklo_epi16(in0, in1);
  const __m256i t1 = _mm256_unpackhi_epi16(in0, in1);
  const __m256i u0 = _mm256_madd_epi16(t0, w0);
  const __m256i u1 = _mm256_madd_epi16(t1, w0);
  const __m256i v0 = _mm256_madd_epi16(t0, w1);
  const __m256i v1 = _mm256_madd_epi16(t1, w1);
  const __m256i c0 = _mm256_srai_epi32(_mm256_add_epi32(u0, r), INV_COS_BIT);
  const __m256i c1 = _mm256_srai_epi32(_mm256_add_epi32(u1, r), INV_COS_BIT);
  const __m256i d0 = _mm256_srai_epi32(_mm256_add_epi32(v0, r), INV_COS_BIT);
  const __m256i d1 = _mm256_srai_epi32(_mm256_add_epi32(v1, r), INV_COS_BIT);
  *out0 = _mm256_packs_epi32(c0, c1);
  *out1 = _mm256_packs_epi32(d0, d1);
}

// Weights for the 8x8 low-eob paths below. They are the cospi_arr(12)
// values (cNN == cospi[NN]), pre-scaled by 8 where they are applied with
// _mm*_mulhrs_epi16, and paired for _mm*_madd_epi16 otherwise.
static_assert(INV_COS_BIT == 12,
              "kInvTxfm8x8CosWeights assumes INV_COS_BIT == 12");

enum {
  kWM32P32,
  kWP32P32,
  kWP32M32,
  kWCos48_16,
  kWCos56_8,
  kWCosM40_24,
  kWIadst02W4,
  kWIadst31W4,
  kWIadstS3Even,
  kWIadstS3Odd,
  kWIadst60M4,
  kWIadst44M20,
  kWIadst36_28,
  kWIadst52_12,
  kWCos32x8,
  kWScaleM1,
  kWScaleM4,
  kInvTxfm8x8CosWeightRows
};

DECLARE_ALIGNED(32, static const int16_t,
                kInvTxfm8x8CosWeights[kInvTxfm8x8CosWeightRows][16]) = {
  // (-c32, c32) x8
  [kWM32P32] = { -2896, 2896, -2896, 2896, -2896, 2896, -2896, 2896, -2896,
                 2896, -2896, 2896, -2896, 2896, -2896, 2896 },
  // (c32, c32) x8
  [kWP32P32] = { 2896, 2896, 2896, 2896, 2896, 2896, 2896, 2896, 2896, 2896,
                 2896, 2896, 2896, 2896, 2896, 2896 },
  // (c32, -c32) x8
  [kWP32M32] = { 2896, -2896, 2896, -2896, 2896, -2896, 2896, -2896, 2896,
                 -2896, 2896, -2896, 2896, -2896, 2896, -2896 },
  // c48*8 (x8) | c16*8 (x8)
  [kWCos48_16] = { 1567 * 8, 1567 * 8, 1567 * 8, 1567 * 8, 1567 * 8, 1567 * 8,
                   1567 * 8, 1567 * 8, 3784 * 8, 3784 * 8, 3784 * 8, 3784 * 8,
                   3784 * 8, 3784 * 8, 3784 * 8, 3784 * 8 },
  // c56*8 (x8) | c8*8 (x8)
  [kWCos56_8] = { 799 * 8, 799 * 8, 799 * 8, 799 * 8, 799 * 8, 799 * 8, 799 * 8,
                  799 * 8, 4017 * 8, 4017 * 8, 4017 * 8, 4017 * 8, 4017 * 8,
                  4017 * 8, 4017 * 8, 4017 * 8 },
  // -c40*8 (x8) | c24*8 (x8)
  [kWCosM40_24] = { -2276 * 8, -2276 * 8, -2276 * 8, -2276 * 8, -2276 * 8,
                    -2276 * 8, -2276 * 8, -2276 * 8, 3406 * 8, 3406 * 8,
                    3406 * 8, 3406 * 8, 3406 * 8, 3406 * 8, 3406 * 8,
                    3406 * 8 },
  // c60*8 (x4), -c4*8 (x4) | c44*8 (x4), -c20*8 (x4)
  [kWIadst02W4] = { 401 * 8, 401 * 8, 401 * 8, 401 * 8, -4076 * 8, -4076 * 8,
                    -4076 * 8, -4076 * 8, 1931 * 8, 1931 * 8, 1931 * 8,
                    1931 * 8, -3612 * 8, -3612 * 8, -3612 * 8, -3612 * 8 },
  // c36*8 (x4), c28*8 (x4) | c52*8 (x4), c12*8 (x4)
  [kWIadst31W4] = { 2598 * 8, 2598 * 8, 2598 * 8, 2598 * 8, 3166 * 8, 3166 * 8,
                    3166 * 8, 3166 * 8, 1189 * 8, 1189 * 8, 1189 * 8, 1189 * 8,
                    3920 * 8, 3920 * 8, 3920 * 8, 3920 * 8 },
  // (c16, c48) x4 | (-c48, c16) x4
  [kWIadstS3Even] = { 3784, 1567, 3784, 1567, 3784, 1567, 3784, 1567, -1567,
                      3784, -1567, 3784, -1567, 3784, -1567, 3784 },
  // (c48, -c16) x4 | (c16, c48) x4
  [kWIadstS3Odd] = { 1567, -3784, 1567, -3784, 1567, -3784, 1567, -3784, 3784,
                     1567, 3784, 1567, 3784, 1567, 3784, 1567 },
  // c60*8 (x8) | -c4*8 (x8)
  [kWIadst60M4] = { 401 * 8, 401 * 8, 401 * 8, 401 * 8, 401 * 8, 401 * 8,
                    401 * 8, 401 * 8, -4076 * 8, -4076 * 8, -4076 * 8,
                    -4076 * 8, -4076 * 8, -4076 * 8, -4076 * 8, -4076 * 8 },
  // c44*8 (x8) | -c20*8 (x8)
  [kWIadst44M20] = { 1931 * 8, 1931 * 8, 1931 * 8, 1931 * 8, 1931 * 8, 1931 * 8,
                     1931 * 8, 1931 * 8, -3612 * 8, -3612 * 8, -3612 * 8,
                     -3612 * 8, -3612 * 8, -3612 * 8, -3612 * 8, -3612 * 8 },
  // c36*8 (x8) | c28*8 (x8)
  [kWIadst36_28] = { 2598 * 8, 2598 * 8, 2598 * 8, 2598 * 8, 2598 * 8, 2598 * 8,
                     2598 * 8, 2598 * 8, 3166 * 8, 3166 * 8, 3166 * 8, 3166 * 8,
                     3166 * 8, 3166 * 8, 3166 * 8, 3166 * 8 },
  // c52*8 (x8) | c12*8 (x8)
  [kWIadst52_12] = { 1189 * 8, 1189 * 8, 1189 * 8, 1189 * 8, 1189 * 8, 1189 * 8,
                     1189 * 8, 1189 * 8, 3920 * 8, 3920 * 8, 3920 * 8, 3920 * 8,
                     3920 * 8, 3920 * 8, 3920 * 8, 3920 * 8 },
  // c32*8 (x16)
  [kWCos32x8] = { 2896 * 8, 2896 * 8, 2896 * 8, 2896 * 8, 2896 * 8, 2896 * 8,
                  2896 * 8, 2896 * 8, 2896 * 8, 2896 * 8, 2896 * 8, 2896 * 8,
                  2896 * 8, 2896 * 8, 2896 * 8, 2896 * 8 },
  // 1 << 14: mulhrs rounding shift by 1
  [kWScaleM1] = { 16384, 16384, 16384, 16384, 16384, 16384, 16384, 16384, 16384,
                  16384, 16384, 16384, 16384, 16384, 16384, 16384 },
  // 1 << 11: mulhrs rounding shift by 4
  [kWScaleM4] = { 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
                  2048, 2048, 2048, 2048, 2048, 2048 },
};

DECLARE_ALIGNED(16, static const int16_t, kInvTxfm8x8Cos128[][8]) = {
  // [0] alternating (c60*8, -c4*8)
  { 401 * 8, -4076 * 8, 401 * 8, -4076 * 8, 401 * 8, -4076 * 8, 401 * 8,
    -4076 * 8 },
  // [1] (c16, c48, c48, -c16) x2
  { 3784, 1567, 1567, -3784, 3784, 1567, 1567, -3784 },
  // [2] (c32, c32, c32, -c32) x2
  { 2896, 2896, 2896, -2896, 2896, 2896, 2896, -2896 },
};

DECLARE_ALIGNED(16, static const uint8_t, kInvTxfm8x8PermMask[][16]) = {
  // [0] [x0, x4, x6, x2, x3, x7, x5, x1] from [x0, x1, x4, x5, x2, x3, x6, x7]
  { 0, 1, 4, 5, 12, 13, 8, 9, 10, 11, 14, 15, 6, 7, 2, 3 },
  // [1] reverse 8 int16 lanes
  { 14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1 },
};

static AOM_FORCE_INLINE void write_8x8_256_avx2(const __m256i in_256[4],
                                                uint8_t *output, int stride) {
  const __m256i scale =
      _mm256_load_si256((const __m256i *)kInvTxfm8x8CosWeights[kWScaleM4]);
  const __m256i zero = _mm256_setzero_si256();

  const __m256i s0 = _mm256_mulhrs_epi16(in_256[0], scale);
  const __m256i s1 = _mm256_mulhrs_epi16(in_256[1], scale);
  const __m256i s2 = _mm256_mulhrs_epi16(in_256[2], scale);
  const __m256i s3 = _mm256_mulhrs_epi16(in_256[3], scale);

  const __m128i v0 = _mm_loadl_epi64((const __m128i *)output);
  const __m128i v1 = _mm_loadl_epi64((const __m128i *)(output + stride));
  const __m128i v2 = _mm_loadl_epi64((const __m128i *)(output + 2 * stride));
  const __m128i v3 = _mm_loadl_epi64((const __m128i *)(output + 3 * stride));
  const __m128i v4 = _mm_loadl_epi64((const __m128i *)(output + 4 * stride));
  const __m128i v5 = _mm_loadl_epi64((const __m128i *)(output + 5 * stride));
  const __m128i v6 = _mm_loadl_epi64((const __m128i *)(output + 6 * stride));
  const __m128i v7 = _mm_loadl_epi64((const __m128i *)(output + 7 * stride));

  const __m256i v01 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(v0), v1, 1);
  const __m256i v23 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(v2), v3, 1);
  const __m256i v45 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(v4), v5, 1);
  const __m256i v67 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(v6), v7, 1);

  const __m256i x0 = _mm256_adds_epi16(s0, _mm256_unpacklo_epi8(v01, zero));
  const __m256i x1 = _mm256_adds_epi16(s1, _mm256_unpacklo_epi8(v23, zero));
  const __m256i x2 = _mm256_adds_epi16(s2, _mm256_unpacklo_epi8(v45, zero));
  const __m256i x3 = _mm256_adds_epi16(s3, _mm256_unpacklo_epi8(v67, zero));

  const __m256i res_0123 = _mm256_packus_epi16(x0, x1);
  const __m256i res_4567 = _mm256_packus_epi16(x2, x3);

  const __m128i res_02 = _mm256_castsi256_si128(res_0123);
  const __m128i res_13 = _mm256_extracti128_si256(res_0123, 1);
  const __m128i res_46 = _mm256_castsi256_si128(res_4567);
  const __m128i res_57 = _mm256_extracti128_si256(res_4567, 1);

  _mm_storel_epi64((__m128i *)output, res_02);
  _mm_storel_epi64((__m128i *)(output + stride), res_13);
  _mm_storel_epi64((__m128i *)(output + 2 * stride),
                   _mm_unpackhi_epi64(res_02, res_02));
  _mm_storel_epi64((__m128i *)(output + 3 * stride),
                   _mm_unpackhi_epi64(res_13, res_13));
  _mm_storel_epi64((__m128i *)(output + 4 * stride), res_46);
  _mm_storel_epi64((__m128i *)(output + 5 * stride), res_57);
  _mm_storel_epi64((__m128i *)(output + 6 * stride),
                   _mm_unpackhi_epi64(res_46, res_46));
  _mm_storel_epi64((__m128i *)(output + 7 * stride),
                   _mm_unpackhi_epi64(res_57, res_57));
}

DECLARE_ALIGNED(32, static const uint8_t, kInvTxfm8x8Perm256Rev16[32]) = {
  14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1,
  14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1,
};

static AOM_FORCE_INLINE void round_and_transpose_pairs_avx2(
    const __m256i y01_45, const __m256i y23_67, int lr_flip, __m128i cols[4]) {
  const __m256i scale =
      _mm256_load_si256((const __m256i *)kInvTxfm8x8CosWeights[kWScaleM1]);
  const __m256i s0 = _mm256_mulhrs_epi16(y01_45, scale);
  const __m256i s1 = _mm256_mulhrs_epi16(y23_67, scale);
  __m256i c01 = _mm256_permute4x64_epi64(_mm256_unpacklo_epi32(s0, s1), 0xd8);
  __m256i c23 = _mm256_permute4x64_epi64(_mm256_unpackhi_epi32(s0, s1), 0xd8);
  if (UNLIKELY(lr_flip)) {
    const __m256i rev =
        _mm256_load_si256((const __m256i *)kInvTxfm8x8Perm256Rev16);
    c01 = _mm256_shuffle_epi8(c01, rev);
    c23 = _mm256_shuffle_epi8(c23, rev);
  }
  cols[0] = _mm256_castsi256_si128(c01);
  cols[1] = _mm256_extracti128_si256(c01, 1);
  cols[2] = _mm256_castsi256_si128(c23);
  cols[3] = _mm256_extracti128_si256(c23, 1);
}

static AOM_FORCE_INLINE void idct8_low4_w4_pairs_avx2(const int32_t *in,
                                                      __m256i *y01_45,
                                                      __m256i *y23_67) {
  const __m128i a0 = _mm_load_si128((const __m128i *)(in + 0));
  const __m128i a1 = _mm_load_si128((const __m128i *)(in + 8));
  const __m128i a2 = _mm_load_si128((const __m128i *)(in + 16));
  const __m128i a3 = _mm_load_si128((const __m128i *)(in + 24));
  const __m128i in0 = _mm_packs_epi32(a0, a0);
  const __m128i in1 = _mm_packs_epi32(a1, a1);
  const __m128i in2 = _mm_packs_epi32(a2, a2);
  const __m128i in3 = _mm_packs_epi32(a3, a3);

  const __m128i x0 = _mm_mulhrs_epi16(
      in0, _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWCos32x8]));
  const __m128i x2_a = _mm_mulhrs_epi16(
      in2, _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWCos48_16]));
  const __m128i x2_b = _mm_mulhrs_epi16(
      in2,
      _mm_load_si128((const __m128i *)&kInvTxfm8x8CosWeights[kWCos48_16][8]));

  const __m128i e0 = _mm_adds_epi16(x0, x2_b);
  const __m128i e1 = _mm_adds_epi16(x0, x2_a);
  const __m128i e2 = _mm_subs_epi16(x0, x2_a);
  const __m128i e3 = _mm_subs_epi16(x0, x2_b);

  const __m128i o_a = _mm_mulhrs_epi16(
      in1, _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWCos56_8]));
  const __m128i o_d = _mm_mulhrs_epi16(
      in1,
      _mm_load_si128((const __m128i *)&kInvTxfm8x8CosWeights[kWCos56_8][8]));
  const __m128i o_b = _mm_mulhrs_epi16(
      in3, _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWCosM40_24]));
  const __m128i o_c = _mm_mulhrs_epi16(
      in3,
      _mm_load_si128((const __m128i *)&kInvTxfm8x8CosWeights[kWCosM40_24][8]));

  const __m128i s3_4 = _mm_adds_epi16(o_a, o_b);
  const __m128i s3_5 = _mm_subs_epi16(o_a, o_b);
  const __m128i s3_6 = _mm_subs_epi16(o_d, o_c);
  const __m128i s3_7 = _mm_adds_epi16(o_d, o_c);

  const __m128i t56 = _mm_unpacklo_epi16(s3_5, s3_6);
  const __m128i r128 = _mm_set1_epi32(1 << (INV_COS_BIT - 1));
  const __m128i m5 = _mm_srai_epi32(
      _mm_add_epi32(
          _mm_madd_epi16(
              t56,
              _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWM32P32])),
          r128),
      INV_COS_BIT);
  const __m128i m6 = _mm_srai_epi32(
      _mm_add_epi32(
          _mm_madd_epi16(
              t56,
              _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWP32P32])),
          r128),
      INV_COS_BIT);
  const __m128i s4_5 = _mm_packs_epi32(m5, m5);
  const __m128i s4_6 = _mm_packs_epi32(m6, m6);

  const __m128i out0 = _mm_adds_epi16(e0, s3_7);
  const __m128i out7 = _mm_subs_epi16(e0, s3_7);
  const __m128i out1 = _mm_adds_epi16(e1, s4_6);
  const __m128i out6 = _mm_subs_epi16(e1, s4_6);
  const __m128i out2 = _mm_adds_epi16(e2, s4_5);
  const __m128i out5 = _mm_subs_epi16(e2, s4_5);
  const __m128i out3 = _mm_adds_epi16(e3, s3_4);
  const __m128i out4 = _mm_subs_epi16(e3, s3_4);

  *y01_45 = _mm256_inserti128_si256(
      _mm256_castsi128_si256(_mm_unpacklo_epi16(out0, out1)),
      _mm_unpacklo_epi16(out4, out5), 1);
  *y23_67 = _mm256_inserti128_si256(
      _mm256_castsi128_si256(_mm_unpacklo_epi16(out2, out3)),
      _mm_unpacklo_epi16(out6, out7), 1);
}

static AOM_FORCE_INLINE void iadst8_low4_w4_pairs_avx2(const int32_t *in,
                                                       __m256i *y01_45,
                                                       __m256i *y23_67) {
  const __m128i a0 = _mm_load_si128((const __m128i *)(in + 0));
  const __m128i a1 = _mm_load_si128((const __m128i *)(in + 8));
  const __m128i a2 = _mm_load_si128((const __m128i *)(in + 16));
  const __m128i a3 = _mm_load_si128((const __m128i *)(in + 24));
  const __m128i p0 = _mm_packs_epi32(a0, a0);
  const __m128i p1 = _mm_packs_epi32(a1, a1);
  const __m128i p2 = _mm_packs_epi32(a2, a2);
  const __m128i p3 = _mm_packs_epi32(a3, a3);

  const __m256i row02 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(p0), p2, 1);
  const __m256i row31 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(p3), p1, 1);

  const __m256i y02 = _mm256_mulhrs_epi16(
      row02,
      _mm256_load_si256((const __m256i *)kInvTxfm8x8CosWeights[kWIadst02W4]));
  const __m256i y31 = _mm256_mulhrs_epi16(
      row31,
      _mm256_load_si256((const __m256i *)kInvTxfm8x8CosWeights[kWIadst31W4]));

  const __m256i y03 = _mm256_adds_epi16(y02, y31);
  const __m256i y47 = _mm256_subs_epi16(y02, y31);

  const __m256i r256 = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));
  const __m256i pair45_67 =
      _mm256_unpacklo_epi16(y47, _mm256_shuffle_epi32(y47, 0x4e));
  const __m256i me = _mm256_srai_epi32(
      _mm256_add_epi32(
          _mm256_madd_epi16(
              pair45_67,
              _mm256_load_si256(
                  (const __m256i *)kInvTxfm8x8CosWeights[kWIadstS3Even])),
          r256),
      INV_COS_BIT);
  const __m256i mo = _mm256_srai_epi32(
      _mm256_add_epi32(
          _mm256_madd_epi16(
              pair45_67,
              _mm256_load_si256(
                  (const __m256i *)kInvTxfm8x8CosWeights[kWIadstS3Odd])),
          r256),
      INV_COS_BIT);
  const __m256i y47_p = _mm256_packs_epi32(me, mo);

  const __m256i pa = _mm256_permute2x128_si256(y03, y47_p, 0x20);
  const __m256i pb = _mm256_permute2x128_si256(y03, y47_p, 0x31);
  const __m256i sum_0145 = _mm256_adds_epi16(pa, pb);
  const __m256i dif_2367 = _mm256_subs_epi16(pa, pb);

  const __m256i pair23_67 =
      _mm256_unpacklo_epi16(dif_2367, _mm256_shuffle_epi32(dif_2367, 0x4e));
  const __m256i ne = _mm256_srai_epi32(
      _mm256_add_epi32(
          _mm256_madd_epi16(
              pair23_67, _mm256_load_si256(
                             (const __m256i *)kInvTxfm8x8CosWeights[kWP32P32])),
          r256),
      INV_COS_BIT);
  const __m256i no = _mm256_srai_epi32(
      _mm256_add_epi32(
          _mm256_madd_epi16(
              pair23_67, _mm256_load_si256(
                             (const __m256i *)kInvTxfm8x8CosWeights[kWP32M32])),
          r256),
      INV_COS_BIT);
  const __m256i dif_out = _mm256_packs_epi32(ne, no);

  const __m256i zero = _mm256_setzero_si256();
  const __m256i neg_sum = _mm256_subs_epi16(zero, sum_0145);
  const __m256i neg_dif = _mm256_subs_epi16(zero, dif_out);
  const __m256i s_m = _mm256_blend_epi32(sum_0145, neg_sum, 0x3c);
  const __m256i d_m = _mm256_blend_epi32(neg_dif, dif_out, 0x3c);

  const __m256i s_01_67 = _mm256_permute4x64_epi64(s_m, 0x78);
  const __m256i d_23_45 = _mm256_permute4x64_epi64(d_m, 0xd2);
  const __m256i vp01_67 =
      _mm256_unpacklo_epi16(s_01_67, _mm256_shuffle_epi32(s_01_67, 0x4e));
  const __m256i vp23_45 =
      _mm256_unpacklo_epi16(d_23_45, _mm256_shuffle_epi32(d_23_45, 0x4e));
  *y01_45 = _mm256_blend_epi32(vp01_67, vp23_45, 0xf0);
  *y23_67 =
      _mm256_inserti128_si256(vp23_45, _mm256_extracti128_si256(vp01_67, 1), 1);
}

static AOM_FORCE_INLINE void idct8_low4_xmm_avx2(const __m128i cols[4],
                                                 int ud_flip, __m256i out[4]) {
  const __m128i in0 = cols[0];
  const __m128i in1 = cols[1];
  const __m128i in2 = cols[2];
  const __m128i in3 = cols[3];

  const __m128i x0 = _mm_mulhrs_epi16(
      in0, _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWCos32x8]));
  const __m128i x2_a = _mm_mulhrs_epi16(
      in2, _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWCos48_16]));
  const __m128i x2_b = _mm_mulhrs_epi16(
      in2,
      _mm_load_si128((const __m128i *)&kInvTxfm8x8CosWeights[kWCos48_16][8]));

  const __m128i e0 = _mm_adds_epi16(x0, x2_b);
  const __m128i e1 = _mm_adds_epi16(x0, x2_a);
  const __m128i e2 = _mm_subs_epi16(x0, x2_a);
  const __m128i e3 = _mm_subs_epi16(x0, x2_b);

  const __m128i o_a = _mm_mulhrs_epi16(
      in1, _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWCos56_8]));
  const __m128i o_d = _mm_mulhrs_epi16(
      in1,
      _mm_load_si128((const __m128i *)&kInvTxfm8x8CosWeights[kWCos56_8][8]));
  const __m128i o_b = _mm_mulhrs_epi16(
      in3, _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWCosM40_24]));
  const __m128i o_c = _mm_mulhrs_epi16(
      in3,
      _mm_load_si128((const __m128i *)&kInvTxfm8x8CosWeights[kWCosM40_24][8]));

  const __m128i s3_4 = _mm_adds_epi16(o_a, o_b);
  const __m128i s3_5 = _mm_subs_epi16(o_a, o_b);
  const __m128i s3_6 = _mm_subs_epi16(o_d, o_c);
  const __m128i s3_7 = _mm_adds_epi16(o_d, o_c);

  const __m128i t56_lo = _mm_unpacklo_epi16(s3_5, s3_6);
  const __m128i t56_hi = _mm_unpackhi_epi16(s3_5, s3_6);
  const __m128i r128 = _mm_set1_epi32(1 << (INV_COS_BIT - 1));
  const __m128i w_m32p32 =
      _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWM32P32]);
  const __m128i w_p32p32 =
      _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWP32P32]);
  const __m128i m5_lo = _mm_srai_epi32(
      _mm_add_epi32(_mm_madd_epi16(t56_lo, w_m32p32), r128), INV_COS_BIT);
  const __m128i m5_hi = _mm_srai_epi32(
      _mm_add_epi32(_mm_madd_epi16(t56_hi, w_m32p32), r128), INV_COS_BIT);
  const __m128i m6_lo = _mm_srai_epi32(
      _mm_add_epi32(_mm_madd_epi16(t56_lo, w_p32p32), r128), INV_COS_BIT);
  const __m128i m6_hi = _mm_srai_epi32(
      _mm_add_epi32(_mm_madd_epi16(t56_hi, w_p32p32), r128), INV_COS_BIT);
  const __m128i s4_5 = _mm_packs_epi32(m5_lo, m5_hi);
  const __m128i s4_6 = _mm_packs_epi32(m6_lo, m6_hi);

  const __m128i out0 = _mm_adds_epi16(e0, s3_7);
  const __m128i out7 = _mm_subs_epi16(e0, s3_7);
  const __m128i out1 = _mm_adds_epi16(e1, s4_6);
  const __m128i out6 = _mm_subs_epi16(e1, s4_6);
  const __m128i out2 = _mm_adds_epi16(e2, s4_5);
  const __m128i out5 = _mm_subs_epi16(e2, s4_5);
  const __m128i out3 = _mm_adds_epi16(e3, s3_4);
  const __m128i out4 = _mm_subs_epi16(e3, s3_4);

  if (UNLIKELY(ud_flip)) {
    out[0] = _mm256_inserti128_si256(_mm256_castsi128_si256(out7), out6, 1);
    out[1] = _mm256_inserti128_si256(_mm256_castsi128_si256(out5), out4, 1);
    out[2] = _mm256_inserti128_si256(_mm256_castsi128_si256(out3), out2, 1);
    out[3] = _mm256_inserti128_si256(_mm256_castsi128_si256(out1), out0, 1);
  } else {
    out[0] = _mm256_inserti128_si256(_mm256_castsi128_si256(out0), out1, 1);
    out[1] = _mm256_inserti128_si256(_mm256_castsi128_si256(out2), out3, 1);
    out[2] = _mm256_inserti128_si256(_mm256_castsi128_si256(out4), out5, 1);
    out[3] = _mm256_inserti128_si256(_mm256_castsi128_si256(out6), out7, 1);
  }
}

static AOM_FORCE_INLINE void iadst8_low4_xmm_avx2(const __m128i cols[4],
                                                  int ud_flip, __m256i out[4]) {
  const __m256i c0 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(cols[0]), cols[0], 1);
  const __m256i c1 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(cols[1]), cols[1], 1);
  const __m256i c2 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(cols[2]), cols[2], 1);
  const __m256i c3 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(cols[3]), cols[3], 1);

  const __m256i x01 = _mm256_mulhrs_epi16(
      c0,
      _mm256_load_si256((const __m256i *)kInvTxfm8x8CosWeights[kWIadst60M4]));
  const __m256i x23 = _mm256_mulhrs_epi16(
      c2,
      _mm256_load_si256((const __m256i *)kInvTxfm8x8CosWeights[kWIadst44M20]));
  const __m256i x45 = _mm256_mulhrs_epi16(
      c3,
      _mm256_load_si256((const __m256i *)kInvTxfm8x8CosWeights[kWIadst36_28]));
  const __m256i x67 = _mm256_mulhrs_epi16(
      c1,
      _mm256_load_si256((const __m256i *)kInvTxfm8x8CosWeights[kWIadst52_12]));

  const __m256i t01 = _mm256_adds_epi16(x01, x45);
  const __m256i t45 = _mm256_subs_epi16(x01, x45);
  const __m256i t23 = _mm256_adds_epi16(x23, x67);
  const __m256i t67 = _mm256_subs_epi16(x23, x67);

  const __m256i r256 = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));
  __m256i a46 = _mm256_permute2x128_si256(t45, t67, 0x20);
  __m256i b57 = _mm256_permute2x128_si256(t45, t67, 0x31);
  btf_16_dual256_avx2(
      _mm256_load_si256((const __m256i *)kInvTxfm8x8CosWeights[kWIadstS3Even]),
      _mm256_load_si256((const __m256i *)kInvTxfm8x8CosWeights[kWIadstS3Odd]),
      r256, a46, b57, &a46, &b57);

  const __m256i s45 = _mm256_permute2x128_si256(a46, b57, 0x20);
  const __m256i s67 = _mm256_permute2x128_si256(a46, b57, 0x31);

  const __m256i u01 = _mm256_adds_epi16(t01, t23);
  const __m256i u23 = _mm256_subs_epi16(t01, t23);
  const __m256i u45 = _mm256_adds_epi16(s45, s67);
  const __m256i u67 = _mm256_subs_epi16(s45, s67);

  __m256i v26 = _mm256_permute2x128_si256(u23, u67, 0x20);
  __m256i v37 = _mm256_permute2x128_si256(u23, u67, 0x31);
  btf_16_dual256_avx2(
      _mm256_load_si256((const __m256i *)kInvTxfm8x8CosWeights[kWP32P32]),
      _mm256_load_si256((const __m256i *)kInvTxfm8x8CosWeights[kWP32M32]), r256,
      v26, v37, &v26, &v37);

  const __m256i zero = _mm256_setzero_si256();
  const __m256i nu01 = _mm256_subs_epi16(zero, u01);
  const __m256i nu45 = _mm256_subs_epi16(zero, u45);
  const __m256i nv26 = _mm256_subs_epi16(zero, v26);
  const __m256i nv37 = _mm256_subs_epi16(zero, v37);

  const __m256i r01 = _mm256_permute2x128_si256(u01, nu45, 0x20);
  const __m256i r23 = _mm256_permute2x128_si256(v26, nv26, 0x21);
  const __m256i r45 = _mm256_permute2x128_si256(v37, nv37, 0x30);
  const __m256i r67 = _mm256_permute2x128_si256(u45, nu01, 0x31);

  if (UNLIKELY(ud_flip)) {
    out[0] = _mm256_permute2x128_si256(r67, r67, 0x01);
    out[1] = _mm256_permute2x128_si256(r45, r45, 0x01);
    out[2] = _mm256_permute2x128_si256(r23, r23, 0x01);
    out[3] = _mm256_permute2x128_si256(r01, r01, 0x01);
  } else {
    out[0] = r01;
    out[1] = r23;
    out[2] = r45;
    out[3] = r67;
  }
}

static AOM_FORCE_INLINE __m128i iadst8_low1_row_round_avx2(int32_t dc32,
                                                           int lr_flip) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i dc_s =
      _mm_broadcastw_epi16(_mm_packs_epi32(_mm_cvtsi32_si128(dc32), zero));
  const __m128i y01 = _mm_mulhrs_epi16(
      dc_s, _mm_load_si128((const __m128i *)kInvTxfm8x8Cos128[0]));
  const __m128i r128 = _mm_set1_epi32(1 << (INV_COS_BIT - 1));
  const __m128i u45 = _mm_srai_epi32(
      _mm_add_epi32(
          _mm_madd_epi16(y01,
                         _mm_load_si128((const __m128i *)kInvTxfm8x8Cos128[1])),
          r128),
      INV_COS_BIT);
  const __m128i x45 = _mm_packs_epi32(u45, u45);
  const __m128i u0145 = _mm_unpacklo_epi64(y01, x45);
  const __m128i v2367 = _mm_srai_epi32(
      _mm_add_epi32(
          _mm_madd_epi16(u0145,
                         _mm_load_si128((const __m128i *)kInvTxfm8x8Cos128[2])),
          r128),
      INV_COS_BIT);
  const __m128i x2367 = _mm_packs_epi32(v2367, v2367);
  const __m128i even_odd =
      _mm_unpacklo_epi64(_mm_unpacklo_epi32(y01, x45), x2367);
  const __m128i p = _mm_shuffle_epi8(
      even_odd, _mm_load_si128((const __m128i *)kInvTxfm8x8PermMask[0]));
  const __m128i np = _mm_subs_epi16(zero, p);
  __m128i row_vec = _mm_blend_epi16(p, np, 0xaa);
  row_vec = _mm_mulhrs_epi16(
      row_vec,
      _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWScaleM1]));
  if (UNLIKELY(lr_flip)) {
    row_vec = _mm_shuffle_epi8(
        row_vec, _mm_load_si128((const __m128i *)kInvTxfm8x8PermMask[1]));
  }
  return row_vec;
}

static AOM_FORCE_INLINE void iadst8_low1_col_avx2(const __m128i row_vec,
                                                  int ud_flip, __m256i out[4]) {
  const __m128i x0 = _mm_mulhrs_epi16(
      row_vec,
      _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWIadst60M4]));
  const __m128i x1 = _mm_mulhrs_epi16(
      row_vec,
      _mm_load_si128((const __m128i *)&kInvTxfm8x8CosWeights[kWIadst60M4][8]));
  const __m128i t_lo = _mm_unpacklo_epi16(x0, x1);
  const __m128i t_hi = _mm_unpackhi_epi16(x0, x1);
  const __m128i r128 = _mm_set1_epi32(1 << (INV_COS_BIT - 1));
  const __m128i w_s3_even =
      _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWIadstS3Even]);
  const __m128i w_s3_odd =
      _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWIadstS3Odd]);
  const __m128i u4_lo = _mm_srai_epi32(
      _mm_add_epi32(_mm_madd_epi16(t_lo, w_s3_even), r128), INV_COS_BIT);
  const __m128i u4_hi = _mm_srai_epi32(
      _mm_add_epi32(_mm_madd_epi16(t_hi, w_s3_even), r128), INV_COS_BIT);
  const __m128i u5_lo = _mm_srai_epi32(
      _mm_add_epi32(_mm_madd_epi16(t_lo, w_s3_odd), r128), INV_COS_BIT);
  const __m128i u5_hi = _mm_srai_epi32(
      _mm_add_epi32(_mm_madd_epi16(t_hi, w_s3_odd), r128), INV_COS_BIT);
  const __m128i x4 = _mm_packs_epi32(u4_lo, u4_hi);
  const __m128i x5 = _mm_packs_epi32(u5_lo, u5_hi);

  const __m256i x01 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(x0), x1, 1);
  const __m256i x45 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(x4), x5, 1);
  __m256i x26 = _mm256_inserti128_si256(_mm256_castsi128_si256(x0), x4, 1);
  __m256i x37 = _mm256_inserti128_si256(_mm256_castsi128_si256(x1), x5, 1);
  const __m256i r256 = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));
  btf_16_dual256_avx2(
      _mm256_load_si256((const __m256i *)kInvTxfm8x8CosWeights[kWP32P32]),
      _mm256_load_si256((const __m256i *)kInvTxfm8x8CosWeights[kWP32M32]), r256,
      x26, x37, &x26, &x37);

  const __m256i zero = _mm256_setzero_si256();
  const __m256i nx01 = _mm256_subs_epi16(zero, x01);
  const __m256i nx45 = _mm256_subs_epi16(zero, x45);
  const __m256i nx26 = _mm256_subs_epi16(zero, x26);
  const __m256i nx37 = _mm256_subs_epi16(zero, x37);

  const __m256i r01 = _mm256_permute2x128_si256(x01, nx45, 0x20);
  const __m256i r23 = _mm256_permute2x128_si256(x26, nx26, 0x21);
  const __m256i r45 = _mm256_permute2x128_si256(x37, nx37, 0x30);
  const __m256i r67 = _mm256_permute2x128_si256(x45, nx01, 0x31);

  if (UNLIKELY(ud_flip)) {
    out[0] = _mm256_permute2x128_si256(r67, r67, 0x01);
    out[1] = _mm256_permute2x128_si256(r45, r45, 0x01);
    out[2] = _mm256_permute2x128_si256(r23, r23, 0x01);
    out[3] = _mm256_permute2x128_si256(r01, r01, 0x01);
  } else {
    out[0] = r01;
    out[1] = r23;
    out[2] = r45;
    out[3] = r67;
  }
}

static inline __m256i pair_set_w16x2_epi16(int16_t a, int16_t b, int16_t c,
                                           int16_t d) {
  const int32_t lo =
      (int32_t)(((uint32_t)(uint16_t)a) | (((uint32_t)(uint16_t)b) << 16));
  const int32_t hi =
      (int32_t)(((uint32_t)(uint16_t)c) | (((uint32_t)(uint16_t)d) << 16));
  return _mm256_setr_epi32(lo, lo, lo, lo, hi, hi, hi, hi);
}

static AOM_FORCE_INLINE __m256i btf_16_w16_out0_avx2(const __m256i w,
                                                     const __m256i in0,
                                                     const __m256i in1,
                                                     const __m256i r,
                                                     const int32_t cos_bit) {
  const __m256i t0 = _mm256_unpacklo_epi16(in0, in1);
  const __m256i t1 = _mm256_unpackhi_epi16(in0, in1);
  const __m256i u0 = _mm256_madd_epi16(t0, w);
  const __m256i u1 = _mm256_madd_epi16(t1, w);
  const __m256i c0 = _mm256_srai_epi32(_mm256_add_epi32(u0, r), cos_bit);
  const __m256i c1 = _mm256_srai_epi32(_mm256_add_epi32(u1, r), cos_bit);
  return _mm256_packs_epi32(c0, c1);
}

static AOM_FORCE_INLINE void idct8x8_dual_avx2(const __m256i *p, __m256i *q) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m256i w_s2_0 =
      pair_set_w16x2_epi16(cospi[56], -cospi[8], cospi[24], -cospi[40]);
  const __m256i w_s2_1 =
      pair_set_w16x2_epi16(cospi[8], cospi[56], cospi[40], cospi[24]);
  const __m256i w_s3_0 =
      pair_set_w16x2_epi16(cospi[32], cospi[32], cospi[48], -cospi[16]);
  const __m256i w_s3_1 =
      pair_set_w16x2_epi16(cospi[32], -cospi[32], cospi[16], cospi[48]);
  const __m256i w_x56 =
      pair_set_w16x2_epi16(-cospi[32], cospi[32], cospi[32], cospi[32]);

  __m256i x45 = _mm256_permute2x128_si256(p[0], p[2], 0x31);
  __m256i x76 = _mm256_permute2x128_si256(p[3], p[1], 0x20);
  btf_16_w16_avx2(w_s2_0, w_s2_1, &x45, &x76, r, INV_COS_BIT);

  __m256i x02 = _mm256_permute2x128_si256(p[0], p[1], 0x30);
  __m256i x13 = _mm256_permute2x128_si256(p[2], p[3], 0x30);
  btf_16_w16_avx2(w_s3_0, w_s3_1, &x02, &x13, r, INV_COS_BIT);

  const __m256i in_01 = _mm256_permute2x128_si256(x02, x13, 0x20);
  const __m256i in_32 = _mm256_permute2x128_si256(x13, x02, 0x31);
  const __m256i s01 = _mm256_adds_epi16(in_01, in_32);
  const __m256i d32 = _mm256_subs_epi16(in_01, in_32);

  const __m256i in_47 = _mm256_permute2x128_si256(x45, x76, 0x20);
  const __m256i in_56 = _mm256_permute2x128_si256(x45, x76, 0x31);
  const __m256i s47 = _mm256_adds_epi16(in_47, in_56);
  const __m256i d56 = _mm256_subs_epi16(in_47, in_56);

  const __m256i v0 = d56;
  const __m256i v1 = _mm256_permute2x128_si256(d56, d56, 0x01);
  const __m256i new_x56 = btf_16_w16_out0_avx2(w_x56, v0, v1, r, INV_COS_BIT);

  const __m256i in_76 = _mm256_permute2x128_si256(s47, new_x56, 0x31);
  const __m256i in_45 = _mm256_permute2x128_si256(s47, new_x56, 0x20);

  q[0] = _mm256_adds_epi16(s01, in_76);
  q[1] = _mm256_adds_epi16(d32, in_45);
  q[2] = _mm256_subs_epi16(d32, in_45);
  q[3] = _mm256_subs_epi16(s01, in_76);
}

static AOM_FORCE_INLINE void iadst8x8_dual_avx2(const __m256i *p, __m256i *q) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));
  const __m256i zero = _mm256_setzero_si256();

  const __m256i w_s2_01_0 =
      pair_set_w16x2_epi16(cospi[4], cospi[60], cospi[20], cospi[44]);
  const __m256i w_s2_01_1 =
      pair_set_w16x2_epi16(cospi[60], -cospi[4], cospi[44], -cospi[20]);
  const __m256i w_s2_23_0 =
      pair_set_w16x2_epi16(cospi[36], cospi[28], cospi[52], cospi[12]);
  const __m256i w_s2_23_1 =
      pair_set_w16x2_epi16(cospi[28], -cospi[36], cospi[12], -cospi[52]);
  const __m256i w_s4_0 =
      pair_set_w16x2_epi16(cospi[16], cospi[48], -cospi[48], cospi[16]);
  const __m256i w_s4_1 =
      pair_set_w16x2_epi16(cospi[48], -cospi[16], cospi[16], cospi[48]);
  const __m256i w_s6_0 = pair_set_w16_epi16(cospi[32], cospi[32]);
  const __m256i w_s6_1 = pair_set_w16_epi16(cospi[32], -cospi[32]);

  __m256i v02 = _mm256_permute2x128_si256(p[3], p[2], 0x30);
  __m256i v13 = _mm256_permute2x128_si256(p[0], p[1], 0x30);
  __m256i v46 = _mm256_permute2x128_si256(p[1], p[0], 0x30);
  __m256i v57 = _mm256_permute2x128_si256(p[2], p[3], 0x30);

  btf_16_w16_avx2(w_s2_01_0, w_s2_01_1, &v02, &v13, r, INV_COS_BIT);
  btf_16_w16_avx2(w_s2_23_0, w_s2_23_1, &v46, &v57, r, INV_COS_BIT);

  const __m256i t02 = v02;
  v02 = _mm256_adds_epi16(t02, v46);
  v46 = _mm256_subs_epi16(t02, v46);
  const __m256i t13 = v13;
  v13 = _mm256_adds_epi16(t13, v57);
  v57 = _mm256_subs_epi16(t13, v57);

  btf_16_w16_avx2(w_s4_0, w_s4_1, &v46, &v57, r, INV_COS_BIT);

  __m256i u04 = _mm256_permute2x128_si256(v02, v46, 0x20);
  __m256i u26 = _mm256_permute2x128_si256(v02, v46, 0x31);
  __m256i u15 = _mm256_permute2x128_si256(v13, v57, 0x20);
  __m256i u37 = _mm256_permute2x128_si256(v13, v57, 0x31);

  const __m256i s04 = u04;
  u04 = _mm256_adds_epi16(s04, u26);
  u26 = _mm256_subs_epi16(s04, u26);
  const __m256i s15 = u15;
  u15 = _mm256_adds_epi16(s15, u37);
  u37 = _mm256_subs_epi16(s15, u37);

  btf_16_w16_avx2(w_s6_0, w_s6_1, &u26, &u37, r, INV_COS_BIT);

  const __m256i neg_u04 = _mm256_subs_epi16(zero, u04);
  q[0] = _mm256_blend_epi32(u04, neg_u04, 0xF0);

  const __m256i neg_u26 = _mm256_subs_epi16(zero, u26);
  q[1] = _mm256_blend_epi32(neg_u26, u26, 0xF0);

  const __m256i neg_u37 = _mm256_subs_epi16(zero, u37);
  q[2] = _mm256_blend_epi32(u37, neg_u37, 0xF0);

  const __m256i neg_u15 = _mm256_subs_epi16(zero, u15);
  q[3] = _mm256_blend_epi32(neg_u15, u15, 0xF0);
}

static AOM_FORCE_INLINE void round_shift_transpose_8x8_dual_avx2(
    const __m256i *in, int bit, int lr_flip, __m256i *out) {
  const __m256i scale = _mm256_set1_epi16(1 << (15 + bit));
  __m256i p0, p1, p2, p3;
  if (UNLIKELY(lr_flip)) {
    p0 = _mm256_mulhrs_epi16(in[3], scale);
    p1 = _mm256_mulhrs_epi16(in[2], scale);
    p2 = _mm256_mulhrs_epi16(in[1], scale);
    p3 = _mm256_mulhrs_epi16(in[0], scale);
  } else {
    p0 = _mm256_mulhrs_epi16(in[0], scale);
    p1 = _mm256_mulhrs_epi16(in[1], scale);
    p2 = _mm256_mulhrs_epi16(in[2], scale);
    p3 = _mm256_mulhrs_epi16(in[3], scale);
  }

  const __m256i a0 = _mm256_permute2x128_si256(p0, p2, 0x20);
  const __m256i a1 = _mm256_permute2x128_si256(p0, p2, 0x31);
  const __m256i a2 = _mm256_permute2x128_si256(p1, p3, 0x31);
  const __m256i a3 = _mm256_permute2x128_si256(p1, p3, 0x20);

  const __m256i aa0 = _mm256_unpacklo_epi16(a0, a1);
  const __m256i aa1 = _mm256_unpackhi_epi16(a0, a1);
  const __m256i aa2 = _mm256_unpacklo_epi16(a2, a3);
  const __m256i aa3 = _mm256_unpackhi_epi16(a2, a3);

  const __m256i bb0 = _mm256_unpacklo_epi32(aa0, aa2);
  const __m256i bb1 = _mm256_unpackhi_epi32(aa0, aa2);
  const __m256i bb2 = _mm256_unpacklo_epi32(aa1, aa3);
  const __m256i bb3 = _mm256_unpackhi_epi32(aa1, aa3);

  out[0] = _mm256_permute4x64_epi64(bb0, 0xd8);
  out[1] = _mm256_permute4x64_epi64(bb1, 0x8d);
  out[2] = _mm256_permute4x64_epi64(bb2, 0xd8);
  out[3] = _mm256_permute4x64_epi64(bb3, 0x8d);
}

static AOM_FORCE_INLINE void write_round_shift_add_8x8_dual_avx2(
    const __m256i *in, int bit, uint8_t *output, int stride, int flipud) {
  __m256i q[4];
  if (UNLIKELY(flipud)) {
    q[0] = in[3];
    q[1] = in[2];
    q[2] = in[1];
    q[3] = in[0];
  } else {
    q[0] = in[0];
    q[1] = in[1];
    q[2] = in[2];
    q[3] = in[3];
  }

  const __m256i scale = _mm256_set1_epi16(1 << (15 + bit));
  const __m256i zero = _mm256_setzero_si256();

  const __m256i s0 = _mm256_mulhrs_epi16(q[0], scale);
  const __m256i s1 = _mm256_mulhrs_epi16(q[1], scale);
  const __m256i s2 = _mm256_mulhrs_epi16(q[2], scale);
  const __m256i s3 = _mm256_mulhrs_epi16(q[3], scale);

  const __m256i s1_23 = _mm256_permute2x128_si256(s1, s1, 0x01);
  const __m256i s3_67 = _mm256_permute2x128_si256(s3, s3, 0x01);

  const __m128i v0 = _mm_loadl_epi64((const __m128i *)output);
  const __m128i v1 = _mm_loadl_epi64((const __m128i *)(output + stride));
  const __m128i v2 = _mm_loadl_epi64((const __m128i *)(output + 2 * stride));
  const __m128i v3 = _mm_loadl_epi64((const __m128i *)(output + 3 * stride));
  const __m128i v4 = _mm_loadl_epi64((const __m128i *)(output + 4 * stride));
  const __m128i v5 = _mm_loadl_epi64((const __m128i *)(output + 5 * stride));
  const __m128i v6 = _mm_loadl_epi64((const __m128i *)(output + 6 * stride));
  const __m128i v7 = _mm_loadl_epi64((const __m128i *)(output + 7 * stride));

  const __m256i v01 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(v0), v1, 1);
  const __m256i v23 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(v2), v3, 1);
  const __m256i v45 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(v4), v5, 1);
  const __m256i v67 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(v6), v7, 1);

  const __m256i u01 = _mm256_unpacklo_epi8(v01, zero);
  const __m256i u23 = _mm256_unpacklo_epi8(v23, zero);
  const __m256i u45 = _mm256_unpacklo_epi8(v45, zero);
  const __m256i u67 = _mm256_unpacklo_epi8(v67, zero);

  const __m256i x01 = _mm256_adds_epi16(s0, u01);
  const __m256i x23 = _mm256_adds_epi16(s1_23, u23);
  const __m256i x45 = _mm256_adds_epi16(s2, u45);
  const __m256i x67 = _mm256_adds_epi16(s3_67, u67);

  const __m256i res_0123 = _mm256_packus_epi16(x01, x23);
  const __m256i res_4567 = _mm256_packus_epi16(x45, x67);

  const __m128i res_02 = _mm256_castsi256_si128(res_0123);
  const __m128i res_13 = _mm256_extracti128_si256(res_0123, 1);
  const __m128i res_46 = _mm256_castsi256_si128(res_4567);
  const __m128i res_57 = _mm256_extracti128_si256(res_4567, 1);

  _mm_storel_epi64((__m128i *)output, res_02);
  _mm_storel_epi64((__m128i *)(output + stride), res_13);
  _mm_storel_epi64((__m128i *)(output + 2 * stride),
                   _mm_unpackhi_epi64(res_02, res_02));
  _mm_storel_epi64((__m128i *)(output + 3 * stride),
                   _mm_unpackhi_epi64(res_13, res_13));
  _mm_storel_epi64((__m128i *)(output + 4 * stride), res_46);
  _mm_storel_epi64((__m128i *)(output + 5 * stride), res_57);
  _mm_storel_epi64((__m128i *)(output + 6 * stride),
                   _mm_unpackhi_epi64(res_46, res_46));
  _mm_storel_epi64((__m128i *)(output + 7 * stride),
                   _mm_unpackhi_epi64(res_57, res_57));
}

static AOM_FORCE_INLINE void load_8x8_dual_avx2(const int32_t *in, __m256i *p) {
  const __m256i r0 = _mm256_load_si256((const __m256i *)(in + 0));
  const __m256i r1 = _mm256_load_si256((const __m256i *)(in + 8));
  const __m256i r2 = _mm256_load_si256((const __m256i *)(in + 16));
  const __m256i r3 = _mm256_load_si256((const __m256i *)(in + 24));
  const __m256i r4 = _mm256_load_si256((const __m256i *)(in + 32));
  const __m256i r5 = _mm256_load_si256((const __m256i *)(in + 40));
  const __m256i r6 = _mm256_load_si256((const __m256i *)(in + 48));
  const __m256i r7 = _mm256_load_si256((const __m256i *)(in + 56));

  p[0] = _mm256_permute4x64_epi64(_mm256_packs_epi32(r0, r1), 0xd8);
  p[1] = _mm256_permute4x64_epi64(_mm256_packs_epi32(r2, r3), 0x8d);
  p[2] = _mm256_permute4x64_epi64(_mm256_packs_epi32(r4, r5), 0xd8);
  p[3] = _mm256_permute4x64_epi64(_mm256_packs_epi32(r6, r7), 0x8d);
}

static AOM_NOINLINE void lowbd_inv_txfm2d_8x8_non_dct_avx2(const int32_t *input,
                                                           uint8_t *output,
                                                           int stride,
                                                           TX_TYPE tx_type,
                                                           int eob) {
  assert(hitx_1d_tab[tx_type] < 2);
  assert(vitx_1d_tab[tx_type] < 2);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  if (eob > 10) {
    __m256i p[4], q[4];
    load_8x8_dual_avx2(input, p);
    if (hitx_1d_tab[tx_type] == IDCT_1D) {
      idct8x8_dual_avx2(p, q);
    } else {
      iadst8x8_dual_avx2(p, q);
    }
    round_shift_transpose_8x8_dual_avx2(q, -1, lr_flip, p);
    if (vitx_1d_tab[tx_type] == IDCT_1D) {
      idct8x8_dual_avx2(p, q);
    } else {
      iadst8x8_dual_avx2(p, q);
    }
    write_round_shift_add_8x8_dual_avx2(q, -4, output, stride, ud_flip);
    return;
  }

  __m256i out256[4];
  if (UNLIKELY(eob == 1)) {
    __m128i row_vec;
    if (hitx_1d_tab[tx_type] == IDCT_1D) {
      const __m128i zero128 = _mm_setzero_si128();
      const __m128i dc16 =
          _mm_packs_epi32(_mm_cvtsi32_si128(input[0]), zero128);
      const __m128i row_scaled = _mm_mulhrs_epi16(
          _mm_broadcastw_epi16(dc16),
          _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWCos32x8]));
      row_vec = _mm_mulhrs_epi16(
          row_scaled,
          _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWScaleM1]));
    } else {
      row_vec = iadst8_low1_row_round_avx2(input[0], lr_flip);
    }
    if (vitx_1d_tab[tx_type] == IDCT_1D) {
      const __m128i col = _mm_mulhrs_epi16(
          row_vec,
          _mm_load_si128((const __m128i *)kInvTxfm8x8CosWeights[kWCos32x8]));
      const __m256i yc =
          _mm256_inserti128_si256(_mm256_castsi128_si256(col), col, 1);
      out256[0] = yc;
      out256[1] = yc;
      out256[2] = yc;
      out256[3] = yc;
    } else {
      iadst8_low1_col_avx2(row_vec, ud_flip, out256);
    }
  } else {
    __m256i y01_45, y23_67;
    __m128i cols[4];
    if (hitx_1d_tab[tx_type] == IDCT_1D) {
      idct8_low4_w4_pairs_avx2(input, &y01_45, &y23_67);
    } else {
      iadst8_low4_w4_pairs_avx2(input, &y01_45, &y23_67);
    }
    round_and_transpose_pairs_avx2(y01_45, y23_67, lr_flip, cols);
    if (vitx_1d_tab[tx_type] == IDCT_1D) {
      idct8_low4_xmm_avx2(cols, ud_flip, out256);
    } else {
      iadst8_low4_xmm_avx2(cols, ud_flip, out256);
    }
  }
  write_8x8_256_avx2(out256, output, stride);
}

// AVX2 implementation has the advantage when combined multiple operations
// together.
static inline void lowbd_inv_txfm2d_8x8_no_identity_avx2(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  (void)tx_size;
  if (LIKELY(tx_type == DCT_DCT)) {
    if (eob == 1) {
      const int32_t *cospi = cospi_arr(INV_COS_BIT);
      const __m128i w = _mm_set1_epi16(cospi[32] * 8);
      const __m128i s0 = _mm_set1_epi16(1 << (15 - 1));
      const __m128i s1 = _mm_set1_epi16(1 << (15 - 4));
      __m128i dc = _mm_cvtsi32_si128(input[0]);
      dc = _mm_packs_epi32(dc, dc);
      dc = _mm_mulhrs_epi16(dc, w);
      dc = _mm_mulhrs_epi16(dc, s0);
      dc = _mm_mulhrs_epi16(dc, w);
      dc = _mm_mulhrs_epi16(dc, s1);
      const __m256i dc256 = _mm256_broadcastw_epi16(dc);
      const __m256i zero = _mm256_setzero_si256();
      const __m128i v0 = _mm_loadl_epi64((__m128i const *)(output));
      const __m128i v1 = _mm_loadl_epi64((__m128i const *)(output + stride));
      const __m128i v2 =
          _mm_loadl_epi64((__m128i const *)(output + 2 * stride));
      const __m128i v3 =
          _mm_loadl_epi64((__m128i const *)(output + 3 * stride));
      const __m128i v4 =
          _mm_loadl_epi64((__m128i const *)(output + 4 * stride));
      const __m128i v5 =
          _mm_loadl_epi64((__m128i const *)(output + 5 * stride));
      const __m128i v6 =
          _mm_loadl_epi64((__m128i const *)(output + 6 * stride));
      const __m128i v7 =
          _mm_loadl_epi64((__m128i const *)(output + 7 * stride));
      const __m256i v01 =
          _mm256_inserti128_si256(_mm256_castsi128_si256(v0), v1, 1);
      const __m256i v23 =
          _mm256_inserti128_si256(_mm256_castsi128_si256(v2), v3, 1);
      const __m256i v45 =
          _mm256_inserti128_si256(_mm256_castsi128_si256(v4), v5, 1);
      const __m256i v67 =
          _mm256_inserti128_si256(_mm256_castsi128_si256(v6), v7, 1);
      const __m256i x0 =
          _mm256_adds_epi16(dc256, _mm256_unpacklo_epi8(v01, zero));
      const __m256i x1 =
          _mm256_adds_epi16(dc256, _mm256_unpacklo_epi8(v23, zero));
      const __m256i x2 =
          _mm256_adds_epi16(dc256, _mm256_unpacklo_epi8(v45, zero));
      const __m256i x3 =
          _mm256_adds_epi16(dc256, _mm256_unpacklo_epi8(v67, zero));
      const __m256i res_0123 = _mm256_packus_epi16(x0, x1);
      const __m256i res_4567 = _mm256_packus_epi16(x2, x3);
      const __m128i res_02 = _mm256_castsi256_si128(res_0123);
      const __m128i res_13 = _mm256_extracti128_si256(res_0123, 1);
      const __m128i res_46 = _mm256_castsi256_si128(res_4567);
      const __m128i res_57 = _mm256_extracti128_si256(res_4567, 1);
      _mm_storel_epi64((__m128i *)(output), res_02);
      _mm_storel_epi64((__m128i *)(output + stride), res_13);
      _mm_storel_epi64((__m128i *)(output + 2 * stride),
                       _mm_unpackhi_epi64(res_02, res_02));
      _mm_storel_epi64((__m128i *)(output + 3 * stride),
                       _mm_unpackhi_epi64(res_13, res_13));
      _mm_storel_epi64((__m128i *)(output + 4 * stride), res_46);
      _mm_storel_epi64((__m128i *)(output + 5 * stride), res_57);
      _mm_storel_epi64((__m128i *)(output + 6 * stride),
                       _mm_unpackhi_epi64(res_46, res_46));
      _mm_storel_epi64((__m128i *)(output + 7 * stride),
                       _mm_unpackhi_epi64(res_57, res_57));
      return;
    }
    if (eob <= 10) {
      __m256i y01_45, y23_67, out256[4];
      __m128i cols[4];
      idct8_low4_w4_pairs_avx2(input, &y01_45, &y23_67);
      round_and_transpose_pairs_avx2(y01_45, y23_67, 0, cols);
      idct8_low4_xmm_avx2(cols, 0, out256);
      write_8x8_256_avx2(out256, output, stride);
      return;
    }
    __m256i p[4], q[4];
    load_8x8_dual_avx2(input, p);
    idct8x8_dual_avx2(p, q);
    round_shift_transpose_8x8_dual_avx2(q, -1, 0, p);
    idct8x8_dual_avx2(p, q);
    write_round_shift_add_8x8_dual_avx2(q, -4, output, stride, 0);
    return;
  }
  lowbd_inv_txfm2d_8x8_non_dct_avx2(input, output, stride, tx_type, eob);
}

// AVX2 implementation of 8x8 inverse transform. Observed that coding AVX2 for
// tx_type with identity in either of the direction has no advantage.
static void lowbd_inv_txfm2d_add_8x8_avx2(const int32_t *input, uint8_t *output,
                                          int stride, TX_TYPE tx_type,
                                          TX_SIZE tx_size, int eob) {
  if (LIKELY(tx_type < IDTX)) {
    lowbd_inv_txfm2d_8x8_no_identity_avx2(input, output, stride, tx_type,
                                          tx_size, eob);
    return;
  }
  switch (tx_type) {
    case IDTX:
      av1_lowbd_inv_txfm2d_add_idtx_ssse3(input, output, stride, tx_size);
      break;
    case V_DCT:
    case V_ADST:
    case V_FLIPADST:
      av1_lowbd_inv_txfm2d_add_h_identity_ssse3(input, output, stride, tx_type,
                                                tx_size, eob);
      break;
    case H_DCT:
    case H_ADST:
    case H_FLIPADST:
      av1_lowbd_inv_txfm2d_add_v_identity_ssse3(input, output, stride, tx_type,
                                                tx_size, eob);
      break;
    default:
      lowbd_inv_txfm2d_8x8_no_identity_avx2(input, output, stride, tx_type,
                                            tx_size, eob);
  }
}

// for 32x32,32x64,64x32,64x64,16x32,32x16,64x16,16x64
static inline void lowbd_inv_txfm2d_add_universe_avx2(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  if (LIKELY(tx_type < IDTX)) {
    lowbd_inv_txfm2d_add_no_identity_avx2(input, output, stride, tx_type,
                                          tx_size, eob);
    return;
  }
  switch (tx_type) {
    case IDTX:
      lowbd_inv_txfm2d_add_idtx_avx2(input, output, stride, tx_size, eob);
      break;
    case V_DCT:
    case V_ADST:
    case V_FLIPADST:
      lowbd_inv_txfm2d_add_h_identity_avx2(input, output, stride, tx_type,
                                           tx_size, eob);
      break;
    case H_DCT:
    case H_ADST:
    case H_FLIPADST:
      lowbd_inv_txfm2d_add_v_identity_avx2(input, output, stride, tx_type,
                                           tx_size, eob);
      break;
    default:
      av1_lowbd_inv_txfm2d_add_ssse3(input, output, stride, tx_type, tx_size,
                                     eob);
      break;
  }
}

void av1_lowbd_inv_txfm2d_add_avx2(const int32_t *input, uint8_t *output,
                                   int stride, TX_TYPE tx_type, TX_SIZE tx_size,
                                   int eob) {
  switch (tx_size) {
    case TX_4X4:
    case TX_4X8:
    case TX_8X4:
    case TX_8X16:
    case TX_16X8:
    case TX_4X16:
    case TX_16X4:
    case TX_8X32:
    case TX_32X8:
      av1_lowbd_inv_txfm2d_add_ssse3(input, output, stride, tx_type, tx_size,
                                     eob);
      break;
    case TX_8X8:
      lowbd_inv_txfm2d_add_8x8_avx2(input, output, stride, tx_type, tx_size,
                                    eob);
      break;
    default:
      assert(tx_size < TX_SIZES_ALL);
      lowbd_inv_txfm2d_add_universe_avx2(input, output, stride, tx_type,
                                         tx_size, eob);
      break;
  }
}

void av1_inv_txfm_add_avx2(const tran_low_t *dqcoeff, uint8_t *dst, int stride,
                           const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  if (!txfm_param->lossless) {
    av1_lowbd_inv_txfm2d_add_avx2(dqcoeff, dst, stride, tx_type,
                                  txfm_param->tx_size, txfm_param->eob);
  } else {
    av1_inv_txfm_add_c(dqcoeff, dst, stride, txfm_param);
  }
}
