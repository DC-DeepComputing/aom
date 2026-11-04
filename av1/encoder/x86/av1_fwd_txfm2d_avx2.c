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

#include "config/av1_rtcd.h"

#include "av1/common/enums.h"
#include "av1/common/av1_txfm.h"
#include "av1/encoder/x86/av1_fwd_txfm_avx2.h"
#include "av1/common/x86/av1_txfm_sse2.h"
#include "av1/encoder/av1_fwd_txfm1d_cfg.h"
#include "av1/encoder/x86/av1_txfm1d_sse4.h"
#include "av1/encoder/x86/av1_fwd_txfm_sse2.h"
#include "aom_dsp/x86/txfm_common_avx2.h"

static AOM_FORCE_INLINE void fdct16x16_new_avx2(const __m256i *input,
                                                __m256i *output,
                                                int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));

  __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
  __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
  __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
  __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
  __m256i cospi_p24_p40 = pair_set_w16_epi16(cospi[24], cospi[40]);
  __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
  __m256i cospi_p60_p04 = pair_set_w16_epi16(cospi[60], cospi[4]);
  __m256i cospi_m04_p60 = pair_set_w16_epi16(-cospi[4], cospi[60]);
  __m256i cospi_p28_p36 = pair_set_w16_epi16(cospi[28], cospi[36]);
  __m256i cospi_m36_p28 = pair_set_w16_epi16(-cospi[36], cospi[28]);
  __m256i cospi_p44_p20 = pair_set_w16_epi16(cospi[44], cospi[20]);
  __m256i cospi_m20_p44 = pair_set_w16_epi16(-cospi[20], cospi[44]);
  __m256i cospi_p12_p52 = pair_set_w16_epi16(cospi[12], cospi[52]);
  __m256i cospi_m52_p12 = pair_set_w16_epi16(-cospi[52], cospi[12]);

  // stage 1: split into 8-register even half (x0_0..x0_7) and odd half
  // (x1[0..7])
  __m256i x0_0, x0_1, x0_2, x0_3, x0_4, x0_5, x0_6, x0_7;
  __m256i x1[8];
#define STAGE1_PAIR(k)                           \
  do {                                           \
    const __m256i in_a = input[k];               \
    const __m256i in_b = input[15 - (k)];        \
    x0_##k = _mm256_adds_epi16(in_a, in_b);      \
    x1[7 - (k)] = _mm256_subs_epi16(in_a, in_b); \
  } while (0)
  STAGE1_PAIR(0);
  STAGE1_PAIR(1);
  STAGE1_PAIR(2);
  STAGE1_PAIR(3);
  STAGE1_PAIR(4);
  STAGE1_PAIR(5);
  STAGE1_PAIR(6);
  STAGE1_PAIR(7);
#undef STAGE1_PAIR

  // Even half stages 2..5 -> output[0, 2, 4, 6, 8, 10, 12, 14]
  btf_16_adds_subs_avx2(&x0_0, &x0_7);
  btf_16_adds_subs_avx2(&x0_1, &x0_6);
  btf_16_adds_subs_avx2(&x0_2, &x0_5);
  btf_16_adds_subs_avx2(&x0_3, &x0_4);

  btf_16_adds_subs_avx2(&x0_0, &x0_3);
  btf_16_adds_subs_avx2(&x0_1, &x0_2);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x0_5, &x0_6, _r, cos_bit);

  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x0_0, &x0_1, _r, cos_bit);
  btf_16_w16_avx2(cospi_p48_p16, cospi_m16_p48, &x0_2, &x0_3, _r, cos_bit);
  output[0] = x0_0;
  output[8] = x0_1;
  output[4] = x0_2;
  output[12] = x0_3;

  btf_16_adds_subs_avx2(&x0_4, &x0_5);
  btf_16_adds_subs_avx2(&x0_7, &x0_6);
  btf_16_w16_avx2(cospi_p56_p08, cospi_m08_p56, &x0_4, &x0_7, _r, cos_bit);
  btf_16_w16_avx2(cospi_p24_p40, cospi_m40_p24, &x0_5, &x0_6, _r, cos_bit);
  output[2] = x0_4;
  output[6] = x0_6;
  output[10] = x0_5;
  output[14] = x0_7;

  // Odd half stages 2..6 -> output[1, 3, 5, 7, 9, 11, 13, 15]
  __m256i x1_0 = x1[0], x1_1 = x1[1], x1_2 = x1[2], x1_3 = x1[3];
  __m256i x1_4 = x1[4], x1_5 = x1[5], x1_6 = x1[6], x1_7 = x1[7];
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1_2, &x1_5, _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1_3, &x1_4, _r, cos_bit);

  btf_16_adds_subs_avx2(&x1_0, &x1_3);
  btf_16_adds_subs_avx2(&x1_1, &x1_2);
  btf_16_adds_subs_avx2(&x1_7, &x1_4);
  btf_16_adds_subs_avx2(&x1_6, &x1_5);

  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x1_1, &x1_6, _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x1_2, &x1_5, _r, cos_bit);

  btf_16_adds_subs_avx2(&x1_0, &x1_1);
  btf_16_adds_subs_avx2(&x1_3, &x1_2);
  btf_16_adds_subs_avx2(&x1_4, &x1_5);
  btf_16_adds_subs_avx2(&x1_7, &x1_6);

  btf_16_w16_avx2(cospi_p60_p04, cospi_m04_p60, &x1_0, &x1_7, _r, cos_bit);
  btf_16_w16_avx2(cospi_p28_p36, cospi_m36_p28, &x1_1, &x1_6, _r, cos_bit);
  btf_16_w16_avx2(cospi_p44_p20, cospi_m20_p44, &x1_2, &x1_5, _r, cos_bit);
  btf_16_w16_avx2(cospi_p12_p52, cospi_m52_p12, &x1_3, &x1_4, _r, cos_bit);

  output[1] = x1_0;
  output[3] = x1_4;
  output[5] = x1_2;
  output[7] = x1_6;
  output[9] = x1_1;
  output[11] = x1_5;
  output[13] = x1_3;
  output[15] = x1_7;
}

static inline void fdct16x16_cb13_avx2(const __m256i *input, __m256i *output,
                                       int8_t cos_bit) {
  (void)cos_bit;
  fdct16x16_new_avx2(input, output, 13);
}

static AOM_FORCE_INLINE void fdct16x32_new_avx2(const __m256i *input,
                                                __m256i *output,
                                                int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);

  __m256i x_eo[8];
  __m256i x_odd[16];
  __m256i x0, x1, x2, x3, x4, x5, x6, x7;

  // Stage 1 + Stage 2 (even half): partition into Group A (x0..x7),
  // Group B (x_eo[0..7]), and Group C (x_odd[0..15]).
#define STAGE12_PAIR(k, dst_even)                 \
  do {                                            \
    const __m256i a0 = input[k];                  \
    const __m256i b0 = input[31 - (k)];           \
    const __m256i a1 = input[15 - (k)];           \
    const __m256i b1 = input[16 + (k)];           \
    const __m256i s0 = _mm256_adds_epi16(a0, b0); \
    const __m256i s1 = _mm256_adds_epi16(a1, b1); \
    x_odd[15 - (k)] = _mm256_subs_epi16(a0, b0);  \
    x_odd[k] = _mm256_subs_epi16(a1, b1);         \
    dst_even = _mm256_adds_epi16(s0, s1);         \
    x_eo[7 - (k)] = _mm256_subs_epi16(s0, s1);    \
  } while (0)

  STAGE12_PAIR(0, x0);
  STAGE12_PAIR(1, x1);
  STAGE12_PAIR(2, x2);
  STAGE12_PAIR(3, x3);
  STAGE12_PAIR(4, x4);
  STAGE12_PAIR(5, x5);
  STAGE12_PAIR(6, x6);
  STAGE12_PAIR(7, x7);
#undef STAGE12_PAIR

  // Sub-problem A: x1[0..7] (8 registers, Stages 3..6 -> output)
  {
    const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
    btf_16_adds_subs_avx2(&x0, &x7);
    btf_16_adds_subs_avx2(&x1, &x6);
    btf_16_adds_subs_avx2(&x2, &x5);
    btf_16_adds_subs_avx2(&x3, &x4);

    btf_16_adds_subs_avx2(&x0, &x3);
    btf_16_adds_subs_avx2(&x1, &x2);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi[32], cospi[32]),
                    pair_set_w16_epi16(cospi[32], cospi[32]), &x5, &x6, _r,
                    cos_bit);

    btf_16_w16_avx2(pair_set_w16_epi16(cospi[32], cospi[32]),
                    pair_set_w16_epi16(cospi[32], -cospi[32]), &x0, &x1, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[48], cospi[16]),
                    pair_set_w16_epi16(-cospi[16], cospi[48]), &x2, &x3, _r,
                    cos_bit);
    output[0] = x0;
    output[16] = x1;
    output[8] = x2;
    output[24] = x3;

    btf_16_adds_subs_avx2(&x4, &x5);
    btf_16_adds_subs_avx2(&x7, &x6);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[56], cospi[8]),
                    pair_set_w16_epi16(-cospi[8], cospi[56]), &x4, &x7, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[24], cospi[40]),
                    pair_set_w16_epi16(-cospi[40], cospi[24]), &x5, &x6, _r,
                    cos_bit);
    output[4] = x4;
    output[12] = x6;
    output[20] = x5;
    output[28] = x7;
  }

  // Sub-problem B: x1[8..15] (8 registers, Stages 3..7 -> output)
  {
    const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
    __m256i x8 = x_eo[0];
    __m256i x9 = x_eo[1];
    __m256i x10 = x_eo[2];
    __m256i x11 = x_eo[3];
    __m256i x12 = x_eo[4];
    __m256i x13 = x_eo[5];
    __m256i x14 = x_eo[6];
    __m256i x15 = x_eo[7];

    const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
    const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x10, &x13, _r, cos_bit);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x11, &x12, _r, cos_bit);

    btf_16_adds_subs_avx2(&x8, &x11);
    btf_16_adds_subs_avx2(&x9, &x10);
    btf_16_adds_subs_avx2(&x15, &x12);
    btf_16_adds_subs_avx2(&x14, &x13);

    const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
    btf_16_w16_avx2(cospi_m16_p48, pair_set_w16_epi16(cospi[48], cospi[16]),
                    &x9, &x14, _r, cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi[48], -cospi[16]), cospi_m16_p48,
                    &x10, &x13, _r, cos_bit);

    btf_16_adds_subs_avx2(&x8, &x9);
    btf_16_adds_subs_avx2(&x11, &x10);
    btf_16_adds_subs_avx2(&x12, &x13);
    btf_16_adds_subs_avx2(&x15, &x14);

    btf_16_w16_avx2(pair_set_w16_epi16(cospi[60], cospi[4]),
                    pair_set_w16_epi16(-cospi[4], cospi[60]), &x8, &x15, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[28], cospi[36]),
                    pair_set_w16_epi16(-cospi[36], cospi[28]), &x9, &x14, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[44], cospi[20]),
                    pair_set_w16_epi16(-cospi[20], cospi[44]), &x10, &x13, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[12], cospi[52]),
                    pair_set_w16_epi16(-cospi[52], cospi[12]), &x11, &x12, _r,
                    cos_bit);

    output[2] = x8;
    output[6] = x12;
    output[10] = x10;
    output[14] = x14;
    output[18] = x9;
    output[22] = x13;
    output[26] = x11;
    output[30] = x15;
  }

  // Sub-problem C: x1[16..31]
  {
    const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
    __m256i x20 = x_odd[4];
    __m256i x21 = x_odd[5];
    __m256i x22 = x_odd[6];
    __m256i x23 = x_odd[7];
    __m256i x24 = x_odd[8];
    __m256i x25 = x_odd[9];
    __m256i x26 = x_odd[10];
    __m256i x27 = x_odd[11];

    // Stage 2 (odd half: only x20..x27)
    const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
    const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x20, &x27, _r, cos_bit);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x21, &x26, _r, cos_bit);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x22, &x25, _r, cos_bit);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x23, &x24, _r, cos_bit);

    // Stage 3 (odd half): replace x20..x27 in-place with x16..x19, x28..x31
    const __m256i o16 = x_odd[0];
    x_eo[3] = _mm256_subs_epi16(o16, x23);
    __m256i x16 = _mm256_adds_epi16(o16, x23);
    const __m256i o17 = x_odd[1];
    x_eo[2] = _mm256_subs_epi16(o17, x22);
    __m256i x17 = _mm256_adds_epi16(o17, x22);
    const __m256i o18 = x_odd[2];
    x_eo[1] = _mm256_subs_epi16(o18, x21);
    __m256i x18 = _mm256_adds_epi16(o18, x21);
    const __m256i o19 = x_odd[3];
    x_eo[0] = _mm256_subs_epi16(o19, x20);
    __m256i x19 = _mm256_adds_epi16(o19, x20);
    const __m256i o31 = x_odd[15];
    x_eo[4] = _mm256_subs_epi16(o31, x24);
    __m256i x31 = _mm256_adds_epi16(o31, x24);
    const __m256i o30 = x_odd[14];
    x_eo[5] = _mm256_subs_epi16(o30, x25);
    __m256i x30 = _mm256_adds_epi16(o30, x25);
    const __m256i o29 = x_odd[13];
    x_eo[6] = _mm256_subs_epi16(o29, x26);
    __m256i x29 = _mm256_adds_epi16(o29, x26);
    const __m256i o28 = x_odd[12];
    x_eo[7] = _mm256_subs_epi16(o28, x27);
    __m256i x28 = _mm256_adds_epi16(o28, x27);

    // Sub-problem C1: {x16..x19, x28..x31} (8 registers, Stages 4..8 -> output)
    const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
    const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
    btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x18, &x29, _r, cos_bit);
    btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x19, &x28, _r, cos_bit);

    btf_16_adds_subs_avx2(&x16, &x19);
    btf_16_adds_subs_avx2(&x17, &x18);
    btf_16_adds_subs_avx2(&x31, &x28);
    btf_16_adds_subs_avx2(&x30, &x29);

    const __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
    btf_16_w16_avx2(cospi_m08_p56, pair_set_w16_epi16(cospi[56], cospi[8]),
                    &x17, &x30, _r, cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi[56], -cospi[8]), cospi_m08_p56,
                    &x18, &x29, _r, cos_bit);

    btf_16_adds_subs_avx2(&x16, &x17);
    btf_16_adds_subs_avx2(&x19, &x18);
    btf_16_adds_subs_avx2(&x28, &x29);
    btf_16_adds_subs_avx2(&x31, &x30);

    btf_16_w16_avx2(pair_set_w16_epi16(cospi[62], cospi[2]),
                    pair_set_w16_epi16(-cospi[2], cospi[62]), &x16, &x31, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[30], cospi[34]),
                    pair_set_w16_epi16(-cospi[34], cospi[30]), &x17, &x30, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[46], cospi[18]),
                    pair_set_w16_epi16(-cospi[18], cospi[46]), &x18, &x29, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[14], cospi[50]),
                    pair_set_w16_epi16(-cospi[50], cospi[14]), &x19, &x28, _r,
                    cos_bit);

    output[1] = x16;
    output[7] = x28;
    output[9] = x18;
    output[15] = x30;
    output[17] = x17;
    output[23] = x29;
    output[25] = x19;
    output[31] = x31;
  }

  // Sub-problem C2: {x20..x27} (8 registers, Stages 4..8 -> output)
  {
    const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
    __m256i x20 = x_eo[0];
    __m256i x21 = x_eo[1];
    __m256i x22 = x_eo[2];
    __m256i x23 = x_eo[3];
    __m256i x24 = x_eo[4];
    __m256i x25 = x_eo[5];
    __m256i x26 = x_eo[6];
    __m256i x27 = x_eo[7];

    const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
    const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
    btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x20, &x27, _r, cos_bit);
    btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x21, &x26, _r, cos_bit);

    btf_16_adds_subs_avx2(&x23, &x20);
    btf_16_adds_subs_avx2(&x22, &x21);
    btf_16_adds_subs_avx2(&x24, &x27);
    btf_16_adds_subs_avx2(&x25, &x26);

    const __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
    btf_16_w16_avx2(cospi_m40_p24, pair_set_w16_epi16(cospi[24], cospi[40]),
                    &x21, &x26, _r, cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi[24], -cospi[40]), cospi_m40_p24,
                    &x22, &x25, _r, cos_bit);

    btf_16_adds_subs_avx2(&x20, &x21);
    btf_16_adds_subs_avx2(&x23, &x22);
    btf_16_adds_subs_avx2(&x24, &x25);
    btf_16_adds_subs_avx2(&x27, &x26);

    btf_16_w16_avx2(pair_set_w16_epi16(cospi[54], cospi[10]),
                    pair_set_w16_epi16(-cospi[10], cospi[54]), &x20, &x27, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[22], cospi[42]),
                    pair_set_w16_epi16(-cospi[42], cospi[22]), &x21, &x26, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[38], cospi[26]),
                    pair_set_w16_epi16(-cospi[26], cospi[38]), &x22, &x25, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[6], cospi[58]),
                    pair_set_w16_epi16(-cospi[58], cospi[6]), &x23, &x24, _r,
                    cos_bit);

    output[3] = x24;
    output[5] = x20;
    output[11] = x26;
    output[13] = x22;
    output[19] = x25;
    output[21] = x21;
    output[27] = x27;
    output[29] = x23;
  }
}

static inline void fdct16x32_avx2(const __m256i *input, __m256i *output,
                                  int8_t cos_bit) {
  (void)cos_bit;
  assert(cos_bit == 12);
  fdct16x32_new_avx2(input, output, 12);
}

static inline void fdct16x32_cb13_avx2(const __m256i *input, __m256i *output,
                                       int8_t cos_bit) {
  (void)cos_bit;
  assert(cos_bit == 13);
  fdct16x32_new_avx2(input, output, 13);
}

static inline void fdct16x64_new_avx2(const __m256i *input, __m256i *output,
                                      int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));

  __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
  __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
  __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
  __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
  __m256i cospi_m56_m08 = pair_set_w16_epi16(-cospi[56], -cospi[8]);
  __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
  __m256i cospi_p24_p40 = pair_set_w16_epi16(cospi[24], cospi[40]);
  __m256i cospi_m24_m40 = pair_set_w16_epi16(-cospi[24], -cospi[40]);
  __m256i cospi_p60_p04 = pair_set_w16_epi16(cospi[60], cospi[4]);
  __m256i cospi_m04_p60 = pair_set_w16_epi16(-cospi[4], cospi[60]);
  __m256i cospi_p28_p36 = pair_set_w16_epi16(cospi[28], cospi[36]);
  __m256i cospi_m36_p28 = pair_set_w16_epi16(-cospi[36], cospi[28]);
  __m256i cospi_p44_p20 = pair_set_w16_epi16(cospi[44], cospi[20]);
  __m256i cospi_m20_p44 = pair_set_w16_epi16(-cospi[20], cospi[44]);
  __m256i cospi_p12_p52 = pair_set_w16_epi16(cospi[12], cospi[52]);
  __m256i cospi_m52_p12 = pair_set_w16_epi16(-cospi[52], cospi[12]);
  __m256i cospi_m60_m04 = pair_set_w16_epi16(-cospi[60], -cospi[4]);
  __m256i cospi_m28_m36 = pair_set_w16_epi16(-cospi[28], -cospi[36]);
  __m256i cospi_m44_m20 = pair_set_w16_epi16(-cospi[44], -cospi[20]);
  __m256i cospi_m12_m52 = pair_set_w16_epi16(-cospi[12], -cospi[52]);
  __m256i cospi_p62_p02 = pair_set_w16_epi16(cospi[62], cospi[2]);
  __m256i cospi_m02_p62 = pair_set_w16_epi16(-cospi[2], cospi[62]);
  __m256i cospi_p30_p34 = pair_set_w16_epi16(cospi[30], cospi[34]);
  __m256i cospi_m34_p30 = pair_set_w16_epi16(-cospi[34], cospi[30]);
  __m256i cospi_p46_p18 = pair_set_w16_epi16(cospi[46], cospi[18]);
  __m256i cospi_m18_p46 = pair_set_w16_epi16(-cospi[18], cospi[46]);
  __m256i cospi_p14_p50 = pair_set_w16_epi16(cospi[14], cospi[50]);
  __m256i cospi_m50_p14 = pair_set_w16_epi16(-cospi[50], cospi[14]);
  __m256i cospi_p54_p10 = pair_set_w16_epi16(cospi[54], cospi[10]);
  __m256i cospi_m10_p54 = pair_set_w16_epi16(-cospi[10], cospi[54]);
  __m256i cospi_p22_p42 = pair_set_w16_epi16(cospi[22], cospi[42]);
  __m256i cospi_m42_p22 = pair_set_w16_epi16(-cospi[42], cospi[22]);
  __m256i cospi_p38_p26 = pair_set_w16_epi16(cospi[38], cospi[26]);
  __m256i cospi_m26_p38 = pair_set_w16_epi16(-cospi[26], cospi[38]);
  __m256i cospi_p06_p58 = pair_set_w16_epi16(cospi[6], cospi[58]);
  __m256i cospi_m58_p06 = pair_set_w16_epi16(-cospi[58], cospi[6]);
  __m256i cospi_p63_p01 = pair_set_w16_epi16(cospi[63], cospi[1]);
  __m256i cospi_m01_p63 = pair_set_w16_epi16(-cospi[1], cospi[63]);
  __m256i cospi_p31_p33 = pair_set_w16_epi16(cospi[31], cospi[33]);
  __m256i cospi_m33_p31 = pair_set_w16_epi16(-cospi[33], cospi[31]);
  __m256i cospi_p47_p17 = pair_set_w16_epi16(cospi[47], cospi[17]);
  __m256i cospi_m17_p47 = pair_set_w16_epi16(-cospi[17], cospi[47]);
  __m256i cospi_p15_p49 = pair_set_w16_epi16(cospi[15], cospi[49]);
  __m256i cospi_m49_p15 = pair_set_w16_epi16(-cospi[49], cospi[15]);
  __m256i cospi_p55_p09 = pair_set_w16_epi16(cospi[55], cospi[9]);
  __m256i cospi_m09_p55 = pair_set_w16_epi16(-cospi[9], cospi[55]);
  __m256i cospi_p23_p41 = pair_set_w16_epi16(cospi[23], cospi[41]);
  __m256i cospi_m41_p23 = pair_set_w16_epi16(-cospi[41], cospi[23]);
  __m256i cospi_p39_p25 = pair_set_w16_epi16(cospi[39], cospi[25]);
  __m256i cospi_m25_p39 = pair_set_w16_epi16(-cospi[25], cospi[39]);
  __m256i cospi_p07_p57 = pair_set_w16_epi16(cospi[7], cospi[57]);
  __m256i cospi_m57_p07 = pair_set_w16_epi16(-cospi[57], cospi[7]);
  __m256i cospi_p59_p05 = pair_set_w16_epi16(cospi[59], cospi[5]);
  __m256i cospi_m05_p59 = pair_set_w16_epi16(-cospi[5], cospi[59]);
  __m256i cospi_p27_p37 = pair_set_w16_epi16(cospi[27], cospi[37]);
  __m256i cospi_m37_p27 = pair_set_w16_epi16(-cospi[37], cospi[27]);
  __m256i cospi_p43_p21 = pair_set_w16_epi16(cospi[43], cospi[21]);
  __m256i cospi_m21_p43 = pair_set_w16_epi16(-cospi[21], cospi[43]);
  __m256i cospi_p11_p53 = pair_set_w16_epi16(cospi[11], cospi[53]);
  __m256i cospi_m53_p11 = pair_set_w16_epi16(-cospi[53], cospi[11]);
  __m256i cospi_p51_p13 = pair_set_w16_epi16(cospi[51], cospi[13]);
  __m256i cospi_m13_p51 = pair_set_w16_epi16(-cospi[13], cospi[51]);
  __m256i cospi_p19_p45 = pair_set_w16_epi16(cospi[19], cospi[45]);
  __m256i cospi_m45_p19 = pair_set_w16_epi16(-cospi[45], cospi[19]);
  __m256i cospi_p35_p29 = pair_set_w16_epi16(cospi[35], cospi[29]);
  __m256i cospi_m29_p35 = pair_set_w16_epi16(-cospi[29], cospi[35]);
  __m256i cospi_p03_p61 = pair_set_w16_epi16(cospi[3], cospi[61]);
  __m256i cospi_m61_p03 = pair_set_w16_epi16(-cospi[61], cospi[3]);

  // stage 1
  __m256i x1[64];
  btf_16_adds_subs_out_avx2(&x1[0], &x1[63], input[0], input[63]);
  btf_16_adds_subs_out_avx2(&x1[1], &x1[62], input[1], input[62]);
  btf_16_adds_subs_out_avx2(&x1[2], &x1[61], input[2], input[61]);
  btf_16_adds_subs_out_avx2(&x1[3], &x1[60], input[3], input[60]);
  btf_16_adds_subs_out_avx2(&x1[4], &x1[59], input[4], input[59]);
  btf_16_adds_subs_out_avx2(&x1[5], &x1[58], input[5], input[58]);
  btf_16_adds_subs_out_avx2(&x1[6], &x1[57], input[6], input[57]);
  btf_16_adds_subs_out_avx2(&x1[7], &x1[56], input[7], input[56]);
  btf_16_adds_subs_out_avx2(&x1[8], &x1[55], input[8], input[55]);
  btf_16_adds_subs_out_avx2(&x1[9], &x1[54], input[9], input[54]);
  btf_16_adds_subs_out_avx2(&x1[10], &x1[53], input[10], input[53]);
  btf_16_adds_subs_out_avx2(&x1[11], &x1[52], input[11], input[52]);
  btf_16_adds_subs_out_avx2(&x1[12], &x1[51], input[12], input[51]);
  btf_16_adds_subs_out_avx2(&x1[13], &x1[50], input[13], input[50]);
  btf_16_adds_subs_out_avx2(&x1[14], &x1[49], input[14], input[49]);
  btf_16_adds_subs_out_avx2(&x1[15], &x1[48], input[15], input[48]);
  btf_16_adds_subs_out_avx2(&x1[16], &x1[47], input[16], input[47]);
  btf_16_adds_subs_out_avx2(&x1[17], &x1[46], input[17], input[46]);
  btf_16_adds_subs_out_avx2(&x1[18], &x1[45], input[18], input[45]);
  btf_16_adds_subs_out_avx2(&x1[19], &x1[44], input[19], input[44]);
  btf_16_adds_subs_out_avx2(&x1[20], &x1[43], input[20], input[43]);
  btf_16_adds_subs_out_avx2(&x1[21], &x1[42], input[21], input[42]);
  btf_16_adds_subs_out_avx2(&x1[22], &x1[41], input[22], input[41]);
  btf_16_adds_subs_out_avx2(&x1[23], &x1[40], input[23], input[40]);
  btf_16_adds_subs_out_avx2(&x1[24], &x1[39], input[24], input[39]);
  btf_16_adds_subs_out_avx2(&x1[25], &x1[38], input[25], input[38]);
  btf_16_adds_subs_out_avx2(&x1[26], &x1[37], input[26], input[37]);
  btf_16_adds_subs_out_avx2(&x1[27], &x1[36], input[27], input[36]);
  btf_16_adds_subs_out_avx2(&x1[28], &x1[35], input[28], input[35]);
  btf_16_adds_subs_out_avx2(&x1[29], &x1[34], input[29], input[34]);
  btf_16_adds_subs_out_avx2(&x1[30], &x1[33], input[30], input[33]);
  btf_16_adds_subs_out_avx2(&x1[31], &x1[32], input[31], input[32]);

  // stage 2
  btf_16_adds_subs_avx2(&x1[0], &x1[31]);
  btf_16_adds_subs_avx2(&x1[1], &x1[30]);
  btf_16_adds_subs_avx2(&x1[2], &x1[29]);
  btf_16_adds_subs_avx2(&x1[3], &x1[28]);
  btf_16_adds_subs_avx2(&x1[4], &x1[27]);
  btf_16_adds_subs_avx2(&x1[5], &x1[26]);
  btf_16_adds_subs_avx2(&x1[6], &x1[25]);
  btf_16_adds_subs_avx2(&x1[7], &x1[24]);
  btf_16_adds_subs_avx2(&x1[8], &x1[23]);
  btf_16_adds_subs_avx2(&x1[9], &x1[22]);
  btf_16_adds_subs_avx2(&x1[10], &x1[21]);
  btf_16_adds_subs_avx2(&x1[11], &x1[20]);
  btf_16_adds_subs_avx2(&x1[12], &x1[19]);
  btf_16_adds_subs_avx2(&x1[13], &x1[18]);
  btf_16_adds_subs_avx2(&x1[14], &x1[17]);
  btf_16_adds_subs_avx2(&x1[15], &x1[16]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[40], &x1[55], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[41], &x1[54], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[42], &x1[53], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[43], &x1[52], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[44], &x1[51], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[45], &x1[50], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[46], &x1[49], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[47], &x1[48], _r, cos_bit);

  // stage 3
  btf_16_adds_subs_avx2(&x1[0], &x1[15]);
  btf_16_adds_subs_avx2(&x1[1], &x1[14]);
  btf_16_adds_subs_avx2(&x1[2], &x1[13]);
  btf_16_adds_subs_avx2(&x1[3], &x1[12]);
  btf_16_adds_subs_avx2(&x1[4], &x1[11]);
  btf_16_adds_subs_avx2(&x1[5], &x1[10]);
  btf_16_adds_subs_avx2(&x1[6], &x1[9]);
  btf_16_adds_subs_avx2(&x1[7], &x1[8]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[20], &x1[27], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[21], &x1[26], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[22], &x1[25], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[23], &x1[24], _r, cos_bit);
  btf_16_adds_subs_avx2(&x1[32], &x1[47]);
  btf_16_adds_subs_avx2(&x1[33], &x1[46]);
  btf_16_adds_subs_avx2(&x1[34], &x1[45]);
  btf_16_adds_subs_avx2(&x1[35], &x1[44]);
  btf_16_adds_subs_avx2(&x1[36], &x1[43]);
  btf_16_adds_subs_avx2(&x1[37], &x1[42]);
  btf_16_adds_subs_avx2(&x1[38], &x1[41]);
  btf_16_adds_subs_avx2(&x1[39], &x1[40]);
  btf_16_adds_subs_avx2(&x1[63], &x1[48]);
  btf_16_adds_subs_avx2(&x1[62], &x1[49]);
  btf_16_adds_subs_avx2(&x1[61], &x1[50]);
  btf_16_adds_subs_avx2(&x1[60], &x1[51]);
  btf_16_adds_subs_avx2(&x1[59], &x1[52]);
  btf_16_adds_subs_avx2(&x1[58], &x1[53]);
  btf_16_adds_subs_avx2(&x1[57], &x1[54]);
  btf_16_adds_subs_avx2(&x1[56], &x1[55]);

  // stage 4
  btf_16_adds_subs_avx2(&x1[0], &x1[7]);
  btf_16_adds_subs_avx2(&x1[1], &x1[6]);
  btf_16_adds_subs_avx2(&x1[2], &x1[5]);
  btf_16_adds_subs_avx2(&x1[3], &x1[4]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[10], &x1[13], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[11], &x1[12], _r, cos_bit);
  btf_16_adds_subs_avx2(&x1[16], &x1[23]);
  btf_16_adds_subs_avx2(&x1[17], &x1[22]);
  btf_16_adds_subs_avx2(&x1[18], &x1[21]);
  btf_16_adds_subs_avx2(&x1[19], &x1[20]);
  btf_16_adds_subs_avx2(&x1[31], &x1[24]);
  btf_16_adds_subs_avx2(&x1[30], &x1[25]);
  btf_16_adds_subs_avx2(&x1[29], &x1[26]);
  btf_16_adds_subs_avx2(&x1[28], &x1[27]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x1[36], &x1[59], _r, cos_bit);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x1[37], &x1[58], _r, cos_bit);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x1[38], &x1[57], _r, cos_bit);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x1[39], &x1[56], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x1[40], &x1[55], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x1[41], &x1[54], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x1[42], &x1[53], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x1[43], &x1[52], _r, cos_bit);

  // stage 5
  btf_16_adds_subs_avx2(&x1[0], &x1[3]);
  btf_16_adds_subs_avx2(&x1[1], &x1[2]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[5], &x1[6], _r, cos_bit);
  btf_16_adds_subs_avx2(&x1[8], &x1[11]);
  btf_16_adds_subs_avx2(&x1[9], &x1[10]);
  btf_16_adds_subs_avx2(&x1[15], &x1[12]);
  btf_16_adds_subs_avx2(&x1[14], &x1[13]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x1[18], &x1[29], _r, cos_bit);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x1[19], &x1[28], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x1[20], &x1[27], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x1[21], &x1[26], _r, cos_bit);
  btf_16_adds_subs_avx2(&x1[32], &x1[39]);
  btf_16_adds_subs_avx2(&x1[33], &x1[38]);
  btf_16_adds_subs_avx2(&x1[34], &x1[37]);
  btf_16_adds_subs_avx2(&x1[35], &x1[36]);
  btf_16_adds_subs_avx2(&x1[47], &x1[40]);
  btf_16_adds_subs_avx2(&x1[46], &x1[41]);
  btf_16_adds_subs_avx2(&x1[45], &x1[42]);
  btf_16_adds_subs_avx2(&x1[44], &x1[43]);
  btf_16_adds_subs_avx2(&x1[48], &x1[55]);
  btf_16_adds_subs_avx2(&x1[49], &x1[54]);
  btf_16_adds_subs_avx2(&x1[50], &x1[53]);
  btf_16_adds_subs_avx2(&x1[51], &x1[52]);
  btf_16_adds_subs_avx2(&x1[63], &x1[56]);
  btf_16_adds_subs_avx2(&x1[62], &x1[57]);
  btf_16_adds_subs_avx2(&x1[61], &x1[58]);
  btf_16_adds_subs_avx2(&x1[60], &x1[59]);

  // stage 6
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x1[0], &x1[1], _r, cos_bit);
  btf_16_w16_avx2(cospi_p48_p16, cospi_m16_p48, &x1[2], &x1[3], _r, cos_bit);
  btf_16_adds_subs_avx2(&x1[4], &x1[5]);
  btf_16_adds_subs_avx2(&x1[7], &x1[6]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x1[9], &x1[14], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x1[10], &x1[13], _r, cos_bit);
  btf_16_adds_subs_avx2(&x1[16], &x1[19]);
  btf_16_adds_subs_avx2(&x1[17], &x1[18]);
  btf_16_adds_subs_avx2(&x1[23], &x1[20]);
  btf_16_adds_subs_avx2(&x1[22], &x1[21]);
  btf_16_adds_subs_avx2(&x1[24], &x1[27]);
  btf_16_adds_subs_avx2(&x1[25], &x1[26]);
  btf_16_adds_subs_avx2(&x1[31], &x1[28]);
  btf_16_adds_subs_avx2(&x1[30], &x1[29]);
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, &x1[34], &x1[61], _r, cos_bit);
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, &x1[35], &x1[60], _r, cos_bit);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, &x1[36], &x1[59], _r, cos_bit);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, &x1[37], &x1[58], _r, cos_bit);
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, &x1[42], &x1[53], _r, cos_bit);
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, &x1[43], &x1[52], _r, cos_bit);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, &x1[44], &x1[51], _r, cos_bit);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, &x1[45], &x1[50], _r, cos_bit);

  // stage 7
  btf_16_w16_avx2(cospi_p56_p08, cospi_m08_p56, &x1[4], &x1[7], _r, cos_bit);
  btf_16_w16_avx2(cospi_p24_p40, cospi_m40_p24, &x1[5], &x1[6], _r, cos_bit);
  btf_16_adds_subs_avx2(&x1[8], &x1[9]);
  btf_16_adds_subs_avx2(&x1[11], &x1[10]);
  btf_16_adds_subs_avx2(&x1[12], &x1[13]);
  btf_16_adds_subs_avx2(&x1[15], &x1[14]);
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, &x1[17], &x1[30], _r, cos_bit);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, &x1[18], &x1[29], _r, cos_bit);
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, &x1[21], &x1[26], _r, cos_bit);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, &x1[22], &x1[25], _r, cos_bit);
  btf_16_adds_subs_avx2(&x1[32], &x1[35]);
  btf_16_adds_subs_avx2(&x1[33], &x1[34]);
  btf_16_adds_subs_avx2(&x1[39], &x1[36]);
  btf_16_adds_subs_avx2(&x1[38], &x1[37]);
  btf_16_adds_subs_avx2(&x1[40], &x1[43]);
  btf_16_adds_subs_avx2(&x1[41], &x1[42]);
  btf_16_adds_subs_avx2(&x1[47], &x1[44]);
  btf_16_adds_subs_avx2(&x1[46], &x1[45]);
  btf_16_adds_subs_avx2(&x1[48], &x1[51]);
  btf_16_adds_subs_avx2(&x1[49], &x1[50]);
  btf_16_adds_subs_avx2(&x1[55], &x1[52]);
  btf_16_adds_subs_avx2(&x1[54], &x1[53]);
  btf_16_adds_subs_avx2(&x1[56], &x1[59]);
  btf_16_adds_subs_avx2(&x1[57], &x1[58]);
  btf_16_adds_subs_avx2(&x1[63], &x1[60]);
  btf_16_adds_subs_avx2(&x1[62], &x1[61]);

  // stage 8
  btf_16_w16_avx2(cospi_p60_p04, cospi_m04_p60, &x1[8], &x1[15], _r, cos_bit);
  btf_16_w16_avx2(cospi_p28_p36, cospi_m36_p28, &x1[9], &x1[14], _r, cos_bit);
  btf_16_w16_avx2(cospi_p44_p20, cospi_m20_p44, &x1[10], &x1[13], _r, cos_bit);
  btf_16_w16_avx2(cospi_p12_p52, cospi_m52_p12, &x1[11], &x1[12], _r, cos_bit);
  btf_16_adds_subs_avx2(&x1[16], &x1[17]);
  btf_16_adds_subs_avx2(&x1[19], &x1[18]);
  btf_16_adds_subs_avx2(&x1[20], &x1[21]);
  btf_16_adds_subs_avx2(&x1[23], &x1[22]);
  btf_16_adds_subs_avx2(&x1[24], &x1[25]);
  btf_16_adds_subs_avx2(&x1[27], &x1[26]);
  btf_16_adds_subs_avx2(&x1[28], &x1[29]);
  btf_16_adds_subs_avx2(&x1[31], &x1[30]);
  btf_16_w16_avx2(cospi_m04_p60, cospi_p60_p04, &x1[33], &x1[62], _r, cos_bit);
  btf_16_w16_avx2(cospi_m60_m04, cospi_m04_p60, &x1[34], &x1[61], _r, cos_bit);
  btf_16_w16_avx2(cospi_m36_p28, cospi_p28_p36, &x1[37], &x1[58], _r, cos_bit);
  btf_16_w16_avx2(cospi_m28_m36, cospi_m36_p28, &x1[38], &x1[57], _r, cos_bit);
  btf_16_w16_avx2(cospi_m20_p44, cospi_p44_p20, &x1[41], &x1[54], _r, cos_bit);
  btf_16_w16_avx2(cospi_m44_m20, cospi_m20_p44, &x1[42], &x1[53], _r, cos_bit);
  btf_16_w16_avx2(cospi_m52_p12, cospi_p12_p52, &x1[45], &x1[50], _r, cos_bit);
  btf_16_w16_avx2(cospi_m12_m52, cospi_m52_p12, &x1[46], &x1[49], _r, cos_bit);

  // stage 9
  btf_16_w16_avx2(cospi_p62_p02, cospi_m02_p62, &x1[16], &x1[31], _r, cos_bit);
  btf_16_w16_avx2(cospi_p30_p34, cospi_m34_p30, &x1[17], &x1[30], _r, cos_bit);
  btf_16_w16_avx2(cospi_p46_p18, cospi_m18_p46, &x1[18], &x1[29], _r, cos_bit);
  btf_16_w16_avx2(cospi_p14_p50, cospi_m50_p14, &x1[19], &x1[28], _r, cos_bit);
  btf_16_w16_avx2(cospi_p54_p10, cospi_m10_p54, &x1[20], &x1[27], _r, cos_bit);
  btf_16_w16_avx2(cospi_p22_p42, cospi_m42_p22, &x1[21], &x1[26], _r, cos_bit);
  btf_16_w16_avx2(cospi_p38_p26, cospi_m26_p38, &x1[22], &x1[25], _r, cos_bit);
  btf_16_w16_avx2(cospi_p06_p58, cospi_m58_p06, &x1[23], &x1[24], _r, cos_bit);
  btf_16_adds_subs_avx2(&x1[32], &x1[33]);
  btf_16_adds_subs_avx2(&x1[35], &x1[34]);
  btf_16_adds_subs_avx2(&x1[36], &x1[37]);
  btf_16_adds_subs_avx2(&x1[39], &x1[38]);
  btf_16_adds_subs_avx2(&x1[40], &x1[41]);
  btf_16_adds_subs_avx2(&x1[43], &x1[42]);
  btf_16_adds_subs_avx2(&x1[44], &x1[45]);
  btf_16_adds_subs_avx2(&x1[47], &x1[46]);
  btf_16_adds_subs_avx2(&x1[48], &x1[49]);
  btf_16_adds_subs_avx2(&x1[51], &x1[50]);
  btf_16_adds_subs_avx2(&x1[52], &x1[53]);
  btf_16_adds_subs_avx2(&x1[55], &x1[54]);
  btf_16_adds_subs_avx2(&x1[56], &x1[57]);
  btf_16_adds_subs_avx2(&x1[59], &x1[58]);
  btf_16_adds_subs_avx2(&x1[60], &x1[61]);
  btf_16_adds_subs_avx2(&x1[63], &x1[62]);

  // stage 10
  btf_16_w16_avx2(cospi_p63_p01, cospi_m01_p63, &x1[32], &x1[63], _r, cos_bit);
  btf_16_w16_avx2(cospi_p31_p33, cospi_m33_p31, &x1[33], &x1[62], _r, cos_bit);
  btf_16_w16_avx2(cospi_p47_p17, cospi_m17_p47, &x1[34], &x1[61], _r, cos_bit);
  btf_16_w16_avx2(cospi_p15_p49, cospi_m49_p15, &x1[35], &x1[60], _r, cos_bit);
  btf_16_w16_avx2(cospi_p55_p09, cospi_m09_p55, &x1[36], &x1[59], _r, cos_bit);
  btf_16_w16_avx2(cospi_p23_p41, cospi_m41_p23, &x1[37], &x1[58], _r, cos_bit);
  btf_16_w16_avx2(cospi_p39_p25, cospi_m25_p39, &x1[38], &x1[57], _r, cos_bit);
  btf_16_w16_avx2(cospi_p07_p57, cospi_m57_p07, &x1[39], &x1[56], _r, cos_bit);
  btf_16_w16_avx2(cospi_p59_p05, cospi_m05_p59, &x1[40], &x1[55], _r, cos_bit);
  btf_16_w16_avx2(cospi_p27_p37, cospi_m37_p27, &x1[41], &x1[54], _r, cos_bit);
  btf_16_w16_avx2(cospi_p43_p21, cospi_m21_p43, &x1[42], &x1[53], _r, cos_bit);
  btf_16_w16_avx2(cospi_p11_p53, cospi_m53_p11, &x1[43], &x1[52], _r, cos_bit);
  btf_16_w16_avx2(cospi_p51_p13, cospi_m13_p51, &x1[44], &x1[51], _r, cos_bit);
  btf_16_w16_avx2(cospi_p19_p45, cospi_m45_p19, &x1[45], &x1[50], _r, cos_bit);
  btf_16_w16_avx2(cospi_p35_p29, cospi_m29_p35, &x1[46], &x1[49], _r, cos_bit);
  btf_16_w16_avx2(cospi_p03_p61, cospi_m61_p03, &x1[47], &x1[48], _r, cos_bit);

  // stage 11
  output[0] = x1[0];
  output[1] = x1[32];
  output[2] = x1[16];
  output[3] = x1[48];
  output[4] = x1[8];
  output[5] = x1[40];
  output[6] = x1[24];
  output[7] = x1[56];
  output[8] = x1[4];
  output[9] = x1[36];
  output[10] = x1[20];
  output[11] = x1[52];
  output[12] = x1[12];
  output[13] = x1[44];
  output[14] = x1[28];
  output[15] = x1[60];
  output[16] = x1[2];
  output[17] = x1[34];
  output[18] = x1[18];
  output[19] = x1[50];
  output[20] = x1[10];
  output[21] = x1[42];
  output[22] = x1[26];
  output[23] = x1[58];
  output[24] = x1[6];
  output[25] = x1[38];
  output[26] = x1[22];
  output[27] = x1[54];
  output[28] = x1[14];
  output[29] = x1[46];
  output[30] = x1[30];
  output[31] = x1[62];
  output[32] = x1[1];
  output[33] = x1[33];
  output[34] = x1[17];
  output[35] = x1[49];
  output[36] = x1[9];
  output[37] = x1[41];
  output[38] = x1[25];
  output[39] = x1[57];
  output[40] = x1[5];
  output[41] = x1[37];
  output[42] = x1[21];
  output[43] = x1[53];
  output[44] = x1[13];
  output[45] = x1[45];
  output[46] = x1[29];
  output[47] = x1[61];
  output[48] = x1[3];
  output[49] = x1[35];
  output[50] = x1[19];
  output[51] = x1[51];
  output[52] = x1[11];
  output[53] = x1[43];
  output[54] = x1[27];
  output[55] = x1[59];
  output[56] = x1[7];
  output[57] = x1[39];
  output[58] = x1[23];
  output[59] = x1[55];
  output[60] = x1[15];
  output[61] = x1[47];
  output[62] = x1[31];
  output[63] = x1[63];
}

static inline void fdct32_avx2(const __m256i *input, __m256i *output,
                               int8_t cos_bit_arg) {
  (void)cos_bit_arg;
  const int8_t cos_bit = 11;
  __m256i x1[32];
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
  // stage 0
  // stage 1
  btf_32_add_sub_out_avx2(&x1[0], &x1[31], input[0], input[31]);
  btf_32_add_sub_out_avx2(&x1[1], &x1[30], input[1], input[30]);
  btf_32_add_sub_out_avx2(&x1[2], &x1[29], input[2], input[29]);
  btf_32_add_sub_out_avx2(&x1[3], &x1[28], input[3], input[28]);
  btf_32_add_sub_out_avx2(&x1[4], &x1[27], input[4], input[27]);
  btf_32_add_sub_out_avx2(&x1[5], &x1[26], input[5], input[26]);
  btf_32_add_sub_out_avx2(&x1[6], &x1[25], input[6], input[25]);
  btf_32_add_sub_out_avx2(&x1[7], &x1[24], input[7], input[24]);
  btf_32_add_sub_out_avx2(&x1[8], &x1[23], input[8], input[23]);
  btf_32_add_sub_out_avx2(&x1[9], &x1[22], input[9], input[22]);
  btf_32_add_sub_out_avx2(&x1[10], &x1[21], input[10], input[21]);
  btf_32_add_sub_out_avx2(&x1[11], &x1[20], input[11], input[20]);
  btf_32_add_sub_out_avx2(&x1[12], &x1[19], input[12], input[19]);
  btf_32_add_sub_out_avx2(&x1[13], &x1[18], input[13], input[18]);
  btf_32_add_sub_out_avx2(&x1[14], &x1[17], input[14], input[17]);
  btf_32_add_sub_out_avx2(&x1[15], &x1[16], input[15], input[16]);

  // stage 2
  btf_32_add_sub_avx2(&x1[0], &x1[15]);
  btf_32_add_sub_avx2(&x1[1], &x1[14]);
  btf_32_add_sub_avx2(&x1[2], &x1[13]);
  btf_32_add_sub_avx2(&x1[3], &x1[12]);
  btf_32_add_sub_avx2(&x1[4], &x1[11]);
  btf_32_add_sub_avx2(&x1[5], &x1[10]);
  btf_32_add_sub_avx2(&x1[6], &x1[9]);
  btf_32_add_sub_avx2(&x1[7], &x1[8]);
  btf_32_avx2_type0(-cospi[32], cospi[32], &x1[20], &x1[27], _r, cos_bit);
  btf_32_avx2_type0(-cospi[32], cospi[32], &x1[21], &x1[26], _r, cos_bit);
  btf_32_avx2_type0(-cospi[32], cospi[32], &x1[22], &x1[25], _r, cos_bit);
  btf_32_avx2_type0(-cospi[32], cospi[32], &x1[23], &x1[24], _r, cos_bit);

  // stage 3
  btf_32_add_sub_avx2(&x1[0], &x1[7]);
  btf_32_add_sub_avx2(&x1[1], &x1[6]);
  btf_32_add_sub_avx2(&x1[2], &x1[5]);
  btf_32_add_sub_avx2(&x1[3], &x1[4]);
  btf_32_avx2_type0(-cospi[32], cospi[32], &x1[10], &x1[13], _r, cos_bit);
  btf_32_avx2_type0(-cospi[32], cospi[32], &x1[11], &x1[12], _r, cos_bit);
  btf_32_add_sub_avx2(&x1[16], &x1[23]);
  btf_32_add_sub_avx2(&x1[17], &x1[22]);
  btf_32_add_sub_avx2(&x1[18], &x1[21]);
  btf_32_add_sub_avx2(&x1[19], &x1[20]);
  btf_32_add_sub_avx2(&x1[31], &x1[24]);
  btf_32_add_sub_avx2(&x1[30], &x1[25]);
  btf_32_add_sub_avx2(&x1[29], &x1[26]);
  btf_32_add_sub_avx2(&x1[28], &x1[27]);

  // stage 4
  btf_32_add_sub_avx2(&x1[0], &x1[3]);
  btf_32_add_sub_avx2(&x1[1], &x1[2]);
  btf_32_avx2_type0(-cospi[32], cospi[32], &x1[5], &x1[6], _r, cos_bit);
  btf_32_add_sub_avx2(&x1[8], &x1[11]);
  btf_32_add_sub_avx2(&x1[9], &x1[10]);
  btf_32_add_sub_avx2(&x1[15], &x1[12]);
  btf_32_add_sub_avx2(&x1[14], &x1[13]);
  btf_32_avx2_type0(-cospi[16], cospi[48], &x1[18], &x1[29], _r, cos_bit);
  btf_32_avx2_type0(-cospi[16], cospi[48], &x1[19], &x1[28], _r, cos_bit);
  btf_32_avx2_type0(-cospi[48], -cospi[16], &x1[20], &x1[27], _r, cos_bit);
  btf_32_avx2_type0(-cospi[48], -cospi[16], &x1[21], &x1[26], _r, cos_bit);

  // stage 5
  btf_32_avx2_type0(cospi[32], cospi[32], &x1[0], &x1[1], _r, cos_bit);
  btf_32_avx2_type1(cospi[48], cospi[16], &x1[2], &x1[3], _r, cos_bit);
  btf_32_add_sub_avx2(&x1[4], &x1[5]);
  btf_32_add_sub_avx2(&x1[7], &x1[6]);
  btf_32_avx2_type0(-cospi[16], cospi[48], &x1[9], &x1[14], _r, cos_bit);
  btf_32_avx2_type0(-cospi[48], -cospi[16], &x1[10], &x1[13], _r, cos_bit);
  btf_32_add_sub_avx2(&x1[16], &x1[19]);
  btf_32_add_sub_avx2(&x1[17], &x1[18]);
  btf_32_add_sub_avx2(&x1[23], &x1[20]);
  btf_32_add_sub_avx2(&x1[22], &x1[21]);
  btf_32_add_sub_avx2(&x1[24], &x1[27]);
  btf_32_add_sub_avx2(&x1[25], &x1[26]);
  btf_32_add_sub_avx2(&x1[31], &x1[28]);
  btf_32_add_sub_avx2(&x1[30], &x1[29]);

  // stage 6
  btf_32_avx2_type1(cospi[56], cospi[8], &x1[4], &x1[7], _r, cos_bit);
  btf_32_avx2_type1(cospi[24], cospi[40], &x1[5], &x1[6], _r, cos_bit);
  btf_32_add_sub_avx2(&x1[8], &x1[9]);
  btf_32_add_sub_avx2(&x1[11], &x1[10]);
  btf_32_add_sub_avx2(&x1[12], &x1[13]);
  btf_32_add_sub_avx2(&x1[15], &x1[14]);
  btf_32_avx2_type0(-cospi[8], cospi[56], &x1[17], &x1[30], _r, cos_bit);
  btf_32_avx2_type0(-cospi[56], -cospi[8], &x1[18], &x1[29], _r, cos_bit);
  btf_32_avx2_type0(-cospi[40], cospi[24], &x1[21], &x1[26], _r, cos_bit);
  btf_32_avx2_type0(-cospi[24], -cospi[40], &x1[22], &x1[25], _r, cos_bit);

  // stage 7
  btf_32_avx2_type1(cospi[60], cospi[4], &x1[8], &x1[15], _r, cos_bit);
  btf_32_avx2_type1(cospi[28], cospi[36], &x1[9], &x1[14], _r, cos_bit);
  btf_32_avx2_type1(cospi[44], cospi[20], &x1[10], &x1[13], _r, cos_bit);
  btf_32_avx2_type1(cospi[12], cospi[52], &x1[11], &x1[12], _r, cos_bit);
  btf_32_add_sub_avx2(&x1[16], &x1[17]);
  btf_32_add_sub_avx2(&x1[19], &x1[18]);
  btf_32_add_sub_avx2(&x1[20], &x1[21]);
  btf_32_add_sub_avx2(&x1[23], &x1[22]);
  btf_32_add_sub_avx2(&x1[24], &x1[25]);
  btf_32_add_sub_avx2(&x1[27], &x1[26]);
  btf_32_add_sub_avx2(&x1[28], &x1[29]);
  btf_32_add_sub_avx2(&x1[31], &x1[30]);

  // stage 8
  btf_32_avx2_type1(cospi[62], cospi[2], &x1[16], &x1[31], _r, cos_bit);
  btf_32_avx2_type1(cospi[30], cospi[34], &x1[17], &x1[30], _r, cos_bit);
  btf_32_avx2_type1(cospi[46], cospi[18], &x1[18], &x1[29], _r, cos_bit);
  btf_32_avx2_type1(cospi[14], cospi[50], &x1[19], &x1[28], _r, cos_bit);
  btf_32_avx2_type1(cospi[54], cospi[10], &x1[20], &x1[27], _r, cos_bit);
  btf_32_avx2_type1(cospi[22], cospi[42], &x1[21], &x1[26], _r, cos_bit);
  btf_32_avx2_type1(cospi[38], cospi[26], &x1[22], &x1[25], _r, cos_bit);
  btf_32_avx2_type1(cospi[6], cospi[58], &x1[23], &x1[24], _r, cos_bit);

  // stage 9
  output[0] = x1[0];
  output[1] = x1[16];
  output[2] = x1[8];
  output[3] = x1[24];
  output[4] = x1[4];
  output[5] = x1[20];
  output[6] = x1[12];
  output[7] = x1[28];
  output[8] = x1[2];
  output[9] = x1[18];
  output[10] = x1[10];
  output[11] = x1[26];
  output[12] = x1[6];
  output[13] = x1[22];
  output[14] = x1[14];
  output[15] = x1[30];
  output[16] = x1[1];
  output[17] = x1[17];
  output[18] = x1[9];
  output[19] = x1[25];
  output[20] = x1[5];
  output[21] = x1[21];
  output[22] = x1[13];
  output[23] = x1[29];
  output[24] = x1[3];
  output[25] = x1[19];
  output[26] = x1[11];
  output[27] = x1[27];
  output[28] = x1[7];
  output[29] = x1[23];
  output[30] = x1[15];
  output[31] = x1[31];
}

static inline void fdct64_new_avx2(const __m256i *input, __m256i *output,
                                   int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));

  __m256i cospi_m32 = _mm256_set1_epi32(-cospi[32]);
  __m256i cospi_p32 = _mm256_set1_epi32(cospi[32]);
  __m256i cospi_m16 = _mm256_set1_epi32(-cospi[16]);
  __m256i cospi_p48 = _mm256_set1_epi32(cospi[48]);
  __m256i cospi_m48 = _mm256_set1_epi32(-cospi[48]);
  __m256i cospi_p16 = _mm256_set1_epi32(cospi[16]);
  __m256i cospi_m08 = _mm256_set1_epi32(-cospi[8]);
  __m256i cospi_p56 = _mm256_set1_epi32(cospi[56]);
  __m256i cospi_m56 = _mm256_set1_epi32(-cospi[56]);
  __m256i cospi_m40 = _mm256_set1_epi32(-cospi[40]);
  __m256i cospi_p24 = _mm256_set1_epi32(cospi[24]);
  __m256i cospi_m24 = _mm256_set1_epi32(-cospi[24]);
  __m256i cospi_p08 = _mm256_set1_epi32(cospi[8]);
  __m256i cospi_p40 = _mm256_set1_epi32(cospi[40]);
  __m256i cospi_p60 = _mm256_set1_epi32(cospi[60]);
  __m256i cospi_p04 = _mm256_set1_epi32(cospi[4]);
  __m256i cospi_p28 = _mm256_set1_epi32(cospi[28]);
  __m256i cospi_p36 = _mm256_set1_epi32(cospi[36]);
  __m256i cospi_p44 = _mm256_set1_epi32(cospi[44]);
  __m256i cospi_p20 = _mm256_set1_epi32(cospi[20]);
  __m256i cospi_p12 = _mm256_set1_epi32(cospi[12]);
  __m256i cospi_p52 = _mm256_set1_epi32(cospi[52]);
  __m256i cospi_m04 = _mm256_set1_epi32(-cospi[4]);
  __m256i cospi_m60 = _mm256_set1_epi32(-cospi[60]);
  __m256i cospi_m36 = _mm256_set1_epi32(-cospi[36]);
  __m256i cospi_m28 = _mm256_set1_epi32(-cospi[28]);
  __m256i cospi_m20 = _mm256_set1_epi32(-cospi[20]);
  __m256i cospi_m44 = _mm256_set1_epi32(-cospi[44]);
  __m256i cospi_m52 = _mm256_set1_epi32(-cospi[52]);
  __m256i cospi_m12 = _mm256_set1_epi32(-cospi[12]);
  __m256i cospi_p62 = _mm256_set1_epi32(cospi[62]);
  __m256i cospi_p02 = _mm256_set1_epi32(cospi[2]);
  __m256i cospi_p30 = _mm256_set1_epi32(cospi[30]);
  __m256i cospi_p34 = _mm256_set1_epi32(cospi[34]);
  __m256i cospi_p46 = _mm256_set1_epi32(cospi[46]);
  __m256i cospi_p18 = _mm256_set1_epi32(cospi[18]);
  __m256i cospi_p14 = _mm256_set1_epi32(cospi[14]);
  __m256i cospi_p50 = _mm256_set1_epi32(cospi[50]);
  __m256i cospi_p54 = _mm256_set1_epi32(cospi[54]);
  __m256i cospi_p10 = _mm256_set1_epi32(cospi[10]);
  __m256i cospi_p22 = _mm256_set1_epi32(cospi[22]);
  __m256i cospi_p42 = _mm256_set1_epi32(cospi[42]);
  __m256i cospi_p38 = _mm256_set1_epi32(cospi[38]);
  __m256i cospi_p26 = _mm256_set1_epi32(cospi[26]);
  __m256i cospi_p06 = _mm256_set1_epi32(cospi[6]);
  __m256i cospi_p58 = _mm256_set1_epi32(cospi[58]);
  __m256i cospi_p63 = _mm256_set1_epi32(cospi[63]);
  __m256i cospi_p01 = _mm256_set1_epi32(cospi[1]);
  __m256i cospi_p31 = _mm256_set1_epi32(cospi[31]);
  __m256i cospi_p33 = _mm256_set1_epi32(cospi[33]);
  __m256i cospi_p47 = _mm256_set1_epi32(cospi[47]);
  __m256i cospi_p17 = _mm256_set1_epi32(cospi[17]);
  __m256i cospi_p15 = _mm256_set1_epi32(cospi[15]);
  __m256i cospi_p49 = _mm256_set1_epi32(cospi[49]);
  __m256i cospi_p55 = _mm256_set1_epi32(cospi[55]);
  __m256i cospi_p09 = _mm256_set1_epi32(cospi[9]);
  __m256i cospi_p23 = _mm256_set1_epi32(cospi[23]);
  __m256i cospi_p41 = _mm256_set1_epi32(cospi[41]);
  __m256i cospi_p39 = _mm256_set1_epi32(cospi[39]);
  __m256i cospi_p25 = _mm256_set1_epi32(cospi[25]);
  __m256i cospi_p07 = _mm256_set1_epi32(cospi[7]);
  __m256i cospi_p57 = _mm256_set1_epi32(cospi[57]);
  __m256i cospi_p59 = _mm256_set1_epi32(cospi[59]);
  __m256i cospi_p05 = _mm256_set1_epi32(cospi[5]);
  __m256i cospi_p27 = _mm256_set1_epi32(cospi[27]);
  __m256i cospi_p37 = _mm256_set1_epi32(cospi[37]);
  __m256i cospi_p43 = _mm256_set1_epi32(cospi[43]);
  __m256i cospi_p21 = _mm256_set1_epi32(cospi[21]);
  __m256i cospi_p11 = _mm256_set1_epi32(cospi[11]);
  __m256i cospi_p53 = _mm256_set1_epi32(cospi[53]);
  __m256i cospi_p51 = _mm256_set1_epi32(cospi[51]);
  __m256i cospi_p13 = _mm256_set1_epi32(cospi[13]);
  __m256i cospi_p19 = _mm256_set1_epi32(cospi[19]);
  __m256i cospi_p45 = _mm256_set1_epi32(cospi[45]);
  __m256i cospi_p35 = _mm256_set1_epi32(cospi[35]);
  __m256i cospi_p29 = _mm256_set1_epi32(cospi[29]);
  __m256i cospi_p03 = _mm256_set1_epi32(cospi[3]);
  __m256i cospi_p61 = _mm256_set1_epi32(cospi[61]);

  // stage 1
  __m256i x1[64];
  btf_32_add_sub_out_avx2(&x1[0], &x1[63], input[0], input[63]);
  btf_32_add_sub_out_avx2(&x1[1], &x1[62], input[1], input[62]);
  btf_32_add_sub_out_avx2(&x1[2], &x1[61], input[2], input[61]);
  btf_32_add_sub_out_avx2(&x1[3], &x1[60], input[3], input[60]);
  btf_32_add_sub_out_avx2(&x1[4], &x1[59], input[4], input[59]);
  btf_32_add_sub_out_avx2(&x1[5], &x1[58], input[5], input[58]);
  btf_32_add_sub_out_avx2(&x1[6], &x1[57], input[6], input[57]);
  btf_32_add_sub_out_avx2(&x1[7], &x1[56], input[7], input[56]);
  btf_32_add_sub_out_avx2(&x1[8], &x1[55], input[8], input[55]);
  btf_32_add_sub_out_avx2(&x1[9], &x1[54], input[9], input[54]);
  btf_32_add_sub_out_avx2(&x1[10], &x1[53], input[10], input[53]);
  btf_32_add_sub_out_avx2(&x1[11], &x1[52], input[11], input[52]);
  btf_32_add_sub_out_avx2(&x1[12], &x1[51], input[12], input[51]);
  btf_32_add_sub_out_avx2(&x1[13], &x1[50], input[13], input[50]);
  btf_32_add_sub_out_avx2(&x1[14], &x1[49], input[14], input[49]);
  btf_32_add_sub_out_avx2(&x1[15], &x1[48], input[15], input[48]);
  btf_32_add_sub_out_avx2(&x1[16], &x1[47], input[16], input[47]);
  btf_32_add_sub_out_avx2(&x1[17], &x1[46], input[17], input[46]);
  btf_32_add_sub_out_avx2(&x1[18], &x1[45], input[18], input[45]);
  btf_32_add_sub_out_avx2(&x1[19], &x1[44], input[19], input[44]);
  btf_32_add_sub_out_avx2(&x1[20], &x1[43], input[20], input[43]);
  btf_32_add_sub_out_avx2(&x1[21], &x1[42], input[21], input[42]);
  btf_32_add_sub_out_avx2(&x1[22], &x1[41], input[22], input[41]);
  btf_32_add_sub_out_avx2(&x1[23], &x1[40], input[23], input[40]);
  btf_32_add_sub_out_avx2(&x1[24], &x1[39], input[24], input[39]);
  btf_32_add_sub_out_avx2(&x1[25], &x1[38], input[25], input[38]);
  btf_32_add_sub_out_avx2(&x1[26], &x1[37], input[26], input[37]);
  btf_32_add_sub_out_avx2(&x1[27], &x1[36], input[27], input[36]);
  btf_32_add_sub_out_avx2(&x1[28], &x1[35], input[28], input[35]);
  btf_32_add_sub_out_avx2(&x1[29], &x1[34], input[29], input[34]);
  btf_32_add_sub_out_avx2(&x1[30], &x1[33], input[30], input[33]);
  btf_32_add_sub_out_avx2(&x1[31], &x1[32], input[31], input[32]);

  // stage 2
  btf_32_add_sub_avx2(&x1[0], &x1[31]);
  btf_32_add_sub_avx2(&x1[1], &x1[30]);
  btf_32_add_sub_avx2(&x1[2], &x1[29]);
  btf_32_add_sub_avx2(&x1[3], &x1[28]);
  btf_32_add_sub_avx2(&x1[4], &x1[27]);
  btf_32_add_sub_avx2(&x1[5], &x1[26]);
  btf_32_add_sub_avx2(&x1[6], &x1[25]);
  btf_32_add_sub_avx2(&x1[7], &x1[24]);
  btf_32_add_sub_avx2(&x1[8], &x1[23]);
  btf_32_add_sub_avx2(&x1[9], &x1[22]);
  btf_32_add_sub_avx2(&x1[10], &x1[21]);
  btf_32_add_sub_avx2(&x1[11], &x1[20]);
  btf_32_add_sub_avx2(&x1[12], &x1[19]);
  btf_32_add_sub_avx2(&x1[13], &x1[18]);
  btf_32_add_sub_avx2(&x1[14], &x1[17]);
  btf_32_add_sub_avx2(&x1[15], &x1[16]);
  btf_32_avx2_type0_new(cospi_m32, cospi_p32, &x1[40], &x1[55], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m32, cospi_p32, &x1[41], &x1[54], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m32, cospi_p32, &x1[42], &x1[53], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m32, cospi_p32, &x1[43], &x1[52], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m32, cospi_p32, &x1[44], &x1[51], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m32, cospi_p32, &x1[45], &x1[50], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m32, cospi_p32, &x1[46], &x1[49], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m32, cospi_p32, &x1[47], &x1[48], _r, cos_bit);

  // stage 3
  btf_32_add_sub_avx2(&x1[0], &x1[15]);
  btf_32_add_sub_avx2(&x1[1], &x1[14]);
  btf_32_add_sub_avx2(&x1[2], &x1[13]);
  btf_32_add_sub_avx2(&x1[3], &x1[12]);
  btf_32_add_sub_avx2(&x1[4], &x1[11]);
  btf_32_add_sub_avx2(&x1[5], &x1[10]);
  btf_32_add_sub_avx2(&x1[6], &x1[9]);
  btf_32_add_sub_avx2(&x1[7], &x1[8]);
  btf_32_avx2_type0_new(cospi_m32, cospi_p32, &x1[20], &x1[27], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m32, cospi_p32, &x1[21], &x1[26], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m32, cospi_p32, &x1[22], &x1[25], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m32, cospi_p32, &x1[23], &x1[24], _r, cos_bit);
  btf_32_add_sub_avx2(&x1[32], &x1[47]);
  btf_32_add_sub_avx2(&x1[33], &x1[46]);
  btf_32_add_sub_avx2(&x1[34], &x1[45]);
  btf_32_add_sub_avx2(&x1[35], &x1[44]);
  btf_32_add_sub_avx2(&x1[36], &x1[43]);
  btf_32_add_sub_avx2(&x1[37], &x1[42]);
  btf_32_add_sub_avx2(&x1[38], &x1[41]);
  btf_32_add_sub_avx2(&x1[39], &x1[40]);
  btf_32_add_sub_avx2(&x1[63], &x1[48]);
  btf_32_add_sub_avx2(&x1[62], &x1[49]);
  btf_32_add_sub_avx2(&x1[61], &x1[50]);
  btf_32_add_sub_avx2(&x1[60], &x1[51]);
  btf_32_add_sub_avx2(&x1[59], &x1[52]);
  btf_32_add_sub_avx2(&x1[58], &x1[53]);
  btf_32_add_sub_avx2(&x1[57], &x1[54]);
  btf_32_add_sub_avx2(&x1[56], &x1[55]);

  // stage 4
  btf_32_add_sub_avx2(&x1[0], &x1[7]);
  btf_32_add_sub_avx2(&x1[1], &x1[6]);
  btf_32_add_sub_avx2(&x1[2], &x1[5]);
  btf_32_add_sub_avx2(&x1[3], &x1[4]);
  btf_32_avx2_type0_new(cospi_m32, cospi_p32, &x1[10], &x1[13], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m32, cospi_p32, &x1[11], &x1[12], _r, cos_bit);
  btf_32_add_sub_avx2(&x1[16], &x1[23]);
  btf_32_add_sub_avx2(&x1[17], &x1[22]);
  btf_32_add_sub_avx2(&x1[18], &x1[21]);
  btf_32_add_sub_avx2(&x1[19], &x1[20]);
  btf_32_add_sub_avx2(&x1[31], &x1[24]);
  btf_32_add_sub_avx2(&x1[30], &x1[25]);
  btf_32_add_sub_avx2(&x1[29], &x1[26]);
  btf_32_add_sub_avx2(&x1[28], &x1[27]);
  btf_32_avx2_type0_new(cospi_m16, cospi_p48, &x1[36], &x1[59], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m16, cospi_p48, &x1[37], &x1[58], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m16, cospi_p48, &x1[38], &x1[57], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m16, cospi_p48, &x1[39], &x1[56], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m48, cospi_m16, &x1[40], &x1[55], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m48, cospi_m16, &x1[41], &x1[54], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m48, cospi_m16, &x1[42], &x1[53], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m48, cospi_m16, &x1[43], &x1[52], _r, cos_bit);

  // stage 5
  btf_32_add_sub_avx2(&x1[0], &x1[3]);
  btf_32_add_sub_avx2(&x1[1], &x1[2]);
  btf_32_avx2_type0_new(cospi_m32, cospi_p32, &x1[5], &x1[6], _r, cos_bit);
  btf_32_add_sub_avx2(&x1[8], &x1[11]);
  btf_32_add_sub_avx2(&x1[9], &x1[10]);
  btf_32_add_sub_avx2(&x1[15], &x1[12]);
  btf_32_add_sub_avx2(&x1[14], &x1[13]);
  btf_32_avx2_type0_new(cospi_m16, cospi_p48, &x1[18], &x1[29], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m16, cospi_p48, &x1[19], &x1[28], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m48, cospi_m16, &x1[20], &x1[27], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m48, cospi_m16, &x1[21], &x1[26], _r, cos_bit);
  btf_32_add_sub_avx2(&x1[32], &x1[39]);
  btf_32_add_sub_avx2(&x1[33], &x1[38]);
  btf_32_add_sub_avx2(&x1[34], &x1[37]);
  btf_32_add_sub_avx2(&x1[35], &x1[36]);
  btf_32_add_sub_avx2(&x1[47], &x1[40]);
  btf_32_add_sub_avx2(&x1[46], &x1[41]);
  btf_32_add_sub_avx2(&x1[45], &x1[42]);
  btf_32_add_sub_avx2(&x1[44], &x1[43]);
  btf_32_add_sub_avx2(&x1[48], &x1[55]);
  btf_32_add_sub_avx2(&x1[49], &x1[54]);
  btf_32_add_sub_avx2(&x1[50], &x1[53]);
  btf_32_add_sub_avx2(&x1[51], &x1[52]);
  btf_32_add_sub_avx2(&x1[63], &x1[56]);
  btf_32_add_sub_avx2(&x1[62], &x1[57]);
  btf_32_add_sub_avx2(&x1[61], &x1[58]);
  btf_32_add_sub_avx2(&x1[60], &x1[59]);

  // stage 6
  btf_32_avx2_type0_new(cospi_p32, cospi_p32, &x1[0], &x1[1], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p48, cospi_p16, &x1[2], &x1[3], _r, cos_bit);
  btf_32_add_sub_avx2(&x1[4], &x1[5]);
  btf_32_add_sub_avx2(&x1[7], &x1[6]);
  btf_32_avx2_type0_new(cospi_m16, cospi_p48, &x1[9], &x1[14], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m48, cospi_m16, &x1[10], &x1[13], _r, cos_bit);
  btf_32_add_sub_avx2(&x1[16], &x1[19]);
  btf_32_add_sub_avx2(&x1[17], &x1[18]);
  btf_32_add_sub_avx2(&x1[23], &x1[20]);
  btf_32_add_sub_avx2(&x1[22], &x1[21]);
  btf_32_add_sub_avx2(&x1[24], &x1[27]);
  btf_32_add_sub_avx2(&x1[25], &x1[26]);
  btf_32_add_sub_avx2(&x1[31], &x1[28]);
  btf_32_add_sub_avx2(&x1[30], &x1[29]);
  btf_32_avx2_type0_new(cospi_m08, cospi_p56, &x1[34], &x1[61], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m08, cospi_p56, &x1[35], &x1[60], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m56, cospi_m08, &x1[36], &x1[59], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m56, cospi_m08, &x1[37], &x1[58], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m40, cospi_p24, &x1[42], &x1[53], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m40, cospi_p24, &x1[43], &x1[52], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m24, cospi_m40, &x1[44], &x1[51], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m24, cospi_m40, &x1[45], &x1[50], _r, cos_bit);

  // stage 7
  btf_32_avx2_type1_new(cospi_p56, cospi_p08, &x1[4], &x1[7], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p24, cospi_p40, &x1[5], &x1[6], _r, cos_bit);
  btf_32_add_sub_avx2(&x1[8], &x1[9]);
  btf_32_add_sub_avx2(&x1[11], &x1[10]);
  btf_32_add_sub_avx2(&x1[12], &x1[13]);
  btf_32_add_sub_avx2(&x1[15], &x1[14]);
  btf_32_avx2_type0_new(cospi_m08, cospi_p56, &x1[17], &x1[30], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m56, cospi_m08, &x1[18], &x1[29], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m40, cospi_p24, &x1[21], &x1[26], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m24, cospi_m40, &x1[22], &x1[25], _r, cos_bit);
  btf_32_add_sub_avx2(&x1[32], &x1[35]);
  btf_32_add_sub_avx2(&x1[33], &x1[34]);
  btf_32_add_sub_avx2(&x1[39], &x1[36]);
  btf_32_add_sub_avx2(&x1[38], &x1[37]);
  btf_32_add_sub_avx2(&x1[40], &x1[43]);
  btf_32_add_sub_avx2(&x1[41], &x1[42]);
  btf_32_add_sub_avx2(&x1[47], &x1[44]);
  btf_32_add_sub_avx2(&x1[46], &x1[45]);
  btf_32_add_sub_avx2(&x1[48], &x1[51]);
  btf_32_add_sub_avx2(&x1[49], &x1[50]);
  btf_32_add_sub_avx2(&x1[55], &x1[52]);
  btf_32_add_sub_avx2(&x1[54], &x1[53]);
  btf_32_add_sub_avx2(&x1[56], &x1[59]);
  btf_32_add_sub_avx2(&x1[57], &x1[58]);
  btf_32_add_sub_avx2(&x1[63], &x1[60]);
  btf_32_add_sub_avx2(&x1[62], &x1[61]);

  // stage 8
  btf_32_avx2_type1_new(cospi_p60, cospi_p04, &x1[8], &x1[15], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p28, cospi_p36, &x1[9], &x1[14], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p44, cospi_p20, &x1[10], &x1[13], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p12, cospi_p52, &x1[11], &x1[12], _r, cos_bit);
  btf_32_add_sub_avx2(&x1[16], &x1[17]);
  btf_32_add_sub_avx2(&x1[19], &x1[18]);
  btf_32_add_sub_avx2(&x1[20], &x1[21]);
  btf_32_add_sub_avx2(&x1[23], &x1[22]);
  btf_32_add_sub_avx2(&x1[24], &x1[25]);
  btf_32_add_sub_avx2(&x1[27], &x1[26]);
  btf_32_add_sub_avx2(&x1[28], &x1[29]);
  btf_32_add_sub_avx2(&x1[31], &x1[30]);
  btf_32_avx2_type0_new(cospi_m04, cospi_p60, &x1[33], &x1[62], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m60, cospi_m04, &x1[34], &x1[61], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m36, cospi_p28, &x1[37], &x1[58], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m28, cospi_m36, &x1[38], &x1[57], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m20, cospi_p44, &x1[41], &x1[54], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m44, cospi_m20, &x1[42], &x1[53], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m52, cospi_p12, &x1[45], &x1[50], _r, cos_bit);
  btf_32_avx2_type0_new(cospi_m12, cospi_m52, &x1[46], &x1[49], _r, cos_bit);

  // stage 9
  btf_32_avx2_type1_new(cospi_p62, cospi_p02, &x1[16], &x1[31], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p30, cospi_p34, &x1[17], &x1[30], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p46, cospi_p18, &x1[18], &x1[29], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p14, cospi_p50, &x1[19], &x1[28], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p54, cospi_p10, &x1[20], &x1[27], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p22, cospi_p42, &x1[21], &x1[26], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p38, cospi_p26, &x1[22], &x1[25], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p06, cospi_p58, &x1[23], &x1[24], _r, cos_bit);
  btf_32_add_sub_avx2(&x1[32], &x1[33]);
  btf_32_add_sub_avx2(&x1[35], &x1[34]);
  btf_32_add_sub_avx2(&x1[36], &x1[37]);
  btf_32_add_sub_avx2(&x1[39], &x1[38]);
  btf_32_add_sub_avx2(&x1[40], &x1[41]);
  btf_32_add_sub_avx2(&x1[43], &x1[42]);
  btf_32_add_sub_avx2(&x1[44], &x1[45]);
  btf_32_add_sub_avx2(&x1[47], &x1[46]);
  btf_32_add_sub_avx2(&x1[48], &x1[49]);
  btf_32_add_sub_avx2(&x1[51], &x1[50]);
  btf_32_add_sub_avx2(&x1[52], &x1[53]);
  btf_32_add_sub_avx2(&x1[55], &x1[54]);
  btf_32_add_sub_avx2(&x1[56], &x1[57]);
  btf_32_add_sub_avx2(&x1[59], &x1[58]);
  btf_32_add_sub_avx2(&x1[60], &x1[61]);
  btf_32_add_sub_avx2(&x1[63], &x1[62]);

  // stage 10
  btf_32_avx2_type1_new(cospi_p63, cospi_p01, &x1[32], &x1[63], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p31, cospi_p33, &x1[33], &x1[62], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p47, cospi_p17, &x1[34], &x1[61], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p15, cospi_p49, &x1[35], &x1[60], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p55, cospi_p09, &x1[36], &x1[59], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p23, cospi_p41, &x1[37], &x1[58], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p39, cospi_p25, &x1[38], &x1[57], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p07, cospi_p57, &x1[39], &x1[56], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p59, cospi_p05, &x1[40], &x1[55], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p27, cospi_p37, &x1[41], &x1[54], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p43, cospi_p21, &x1[42], &x1[53], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p11, cospi_p53, &x1[43], &x1[52], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p51, cospi_p13, &x1[44], &x1[51], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p19, cospi_p45, &x1[45], &x1[50], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p35, cospi_p29, &x1[46], &x1[49], _r, cos_bit);
  btf_32_avx2_type1_new(cospi_p03, cospi_p61, &x1[47], &x1[48], _r, cos_bit);

  // stage 11
  output[0] = x1[0];
  output[1] = x1[32];
  output[2] = x1[16];
  output[3] = x1[48];
  output[4] = x1[8];
  output[5] = x1[40];
  output[6] = x1[24];
  output[7] = x1[56];
  output[8] = x1[4];
  output[9] = x1[36];
  output[10] = x1[20];
  output[11] = x1[52];
  output[12] = x1[12];
  output[13] = x1[44];
  output[14] = x1[28];
  output[15] = x1[60];
  output[16] = x1[2];
  output[17] = x1[34];
  output[18] = x1[18];
  output[19] = x1[50];
  output[20] = x1[10];
  output[21] = x1[42];
  output[22] = x1[26];
  output[23] = x1[58];
  output[24] = x1[6];
  output[25] = x1[38];
  output[26] = x1[22];
  output[27] = x1[54];
  output[28] = x1[14];
  output[29] = x1[46];
  output[30] = x1[30];
  output[31] = x1[62];
  output[32] = x1[1];
  output[33] = x1[33];
  output[34] = x1[17];
  output[35] = x1[49];
  output[36] = x1[9];
  output[37] = x1[41];
  output[38] = x1[25];
  output[39] = x1[57];
  output[40] = x1[5];
  output[41] = x1[37];
  output[42] = x1[21];
  output[43] = x1[53];
  output[44] = x1[13];
  output[45] = x1[45];
  output[46] = x1[29];
  output[47] = x1[61];
  output[48] = x1[3];
  output[49] = x1[35];
  output[50] = x1[19];
  output[51] = x1[51];
  output[52] = x1[11];
  output[53] = x1[43];
  output[54] = x1[27];
  output[55] = x1[59];
  output[56] = x1[7];
  output[57] = x1[39];
  output[58] = x1[23];
  output[59] = x1[55];
  output[60] = x1[15];
  output[61] = x1[47];
  output[62] = x1[31];
  output[63] = x1[63];
}

static AOM_FORCE_INLINE void store_row_16bit_to_32bit_w16_avx2(const __m256i v,
                                                               int32_t *out) {
  _mm256_store_si256((__m256i *)out,
                     _mm256_cvtepi16_epi32(_mm256_castsi256_si128(v)));
  _mm256_store_si256((__m256i *)(out + 8),
                     _mm256_cvtepi16_epi32(_mm256_extracti128_si256(v, 1)));
}

static AOM_FORCE_INLINE void fdct16x16_load_sll_avx2(const int16_t *input,
                                                     int stride,
                                                     __m256i *output,
                                                     int8_t cos_bit,
                                                     int sll_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);

  __m256i x0_0, x0_1, x0_2, x0_3, x0_4, x0_5, x0_6, x0_7;
  __m256i x1[8];
  const int16_t *in_top = input;
  const int16_t *in_bot = input + 15 * stride;
#define LOAD_STAGE1_PAIR(k)                                           \
  do {                                                                \
    const __m256i in_a =                                              \
        _mm256_slli_epi16(load_16bit_to_16bit_avx2(in_top), sll_bit); \
    const __m256i in_b =                                              \
        _mm256_slli_epi16(load_16bit_to_16bit_avx2(in_bot), sll_bit); \
    in_top += stride;                                                 \
    in_bot -= stride;                                                 \
    x0_##k = _mm256_adds_epi16(in_a, in_b);                           \
    x1[7 - (k)] = _mm256_subs_epi16(in_a, in_b);                      \
  } while (0)
  LOAD_STAGE1_PAIR(0);
  LOAD_STAGE1_PAIR(1);
  LOAD_STAGE1_PAIR(2);
  LOAD_STAGE1_PAIR(3);
  LOAD_STAGE1_PAIR(4);
  LOAD_STAGE1_PAIR(5);
  LOAD_STAGE1_PAIR(6);
  LOAD_STAGE1_PAIR(7);
#undef LOAD_STAGE1_PAIR

  {
    const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
    btf_16_adds_subs_avx2(&x0_0, &x0_7);
    btf_16_adds_subs_avx2(&x0_1, &x0_6);
    btf_16_adds_subs_avx2(&x0_2, &x0_5);
    btf_16_adds_subs_avx2(&x0_3, &x0_4);

    btf_16_adds_subs_avx2(&x0_0, &x0_3);
    btf_16_adds_subs_avx2(&x0_1, &x0_2);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi[32], cospi[32]),
                    pair_set_w16_epi16(cospi[32], cospi[32]), &x0_5, &x0_6, _r,
                    cos_bit);

    btf_16_w16_avx2(pair_set_w16_epi16(cospi[32], cospi[32]),
                    pair_set_w16_epi16(cospi[32], -cospi[32]), &x0_0, &x0_1, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[48], cospi[16]),
                    pair_set_w16_epi16(-cospi[16], cospi[48]), &x0_2, &x0_3, _r,
                    cos_bit);
    output[0] = x0_0;
    output[8] = x0_1;
    output[4] = x0_2;
    output[12] = x0_3;

    btf_16_adds_subs_avx2(&x0_4, &x0_5);
    btf_16_adds_subs_avx2(&x0_7, &x0_6);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[56], cospi[8]),
                    pair_set_w16_epi16(-cospi[8], cospi[56]), &x0_4, &x0_7, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[24], cospi[40]),
                    pair_set_w16_epi16(-cospi[40], cospi[24]), &x0_5, &x0_6, _r,
                    cos_bit);
    output[2] = x0_4;
    output[6] = x0_6;
    output[10] = x0_5;
    output[14] = x0_7;
  }

  {
    const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
    __m256i x1_0 = x1[0], x1_1 = x1[1], x1_2 = x1[2], x1_3 = x1[3];
    __m256i x1_4 = x1[4], x1_5 = x1[5], x1_6 = x1[6], x1_7 = x1[7];
    const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
    const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1_2, &x1_5, _r, cos_bit);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1_3, &x1_4, _r, cos_bit);

    btf_16_adds_subs_avx2(&x1_0, &x1_3);
    btf_16_adds_subs_avx2(&x1_1, &x1_2);
    btf_16_adds_subs_avx2(&x1_7, &x1_4);
    btf_16_adds_subs_avx2(&x1_6, &x1_5);

    const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
    btf_16_w16_avx2(cospi_m16_p48, pair_set_w16_epi16(cospi[48], cospi[16]),
                    &x1_1, &x1_6, _r, cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi[48], -cospi[16]), cospi_m16_p48,
                    &x1_2, &x1_5, _r, cos_bit);

    btf_16_adds_subs_avx2(&x1_0, &x1_1);
    btf_16_adds_subs_avx2(&x1_3, &x1_2);
    btf_16_adds_subs_avx2(&x1_4, &x1_5);
    btf_16_adds_subs_avx2(&x1_7, &x1_6);

    btf_16_w16_avx2(pair_set_w16_epi16(cospi[60], cospi[4]),
                    pair_set_w16_epi16(-cospi[4], cospi[60]), &x1_0, &x1_7, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[28], cospi[36]),
                    pair_set_w16_epi16(-cospi[36], cospi[28]), &x1_1, &x1_6, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[44], cospi[20]),
                    pair_set_w16_epi16(-cospi[20], cospi[44]), &x1_2, &x1_5, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[12], cospi[52]),
                    pair_set_w16_epi16(-cospi[52], cospi[12]), &x1_3, &x1_4, _r,
                    cos_bit);

    output[1] = x1_0;
    output[3] = x1_4;
    output[5] = x1_2;
    output[7] = x1_6;
    output[9] = x1_1;
    output[11] = x1_5;
    output[13] = x1_3;
    output[15] = x1_7;
  }
}

static void fdct16x16_col_cb13_avx2(const int16_t *in, __m256i *out,
                                    int stride) {
  fdct16x16_load_sll_avx2(in, stride, out, 13, 2);
}

static void fdct16x16_row_cb12_avx2(const __m256i *input, int32_t *output) {
  const int8_t cos_bit = 12;
  const int32_t *cospi = cospi_arr(cos_bit);

  __m256i x0_0, x0_1, x0_2, x0_3, x0_4, x0_5, x0_6, x0_7;
  __m256i x1[8];
#define ROW_STAGE1_PAIR(k)                       \
  do {                                           \
    const __m256i in_a = input[k];               \
    const __m256i in_b = input[15 - (k)];        \
    x0_##k = _mm256_adds_epi16(in_a, in_b);      \
    x1[7 - (k)] = _mm256_subs_epi16(in_a, in_b); \
  } while (0)
  ROW_STAGE1_PAIR(0);
  ROW_STAGE1_PAIR(1);
  ROW_STAGE1_PAIR(2);
  ROW_STAGE1_PAIR(3);
  ROW_STAGE1_PAIR(4);
  ROW_STAGE1_PAIR(5);
  ROW_STAGE1_PAIR(6);
  ROW_STAGE1_PAIR(7);
#undef ROW_STAGE1_PAIR

  {
    const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
    btf_16_adds_subs_avx2(&x0_0, &x0_7);
    btf_16_adds_subs_avx2(&x0_1, &x0_6);
    btf_16_adds_subs_avx2(&x0_2, &x0_5);
    btf_16_adds_subs_avx2(&x0_3, &x0_4);

    btf_16_adds_subs_avx2(&x0_0, &x0_3);
    btf_16_adds_subs_avx2(&x0_1, &x0_2);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi[32], cospi[32]),
                    pair_set_w16_epi16(cospi[32], cospi[32]), &x0_5, &x0_6, _r,
                    cos_bit);

    btf_16_w16_avx2(pair_set_w16_epi16(cospi[32], cospi[32]),
                    pair_set_w16_epi16(cospi[32], -cospi[32]), &x0_0, &x0_1, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[48], cospi[16]),
                    pair_set_w16_epi16(-cospi[16], cospi[48]), &x0_2, &x0_3, _r,
                    cos_bit);
    store_row_16bit_to_32bit_w16_avx2(x0_0, output + 0 * 16);
    store_row_16bit_to_32bit_w16_avx2(x0_1, output + 8 * 16);
    store_row_16bit_to_32bit_w16_avx2(x0_2, output + 4 * 16);
    store_row_16bit_to_32bit_w16_avx2(x0_3, output + 12 * 16);

    btf_16_adds_subs_avx2(&x0_4, &x0_5);
    btf_16_adds_subs_avx2(&x0_7, &x0_6);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[56], cospi[8]),
                    pair_set_w16_epi16(-cospi[8], cospi[56]), &x0_4, &x0_7, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[24], cospi[40]),
                    pair_set_w16_epi16(-cospi[40], cospi[24]), &x0_5, &x0_6, _r,
                    cos_bit);
    store_row_16bit_to_32bit_w16_avx2(x0_4, output + 2 * 16);
    store_row_16bit_to_32bit_w16_avx2(x0_6, output + 6 * 16);
    store_row_16bit_to_32bit_w16_avx2(x0_5, output + 10 * 16);
    store_row_16bit_to_32bit_w16_avx2(x0_7, output + 14 * 16);
  }

  {
    const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
    __m256i x1_0 = x1[0], x1_1 = x1[1], x1_2 = x1[2], x1_3 = x1[3];
    __m256i x1_4 = x1[4], x1_5 = x1[5], x1_6 = x1[6], x1_7 = x1[7];
    const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
    const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1_2, &x1_5, _r, cos_bit);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1_3, &x1_4, _r, cos_bit);

    btf_16_adds_subs_avx2(&x1_0, &x1_3);
    btf_16_adds_subs_avx2(&x1_1, &x1_2);
    btf_16_adds_subs_avx2(&x1_7, &x1_4);
    btf_16_adds_subs_avx2(&x1_6, &x1_5);

    const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
    btf_16_w16_avx2(cospi_m16_p48, pair_set_w16_epi16(cospi[48], cospi[16]),
                    &x1_1, &x1_6, _r, cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi[48], -cospi[16]), cospi_m16_p48,
                    &x1_2, &x1_5, _r, cos_bit);

    btf_16_adds_subs_avx2(&x1_0, &x1_1);
    btf_16_adds_subs_avx2(&x1_3, &x1_2);
    btf_16_adds_subs_avx2(&x1_4, &x1_5);
    btf_16_adds_subs_avx2(&x1_7, &x1_6);

    btf_16_w16_avx2(pair_set_w16_epi16(cospi[60], cospi[4]),
                    pair_set_w16_epi16(-cospi[4], cospi[60]), &x1_0, &x1_7, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[28], cospi[36]),
                    pair_set_w16_epi16(-cospi[36], cospi[28]), &x1_1, &x1_6, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[44], cospi[20]),
                    pair_set_w16_epi16(-cospi[20], cospi[44]), &x1_2, &x1_5, _r,
                    cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[12], cospi[52]),
                    pair_set_w16_epi16(-cospi[52], cospi[12]), &x1_3, &x1_4, _r,
                    cos_bit);

    store_row_16bit_to_32bit_w16_avx2(x1_0, output + 1 * 16);
    store_row_16bit_to_32bit_w16_avx2(x1_4, output + 3 * 16);
    store_row_16bit_to_32bit_w16_avx2(x1_2, output + 5 * 16);
    store_row_16bit_to_32bit_w16_avx2(x1_6, output + 7 * 16);
    store_row_16bit_to_32bit_w16_avx2(x1_1, output + 9 * 16);
    store_row_16bit_to_32bit_w16_avx2(x1_5, output + 11 * 16);
    store_row_16bit_to_32bit_w16_avx2(x1_3, output + 13 * 16);
    store_row_16bit_to_32bit_w16_avx2(x1_7, output + 15 * 16);
  }
}

static AOM_FORCE_INLINE void fadst16x16_new_avx2(const __m256i *input,
                                                 __m256i *output,
                                                 int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
  const __m256i __zero = _mm256_setzero_si256();

  // Step 1: Load {x6, x7, x14, x15} and run Stage 2 (cospi[32])
  __m256i x6 = input[4];
  __m256i x7 = _mm256_subs_epi16(__zero, input[11]);
  __m256i x14 = _mm256_subs_epi16(__zero, input[5]);
  __m256i x15 = input[10];
  {
    const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
    const __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x6, &x7, _r, cos_bit);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x14, &x15, _r, cos_bit);
  }

  // Step 2: Load {x4, x5, x12, x13} and run Stage 3 + Stage 4 (cospi[16, 48])
  __m256i x4 = _mm256_subs_epi16(__zero, input[3]);
  __m256i x5 = input[12];
  __m256i x12 = input[2];
  __m256i x13 = _mm256_subs_epi16(__zero, input[13]);
  btf_16_adds_subs_avx2(&x4, &x6);
  btf_16_adds_subs_avx2(&x5, &x7);
  btf_16_adds_subs_avx2(&x12, &x14);
  btf_16_adds_subs_avx2(&x13, &x15);
  {
    const __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
    const __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
    const __m256i cospi_m48_p16 = pair_set_w16_epi16(-cospi[48], cospi[16]);
    btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, &x4, &x5, _r, cos_bit);
    btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, &x12, &x13, _r, cos_bit);
    btf_16_w16_avx2(cospi_m48_p16, cospi_p16_p48, &x6, &x7, _r, cos_bit);
    btf_16_w16_avx2(cospi_m48_p16, cospi_p16_p48, &x14, &x15, _r, cos_bit);
  }

  // Step 3: Load {x2, x3, x10, x11} and run Stage 2 (cospi[32])
  __m256i x2 = _mm256_subs_epi16(__zero, input[7]);
  __m256i x3 = input[8];
  __m256i x10 = input[6];
  __m256i x11 = _mm256_subs_epi16(__zero, input[9]);
  {
    const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
    const __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x2, &x3, _r, cos_bit);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x10, &x11, _r, cos_bit);
  }

  // Step 4: Load {x0, x8, x1, x9} and run Stage 3 + Stage 5
  __m256i x0 = input[0];
  __m256i x8 = _mm256_subs_epi16(__zero, input[1]);
  btf_16_adds_subs_avx2(&x0, &x2);
  btf_16_adds_subs_avx2(&x8, &x10);
  btf_16_adds_subs_avx2(&x0, &x4);
  btf_16_adds_subs_avx2(&x2, &x6);
  btf_16_adds_subs_avx2(&x8, &x12);
  btf_16_adds_subs_avx2(&x10, &x14);

  __m256i x1 = _mm256_subs_epi16(__zero, input[15]);
  __m256i x9 = input[14];
  btf_16_adds_subs_avx2(&x1, &x3);
  btf_16_adds_subs_avx2(&x9, &x11);
  btf_16_adds_subs_avx2(&x1, &x5);
  btf_16_adds_subs_avx2(&x3, &x7);
  btf_16_adds_subs_avx2(&x9, &x13);
  btf_16_adds_subs_avx2(&x11, &x15);

  // Quad D ({x6, x7, x14, x15}) & Quad C ({x2, x3, x10, x11}): Stages 6..9
  {
    const __m256i cospi_p40_p24 = pair_set_w16_epi16(cospi[40], cospi[24]);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi[24], cospi[40]), cospi_p40_p24,
                    &x14, &x15, _r, cos_bit);
    btf_16_w16_avx2(cospi_p40_p24, pair_set_w16_epi16(cospi[24], -cospi[40]),
                    &x10, &x11, _r, cos_bit);
  }
  btf_16_adds_subs_avx2(&x6, &x14);
  btf_16_adds_subs_avx2(&x7, &x15);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[26], cospi[38]),
                  pair_set_w16_epi16(cospi[38], -cospi[26]), &x6, &x7, _r,
                  cos_bit);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[58], cospi[6]),
                  pair_set_w16_epi16(cospi[6], -cospi[58]), &x14, &x15, _r,
                  cos_bit);
  output[6] = x7;
  output[9] = x6;
  output[1] = x14;
  output[14] = x15;

  btf_16_adds_subs_avx2(&x2, &x10);
  btf_16_adds_subs_avx2(&x3, &x11);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[10], cospi[54]),
                  pair_set_w16_epi16(cospi[54], -cospi[10]), &x2, &x3, _r,
                  cos_bit);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[42], cospi[22]),
                  pair_set_w16_epi16(cospi[22], -cospi[42]), &x10, &x11, _r,
                  cos_bit);
  output[2] = x3;
  output[13] = x2;
  output[5] = x10;
  output[10] = x11;

  // Quad B ({x4, x5, x12, x13}) & Quad A ({x0, x1, x8, x9}): Stages 6..9
  {
    const __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi[56], cospi[8]), cospi_p08_p56,
                    &x12, &x13, _r, cos_bit);
    btf_16_w16_avx2(cospi_p08_p56, pair_set_w16_epi16(cospi[56], -cospi[8]),
                    &x8, &x9, _r, cos_bit);
  }
  btf_16_adds_subs_avx2(&x4, &x12);
  btf_16_adds_subs_avx2(&x5, &x13);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[18], cospi[46]),
                  pair_set_w16_epi16(cospi[46], -cospi[18]), &x4, &x5, _r,
                  cos_bit);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[50], cospi[14]),
                  pair_set_w16_epi16(cospi[14], -cospi[50]), &x12, &x13, _r,
                  cos_bit);
  output[4] = x5;
  output[11] = x4;
  output[3] = x12;
  output[12] = x13;

  btf_16_adds_subs_avx2(&x0, &x8);
  btf_16_adds_subs_avx2(&x1, &x9);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[2], cospi[62]),
                  pair_set_w16_epi16(cospi[62], -cospi[2]), &x0, &x1, _r,
                  cos_bit);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[34], cospi[30]),
                  pair_set_w16_epi16(cospi[30], -cospi[34]), &x8, &x9, _r,
                  cos_bit);
  output[0] = x1;
  output[15] = x0;
  output[7] = x8;
  output[8] = x9;
}

static inline void fadst16x16_cb13_avx2(const __m256i *input, __m256i *output,
                                        int8_t cos_bit) {
  (void)cos_bit;
  fadst16x16_new_avx2(input, output, 13);
}

static void fadst16x16_col_cb13_avx2(const int16_t *in, __m256i *output,
                                     int stride) {
  const int8_t cos_bit = 13;
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
  const __m256i __zero = _mm256_setzero_si256();
#define LOAD_SLL2(idx) \
  _mm256_slli_epi16(load_16bit_to_16bit_avx2(in + (idx) * stride), 2)

  // Step 1: Load {x6, x7, x14, x15} and run Stage 2 (cospi[32])
  __m256i x6 = LOAD_SLL2(4);
  __m256i x7 = _mm256_subs_epi16(__zero, LOAD_SLL2(11));
  __m256i x14 = _mm256_subs_epi16(__zero, LOAD_SLL2(5));
  __m256i x15 = LOAD_SLL2(10);
  {
    const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
    const __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x6, &x7, _r, cos_bit);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x14, &x15, _r, cos_bit);
  }

  // Step 2: Load {x4, x5, x12, x13} and run Stage 3 + Stage 4 (cospi[16, 48])
  __m256i x4 = _mm256_subs_epi16(__zero, LOAD_SLL2(3));
  __m256i x5 = LOAD_SLL2(12);
  __m256i x12 = LOAD_SLL2(2);
  __m256i x13 = _mm256_subs_epi16(__zero, LOAD_SLL2(13));
  btf_16_adds_subs_avx2(&x4, &x6);
  btf_16_adds_subs_avx2(&x5, &x7);
  btf_16_adds_subs_avx2(&x12, &x14);
  btf_16_adds_subs_avx2(&x13, &x15);
  {
    const __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
    const __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
    const __m256i cospi_m48_p16 = pair_set_w16_epi16(-cospi[48], cospi[16]);
    btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, &x4, &x5, _r, cos_bit);
    btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, &x12, &x13, _r, cos_bit);
    btf_16_w16_avx2(cospi_m48_p16, cospi_p16_p48, &x6, &x7, _r, cos_bit);
    btf_16_w16_avx2(cospi_m48_p16, cospi_p16_p48, &x14, &x15, _r, cos_bit);
  }

  // Step 3: Load {x2, x3, x10, x11} and run Stage 2 (cospi[32])
  __m256i x2 = _mm256_subs_epi16(__zero, LOAD_SLL2(7));
  __m256i x3 = LOAD_SLL2(8);
  __m256i x10 = LOAD_SLL2(6);
  __m256i x11 = _mm256_subs_epi16(__zero, LOAD_SLL2(9));
  {
    const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
    const __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x2, &x3, _r, cos_bit);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x10, &x11, _r, cos_bit);
  }

  // Step 4: Load {x0, x8, x1, x9} and run Stage 3 + Stage 5
  __m256i x0 = LOAD_SLL2(0);
  __m256i x8 = _mm256_subs_epi16(__zero, LOAD_SLL2(1));
  btf_16_adds_subs_avx2(&x0, &x2);
  btf_16_adds_subs_avx2(&x8, &x10);
  btf_16_adds_subs_avx2(&x0, &x4);
  btf_16_adds_subs_avx2(&x2, &x6);
  btf_16_adds_subs_avx2(&x8, &x12);
  btf_16_adds_subs_avx2(&x10, &x14);

  __m256i x1 = _mm256_subs_epi16(__zero, LOAD_SLL2(15));
  __m256i x9 = LOAD_SLL2(14);
#undef LOAD_SLL2
  btf_16_adds_subs_avx2(&x1, &x3);
  btf_16_adds_subs_avx2(&x9, &x11);
  btf_16_adds_subs_avx2(&x1, &x5);
  btf_16_adds_subs_avx2(&x3, &x7);
  btf_16_adds_subs_avx2(&x9, &x13);
  btf_16_adds_subs_avx2(&x11, &x15);

  // Quad D ({x6, x7, x14, x15}) & Quad C ({x2, x3, x10, x11}): Stages 6..9
  {
    const __m256i cospi_p40_p24 = pair_set_w16_epi16(cospi[40], cospi[24]);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi[24], cospi[40]), cospi_p40_p24,
                    &x14, &x15, _r, cos_bit);
    btf_16_w16_avx2(cospi_p40_p24, pair_set_w16_epi16(cospi[24], -cospi[40]),
                    &x10, &x11, _r, cos_bit);
  }
  btf_16_adds_subs_avx2(&x6, &x14);
  btf_16_adds_subs_avx2(&x7, &x15);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[26], cospi[38]),
                  pair_set_w16_epi16(cospi[38], -cospi[26]), &x6, &x7, _r,
                  cos_bit);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[58], cospi[6]),
                  pair_set_w16_epi16(cospi[6], -cospi[58]), &x14, &x15, _r,
                  cos_bit);
  output[6] = x7;
  output[9] = x6;
  output[1] = x14;
  output[14] = x15;

  btf_16_adds_subs_avx2(&x2, &x10);
  btf_16_adds_subs_avx2(&x3, &x11);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[10], cospi[54]),
                  pair_set_w16_epi16(cospi[54], -cospi[10]), &x2, &x3, _r,
                  cos_bit);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[42], cospi[22]),
                  pair_set_w16_epi16(cospi[22], -cospi[42]), &x10, &x11, _r,
                  cos_bit);
  output[2] = x3;
  output[13] = x2;
  output[5] = x10;
  output[10] = x11;

  // Quad B ({x4, x5, x12, x13}) & Quad A ({x0, x1, x8, x9}): Stages 6..9
  {
    const __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi[56], cospi[8]), cospi_p08_p56,
                    &x12, &x13, _r, cos_bit);
    btf_16_w16_avx2(cospi_p08_p56, pair_set_w16_epi16(cospi[56], -cospi[8]),
                    &x8, &x9, _r, cos_bit);
  }
  btf_16_adds_subs_avx2(&x4, &x12);
  btf_16_adds_subs_avx2(&x5, &x13);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[18], cospi[46]),
                  pair_set_w16_epi16(cospi[46], -cospi[18]), &x4, &x5, _r,
                  cos_bit);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[50], cospi[14]),
                  pair_set_w16_epi16(cospi[14], -cospi[50]), &x12, &x13, _r,
                  cos_bit);
  output[4] = x5;
  output[11] = x4;
  output[3] = x12;
  output[12] = x13;

  btf_16_adds_subs_avx2(&x0, &x8);
  btf_16_adds_subs_avx2(&x1, &x9);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[2], cospi[62]),
                  pair_set_w16_epi16(cospi[62], -cospi[2]), &x0, &x1, _r,
                  cos_bit);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[34], cospi[30]),
                  pair_set_w16_epi16(cospi[30], -cospi[34]), &x8, &x9, _r,
                  cos_bit);
  output[0] = x1;
  output[15] = x0;
  output[7] = x8;
  output[8] = x9;
}

static void fadst16x16_row_cb12_avx2(const __m256i *input, int32_t *output) {
  const int8_t cos_bit = 12;
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
  const __m256i __zero = _mm256_setzero_si256();

  // Step 1: Load {x6, x7, x14, x15} and run Stage 2 (cospi[32])
  __m256i x6 = input[4];
  __m256i x7 = _mm256_subs_epi16(__zero, input[11]);
  __m256i x14 = _mm256_subs_epi16(__zero, input[5]);
  __m256i x15 = input[10];
  {
    const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
    const __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x6, &x7, _r, cos_bit);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x14, &x15, _r, cos_bit);
  }

  // Step 2: Load {x4, x5, x12, x13} and run Stage 3 + Stage 4 (cospi[16, 48])
  __m256i x4 = _mm256_subs_epi16(__zero, input[3]);
  __m256i x5 = input[12];
  __m256i x12 = input[2];
  __m256i x13 = _mm256_subs_epi16(__zero, input[13]);
  btf_16_adds_subs_avx2(&x4, &x6);
  btf_16_adds_subs_avx2(&x5, &x7);
  btf_16_adds_subs_avx2(&x12, &x14);
  btf_16_adds_subs_avx2(&x13, &x15);
  {
    const __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
    const __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
    const __m256i cospi_m48_p16 = pair_set_w16_epi16(-cospi[48], cospi[16]);
    btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, &x4, &x5, _r, cos_bit);
    btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, &x12, &x13, _r, cos_bit);
    btf_16_w16_avx2(cospi_m48_p16, cospi_p16_p48, &x6, &x7, _r, cos_bit);
    btf_16_w16_avx2(cospi_m48_p16, cospi_p16_p48, &x14, &x15, _r, cos_bit);
  }

  // Step 3: Load {x2, x3, x10, x11} and run Stage 2 (cospi[32])
  __m256i x2 = _mm256_subs_epi16(__zero, input[7]);
  __m256i x3 = input[8];
  __m256i x10 = input[6];
  __m256i x11 = _mm256_subs_epi16(__zero, input[9]);
  {
    const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
    const __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x2, &x3, _r, cos_bit);
    btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x10, &x11, _r, cos_bit);
  }

  // Step 4: Load {x0, x8, x1, x9} and run Stage 3 + Stage 5
  __m256i x0 = input[0];
  __m256i x8 = _mm256_subs_epi16(__zero, input[1]);
  btf_16_adds_subs_avx2(&x0, &x2);
  btf_16_adds_subs_avx2(&x8, &x10);
  btf_16_adds_subs_avx2(&x0, &x4);
  btf_16_adds_subs_avx2(&x2, &x6);
  btf_16_adds_subs_avx2(&x8, &x12);
  btf_16_adds_subs_avx2(&x10, &x14);

  __m256i x1 = _mm256_subs_epi16(__zero, input[15]);
  __m256i x9 = input[14];
  btf_16_adds_subs_avx2(&x1, &x3);
  btf_16_adds_subs_avx2(&x9, &x11);
  btf_16_adds_subs_avx2(&x1, &x5);
  btf_16_adds_subs_avx2(&x3, &x7);
  btf_16_adds_subs_avx2(&x9, &x13);
  btf_16_adds_subs_avx2(&x11, &x15);

  // Quad D ({x6, x7, x14, x15}) & Quad C ({x2, x3, x10, x11}): Stages 6..9
  {
    const __m256i cospi_p40_p24 = pair_set_w16_epi16(cospi[40], cospi[24]);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi[24], cospi[40]), cospi_p40_p24,
                    &x14, &x15, _r, cos_bit);
    btf_16_w16_avx2(cospi_p40_p24, pair_set_w16_epi16(cospi[24], -cospi[40]),
                    &x10, &x11, _r, cos_bit);
  }
  btf_16_adds_subs_avx2(&x6, &x14);
  btf_16_adds_subs_avx2(&x7, &x15);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[26], cospi[38]),
                  pair_set_w16_epi16(cospi[38], -cospi[26]), &x6, &x7, _r,
                  cos_bit);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[58], cospi[6]),
                  pair_set_w16_epi16(cospi[6], -cospi[58]), &x14, &x15, _r,
                  cos_bit);
  store_row_16bit_to_32bit_w16_avx2(x7, output + 6 * 16);
  store_row_16bit_to_32bit_w16_avx2(x6, output + 9 * 16);
  store_row_16bit_to_32bit_w16_avx2(x14, output + 1 * 16);
  store_row_16bit_to_32bit_w16_avx2(x15, output + 14 * 16);

  btf_16_adds_subs_avx2(&x2, &x10);
  btf_16_adds_subs_avx2(&x3, &x11);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[10], cospi[54]),
                  pair_set_w16_epi16(cospi[54], -cospi[10]), &x2, &x3, _r,
                  cos_bit);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[42], cospi[22]),
                  pair_set_w16_epi16(cospi[22], -cospi[42]), &x10, &x11, _r,
                  cos_bit);
  store_row_16bit_to_32bit_w16_avx2(x3, output + 2 * 16);
  store_row_16bit_to_32bit_w16_avx2(x2, output + 13 * 16);
  store_row_16bit_to_32bit_w16_avx2(x10, output + 5 * 16);
  store_row_16bit_to_32bit_w16_avx2(x11, output + 10 * 16);

  // Quad B ({x4, x5, x12, x13}) & Quad A ({x0, x1, x8, x9}): Stages 6..9
  {
    const __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi[56], cospi[8]), cospi_p08_p56,
                    &x12, &x13, _r, cos_bit);
    btf_16_w16_avx2(cospi_p08_p56, pair_set_w16_epi16(cospi[56], -cospi[8]),
                    &x8, &x9, _r, cos_bit);
  }
  btf_16_adds_subs_avx2(&x4, &x12);
  btf_16_adds_subs_avx2(&x5, &x13);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[18], cospi[46]),
                  pair_set_w16_epi16(cospi[46], -cospi[18]), &x4, &x5, _r,
                  cos_bit);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[50], cospi[14]),
                  pair_set_w16_epi16(cospi[14], -cospi[50]), &x12, &x13, _r,
                  cos_bit);
  store_row_16bit_to_32bit_w16_avx2(x5, output + 4 * 16);
  store_row_16bit_to_32bit_w16_avx2(x4, output + 11 * 16);
  store_row_16bit_to_32bit_w16_avx2(x12, output + 3 * 16);
  store_row_16bit_to_32bit_w16_avx2(x13, output + 12 * 16);

  btf_16_adds_subs_avx2(&x0, &x8);
  btf_16_adds_subs_avx2(&x1, &x9);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[2], cospi[62]),
                  pair_set_w16_epi16(cospi[62], -cospi[2]), &x0, &x1, _r,
                  cos_bit);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[34], cospi[30]),
                  pair_set_w16_epi16(cospi[30], -cospi[34]), &x8, &x9, _r,
                  cos_bit);
  store_row_16bit_to_32bit_w16_avx2(x1, output + 0 * 16);
  store_row_16bit_to_32bit_w16_avx2(x0, output + 15 * 16);
  store_row_16bit_to_32bit_w16_avx2(x8, output + 7 * 16);
  store_row_16bit_to_32bit_w16_avx2(x9, output + 8 * 16);
}

static void fidentity16x16_col_cb13_avx2(const int16_t *in, __m256i *output,
                                         int stride) {
  const __m256i scale = _mm256_set1_epi16(2 * NewSqrt2 * 8 - 65536);
  for (int i = 0; i < 16; ++i) {
    const __m256i v =
        _mm256_slli_epi16(load_16bit_to_16bit_avx2(in + i * stride), 2);
    const __m256i t = _mm256_mulhrs_epi16(v, scale);
    output[i] = _mm256_adds_epi16(_mm256_adds_epi16(v, v), t);
  }
}

static void fidentity16x16_row_cb12_avx2(const __m256i *input,
                                         int32_t *output) {
  const __m256i scale = _mm256_set1_epi16(2 * NewSqrt2 * 8 - 65536);
  for (int i = 0; i < 16; ++i) {
    const __m256i v = input[i];
    const __m256i t = _mm256_mulhrs_epi16(v, scale);
    const __m256i res = _mm256_adds_epi16(_mm256_adds_epi16(v, v), t);
    store_row_16bit_to_32bit_w16_avx2(res, output + i * 16);
  }
}

static inline void fidentity16x16_new_avx2(const __m256i *input,
                                           __m256i *output, int8_t cos_bit) {
  (void)cos_bit;
  const __m256i one = _mm256_set1_epi16(1);

  for (int i = 0; i < 16; ++i) {
    const __m256i a_lo = _mm256_unpacklo_epi16(input[i], one);
    const __m256i a_hi = _mm256_unpackhi_epi16(input[i], one);
    const __m256i b_lo = scale_round_avx2(a_lo, 2 * NewSqrt2);
    const __m256i b_hi = scale_round_avx2(a_hi, 2 * NewSqrt2);
    output[i] = _mm256_packs_epi32(b_lo, b_hi);
  }
}

static inline void fidentity16x32_avx2(const __m256i *input, __m256i *output,
                                       int8_t cos_bit) {
  (void)cos_bit;
  for (int i = 0; i < 32; ++i) {
    output[i] = _mm256_slli_epi16(input[i], 2);
  }
}

static inline void store_output_32bit_w16(int32_t *const out,
                                          const __m256i *const in1,
                                          const __m256i *const in2,
                                          const int stride,
                                          const int out_size) {
  for (int i = 0; i < out_size; ++i) {
    _mm256_store_si256((__m256i *)(out + stride * i), in1[i]);
    _mm256_store_si256((__m256i *)(out + stride * i + 8), in2[i]);
  }
}

// Store 8 16 bit values. Sign extend the values.
static inline void store_buffer_16bit_to_32bit_w16_avx2(const __m256i *const in,
                                                        int32_t *out,
                                                        const int stride,
                                                        const int out_size) {
  for (int i = 0; i < out_size; ++i) {
    _mm256_store_si256((__m256i *)(out),
                       _mm256_cvtepi16_epi32(_mm256_castsi256_si128(in[i])));
    _mm256_store_si256(
        (__m256i *)(out + 8),
        _mm256_cvtepi16_epi32(_mm256_extracti128_si256(in[i], 1)));
    out += stride;
  }
}

static inline void store_rect_16bit_to_32bit_avx2(const __m256i a,
                                                  int32_t *const b) {
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i a_reoder = _mm256_permute4x64_epi64(a, 0xd8);
  const __m256i a_lo = _mm256_unpacklo_epi16(a_reoder, one);
  const __m256i a_hi = _mm256_unpackhi_epi16(a_reoder, one);
  const __m256i b_lo = scale_round_avx2(a_lo, NewSqrt2);
  const __m256i b_hi = scale_round_avx2(a_hi, NewSqrt2);
  _mm256_store_si256((__m256i *)b, b_lo);
  _mm256_store_si256((__m256i *)(b + 8), b_hi);
}

static inline void store_rect_buffer_16bit_to_32bit_w16_avx2(
    const __m256i *const in, int32_t *const out, const int stride,
    const int out_size) {
  for (int i = 0; i < out_size; ++i) {
    store_rect_16bit_to_32bit_avx2(in[i], out + i * stride);
  }
}

typedef void (*transform_1d_avx2)(const __m256i *input, __m256i *output,
                                  int8_t cos_bit);

static const transform_1d_avx2 col_txfm16x32_arr[TX_TYPES] = {
  fdct16x32_avx2,       // DCT_DCT
  NULL,                 // ADST_DCT
  NULL,                 // DCT_ADST
  NULL,                 // ADST_ADST
  NULL,                 // FLIPADST_DCT
  NULL,                 // DCT_FLIPADST
  NULL,                 // FLIPADST_FLIPADST
  NULL,                 // ADST_FLIPADST
  NULL,                 // FLIPADST_ADST
  fidentity16x32_avx2,  // IDTX
  fdct16x32_avx2,       // V_DCT
  fidentity16x32_avx2,  // H_DCT
  NULL,                 // V_ADST
  NULL,                 // H_ADST
  NULL,                 // V_FLIPADST
  NULL                  // H_FLIPADST
};

static const transform_1d_avx2 row_txfm16x32_arr[TX_TYPES] = {
  fdct16x32_avx2,       // DCT_DCT
  NULL,                 // ADST_DCT
  NULL,                 // DCT_ADST
  NULL,                 // ADST_ADST
  NULL,                 // FLIPADST_DCT
  NULL,                 // DCT_FLIPADST
  NULL,                 // FLIPADST_FLIPADST
  NULL,                 // ADST_FLIPADST
  NULL,                 // FLIPADST_ADST
  fidentity16x32_avx2,  // IDTX
  fidentity16x32_avx2,  // V_DCT
  fdct16x32_avx2,       // H_DCT
  NULL,                 // V_ADST
  NULL,                 // H_ADST
  NULL,                 // V_FLIPADST
  NULL                  // H_FLIPADST
};

static const transform_1d_avx2 row_txfm16x32_cb13_arr[TX_TYPES] = {
  fdct16x32_cb13_avx2,  // DCT_DCT
  NULL,                 // ADST_DCT
  NULL,                 // DCT_ADST
  NULL,                 // ADST_ADST
  NULL,                 // FLIPADST_DCT
  NULL,                 // DCT_FLIPADST
  NULL,                 // FLIPADST_FLIPADST
  NULL,                 // ADST_FLIPADST
  NULL,                 // FLIPADST_ADST
  fidentity16x32_avx2,  // IDTX
  fidentity16x32_avx2,  // V_DCT
  fdct16x32_cb13_avx2,  // H_DCT
  NULL,                 // V_ADST
  NULL,                 // H_ADST
  NULL,                 // V_FLIPADST
  NULL                  // H_FLIPADST
};

typedef void (*col_txfm16x16_fn)(const int16_t *in, __m256i *out, int stride);
typedef void (*row_txfm16x16_fn)(const __m256i *in, int32_t *out);

static const col_txfm16x16_fn col_txfm16x16_arr[TX_TYPES] = {
  fdct16x16_col_cb13_avx2,       // DCT_DCT
  fadst16x16_col_cb13_avx2,      // ADST_DCT
  fdct16x16_col_cb13_avx2,       // DCT_ADST
  fadst16x16_col_cb13_avx2,      // ADST_ADST
  fadst16x16_col_cb13_avx2,      // FLIPADST_DCT
  fdct16x16_col_cb13_avx2,       // DCT_FLIPADST
  fadst16x16_col_cb13_avx2,      // FLIPADST_FLIPADST
  fadst16x16_col_cb13_avx2,      // ADST_FLIPADST
  fadst16x16_col_cb13_avx2,      // FLIPADST_ADST
  fidentity16x16_col_cb13_avx2,  // IDTX
  fdct16x16_col_cb13_avx2,       // V_DCT
  fidentity16x16_col_cb13_avx2,  // H_DCT
  fadst16x16_col_cb13_avx2,      // V_ADST
  fidentity16x16_col_cb13_avx2,  // H_ADST
  fadst16x16_col_cb13_avx2,      // V_FLIPADST
  fidentity16x16_col_cb13_avx2   // H_FLIPADST
};

static const row_txfm16x16_fn row_txfm16x16_cb12_arr[TX_TYPES] = {
  fdct16x16_row_cb12_avx2,       // DCT_DCT
  fdct16x16_row_cb12_avx2,       // ADST_DCT
  fadst16x16_row_cb12_avx2,      // DCT_ADST
  fadst16x16_row_cb12_avx2,      // ADST_ADST
  fdct16x16_row_cb12_avx2,       // FLIPADST_DCT
  fadst16x16_row_cb12_avx2,      // DCT_FLIPADST
  fadst16x16_row_cb12_avx2,      // FLIPADST_FLIPADST
  fadst16x16_row_cb12_avx2,      // ADST_FLIPADST
  fadst16x16_row_cb12_avx2,      // FLIPADST_ADST
  fidentity16x16_row_cb12_avx2,  // IDTX
  fidentity16x16_row_cb12_avx2,  // V_DCT
  fdct16x16_row_cb12_avx2,       // H_DCT
  fidentity16x16_row_cb12_avx2,  // V_ADST
  fadst16x16_row_cb12_avx2,      // H_ADST
  fidentity16x16_row_cb12_avx2,  // V_FLIPADST
  fadst16x16_row_cb12_avx2       // H_FLIPADST
};

static const transform_1d_avx2 row_txfm16x16_cb13_arr[TX_TYPES] = {
  fdct16x16_cb13_avx2,      // DCT_DCT
  fdct16x16_cb13_avx2,      // ADST_DCT
  fadst16x16_cb13_avx2,     // DCT_ADST
  fadst16x16_cb13_avx2,     // ADST_ADST
  fdct16x16_cb13_avx2,      // FLIPADST_DCT
  fadst16x16_cb13_avx2,     // DCT_FLIPADST
  fadst16x16_cb13_avx2,     // FLIPADST_FLIPADST
  fadst16x16_cb13_avx2,     // ADST_FLIPADST
  fadst16x16_cb13_avx2,     // FLIPADST_ADST
  fidentity16x16_new_avx2,  // IDTX
  fidentity16x16_new_avx2,  // V_DCT
  fdct16x16_cb13_avx2,      // H_DCT
  fidentity16x16_new_avx2,  // V_ADST
  fadst16x16_cb13_avx2,     // H_ADST
  fidentity16x16_new_avx2,  // V_FLIPADST
  fadst16x16_cb13_avx2      // H_FLIPADST
};

#define TRANSPOSE_8X8_AVX2()                                         \
  {                                                                  \
    /* aa0:    00 10 01 11  02 12 03 13 | 40 50 41 51  42 52 43 53*/ \
    /* aa1:    04 14 05 15  06 16 07 17 | 44 54 45 55  46 56 47 57*/ \
    /* aa2:    20 30 21 31  22 32 23 33 | 60 70 61 71  62 72 63 73*/ \
    /* aa3:    24 34 25 35  26 36 27 37 | 64 74 65 75  66 76 67 77*/ \
    const __m256i aa0 = _mm256_unpacklo_epi16(b0, b1);               \
    const __m256i aa1 = _mm256_unpackhi_epi16(b0, b1);               \
    const __m256i aa2 = _mm256_unpacklo_epi16(b2, b3);               \
    const __m256i aa3 = _mm256_unpackhi_epi16(b2, b3);               \
    /* Unpack 32 bit elements resulting in: */                       \
    /* bb0: 00 10 20 30  01 11 21 31 | 40 50 60 70  41 51 61 71*/    \
    /* bb1: 02 12 22 32  03 13 23 33 | 42 52 62 72  43 53 63 73*/    \
    /* bb2: 04 14 24 34  05 15 25 35 | 44 54 64 74  45 55 65 75*/    \
    /* bb2: 06 16 26 36  07 17 27 37 | 46 56 66 76  47 57 67 77*/    \
    const __m256i bb0 = _mm256_unpacklo_epi32(aa0, aa2);             \
    const __m256i bb1 = _mm256_unpackhi_epi32(aa0, aa2);             \
    const __m256i bb2 = _mm256_unpacklo_epi32(aa1, aa3);             \
    const __m256i bb3 = _mm256_unpackhi_epi32(aa1, aa3);             \
    /* bb0: 00 10 20 30 40 50 60 70| 01 11 21 31 41 51 61 71*/       \
    /* bb1: 02 12 22 32 42 52 62 72| 03 13 23 33 43 53 63 73*/       \
    /* bb2: 04 14 24 34 44 54 64 74| 05 15 25 35 45 55 65 75*/       \
    /* bb2: 06 16 26 36 46 56 66 76| 07 17 27 37 47 57 67 77*/       \
    c0 = _mm256_permute4x64_epi64(bb0, 0xd8);                        \
    c1 = _mm256_permute4x64_epi64(bb1, 0xd8);                        \
    c2 = _mm256_permute4x64_epi64(bb2, 0xd8);                        \
    c3 = _mm256_permute4x64_epi64(bb3, 0xd8);                        \
  }

// The 8x8 transforms below keep two 8-sample rows in each __m256i. The four
// vectors of a block are kept in the "mirrored" order
//   p01 = [r0 | r1], p32 = [r3 | r2], p45 = [r4 | r5], p76 = [r7 | r6]
// so that the first butterfly stage of the DCT pairs whole registers.
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

static AOM_FORCE_INLINE void load_8x8_dual_sll_avx2(const int16_t *in,
                                                    int stride, int bit,
                                                    __m256i *p) {
#define LOAD_ROW_PAIR(a, b)                                           \
  _mm256_slli_epi16(                                                  \
      _mm256_inserti128_si256(                                        \
          _mm256_castsi128_si256(                                     \
              _mm_loadu_si128((const __m128i *)(in + (a) * stride))), \
          _mm_loadu_si128((const __m128i *)(in + (b) * stride)), 1),  \
      bit)
  p[0] = LOAD_ROW_PAIR(0, 1);
  p[1] = LOAD_ROW_PAIR(3, 2);
  p[2] = LOAD_ROW_PAIR(4, 5);
  p[3] = LOAD_ROW_PAIR(7, 6);
#undef LOAD_ROW_PAIR
}

// fdct8 on mirrored input p[0..3]. Outputs (o = output row):
// q[0] = [o0 | o2], q[1] = [o4 | o6], q[2] = [o1 | o5], q[3] = [o7 | o3].
static AOM_FORCE_INLINE void fdct8x8_dual_avx2(const __m256i *p, __m256i *q) {
  const int32_t *cospi = cospi_arr(13);
  const __m256i r = _mm256_set1_epi32(1 << 12);
  const __m256i w_s2 =
      pair_set_w16x2_epi16(-cospi[32], cospi[32], cospi[32], cospi[32]);
  const __m256i w0_s3 =
      pair_set_w16x2_epi16(cospi[32], cospi[32], cospi[48], cospi[16]);
  const __m256i w1_s3 =
      pair_set_w16x2_epi16(cospi[32], -cospi[32], -cospi[16], cospi[48]);
  const __m256i w0_s4 =
      pair_set_w16x2_epi16(cospi[56], cospi[8], cospi[24], cospi[40]);
  const __m256i w1_s4 =
      pair_set_w16x2_epi16(-cospi[8], cospi[56], -cospi[40], cospi[24]);

  // stage 1: s01 = [x1_0 | x1_1], d76 = [x1_7 | x1_6], s32 = [x1_3 | x1_2],
  // d45 = [x1_4 | x1_5].
  const __m256i s01 = _mm256_adds_epi16(p[0], p[3]);
  const __m256i d76 = _mm256_subs_epi16(p[0], p[3]);
  const __m256i s32 = _mm256_adds_epi16(p[1], p[2]);
  const __m256i d45 = _mm256_subs_epi16(p[1], p[2]);

  // stage 2: e01 = [x2_0 | x2_1], e32 = [x2_3 | x2_2], x56 = [x2_5 | x2_6].
  const __m256i e01 = _mm256_adds_epi16(s01, s32);
  const __m256i e32 = _mm256_subs_epi16(s01, s32);
  const __m256i x56 =
      btf_16_w16_out0_avx2(w_s2, _mm256_permute2x128_si256(d45, d45, 0x11),
                           _mm256_permute2x128_si256(d76, d76, 0x11), r, 13);

  // stage 3
  __m256i in0 = _mm256_blend_epi32(e01, e32, 0xF0);         // [x2_0 | x2_2]
  __m256i in1 = _mm256_permute2x128_si256(e01, e32, 0x21);  // [x2_1 | x2_3]
  btf_16_w16_avx2(w0_s3, w1_s3, &in0, &in1, r, 13);
  q[0] = in0;
  q[1] = in1;
  const __m256i d47 = _mm256_permute2x128_si256(d45, d76, 0x20);
  const __m256i sum = _mm256_adds_epi16(d47, x56);  // [x3_4 | x3_7]
  const __m256i dif = _mm256_subs_epi16(d47, x56);  // [x3_5 | x3_6]

  // stage 4 and 5
  __m256i in2 = _mm256_permute2x128_si256(sum, dif, 0x20);  // [x3_4 | x3_5]
  __m256i in3 = _mm256_permute2x128_si256(sum, dif, 0x31);  // [x3_7 | x3_6]
  btf_16_w16_avx2(w0_s4, w1_s4, &in2, &in3, r, 13);
  q[2] = in2;
  q[3] = in3;
}

// fadst8 on mirrored input p[0..3]. Outputs (o = output row):
// q[0] = [o0 | o2], q[1] = [o4 | o6], q[2] = [o3 | o1], q[3] = [o7 | o5].
static AOM_FORCE_INLINE void fadst8x8_dual_avx2(const __m256i *p, __m256i *q) {
  const int32_t *cospi = cospi_arr(13);
  const __m256i r = _mm256_set1_epi32(1 << 12);
  const __m256i zero = _mm256_setzero_si256();
  const __m256i w0_s2 = pair_set_w16_epi16(cospi[32], cospi[32]);
  const __m256i w1_s2 = pair_set_w16_epi16(cospi[32], -cospi[32]);
  const __m256i w0_s4 =
      pair_set_w16x2_epi16(cospi[16], cospi[48], -cospi[48], cospi[16]);
  const __m256i w1_s4 =
      pair_set_w16x2_epi16(cospi[48], -cospi[16], cospi[16], cospi[48]);
  const __m256i w0_s7a =
      pair_set_w16x2_epi16(cospi[4], cospi[60], cospi[20], cospi[44]);
  const __m256i w1_s7a =
      pair_set_w16x2_epi16(cospi[60], -cospi[4], cospi[44], -cospi[20]);
  const __m256i w0_s7b =
      pair_set_w16x2_epi16(cospi[36], cospi[28], cospi[52], cospi[12]);
  const __m256i w1_s7b =
      pair_set_w16x2_epi16(cospi[28], -cospi[36], cospi[12], -cospi[52]);

  // stage 1
  const __m256i a04 =
      _mm256_blend_epi32(p[0], _mm256_subs_epi16(zero, p[0]), 0xF0);
  __m256i a26 = _mm256_blend_epi32(_mm256_subs_epi16(zero, p[1]), p[1], 0xF0);
  __m256i a37 = _mm256_blend_epi32(p[2], _mm256_subs_epi16(zero, p[2]), 0xF0);
  const __m256i a15 =
      _mm256_blend_epi32(_mm256_subs_epi16(zero, p[3]), p[3], 0xF0);

  // stage 2: a26 = [x2_2 | x2_6], a37 = [x2_3 | x2_7]
  btf_16_w16_avx2(w0_s2, w1_s2, &a26, &a37, r, 13);

  // stage 3
  const __m256i c04 = _mm256_adds_epi16(a04, a26);
  const __m256i c26 = _mm256_subs_epi16(a04, a26);
  const __m256i c15 = _mm256_adds_epi16(a15, a37);
  const __m256i c37 = _mm256_subs_epi16(a15, a37);

  // stage 4
  __m256i d46 = _mm256_permute2x128_si256(c04, c26, 0x31);
  __m256i d57 = _mm256_permute2x128_si256(c15, c37, 0x31);
  btf_16_w16_avx2(w0_s4, w1_s4, &d46, &d57, r, 13);
  const __m256i d02 = _mm256_permute2x128_si256(c04, c26, 0x20);
  const __m256i d13 = _mm256_permute2x128_si256(c15, c37, 0x20);

  // stage 5, 6 and 7
  __m256i e75 = _mm256_adds_epi16(d02, d46);
  __m256i e31 = _mm256_subs_epi16(d02, d46);
  __m256i e02 = _mm256_adds_epi16(d13, d57);
  __m256i e46 = _mm256_subs_epi16(d13, d57);
  btf_16_w16_avx2(w0_s7a, w1_s7a, &e75, &e02, r, 13);
  btf_16_w16_avx2(w0_s7b, w1_s7b, &e31, &e46, r, 13);
  q[0] = e02;
  q[1] = e46;
  q[2] = e31;
  q[3] = e75;
}

// Column pass for the given 1-D type. Outputs b[i] = [o_i | o_(i+4)].
static AOM_FORCE_INLINE void fwd_txfm8x8_col_dual_avx2(int type,
                                                       const __m256i *p,
                                                       __m256i *b) {
  __m256i q[4];
  switch (type) {
    case DCT_1D:
      fdct8x8_dual_avx2(p, q);
      b[0] = _mm256_permute2x128_si256(q[0], q[1], 0x20);
      b[1] = q[2];
      b[2] = _mm256_permute2x128_si256(q[0], q[1], 0x31);
      b[3] = _mm256_permute4x64_epi64(q[3], 0x4E);
      break;
    case ADST_1D:
    case FLIPADST_1D:
      fadst8x8_dual_avx2(p, q);
      b[0] = _mm256_permute2x128_si256(q[0], q[1], 0x20);
      b[1] = _mm256_permute2x128_si256(q[2], q[3], 0x31);
      b[2] = _mm256_permute2x128_si256(q[0], q[1], 0x31);
      b[3] = _mm256_permute2x128_si256(q[2], q[3], 0x20);
      break;
    default:
      q[0] = _mm256_adds_epi16(p[0], p[0]);
      q[1] = _mm256_adds_epi16(p[1], p[1]);
      q[2] = _mm256_adds_epi16(p[2], p[2]);
      q[3] = _mm256_adds_epi16(p[3], p[3]);
      b[0] = _mm256_permute2x128_si256(q[0], q[2], 0x20);
      b[1] = _mm256_permute2x128_si256(q[0], q[2], 0x31);
      b[2] = _mm256_permute2x128_si256(q[1], q[3], 0x31);
      b[3] = _mm256_permute2x128_si256(q[1], q[3], 0x20);
      break;
  }
}

// Rounds/shifts b[i] = [row_i | row_(i+4)] and transposes the block into the
// mirrored order p[0..3] = [t0 | t1], [t3 | t2], [t4 | t5], [t7 | t6].
static AOM_FORCE_INLINE void round_shift_transpose_8x8_dual_avx2(
    const __m256i *b, int bit, __m256i *p) {
  const __m256i rounding = _mm256_set1_epi16(1 << (bit - 1));
  const __m256i b0 = _mm256_srai_epi16(_mm256_adds_epi16(b[0], rounding), bit);
  const __m256i b1 = _mm256_srai_epi16(_mm256_adds_epi16(b[1], rounding), bit);
  const __m256i b2 = _mm256_srai_epi16(_mm256_adds_epi16(b[2], rounding), bit);
  const __m256i b3 = _mm256_srai_epi16(_mm256_adds_epi16(b[3], rounding), bit);
  const __m256i aa0 = _mm256_unpacklo_epi16(b0, b1);
  const __m256i aa1 = _mm256_unpackhi_epi16(b0, b1);
  const __m256i aa2 = _mm256_unpacklo_epi16(b2, b3);
  const __m256i aa3 = _mm256_unpackhi_epi16(b2, b3);
  const __m256i bb0 = _mm256_unpacklo_epi32(aa0, aa2);
  const __m256i bb1 = _mm256_unpackhi_epi32(aa0, aa2);
  const __m256i bb2 = _mm256_unpacklo_epi32(aa1, aa3);
  const __m256i bb3 = _mm256_unpackhi_epi32(aa1, aa3);
  p[0] = _mm256_permute4x64_epi64(bb0, 0xd8);
  p[1] = _mm256_permute4x64_epi64(bb1, 0x8d);
  p[2] = _mm256_permute4x64_epi64(bb2, 0xd8);
  p[3] = _mm256_permute4x64_epi64(bb3, 0x8d);
}

static AOM_FORCE_INLINE void store_row_pair_16bit_to_32bit_avx2(int32_t *out,
                                                                const __m256i v,
                                                                int row_lo,
                                                                int row_hi) {
  _mm256_storeu_si256((__m256i *)(out + 8 * row_lo),
                      _mm256_cvtepi16_epi32(_mm256_castsi256_si128(v)));
  _mm256_storeu_si256((__m256i *)(out + 8 * row_hi),
                      _mm256_cvtepi16_epi32(_mm256_extracti128_si256(v, 1)));
}

// Row pass for the given 1-D type on mirrored input p[0..3], storing the
// 8x8 result as 32-bit coefficients.
static AOM_FORCE_INLINE void fwd_txfm8x8_row_dual_store_avx2(int type,
                                                             const __m256i *p,
                                                             int32_t *output) {
  __m256i q[4];
  switch (type) {
    case DCT_1D:
      fdct8x8_dual_avx2(p, q);
      store_row_pair_16bit_to_32bit_avx2(output, q[0], 0, 2);
      store_row_pair_16bit_to_32bit_avx2(output, q[1], 4, 6);
      store_row_pair_16bit_to_32bit_avx2(output, q[2], 1, 5);
      store_row_pair_16bit_to_32bit_avx2(output, q[3], 7, 3);
      break;
    case ADST_1D:
    case FLIPADST_1D:
      fadst8x8_dual_avx2(p, q);
      store_row_pair_16bit_to_32bit_avx2(output, q[0], 0, 2);
      store_row_pair_16bit_to_32bit_avx2(output, q[1], 4, 6);
      store_row_pair_16bit_to_32bit_avx2(output, q[2], 3, 1);
      store_row_pair_16bit_to_32bit_avx2(output, q[3], 7, 5);
      break;
    default:
      store_row_pair_16bit_to_32bit_avx2(output, _mm256_adds_epi16(p[0], p[0]),
                                         0, 1);
      store_row_pair_16bit_to_32bit_avx2(output, _mm256_adds_epi16(p[1], p[1]),
                                         3, 2);
      store_row_pair_16bit_to_32bit_avx2(output, _mm256_adds_epi16(p[2], p[2]),
                                         4, 5);
      store_row_pair_16bit_to_32bit_avx2(output, _mm256_adds_epi16(p[3], p[3]),
                                         7, 6);
      break;
  }
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void lowbd_fwd_txfm2d_8x8_non_dct_avx2(const int16_t *input,
                                              int32_t *output, int stride,
                                              TX_TYPE tx_type, int bd) {
  (void)bd;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_8X8];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  assert(shift[0] > 0);
  const int16_t *in_ptr = ud_flip ? (input + 7 * stride) : input;
  const int in_stride = ud_flip ? -stride : stride;
  __m256i p[4], b[4];
  load_8x8_dual_sll_avx2(in_ptr, in_stride, shift[0], p);
  fwd_txfm8x8_col_dual_avx2(vtx_tab[tx_type], p, b);
  assert(shift[1] < 0);
  round_shift_transpose_8x8_dual_avx2(b, -shift[1], p);
  if (UNLIKELY(lr_flip)) {
    // In mirrored order a left-right flip is a register swap.
    const __m256i t0 = p[0];
    const __m256i t1 = p[1];
    p[0] = p[3];
    p[1] = p[2];
    p[2] = t1;
    p[3] = t0;
  }
  assert(shift[2] == 0);
  fwd_txfm8x8_row_dual_store_avx2(htx_tab[tx_type], p, output);
}

static void av1_lowbd_fwd_txfm2d_8x8_avx2(const int16_t *input, int32_t *output,
                                          int stride, TX_TYPE tx_type, int bd) {
  if (UNLIKELY(tx_type != DCT_DCT)) {
    lowbd_fwd_txfm2d_8x8_non_dct_avx2(input, output, stride, tx_type, bd);
    return;
  }
  (void)bd;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_8X8];
  __m256i p[4], b[4];
  load_8x8_dual_sll_avx2(input, stride, shift[0], p);
  fwd_txfm8x8_col_dual_avx2(DCT_1D, p, b);
  round_shift_transpose_8x8_dual_avx2(b, -shift[1], p);
  fwd_txfm8x8_row_dual_store_avx2(DCT_1D, p, output);
}

static AOM_FORCE_INLINE void load_buffer_16bit_to_16bit_sll_avx2(
    const int16_t *in, int stride, __m256i *out, int out_size, int bit) {
  for (int i = 0; i < out_size; ++i) {
    out[i] = _mm256_slli_epi16(load_16bit_to_16bit_avx2(in + i * stride), bit);
  }
}

static AOM_FORCE_INLINE void transpose_round_shift_16bit_16x16_avx2(
    const __m256i *const in, __m256i *const out, int rshift) {
  const __m256i round = _mm256_set1_epi16(1 << (rshift - 1));
  __m256i t[8];

#define LOADL_RS(idx)                                                      \
  do {                                                                     \
    __m256i v =                                                            \
        _mm256_castsi128_si256(_mm_load_si128((const __m128i *)&in[idx])); \
    v = _mm256_inserti128_si256(                                           \
        v, _mm_load_si128((const __m128i *)&in[idx + 8]), 1);              \
    t[idx] = _mm256_srai_epi16(_mm256_adds_epi16(v, round), rshift);       \
  } while (0)

#define LOADR_RS(idx)                                                          \
  do {                                                                         \
    __m256i v =                                                                \
        _mm256_castsi128_si256(_mm_load_si128((const __m128i *)&in[idx] + 1)); \
    v = _mm256_inserti128_si256(                                               \
        v, _mm_load_si128((const __m128i *)&in[idx + 8] + 1), 1);              \
    t[idx] = _mm256_srai_epi16(_mm256_adds_epi16(v, round), rshift);           \
  } while (0)

  LOADL_RS(0);
  LOADL_RS(1);
  LOADL_RS(2);
  LOADL_RS(3);
  LOADL_RS(4);
  LOADL_RS(5);
  LOADL_RS(6);
  LOADL_RS(7);
  transpose2_8x8_avx2(t, out);

  LOADR_RS(0);
  LOADR_RS(1);
  LOADR_RS(2);
  LOADR_RS(3);
  LOADR_RS(4);
  LOADR_RS(5);
  LOADR_RS(6);
  LOADR_RS(7);
  transpose2_8x8_avx2(t, out + 8);

#undef LOADL_RS
#undef LOADR_RS
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void lowbd_fwd_txfm2d_16x16_non_dct_avx2(const int16_t *input,
                                                int32_t *output, int stride,
                                                TX_TYPE tx_type, int bd) {
  (void)bd;
  __m256i buf0[16], buf1[16];
  const col_txfm16x16_fn col_txfm = col_txfm16x16_arr[tx_type];
  const row_txfm16x16_fn row_txfm = row_txfm16x16_cb12_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int16_t *in_ptr = ud_flip ? (input + 15 * stride) : input;
  const int in_stride = ud_flip ? -stride : stride;
  col_txfm(in_ptr, buf0, in_stride);
  transpose_round_shift_16bit_16x16_avx2(buf0, buf1, 2);

  __m256i *buf;
  if (UNLIKELY(lr_flip)) {
    buf = buf0;
    flip_buf_avx2(buf1, buf, 16);
  } else {
    buf = buf1;
  }
  row_txfm(buf, output);
}

static void lowbd_fwd_txfm2d_16x16_avx2(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  if (UNLIKELY(tx_type != DCT_DCT)) {
    lowbd_fwd_txfm2d_16x16_non_dct_avx2(input, output, stride, tx_type, bd);
    return;
  }
  (void)bd;
  __m256i buf0[16];
  __m256i buf_lo[8];
  __m256i x0_0, x0_1, x0_2, x0_3, x0_4, x0_5, x0_6, x0_7;
  __m256i x1[8];

  fdct16x16_load_sll_avx2(input, stride, buf0, 13, 2);

  {
    const __m256i round = _mm256_set1_epi16(2);
    __m256i t[8];
#define LOADL_RS2(idx)                                                       \
  do {                                                                       \
    __m256i v =                                                              \
        _mm256_castsi128_si256(_mm_load_si128((const __m128i *)&buf0[idx])); \
    v = _mm256_inserti128_si256(                                             \
        v, _mm_load_si128((const __m128i *)&buf0[(idx) + 8]), 1);            \
    t[idx] = _mm256_srai_epi16(_mm256_adds_epi16(v, round), 2);              \
  } while (0)
    LOADL_RS2(0);
    LOADL_RS2(1);
    LOADL_RS2(2);
    LOADL_RS2(3);
    LOADL_RS2(4);
    LOADL_RS2(5);
    LOADL_RS2(6);
    LOADL_RS2(7);
#undef LOADL_RS2
    transpose2_8x8_avx2(t, buf_lo);
  }

  {
    const __m256i round = _mm256_set1_epi16(2);
    __m256i r0, r1, r2, r3, r4, r5, r6, r7;
#define LOADR_RS2(idx, dst)                                           \
  do {                                                                \
    __m256i v = _mm256_castsi128_si256(                               \
        _mm_load_si128((const __m128i *)&buf0[idx] + 1));             \
    v = _mm256_inserti128_si256(                                      \
        v, _mm_load_si128((const __m128i *)&buf0[(idx) + 8] + 1), 1); \
    dst = _mm256_srai_epi16(_mm256_adds_epi16(v, round), 2);          \
  } while (0)
    LOADR_RS2(0, r0);
    LOADR_RS2(1, r1);
    LOADR_RS2(2, r2);
    LOADR_RS2(3, r3);
    LOADR_RS2(4, r4);
    LOADR_RS2(5, r5);
    LOADR_RS2(6, r6);
    LOADR_RS2(7, r7);
#undef LOADR_RS2

    const __m256i tr0_0 = _mm256_unpacklo_epi16(r0, r1);
    const __m256i tr0_1 = _mm256_unpackhi_epi16(r0, r1);
    const __m256i tr0_2 = _mm256_unpacklo_epi16(r2, r3);
    const __m256i tr0_3 = _mm256_unpackhi_epi16(r2, r3);
    const __m256i tr0_4 = _mm256_unpacklo_epi16(r4, r5);
    const __m256i tr0_5 = _mm256_unpackhi_epi16(r4, r5);
    const __m256i tr0_6 = _mm256_unpacklo_epi16(r6, r7);
    const __m256i tr0_7 = _mm256_unpackhi_epi16(r6, r7);

    const __m256i tr1_0 = _mm256_unpacklo_epi32(tr0_0, tr0_2);
    const __m256i tr1_1 = _mm256_unpackhi_epi32(tr0_0, tr0_2);
    const __m256i tr1_2 = _mm256_unpacklo_epi32(tr0_1, tr0_3);
    const __m256i tr1_3 = _mm256_unpackhi_epi32(tr0_1, tr0_3);
    const __m256i tr1_4 = _mm256_unpacklo_epi32(tr0_4, tr0_6);
    const __m256i tr1_5 = _mm256_unpackhi_epi32(tr0_4, tr0_6);
    const __m256i tr1_6 = _mm256_unpacklo_epi32(tr0_5, tr0_7);
    const __m256i tr1_7 = _mm256_unpackhi_epi32(tr0_5, tr0_7);

    const __m256i hi0 = _mm256_unpacklo_epi64(tr1_0, tr1_4);
    const __m256i hi1 = _mm256_unpackhi_epi64(tr1_0, tr1_4);
    const __m256i hi2 = _mm256_unpacklo_epi64(tr1_1, tr1_5);
    const __m256i hi3 = _mm256_unpackhi_epi64(tr1_1, tr1_5);
    const __m256i hi4 = _mm256_unpacklo_epi64(tr1_2, tr1_6);
    const __m256i hi5 = _mm256_unpackhi_epi64(tr1_2, tr1_6);
    const __m256i hi6 = _mm256_unpacklo_epi64(tr1_3, tr1_7);
    const __m256i hi7 = _mm256_unpackhi_epi64(tr1_3, tr1_7);

    const __m256i lo0 = buf_lo[0];
    x0_0 = _mm256_adds_epi16(lo0, hi7);
    x1[7] = _mm256_subs_epi16(lo0, hi7);
    const __m256i lo1 = buf_lo[1];
    x0_1 = _mm256_adds_epi16(lo1, hi6);
    x1[6] = _mm256_subs_epi16(lo1, hi6);
    const __m256i lo2 = buf_lo[2];
    x0_2 = _mm256_adds_epi16(lo2, hi5);
    x1[5] = _mm256_subs_epi16(lo2, hi5);
    const __m256i lo3 = buf_lo[3];
    x0_3 = _mm256_adds_epi16(lo3, hi4);
    x1[4] = _mm256_subs_epi16(lo3, hi4);
    const __m256i lo4 = buf_lo[4];
    x0_4 = _mm256_adds_epi16(lo4, hi3);
    x1[3] = _mm256_subs_epi16(lo4, hi3);
    const __m256i lo5 = buf_lo[5];
    x0_5 = _mm256_adds_epi16(lo5, hi2);
    x1[2] = _mm256_subs_epi16(lo5, hi2);
    const __m256i lo6 = buf_lo[6];
    x0_6 = _mm256_adds_epi16(lo6, hi1);
    x1[1] = _mm256_subs_epi16(lo6, hi1);
    const __m256i lo7 = buf_lo[7];
    x0_7 = _mm256_adds_epi16(lo7, hi0);
    x1[0] = _mm256_subs_epi16(lo7, hi0);
  }

  const int32_t *cospi12 = cospi_arr(12);

  // Row pass Even Half (cos_bit = 12) + store
  {
    const __m256i _r = _mm256_set1_epi32(1 << 11);
    btf_16_adds_subs_avx2(&x0_0, &x0_7);
    btf_16_adds_subs_avx2(&x0_1, &x0_6);
    btf_16_adds_subs_avx2(&x0_2, &x0_5);
    btf_16_adds_subs_avx2(&x0_3, &x0_4);

    btf_16_adds_subs_avx2(&x0_0, &x0_3);
    btf_16_adds_subs_avx2(&x0_1, &x0_2);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi12[32], cospi12[32]),
                    pair_set_w16_epi16(cospi12[32], cospi12[32]), &x0_5, &x0_6,
                    _r, 12);

    btf_16_w16_avx2(pair_set_w16_epi16(cospi12[32], cospi12[32]),
                    pair_set_w16_epi16(cospi12[32], -cospi12[32]), &x0_0, &x0_1,
                    _r, 12);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi12[48], cospi12[16]),
                    pair_set_w16_epi16(-cospi12[16], cospi12[48]), &x0_2, &x0_3,
                    _r, 12);
    store_row_16bit_to_32bit_w16_avx2(x0_0, output + 0 * 16);
    store_row_16bit_to_32bit_w16_avx2(x0_1, output + 8 * 16);
    store_row_16bit_to_32bit_w16_avx2(x0_2, output + 4 * 16);
    store_row_16bit_to_32bit_w16_avx2(x0_3, output + 12 * 16);

    btf_16_adds_subs_avx2(&x0_4, &x0_5);
    btf_16_adds_subs_avx2(&x0_7, &x0_6);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi12[56], cospi12[8]),
                    pair_set_w16_epi16(-cospi12[8], cospi12[56]), &x0_4, &x0_7,
                    _r, 12);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi12[24], cospi12[40]),
                    pair_set_w16_epi16(-cospi12[40], cospi12[24]), &x0_5, &x0_6,
                    _r, 12);
    store_row_16bit_to_32bit_w16_avx2(x0_4, output + 2 * 16);
    store_row_16bit_to_32bit_w16_avx2(x0_6, output + 6 * 16);
    store_row_16bit_to_32bit_w16_avx2(x0_5, output + 10 * 16);
    store_row_16bit_to_32bit_w16_avx2(x0_7, output + 14 * 16);
  }

  // Row pass Odd Half (cos_bit = 12) + store
  {
    const __m256i _r = _mm256_set1_epi32(1 << 11);
    __m256i x1_0 = x1[0], x1_1 = x1[1], x1_2 = x1[2], x1_3 = x1[3];
    __m256i x1_4 = x1[4], x1_5 = x1[5], x1_6 = x1[6], x1_7 = x1[7];
    const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi12[32], cospi12[32]);
    const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi12[32], cospi12[32]);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1_2, &x1_5, _r, 12);
    btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1_3, &x1_4, _r, 12);

    btf_16_adds_subs_avx2(&x1_0, &x1_3);
    btf_16_adds_subs_avx2(&x1_1, &x1_2);
    btf_16_adds_subs_avx2(&x1_7, &x1_4);
    btf_16_adds_subs_avx2(&x1_6, &x1_5);

    const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi12[16], cospi12[48]);
    btf_16_w16_avx2(cospi_m16_p48, pair_set_w16_epi16(cospi12[48], cospi12[16]),
                    &x1_1, &x1_6, _r, 12);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi12[48], -cospi12[16]),
                    cospi_m16_p48, &x1_2, &x1_5, _r, 12);

    btf_16_adds_subs_avx2(&x1_0, &x1_1);
    btf_16_adds_subs_avx2(&x1_3, &x1_2);
    btf_16_adds_subs_avx2(&x1_4, &x1_5);
    btf_16_adds_subs_avx2(&x1_7, &x1_6);

    btf_16_w16_avx2(pair_set_w16_epi16(cospi12[60], cospi12[4]),
                    pair_set_w16_epi16(-cospi12[4], cospi12[60]), &x1_0, &x1_7,
                    _r, 12);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi12[28], cospi12[36]),
                    pair_set_w16_epi16(-cospi12[36], cospi12[28]), &x1_1, &x1_6,
                    _r, 12);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi12[44], cospi12[20]),
                    pair_set_w16_epi16(-cospi12[20], cospi12[44]), &x1_2, &x1_5,
                    _r, 12);
    btf_16_w16_avx2(pair_set_w16_epi16(cospi12[12], cospi12[52]),
                    pair_set_w16_epi16(-cospi12[52], cospi12[12]), &x1_3, &x1_4,
                    _r, 12);

    store_row_16bit_to_32bit_w16_avx2(x1_0, output + 1 * 16);
    store_row_16bit_to_32bit_w16_avx2(x1_4, output + 3 * 16);
    store_row_16bit_to_32bit_w16_avx2(x1_2, output + 5 * 16);
    store_row_16bit_to_32bit_w16_avx2(x1_6, output + 7 * 16);
    store_row_16bit_to_32bit_w16_avx2(x1_1, output + 9 * 16);
    store_row_16bit_to_32bit_w16_avx2(x1_5, output + 11 * 16);
    store_row_16bit_to_32bit_w16_avx2(x1_3, output + 13 * 16);
    store_row_16bit_to_32bit_w16_avx2(x1_7, output + 15 * 16);
  }
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void lowbd_fwd_txfm2d_32x32_non_dct_avx2(const int16_t *input,
                                                int32_t *output, int stride,
                                                TX_TYPE tx_type, int bd) {
  (void)bd;
  __m256i buf0[32], buf1[64];
  const transform_1d_avx2 col_txfm = col_txfm16x32_arr[tx_type];
  const transform_1d_avx2 row_txfm = row_txfm16x32_arr[tx_type];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  for (int i = 0; i < 2; i++) {
    if (ud_flip) {
      load_buffer_16bit_to_16bit_flip_avx2(input + 16 * i, stride, buf0, 32);
      round_shift_16bit_w16_avx2(buf0, 32, 2);
    } else {
      load_buffer_16bit_to_16bit_sll_avx2(input + 16 * i, stride, buf0, 32, 2);
    }
    col_txfm(buf0, buf0, 12);
    transpose_round_shift_16bit_16x16_avx2(buf0 + 0 * 16,
                                           buf1 + 0 * 32 + 16 * i, 4);
    transpose_round_shift_16bit_16x16_avx2(buf0 + 1 * 16,
                                           buf1 + 1 * 32 + 16 * i, 4);
  }

  for (int i = 0; i < 2; i++) {
    __m256i *buf;
    if (lr_flip) {
      buf = buf0;
      flip_buf_avx2(buf1 + 32 * i, buf, 32);
    } else {
      buf = buf1 + 32 * i;
    }
    row_txfm(buf, buf, 12);
    store_buffer_16bit_to_32bit_w16_avx2(buf, output + i * 16, 32, 32);
  }
}

static void lowbd_fwd_txfm2d_32x32_avx2(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  if (UNLIKELY(tx_type != DCT_DCT)) {
    lowbd_fwd_txfm2d_32x32_non_dct_avx2(input, output, stride, tx_type, bd);
    return;
  }
  (void)bd;
  __m256i buf0[32], buf1[64];

  for (int i = 0; i < 2; i++) {
    load_buffer_16bit_to_16bit_sll_avx2(input + 16 * i, stride, buf0, 32, 2);
    fdct16x32_avx2(buf0, buf0, 12);
    transpose_round_shift_16bit_16x16_avx2(buf0 + 0 * 16,
                                           buf1 + 0 * 32 + 16 * i, 4);
    transpose_round_shift_16bit_16x16_avx2(buf0 + 1 * 16,
                                           buf1 + 1 * 32 + 16 * i, 4);
  }
  for (int i = 0; i < 2; i++) {
    __m256i *buf = buf1 + 32 * i;
    fdct16x32_avx2(buf, buf, 12);
    store_buffer_16bit_to_32bit_w16_avx2(buf, output + i * 16, 32, 32);
  }
}

static void lowbd_fwd_txfm2d_64x64_avx2(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  const TX_SIZE tx_size = TX_64X64;
  __m256i buf0[64], buf1[256];
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_avx2 col_txfm = fdct16x64_new_avx2;
  const int width_div16 = (width >> 4);
  const int height_div16 = (height >> 4);

  for (int i = 0; i < width_div16; i++) {
    load_buffer_16bit_to_16bit_avx2(input + 16 * i, stride, buf0, height);
    round_shift_16bit_w16_avx2(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col);
    round_shift_16bit_w16_avx2(buf0, height, shift[1]);
    for (int j = 0; j < AOMMIN(2, height_div16); ++j) {
      transpose_16bit_16x16_avx2(buf0 + j * 16, buf1 + j * width + 16 * i);
    }
  }

  for (int i = 0; i < AOMMIN(2, height_div16); i++) {
    __m256i bufA[64];
    __m256i bufB[64];
    __m128i *buf = (__m128i *)(buf1 + width * i);
    for (int j = 0; j < width; ++j) {
      bufA[j] = _mm256_cvtepi16_epi32(buf[j * 2]);
      bufB[j] = _mm256_cvtepi16_epi32(buf[j * 2 + 1]);
    }
    fdct64_new_avx2(bufA, bufA, cos_bit_row);
    fdct64_new_avx2(bufB, bufB, cos_bit_row);
    round_shift_array_32_avx2(bufA, bufA, 32, -shift[2]);
    round_shift_array_32_avx2(bufB, bufB, 32, -shift[2]);
    store_output_32bit_w16(output + i * 16, bufA, bufB, 32, 32);
  }
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void lowbd_fwd_txfm2d_16x32_non_dct_avx2(const int16_t *input,
                                                int32_t *output, int stride,
                                                TX_TYPE tx_type, int bd) {
  (void)bd;
  __m256i buf0[32], buf1[32];
  const transform_1d_avx2 col_txfm = col_txfm16x32_arr[tx_type];
  const transform_1d_avx2 row_txfm = row_txfm16x16_cb13_arr[tx_type];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  if (ud_flip) {
    load_buffer_16bit_to_16bit_flip_avx2(input, stride, buf0, 32);
    round_shift_16bit_w16_avx2(buf0, 32, 2);
  } else {
    load_buffer_16bit_to_16bit_sll_avx2(input, stride, buf0, 32, 2);
  }
  col_txfm(buf0, buf0, 12);
  transpose_round_shift_16bit_16x16_avx2(buf0, buf1, 4);
  transpose_round_shift_16bit_16x16_avx2(buf0 + 16, buf1 + 16, 4);

  for (int i = 0; i < 2; i++) {
    __m256i *buf;
    if (lr_flip) {
      buf = buf0;
      flip_buf_avx2(buf1 + 16 * i, buf, 16);
    } else {
      buf = buf1 + 16 * i;
    }
    row_txfm(buf, buf, 13);
    store_rect_buffer_16bit_to_32bit_w16_avx2(buf, output + i * 16, 32, 16);
  }
}

static void lowbd_fwd_txfm2d_16x32_avx2(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  if (UNLIKELY(tx_type != DCT_DCT)) {
    lowbd_fwd_txfm2d_16x32_non_dct_avx2(input, output, stride, tx_type, bd);
    return;
  }
  (void)bd;
  __m256i buf0[32], buf1[32];

  load_buffer_16bit_to_16bit_sll_avx2(input, stride, buf0, 32, 2);
  fdct16x32_avx2(buf0, buf0, 12);
  transpose_round_shift_16bit_16x16_avx2(buf0, buf1, 4);
  transpose_round_shift_16bit_16x16_avx2(buf0 + 16, buf1 + 16, 4);

  for (int i = 0; i < 2; i++) {
    __m256i *buf = buf1 + 16 * i;
    fdct16x16_new_avx2(buf, buf, 13);
    store_rect_buffer_16bit_to_32bit_w16_avx2(buf, output + i * 16, 32, 16);
  }
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void lowbd_fwd_txfm2d_32x16_non_dct_avx2(const int16_t *input,
                                                int32_t *output, int stride,
                                                TX_TYPE tx_type, int bd) {
  (void)bd;
  __m256i buf0[32], buf1[32];
  const col_txfm16x16_fn col_txfm = col_txfm16x16_arr[tx_type];
  const transform_1d_avx2 row_txfm = row_txfm16x32_cb13_arr[tx_type];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  const int16_t *in_base = ud_flip ? (input + 15 * stride) : input;
  const int in_stride = ud_flip ? -stride : stride;
  for (int i = 0; i < 2; i++) {
    col_txfm(in_base + 16 * i, buf0, in_stride);
    transpose_round_shift_16bit_16x16_avx2(buf0, buf1 + 16 * i, 4);
  }

  __m256i *buf;
  if (lr_flip) {
    buf = buf0;
    flip_buf_avx2(buf1, buf, 32);
  } else {
    buf = buf1;
  }
  row_txfm(buf, buf, 13);
  store_rect_buffer_16bit_to_32bit_w16_avx2(buf, output, 16, 32);
}

static void lowbd_fwd_txfm2d_32x16_avx2(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  if (UNLIKELY(tx_type != DCT_DCT)) {
    lowbd_fwd_txfm2d_32x16_non_dct_avx2(input, output, stride, tx_type, bd);
    return;
  }
  (void)bd;
  __m256i buf0[16], buf1[32];

  for (int i = 0; i < 2; i++) {
    fdct16x16_load_sll_avx2(input + 16 * i, stride, buf0, 13, 2);
    transpose_round_shift_16bit_16x16_avx2(buf0, buf1 + 16 * i, 4);
  }

  fdct16x32_cb13_avx2(buf1, buf1, 13);
  store_rect_buffer_16bit_to_32bit_w16_avx2(buf1, output, 16, 32);
}

static void lowbd_fwd_txfm2d_64x32_avx2(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  const TX_SIZE tx_size = TX_64X32;
  __m256i buf0[64], buf1[256];
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_avx2 col_txfm = col_txfm16x32_arr[tx_type];
  const int width_div16 = (width >> 4);
  const int height_div16 = (height >> 4);

  for (int i = 0; i < width_div16; i++) {
    load_buffer_16bit_to_16bit_avx2(input + 16 * i, stride, buf0, height);
    round_shift_16bit_w16_avx2(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col);
    round_shift_16bit_w16_avx2(buf0, height, shift[1]);
    for (int j = 0; j < AOMMIN(4, height_div16); ++j) {
      transpose_16bit_16x16_avx2(buf0 + j * 16, buf1 + j * width + 16 * i);
    }
  }
  assert(tx_type == DCT_DCT);
  for (int i = 0; i < AOMMIN(2, height_div16); i++) {
    __m256i bufA[64];
    __m256i bufB[64];
    __m128i *buf = (__m128i *)(buf1 + width * i);
    for (int j = 0; j < width; ++j) {
      bufA[j] = _mm256_cvtepi16_epi32(buf[j * 2]);
      bufB[j] = _mm256_cvtepi16_epi32(buf[j * 2 + 1]);
    }
    fdct64_new_avx2(bufA, bufA, cos_bit_row);
    fdct64_new_avx2(bufB, bufB, cos_bit_row);
    round_shift_rect_array_32_avx2(bufA, bufA, 32, -shift[2], NewSqrt2);
    round_shift_rect_array_32_avx2(bufB, bufB, 32, -shift[2], NewSqrt2);

    store_output_32bit_w16(output + i * 16, bufA, bufB, 32, 32);
  }
}

static void lowbd_fwd_txfm2d_32x64_avx2(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  const TX_SIZE tx_size = TX_32X64;
  __m256i buf0[64], buf1[256];
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_avx2 col_txfm = fdct16x64_new_avx2;
  const int width_div16 = (width >> 4);
  const int height_div16 = (height >> 4);

  for (int i = 0; i < width_div16; i++) {
    load_buffer_16bit_to_16bit_avx2(input + 16 * i, stride, buf0, height);
    round_shift_16bit_w16_avx2(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col);
    round_shift_16bit_w16_avx2(buf0, height, shift[1]);
    for (int j = 0; j < AOMMIN(2, height_div16); ++j) {
      transpose_16bit_16x16_avx2(buf0 + j * 16, buf1 + j * width + 16 * i);
    }
  }

  for (int i = 0; i < AOMMIN(2, height_div16); i++) {
    __m256i bufA[32];
    __m256i bufB[32];
    __m128i *buf = (__m128i *)(buf1 + width * i);
    for (int j = 0; j < width; ++j) {
      bufA[j] = _mm256_cvtepi16_epi32(buf[j * 2]);
      bufB[j] = _mm256_cvtepi16_epi32(buf[j * 2 + 1]);
    }
    fdct32_avx2(bufA, bufA, cos_bit_row);
    fdct32_avx2(bufB, bufB, cos_bit_row);
    round_shift_rect_array_32_avx2(bufA, bufA, 32, -shift[2], NewSqrt2);
    round_shift_rect_array_32_avx2(bufB, bufB, 32, -shift[2], NewSqrt2);

    store_output_32bit_w16(output + i * 16, bufA, bufB, 32, 32);
  }
}

static void lowbd_fwd_txfm2d_16x64_avx2(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  const TX_SIZE tx_size = TX_16X64;
  __m256i buf0[64], buf1[64];
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_avx2 col_txfm = fdct16x64_new_avx2;
  const transform_1d_avx2 row_txfm = fdct16x16_new_avx2;
  const int width_div16 = (width >> 4);
  const int height_div16 = (height >> 4);

  for (int i = 0; i < width_div16; i++) {
    load_buffer_16bit_to_16bit_avx2(input + 16 * i, stride, buf0, height);
    round_shift_16bit_w16_avx2(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col);
    round_shift_16bit_w16_avx2(buf0, height, shift[1]);
    for (int j = 0; j < height_div16; ++j) {
      transpose_16bit_16x16_avx2(buf0 + j * 16, buf1 + j * width + 16 * i);
    }
  }

  for (int i = 0; i < AOMMIN(2, height_div16); i++) {
    __m256i *buf = buf1 + width * i;
    row_txfm(buf, buf, cos_bit_row);
    round_shift_16bit_w16_avx2(buf, width, shift[2]);
    store_buffer_16bit_to_32bit_w16_avx2(buf, output + width * i, 32, width);
  }
}

static void lowbd_fwd_txfm2d_64x16_avx2(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  const TX_SIZE tx_size = TX_64X16;
  __m256i buf0[64], buf1[64];
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_avx2 col_txfm = fdct16x16_new_avx2;
  const transform_1d_avx2 row_txfm = fdct16x64_new_avx2;
  const int width_div16 = (width >> 4);
  const int height_div16 = (height >> 4);

  for (int i = 0; i < width_div16; i++) {
    load_buffer_16bit_to_16bit_avx2(input + 16 * i, stride, buf0, height);
    round_shift_16bit_w16_avx2(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col);
    round_shift_16bit_w16_avx2(buf0, height, shift[1]);
    for (int j = 0; j < height_div16; ++j) {
      transpose_16bit_16x16_avx2(buf0 + j * 16, buf1 + j * width + 16 * i);
    }
  }

  for (int i = 0; i < height_div16; i++) {
    __m256i *buf = buf1 + width * i;
    row_txfm(buf, buf, cos_bit_row);
    round_shift_16bit_w16_avx2(buf, width, shift[2]);
    store_buffer_16bit_to_32bit_w16_avx2(buf, output + 16 * i, 16, 32);
  }
  // Zero out the bottom 16x32 area.
  memset(output + 16 * 32, 0, 16 * 32 * sizeof(*output));
}

static AOM_FORCE_INLINE void store_rect_row_pair_16bit_to_32bit_avx2(
    const __m256i a, int32_t *const out, int row_lo, int row_hi) {
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i a_reoder = _mm256_permute4x64_epi64(a, 0xd8);
  const __m256i a_lo = _mm256_unpacklo_epi16(a_reoder, one);
  const __m256i a_hi = _mm256_unpackhi_epi16(a_reoder, one);
  const __m256i b_lo = scale_round_avx2(a_lo, NewSqrt2);
  const __m256i b_hi = scale_round_avx2(a_hi, NewSqrt2);
  _mm256_store_si256((__m256i *)(out + 8 * row_lo), b_lo);
  _mm256_store_si256((__m256i *)(out + 8 * row_hi), b_hi);
}

static inline void fdct8x8_new_avx2(const __m256i *input, __m256i *output,
                                    int8_t cos_bit_arg) {
  (void)cos_bit_arg;
  const int8_t cos_bit = 13;
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m256i __rounding = _mm256_set1_epi32(1 << (cos_bit - 1));

  __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
  __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
  __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
  __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
  __m256i cospi_p24_p40 = pair_set_w16_epi16(cospi[24], cospi[40]);
  __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);

  // stage 1
  __m256i x1[8];
  x1[0] = _mm256_adds_epi16(input[0], input[7]);
  x1[7] = _mm256_subs_epi16(input[0], input[7]);
  x1[1] = _mm256_adds_epi16(input[1], input[6]);
  x1[6] = _mm256_subs_epi16(input[1], input[6]);
  x1[2] = _mm256_adds_epi16(input[2], input[5]);
  x1[5] = _mm256_subs_epi16(input[2], input[5]);
  x1[3] = _mm256_adds_epi16(input[3], input[4]);
  x1[4] = _mm256_subs_epi16(input[3], input[4]);

  // stage 2
  __m256i x2[8];
  x2[0] = _mm256_adds_epi16(x1[0], x1[3]);
  x2[3] = _mm256_subs_epi16(x1[0], x1[3]);
  x2[1] = _mm256_adds_epi16(x1[1], x1[2]);
  x2[2] = _mm256_subs_epi16(x1[1], x1[2]);
  x2[4] = x1[4];
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[5], &x1[6], __rounding,
                  cos_bit);
  x2[5] = x1[5];
  x2[6] = x1[6];
  x2[7] = x1[7];

  // stage 3
  __m256i x3[8];
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x2[0], &x2[1], __rounding,
                  cos_bit);
  x3[0] = x2[0];
  x3[1] = x2[1];
  btf_16_w16_avx2(cospi_p48_p16, cospi_m16_p48, &x2[2], &x2[3], __rounding,
                  cos_bit);
  x3[2] = x2[2];
  x3[3] = x2[3];
  x3[4] = _mm256_adds_epi16(x2[4], x2[5]);
  x3[5] = _mm256_subs_epi16(x2[4], x2[5]);
  x3[6] = _mm256_subs_epi16(x2[7], x2[6]);
  x3[7] = _mm256_adds_epi16(x2[7], x2[6]);

  // stage 4
  __m256i x4[8];
  x4[0] = x3[0];
  x4[1] = x3[1];
  x4[2] = x3[2];
  x4[3] = x3[3];
  btf_16_w16_avx2(cospi_p56_p08, cospi_m08_p56, &x3[4], &x3[7], __rounding,
                  cos_bit);
  x4[4] = x3[4];
  x4[7] = x3[7];
  btf_16_w16_avx2(cospi_p24_p40, cospi_m40_p24, &x3[5], &x3[6], __rounding,
                  cos_bit);
  x4[5] = x3[5];
  x4[6] = x3[6];
  // stage 5
  output[0] = x4[0];
  output[1] = x4[4];
  output[2] = x4[2];
  output[3] = x4[6];
  output[4] = x4[1];
  output[5] = x4[5];
  output[6] = x4[3];
  output[7] = x4[7];
}

static inline void fadst8x8_new_avx2(const __m256i *input, __m256i *output,
                                     int8_t cos_bit_arg) {
  (void)cos_bit_arg;
  const int8_t cos_bit = 13;
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m256i __zero = _mm256_setzero_si256();
  const __m256i __rounding = _mm256_set1_epi32(1 << (cos_bit - 1));

  __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
  __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
  __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
  __m256i cospi_m48_p16 = pair_set_w16_epi16(-cospi[48], cospi[16]);
  __m256i cospi_p04_p60 = pair_set_w16_epi16(cospi[4], cospi[60]);
  __m256i cospi_p60_m04 = pair_set_w16_epi16(cospi[60], -cospi[4]);
  __m256i cospi_p20_p44 = pair_set_w16_epi16(cospi[20], cospi[44]);
  __m256i cospi_p44_m20 = pair_set_w16_epi16(cospi[44], -cospi[20]);
  __m256i cospi_p36_p28 = pair_set_w16_epi16(cospi[36], cospi[28]);
  __m256i cospi_p28_m36 = pair_set_w16_epi16(cospi[28], -cospi[36]);
  __m256i cospi_p52_p12 = pair_set_w16_epi16(cospi[52], cospi[12]);
  __m256i cospi_p12_m52 = pair_set_w16_epi16(cospi[12], -cospi[52]);

  // stage 1
  __m256i x1[8];
  x1[0] = input[0];
  x1[1] = _mm256_subs_epi16(__zero, input[7]);
  x1[2] = _mm256_subs_epi16(__zero, input[3]);
  x1[3] = input[4];
  x1[4] = _mm256_subs_epi16(__zero, input[1]);
  x1[5] = input[6];
  x1[6] = input[2];
  x1[7] = _mm256_subs_epi16(__zero, input[5]);

  // stage 2
  __m256i x2[8];
  x2[0] = x1[0];
  x2[1] = x1[1];
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x1[2], &x1[3], __rounding,
                  cos_bit);
  x2[2] = x1[2];
  x2[3] = x1[3];
  x2[4] = x1[4];
  x2[5] = x1[5];
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x1[6], &x1[7], __rounding,
                  cos_bit);
  x2[6] = x1[6];
  x2[7] = x1[7];

  // stage 3
  __m256i x3[8];
  x3[0] = _mm256_adds_epi16(x2[0], x2[2]);
  x3[2] = _mm256_subs_epi16(x2[0], x2[2]);
  x3[1] = _mm256_adds_epi16(x2[1], x2[3]);
  x3[3] = _mm256_subs_epi16(x2[1], x2[3]);
  x3[4] = _mm256_adds_epi16(x2[4], x2[6]);
  x3[6] = _mm256_subs_epi16(x2[4], x2[6]);
  x3[5] = _mm256_adds_epi16(x2[5], x2[7]);
  x3[7] = _mm256_subs_epi16(x2[5], x2[7]);

  // stage 4
  __m256i x4[8];
  x4[0] = x3[0];
  x4[1] = x3[1];
  x4[2] = x3[2];
  x4[3] = x3[3];
  btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, &x3[4], &x3[5], __rounding,
                  cos_bit);
  x4[4] = x3[4];
  x4[5] = x3[5];
  btf_16_w16_avx2(cospi_m48_p16, cospi_p16_p48, &x3[6], &x3[7], __rounding,
                  cos_bit);
  x4[6] = x3[6];
  x4[7] = x3[7];

  // stage 5
  __m256i x5[8];
  x5[0] = _mm256_adds_epi16(x4[0], x4[4]);
  x5[4] = _mm256_subs_epi16(x4[0], x4[4]);
  x5[1] = _mm256_adds_epi16(x4[1], x4[5]);
  x5[5] = _mm256_subs_epi16(x4[1], x4[5]);
  x5[2] = _mm256_adds_epi16(x4[2], x4[6]);
  x5[6] = _mm256_subs_epi16(x4[2], x4[6]);
  x5[3] = _mm256_adds_epi16(x4[3], x4[7]);
  x5[7] = _mm256_subs_epi16(x4[3], x4[7]);

  // stage 6
  __m256i x6[8];
  btf_16_w16_avx2(cospi_p04_p60, cospi_p60_m04, &x5[0], &x5[1], __rounding,
                  cos_bit);
  x6[0] = x5[0];
  x6[1] = x5[1];
  btf_16_w16_avx2(cospi_p20_p44, cospi_p44_m20, &x5[2], &x5[3], __rounding,
                  cos_bit);
  x6[2] = x5[2];
  x6[3] = x5[3];
  btf_16_w16_avx2(cospi_p36_p28, cospi_p28_m36, &x5[4], &x5[5], __rounding,
                  cos_bit);
  x6[4] = x5[4];
  x6[5] = x5[5];
  btf_16_w16_avx2(cospi_p52_p12, cospi_p12_m52, &x5[6], &x5[7], __rounding,
                  cos_bit);
  x6[6] = x5[6];
  x6[7] = x5[7];

  // stage 7
  output[0] = x6[1];
  output[1] = x6[6];
  output[2] = x6[3];
  output[3] = x6[4];
  output[4] = x6[5];
  output[5] = x6[2];
  output[6] = x6[7];
  output[7] = x6[0];
}

static inline void fidentity8x8_new_avx2(const __m256i *input, __m256i *output,
                                         int8_t cos_bit) {
  (void)cos_bit;

  output[0] = _mm256_adds_epi16(input[0], input[0]);
  output[1] = _mm256_adds_epi16(input[1], input[1]);
  output[2] = _mm256_adds_epi16(input[2], input[2]);
  output[3] = _mm256_adds_epi16(input[3], input[3]);
  output[4] = _mm256_adds_epi16(input[4], input[4]);
  output[5] = _mm256_adds_epi16(input[5], input[5]);
  output[6] = _mm256_adds_epi16(input[6], input[6]);
  output[7] = _mm256_adds_epi16(input[7], input[7]);
}

// Odd half of 16-point DCT on dual-lane inputs d0..d3:
//   d0 = [x1_8 | x1_9], d1 = [x1_11 | x1_10],
//   d2 = [x1_12 | x1_13], d3 = [x1_15 | x1_14].
// Outputs:
//   q_odd[0] = [o1 | o9], q_odd[1] = [o15 | o7],
//   q_odd[2] = [o5 | o13], q_odd[3] = [o11 | o3].
static AOM_FORCE_INLINE void fdct8x16_odd_dual_avx2(__m256i d0, __m256i d1,
                                                    __m256i d2, __m256i d3,
                                                    __m256i *q_odd) {
  const int32_t *cospi = cospi_arr(13);
  const __m256i r = _mm256_set1_epi32(1 << 12);
  const __m256i w0_s2 = pair_set_w16_epi16(-cospi[32], cospi[32]);
  const __m256i w1_s2 = pair_set_w16_epi16(cospi[32], cospi[32]);
  const __m256i w0_s4 =
      pair_set_w16x2_epi16(-cospi[16], cospi[48], -cospi[48], -cospi[16]);
  const __m256i w1_s4 =
      pair_set_w16x2_epi16(cospi[48], cospi[16], -cospi[16], cospi[48]);
  const __m256i w0_s6a =
      pair_set_w16x2_epi16(cospi[60], cospi[4], cospi[28], cospi[36]);
  const __m256i w1_s6a =
      pair_set_w16x2_epi16(-cospi[4], cospi[60], -cospi[36], cospi[28]);
  const __m256i w0_s6b =
      pair_set_w16x2_epi16(cospi[44], cospi[20], cospi[12], cospi[52]);
  const __m256i w1_s6b =
      pair_set_w16x2_epi16(-cospi[20], cospi[44], -cospi[52], cospi[12]);

  btf_16_w16_avx2(w0_s2, w1_s2, &d1, &d2, r, 13);

  const __m256i e01 = _mm256_adds_epi16(d0, d1);
  const __m256i e32 = _mm256_subs_epi16(d0, d1);
  const __m256i e76 = _mm256_adds_epi16(d3, d2);
  const __m256i e45 = _mm256_subs_epi16(d3, d2);

  const __m256i f03 = _mm256_permute2x128_si256(e01, e32, 0x20);
  const __m256i f74 = _mm256_permute2x128_si256(e76, e45, 0x20);
  __m256i f12 = _mm256_permute2x128_si256(e01, e32, 0x31);
  __m256i f65 = _mm256_permute2x128_si256(e76, e45, 0x31);
  btf_16_w16_avx2(w0_s4, w1_s4, &f12, &f65, r, 13);

  const __m256i g03 = _mm256_adds_epi16(f03, f12);
  const __m256i g12 = _mm256_subs_epi16(f03, f12);
  const __m256i g74 = _mm256_adds_epi16(f74, f65);
  const __m256i g65 = _mm256_subs_epi16(f74, f65);

  __m256i in0 = _mm256_permute2x128_si256(g03, g12, 0x20);
  __m256i in1 = _mm256_permute2x128_si256(g74, g65, 0x20);
  __m256i in2 = _mm256_permute2x128_si256(g12, g03, 0x31);
  __m256i in3 = _mm256_permute2x128_si256(g65, g74, 0x31);
  btf_16_w16_avx2(w0_s6a, w1_s6a, &in0, &in1, r, 13);
  btf_16_w16_avx2(w0_s6b, w1_s6b, &in2, &in3, r, 13);
  q_odd[0] = in0;
  q_odd[1] = in1;
  q_odd[2] = in2;
  q_odd[3] = in3;
}

static inline void fdct8x16_new_avx2(const __m256i *input, __m256i *output,
                                     int8_t cos_bit_arg) {
  (void)cos_bit_arg;
  const __m256i x_0_1 = _mm256_permute2x128_si256(input[0], input[1], 0x20);
  const __m256i x_8_9 = _mm256_permute2x128_si256(input[0], input[1], 0x31);
  const __m256i x_7_6 = _mm256_permute2x128_si256(input[7], input[6], 0x20);
  const __m256i x_15_14 = _mm256_permute2x128_si256(input[7], input[6], 0x31);
  const __m256i x_3_2 = _mm256_permute2x128_si256(input[3], input[2], 0x20);
  const __m256i x_11_10 = _mm256_permute2x128_si256(input[3], input[2], 0x31);
  const __m256i x_4_5 = _mm256_permute2x128_si256(input[4], input[5], 0x20);
  const __m256i x_12_13 = _mm256_permute2x128_si256(input[4], input[5], 0x31);

  __m256i s[4];
  s[0] = _mm256_adds_epi16(x_0_1, x_15_14);
  s[1] = _mm256_adds_epi16(x_3_2, x_12_13);
  s[2] = _mm256_adds_epi16(x_4_5, x_11_10);
  s[3] = _mm256_adds_epi16(x_7_6, x_8_9);

  const __m256i d0 = _mm256_subs_epi16(x_7_6, x_8_9);
  const __m256i d1 = _mm256_subs_epi16(x_4_5, x_11_10);
  const __m256i d2 = _mm256_subs_epi16(x_3_2, x_12_13);
  const __m256i d3 = _mm256_subs_epi16(x_0_1, x_15_14);

  __m256i q_even[4], q_odd[4];
  fdct8x8_dual_avx2(s, q_even);
  fdct8x16_odd_dual_avx2(d0, d1, d2, d3, q_odd);

  output[0] = _mm256_permute2x128_si256(q_even[0], q_even[1], 0x20);
  output[1] = q_odd[0];
  output[2] = q_even[2];
  output[3] = _mm256_permute4x64_epi64(q_odd[3], 0x4e);
  output[4] = _mm256_permute2x128_si256(q_even[0], q_even[1], 0x31);
  output[5] = q_odd[2];
  output[6] = _mm256_permute4x64_epi64(q_even[3], 0x4e);
  output[7] = _mm256_permute4x64_epi64(q_odd[1], 0x4e);
}

static inline void fadst8x16_new_avx2(const __m256i *input, __m256i *output,
                                      int8_t cos_bit_arg) {
  (void)cos_bit_arg;
  const int32_t *cospi = cospi_arr(13);
  const __m256i zero = _mm256_setzero_si256();
  const __m256i r = _mm256_set1_epi32(1 << 12);

  const __m256i w0_s2 = pair_set_w16_epi16(cospi[32], cospi[32]);
  const __m256i w1_s2 = pair_set_w16_epi16(cospi[32], -cospi[32]);
  const __m256i w0_s4 =
      pair_set_w16x2_epi16(cospi[16], cospi[48], -cospi[48], cospi[16]);
  const __m256i w1_s4 =
      pair_set_w16x2_epi16(cospi[48], -cospi[16], cospi[16], cospi[48]);

  const __m256i n1 = _mm256_subs_epi16(zero, input[1]);
  const __m256i n3 = _mm256_subs_epi16(zero, input[3]);
  const __m256i n5 = _mm256_subs_epi16(zero, input[5]);
  const __m256i n7 = _mm256_subs_epi16(zero, input[7]);

  // Group A: Stages 1..5
  __m256i g0 = _mm256_permute2x128_si256(input[0], n3, 0x20);
  __m256i g3 = _mm256_permute2x128_si256(input[0], n3, 0x31);
  __m256i g1 = _mm256_permute2x128_si256(n7, input[4], 0x31);
  __m256i g2 = _mm256_permute2x128_si256(n7, input[4], 0x20);
  btf_16_w16_avx2(w0_s2, w1_s2, &g2, &g3, r, 13);
  btf_16_adds_subs_avx2(&g0, &g2);
  btf_16_adds_subs_avx2(&g1, &g3);
  const __m256i a02 = _mm256_permute2x128_si256(g0, g2, 0x20);
  const __m256i a13 = _mm256_permute2x128_si256(g1, g3, 0x20);
  __m256i a46 = _mm256_permute2x128_si256(g0, g2, 0x31);
  __m256i a57 = _mm256_permute2x128_si256(g1, g3, 0x31);
  btf_16_w16_avx2(w0_s4, w1_s4, &a46, &a57, r, 13);
  const __m256i a5_02 = _mm256_adds_epi16(a02, a46);
  const __m256i a5_46 = _mm256_subs_epi16(a02, a46);
  const __m256i a5_13 = _mm256_adds_epi16(a13, a57);
  const __m256i a5_57 = _mm256_subs_epi16(a13, a57);

  // Group B: Stages 1..6
  __m256i h0 = _mm256_permute2x128_si256(n1, input[2], 0x20);
  __m256i h3 = _mm256_permute2x128_si256(n1, input[2], 0x31);
  __m256i h1 = _mm256_permute2x128_si256(input[6], n5, 0x31);
  __m256i h2 = _mm256_permute2x128_si256(input[6], n5, 0x20);
  btf_16_w16_avx2(w0_s2, w1_s2, &h2, &h3, r, 13);
  btf_16_adds_subs_avx2(&h0, &h2);
  btf_16_adds_subs_avx2(&h1, &h3);
  const __m256i b02 = _mm256_permute2x128_si256(h0, h2, 0x20);
  const __m256i b13 = _mm256_permute2x128_si256(h1, h3, 0x20);
  __m256i b46 = _mm256_permute2x128_si256(h0, h2, 0x31);
  __m256i b57 = _mm256_permute2x128_si256(h1, h3, 0x31);
  btf_16_w16_avx2(w0_s4, w1_s4, &b46, &b57, r, 13);
  __m256i b5_02 = _mm256_adds_epi16(b02, b46);
  __m256i b5_46 = _mm256_subs_epi16(b02, b46);
  __m256i b5_13 = _mm256_adds_epi16(b13, b57);
  __m256i b5_57 = _mm256_subs_epi16(b13, b57);

  const __m256i w0_s6a =
      pair_set_w16x2_epi16(cospi[8], cospi[56], cospi[40], cospi[24]);
  const __m256i w1_s6a =
      pair_set_w16x2_epi16(cospi[56], -cospi[8], cospi[24], -cospi[40]);
  const __m256i w0_s6b =
      pair_set_w16x2_epi16(-cospi[56], cospi[8], -cospi[24], cospi[40]);
  btf_16_w16_avx2(w0_s6a, w1_s6a, &b5_02, &b5_13, r, 13);
  btf_16_w16_avx2(w0_s6b, w0_s6a, &b5_46, &b5_57, r, 13);

  // Stages 7 & 8
  __m256i s7_02 = _mm256_adds_epi16(a5_02, b5_02);
  __m256i s7_8_10 = _mm256_subs_epi16(a5_02, b5_02);
  __m256i s7_13 = _mm256_adds_epi16(a5_13, b5_13);
  __m256i s7_9_11 = _mm256_subs_epi16(a5_13, b5_13);
  __m256i s7_46 = _mm256_adds_epi16(a5_46, b5_46);
  __m256i s7_12_14 = _mm256_subs_epi16(a5_46, b5_46);
  __m256i s7_57 = _mm256_adds_epi16(a5_57, b5_57);
  __m256i s7_13_15 = _mm256_subs_epi16(a5_57, b5_57);

  btf_16_w16_avx2(
      pair_set_w16x2_epi16(cospi[2], cospi[62], cospi[10], cospi[54]),
      pair_set_w16x2_epi16(cospi[62], -cospi[2], cospi[54], -cospi[10]), &s7_02,
      &s7_13, r, 13);
  btf_16_w16_avx2(
      pair_set_w16x2_epi16(cospi[18], cospi[46], cospi[26], cospi[38]),
      pair_set_w16x2_epi16(cospi[46], -cospi[18], cospi[38], -cospi[26]),
      &s7_46, &s7_57, r, 13);
  btf_16_w16_avx2(
      pair_set_w16x2_epi16(cospi[34], cospi[30], cospi[42], cospi[22]),
      pair_set_w16x2_epi16(cospi[30], -cospi[34], cospi[22], -cospi[42]),
      &s7_8_10, &s7_9_11, r, 13);
  btf_16_w16_avx2(
      pair_set_w16x2_epi16(cospi[50], cospi[14], cospi[58], cospi[6]),
      pair_set_w16x2_epi16(cospi[14], -cospi[50], cospi[6], -cospi[58]),
      &s7_12_14, &s7_13_15, r, 13);

  output[0] = _mm256_permute2x128_si256(s7_13, s7_9_11, 0x20);
  output[2] = _mm256_permute2x128_si256(s7_13, s7_9_11, 0x31);
  output[4] = _mm256_permute2x128_si256(s7_57, s7_13_15, 0x20);
  output[6] = _mm256_permute2x128_si256(s7_57, s7_13_15, 0x31);
  output[3] = _mm256_permute2x128_si256(s7_12_14, s7_46, 0x20);
  output[1] = _mm256_permute2x128_si256(s7_12_14, s7_46, 0x31);
  output[7] = _mm256_permute2x128_si256(s7_8_10, s7_02, 0x20);
  output[5] = _mm256_permute2x128_si256(s7_8_10, s7_02, 0x31);
}

static inline void fidentity8x16_new_avx2(const __m256i *input, __m256i *output,
                                          int8_t cos_bit) {
  (void)cos_bit;
  const __m256i scale = _mm256_set1_epi16(2 * NewSqrt2 * 8 - 65536);
  for (int i = 0; i < 8; ++i) {
    const __m256i v = input[i];
    const __m256i t = _mm256_mulhrs_epi16(v, scale);
    output[i] = _mm256_adds_epi16(_mm256_adds_epi16(v, v), t);
  }
}

static const transform_1d_avx2 row_txfm8x16_arr[TX_TYPES] = {
  fdct8x8_new_avx2,       // DCT_DCT
  fdct8x8_new_avx2,       // ADST_DCT
  fadst8x8_new_avx2,      // DCT_ADST
  fadst8x8_new_avx2,      // ADST_ADST
  fdct8x8_new_avx2,       // FLIPADST_DCT
  fadst8x8_new_avx2,      // DCT_FLIPADST
  fadst8x8_new_avx2,      // FLIPADST_FLIPADST
  fadst8x8_new_avx2,      // ADST_FLIPADST
  fadst8x8_new_avx2,      // FLIPADST_ADST
  fidentity8x8_new_avx2,  // IDTX
  fidentity8x8_new_avx2,  // V_DCT
  fdct8x8_new_avx2,       // H_DCT
  fidentity8x8_new_avx2,  // V_ADST
  fadst8x8_new_avx2,      // H_ADST
  fidentity8x8_new_avx2,  // V_FLIPADST
  fadst8x8_new_avx2       // H_FLIPADST
};

static const transform_1d_avx2 col_txfm8x16_arr[TX_TYPES] = {
  fdct8x16_new_avx2,       // DCT_DCT
  fadst8x16_new_avx2,      // ADST_DCT
  fdct8x16_new_avx2,       // DCT_ADST
  fadst8x16_new_avx2,      // ADST_ADST
  fadst8x16_new_avx2,      // FLIPADST_DCT
  fdct8x16_new_avx2,       // DCT_FLIPADST
  fadst8x16_new_avx2,      // FLIPADST_FLIPADST
  fadst8x16_new_avx2,      // ADST_FLIPADST
  fadst8x16_new_avx2,      // FLIPADST_ADST
  fidentity8x16_new_avx2,  // IDTX
  fdct8x16_new_avx2,       // V_DCT
  fidentity8x16_new_avx2,  // H_DCT
  fadst8x16_new_avx2,      // V_ADST
  fidentity8x16_new_avx2,  // H_ADST
  fadst8x16_new_avx2,      // V_FLIPADST
  fidentity8x16_new_avx2   // H_FLIPADST
};

static const transform_1d_avx2 col_txfm16x8_arr[TX_TYPES] = {
  fdct8x8_new_avx2,       // DCT_DCT
  fadst8x8_new_avx2,      // ADST_DCT
  fdct8x8_new_avx2,       // DCT_ADST
  fadst8x8_new_avx2,      // ADST_ADST
  fadst8x8_new_avx2,      // FLIPADST_DCT
  fdct8x8_new_avx2,       // DCT_FLIPADST
  fadst8x8_new_avx2,      // FLIPADST_FLIPADST
  fadst8x8_new_avx2,      // ADST_FLIPADST
  fadst8x8_new_avx2,      // FLIPADST_ADST
  fidentity8x8_new_avx2,  // IDTX
  fdct8x8_new_avx2,       // V_DCT
  fidentity8x8_new_avx2,  // H_DCT
  fadst8x8_new_avx2,      // V_ADST
  fidentity8x8_new_avx2,  // H_ADST
  fadst8x8_new_avx2,      // V_FLIPADST
  fidentity8x8_new_avx2,  // H_FLIPADST
};

static const transform_1d_avx2 row_txfm16x8_arr[TX_TYPES] = {
  fdct8x16_new_avx2,       // DCT_DCT
  fdct8x16_new_avx2,       // ADST_DCT
  fadst8x16_new_avx2,      // DCT_ADST
  fadst8x16_new_avx2,      // ADST_ADST
  fdct8x16_new_avx2,       // FLIPADST_DCT
  fadst8x16_new_avx2,      // DCT_FLIPADST
  fadst8x16_new_avx2,      // FLIPADST_FLIPADST
  fadst8x16_new_avx2,      // ADST_FLIPADST
  fadst8x16_new_avx2,      // FLIPADST_ADST
  fidentity8x16_new_avx2,  // IDTX
  fidentity8x16_new_avx2,  // V_DCT
  fdct8x16_new_avx2,       // H_DCT
  fidentity8x16_new_avx2,  // V_ADST
  fadst8x16_new_avx2,      // H_ADST
  fidentity8x16_new_avx2,  // V_FLIPADST
  fadst8x16_new_avx2       // H_FLIPADST
};

static AOM_FORCE_INLINE void load_8x16_dual_sll2_avx2(const int16_t *in,
                                                      int stride,
                                                      __m256i *out) {
  for (int i = 0; i < 8; ++i) {
    const __m128i lo = _mm_loadu_si128((const __m128i *)(in + i * stride));
    const __m128i hi =
        _mm_loadu_si128((const __m128i *)(in + (i + 8) * stride));
    out[i] = _mm256_slli_epi16(
        _mm256_inserti128_si256(_mm256_castsi128_si256(lo), hi, 1), 2);
  }
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void lowbd_fwd_txfm2d_8x16_non_dct_avx2(const int16_t *input,
                                               int32_t *output, int stride,
                                               TX_TYPE tx_type, int bd) {
  (void)bd;
  __m256i buf0[8], buf1[8];
  const transform_1d_avx2 col_txfm = col_txfm8x16_arr[tx_type];
  const transform_1d_avx2 row_txfm = row_txfm8x16_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int16_t *in_ptr = ud_flip ? (input + 15 * stride) : input;
  const int in_stride = ud_flip ? -stride : stride;
  load_8x16_dual_sll2_avx2(in_ptr, in_stride, buf0);
  col_txfm(buf0, buf0, 13);
  round_shift_16bit_w16_avx2(buf0, 8, -2);
  transpose2_8x8_avx2(buf0, buf1);

  __m256i *buf;
  if (UNLIKELY(lr_flip)) {
    buf = buf0;
    flip_buf_avx2(buf1, buf, 8);
  } else {
    buf = buf1;
  }
  row_txfm(buf, buf, 13);
  store_rect_buffer_16bit_to_32bit_w16_avx2(buf, output, 16, 8);
}

static void lowbd_fwd_txfm2d_8x16_avx2(const int16_t *input, int32_t *output,
                                       int stride, TX_TYPE tx_type, int bd) {
  if (UNLIKELY(tx_type != DCT_DCT)) {
    lowbd_fwd_txfm2d_8x16_non_dct_avx2(input, output, stride, tx_type, bd);
    return;
  }
  (void)bd;
  __m256i buf0[8], buf1[8];
#define LOAD_ROW_PAIR_SLL2(r_lo, r_hi)                                      \
  _mm256_slli_epi16(                                                        \
      _mm256_inserti128_si256(                                              \
          _mm256_castsi128_si256(                                           \
              _mm_loadu_si128((const __m128i *)(input + (r_lo) * stride))), \
          _mm_loadu_si128((const __m128i *)(input + (r_hi) * stride)), 1),  \
      2)
  const __m256i x_0_1 = LOAD_ROW_PAIR_SLL2(0, 1);
  const __m256i x_15_14 = LOAD_ROW_PAIR_SLL2(15, 14);
  const __m256i x_3_2 = LOAD_ROW_PAIR_SLL2(3, 2);
  const __m256i x_12_13 = LOAD_ROW_PAIR_SLL2(12, 13);
  const __m256i x_4_5 = LOAD_ROW_PAIR_SLL2(4, 5);
  const __m256i x_11_10 = LOAD_ROW_PAIR_SLL2(11, 10);
  const __m256i x_7_6 = LOAD_ROW_PAIR_SLL2(7, 6);
  const __m256i x_8_9 = LOAD_ROW_PAIR_SLL2(8, 9);
#undef LOAD_ROW_PAIR_SLL2

  __m256i s[4];
  s[0] = _mm256_adds_epi16(x_0_1, x_15_14);
  s[1] = _mm256_adds_epi16(x_3_2, x_12_13);
  s[2] = _mm256_adds_epi16(x_4_5, x_11_10);
  s[3] = _mm256_adds_epi16(x_7_6, x_8_9);

  const __m256i d0 = _mm256_subs_epi16(x_7_6, x_8_9);
  const __m256i d1 = _mm256_subs_epi16(x_4_5, x_11_10);
  const __m256i d2 = _mm256_subs_epi16(x_3_2, x_12_13);
  const __m256i d3 = _mm256_subs_epi16(x_0_1, x_15_14);

  __m256i q_even[4], q_odd[4];
  fdct8x8_dual_avx2(s, q_even);
  fdct8x16_odd_dual_avx2(d0, d1, d2, d3, q_odd);

  const __m256i round2 = _mm256_set1_epi16(2);
  buf0[0] = _mm256_srai_epi16(
      _mm256_adds_epi16(_mm256_permute2x128_si256(q_even[0], q_even[1], 0x20),
                        round2),
      2);
  buf0[1] = _mm256_srai_epi16(_mm256_adds_epi16(q_odd[0], round2), 2);
  buf0[2] = _mm256_srai_epi16(_mm256_adds_epi16(q_even[2], round2), 2);
  buf0[3] = _mm256_srai_epi16(
      _mm256_adds_epi16(_mm256_permute4x64_epi64(q_odd[3], 0x4e), round2), 2);
  buf0[4] = _mm256_srai_epi16(
      _mm256_adds_epi16(_mm256_permute2x128_si256(q_even[0], q_even[1], 0x31),
                        round2),
      2);
  buf0[5] = _mm256_srai_epi16(_mm256_adds_epi16(q_odd[2], round2), 2);
  buf0[6] = _mm256_srai_epi16(
      _mm256_adds_epi16(_mm256_permute4x64_epi64(q_even[3], 0x4e), round2), 2);
  buf0[7] = _mm256_srai_epi16(
      _mm256_adds_epi16(_mm256_permute4x64_epi64(q_odd[1], 0x4e), round2), 2);

  transpose2_8x8_avx2(buf0, buf1);
  fdct8x8_new_avx2(buf1, buf1, 13);
  store_rect_buffer_16bit_to_32bit_w16_avx2(buf1, output, 16, 8);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void lowbd_fwd_txfm2d_16x8_non_dct_avx2(const int16_t *input,
                                               int32_t *output, int stride,
                                               TX_TYPE tx_type, int bd) {
  (void)bd;
  __m256i buf0[8], buf1[8];
  const transform_1d_avx2 col_txfm = col_txfm16x8_arr[tx_type];
  const transform_1d_avx2 row_txfm = row_txfm16x8_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int16_t *in_ptr = ud_flip ? (input + 7 * stride) : input;
  const int in_stride = ud_flip ? -stride : stride;
  load_buffer_16bit_to_16bit_sll_avx2(in_ptr, in_stride, buf0, 8, 2);
  col_txfm(buf0, buf0, 13);
  round_shift_16bit_w16_avx2(buf0, 8, -2);
  transpose2_8x8_avx2(buf0, buf1);

  __m256i *buf;
  if (UNLIKELY(lr_flip)) {
    buf = buf0;
    for (int i = 0; i < 8; ++i) {
      buf[i] = _mm256_permute4x64_epi64(buf1[7 - i], 0x4e);
    }
  } else {
    buf = buf1;
  }
  row_txfm(buf, buf, 13);
  for (int i = 0; i < 8; ++i) {
    store_rect_row_pair_16bit_to_32bit_avx2(buf[i], output, i, i + 8);
  }
}

static void lowbd_fwd_txfm2d_16x8_avx2(const int16_t *input, int32_t *output,
                                       int stride, TX_TYPE tx_type, int bd) {
  if (UNLIKELY(tx_type != DCT_DCT)) {
    lowbd_fwd_txfm2d_16x8_non_dct_avx2(input, output, stride, tx_type, bd);
    return;
  }
  (void)bd;
  __m256i buf0[8], buf1[8];
  load_buffer_16bit_to_16bit_sll_avx2(input, stride, buf0, 8, 2);
  fdct8x8_new_avx2(buf0, buf0, 13);
  round_shift_16bit_w16_avx2(buf0, 8, -2);
  transpose2_8x8_avx2(buf0, buf1);

  const __m256i x_0_1 = _mm256_permute2x128_si256(buf1[0], buf1[1], 0x20);
  const __m256i x_8_9 = _mm256_permute2x128_si256(buf1[0], buf1[1], 0x31);
  const __m256i x_7_6 = _mm256_permute2x128_si256(buf1[7], buf1[6], 0x20);
  const __m256i x_15_14 = _mm256_permute2x128_si256(buf1[7], buf1[6], 0x31);
  const __m256i x_3_2 = _mm256_permute2x128_si256(buf1[3], buf1[2], 0x20);
  const __m256i x_11_10 = _mm256_permute2x128_si256(buf1[3], buf1[2], 0x31);
  const __m256i x_4_5 = _mm256_permute2x128_si256(buf1[4], buf1[5], 0x20);
  const __m256i x_12_13 = _mm256_permute2x128_si256(buf1[4], buf1[5], 0x31);

  __m256i s[4];
  s[0] = _mm256_adds_epi16(x_0_1, x_15_14);
  s[1] = _mm256_adds_epi16(x_3_2, x_12_13);
  s[2] = _mm256_adds_epi16(x_4_5, x_11_10);
  s[3] = _mm256_adds_epi16(x_7_6, x_8_9);

  const __m256i d0 = _mm256_subs_epi16(x_7_6, x_8_9);
  const __m256i d1 = _mm256_subs_epi16(x_4_5, x_11_10);
  const __m256i d2 = _mm256_subs_epi16(x_3_2, x_12_13);
  const __m256i d3 = _mm256_subs_epi16(x_0_1, x_15_14);

  __m256i q_even[4], q_odd[4];
  fdct8x8_dual_avx2(s, q_even);
  fdct8x16_odd_dual_avx2(d0, d1, d2, d3, q_odd);

  store_rect_row_pair_16bit_to_32bit_avx2(q_even[0], output, 0, 4);
  store_rect_row_pair_16bit_to_32bit_avx2(q_even[1], output, 8, 12);
  store_rect_row_pair_16bit_to_32bit_avx2(q_even[2], output, 2, 10);
  store_rect_row_pair_16bit_to_32bit_avx2(q_even[3], output, 14, 6);
  store_rect_row_pair_16bit_to_32bit_avx2(q_odd[0], output, 1, 9);
  store_rect_row_pair_16bit_to_32bit_avx2(q_odd[1], output, 15, 7);
  store_rect_row_pair_16bit_to_32bit_avx2(q_odd[2], output, 5, 13);
  store_rect_row_pair_16bit_to_32bit_avx2(q_odd[3], output, 11, 3);
}

static FwdTxfm2dFunc fwd_txfm2d_func_ls[TX_SIZES_ALL] = {
  av1_lowbd_fwd_txfm2d_4x4_sse2,   // 4x4 transform
  av1_lowbd_fwd_txfm2d_8x8_avx2,   // 8x8 transform
  lowbd_fwd_txfm2d_16x16_avx2,     // 16x16 transform
  lowbd_fwd_txfm2d_32x32_avx2,     // 32x32 transform
  lowbd_fwd_txfm2d_64x64_avx2,     // 64x64 transform
  av1_lowbd_fwd_txfm2d_4x8_sse2,   // 4x8 transform
  av1_lowbd_fwd_txfm2d_8x4_sse2,   // 8x4 transform
  lowbd_fwd_txfm2d_8x16_avx2,      // 8x16 transform
  lowbd_fwd_txfm2d_16x8_avx2,      // 16x8 transform
  lowbd_fwd_txfm2d_16x32_avx2,     // 16x32 transform
  lowbd_fwd_txfm2d_32x16_avx2,     // 32x16 transform
  lowbd_fwd_txfm2d_32x64_avx2,     // 32x64 transform
  lowbd_fwd_txfm2d_64x32_avx2,     // 64x32 transform
  av1_lowbd_fwd_txfm2d_4x16_sse2,  // 4x16 transform
  av1_lowbd_fwd_txfm2d_16x4_sse2,  // 16x4 transform
  av1_lowbd_fwd_txfm2d_8x32_sse2,  // 8x32 transform
  av1_lowbd_fwd_txfm2d_32x8_sse2,  // 32x8 transform
  lowbd_fwd_txfm2d_16x64_avx2,     // 16x64 transform
  lowbd_fwd_txfm2d_64x16_avx2,     // 64x16 transform
};

void av1_lowbd_fwd_txfm_avx2(const int16_t *src_diff, tran_low_t *coeff,
                             int diff_stride, TxfmParam *txfm_param) {
  FwdTxfm2dFunc fwd_txfm2d_func = fwd_txfm2d_func_ls[txfm_param->tx_size];
  if (txfm_param->lossless && txfm_param->tx_size == TX_4X4) {
    av1_lowbd_fwd_txfm_c(src_diff, coeff, diff_stride, txfm_param);
  } else {
    fwd_txfm2d_func(src_diff, coeff, diff_stride, txfm_param->tx_type,
                    txfm_param->bd);
  }
}
