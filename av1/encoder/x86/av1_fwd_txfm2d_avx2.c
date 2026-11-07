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

#include <assert.h>
#include <string.h>

#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "av1/common/av1_inv_txfm2d.h"
#include "av1/common/enums.h"
#include "av1/common/av1_txfm.h"
#include "av1/encoder/hybrid_fwd_txfm.h"
#include "av1/encoder/x86/av1_fwd_txfm_avx2.h"
#include "av1/common/x86/av1_txfm_sse2.h"
#include "av1/encoder/av1_fwd_txfm1d_cfg.h"
#include "av1/encoder/x86/av1_txfm1d_sse4.h"
#include "av1/encoder/x86/av1_fwd_txfm_sse2.h"
#include "aom_dsp/x86/txfm_common_avx2.h"

static AOM_FORCE_INLINE void btf_c32_mp_pp_avx2(__m256i *in0, __m256i *in1,
                                                const __m256i c32_q15) {
  const __m256i a = *in0;
  const __m256i b = *in1;
  *in0 = _mm256_mulhrs_epi16(_mm256_sub_epi16(b, a), c32_q15);
  *in1 = _mm256_mulhrs_epi16(_mm256_add_epi16(a, b), c32_q15);
}

static AOM_FORCE_INLINE void btf_c32_pp_pm_avx2(__m256i *in0, __m256i *in1,
                                                const __m256i c32_q15) {
  const __m256i a = *in0;
  const __m256i b = *in1;
  *in0 = _mm256_mulhrs_epi16(_mm256_add_epi16(a, b), c32_q15);
  *in1 = _mm256_mulhrs_epi16(_mm256_sub_epi16(a, b), c32_q15);
}

static AOM_FORCE_INLINE void store_row_16bit_to_32bit_w16_avx2(const __m256i v,
                                                               int32_t *out) {
  _mm256_store_si256((__m256i *)out,
                     _mm256_cvtepi16_epi32(_mm256_castsi256_si128(v)));
  _mm256_store_si256((__m256i *)(out + 8),
                     _mm256_cvtepi16_epi32(_mm256_extracti128_si256(v, 1)));
}

static AOM_FORCE_INLINE void fdct16x16_new_avx2(const __m256i *input,
                                                __m256i *output,
                                                int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
  const __m256i c32_q15 =
      _mm256_set1_epi16((int16_t)(cospi[32] << (15 - cos_bit)));

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
  btf_c32_mp_pp_avx2(&x0_5, &x0_6, c32_q15);

  if (cos_bit == 13) {
    // All cos_bit == 13 callers satisfy |x0_0 +/- x0_1| <= 32640 <= 32767.
    btf_c32_pp_pm_avx2(&x0_0, &x0_1, c32_q15);
  } else {
    btf_16_w16_avx2(pair_set_w16_epi16(cospi[32], cospi[32]),
                    pair_set_w16_epi16(cospi[32], -cospi[32]), &x0_0, &x0_1, _r,
                    cos_bit);
  }
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
  btf_c32_mp_pp_avx2(&x1_2, &x1_5, c32_q15);
  btf_c32_mp_pp_avx2(&x1_3, &x1_4, c32_q15);

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

static AOM_FORCE_INLINE void fdct16x32_core_avx2(const __m256i *input,
                                                 __m256i *output,
                                                 int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m256i c32_q15 =
      _mm256_set1_epi16((int16_t)(cospi[32] << (15 - cos_bit)));

  __m256i x_eo[8];
  __m256i x0, x1, x2, x3, x4, x5, x6, x7;

  // Stage 1 + Stage 2 (even half): stage x_odd[0..15] directly into the 16
  // odd slots of output (alias-safe when input == output because each k writes
  // only the two odd indices inside {k, 15-k, 16+k, 31-k} after reading all 4).
#define ODD_SLOT_LO(k) (((k) & 1) ? (k) : (15 - (k)))
#define ODD_SLOT_HI(k) (((k) & 1) ? (16 + (k)) : (31 - (k)))
#define STAGE12_PAIR(k, dst_even)                       \
  do {                                                  \
    const __m256i a0 = input[k];                        \
    const __m256i b0 = input[31 - (k)];                 \
    const __m256i a1 = input[15 - (k)];                 \
    const __m256i b1 = input[16 + (k)];                 \
    const __m256i s0 = _mm256_adds_epi16(a0, b0);       \
    const __m256i s1 = _mm256_adds_epi16(a1, b1);       \
    output[ODD_SLOT_HI(k)] = _mm256_subs_epi16(a0, b0); \
    output[ODD_SLOT_LO(k)] = _mm256_subs_epi16(a1, b1); \
    dst_even = _mm256_adds_epi16(s0, s1);               \
    x_eo[7 - (k)] = _mm256_subs_epi16(s0, s1);          \
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
    btf_c32_mp_pp_avx2(&x5, &x6, c32_q15);

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

    btf_c32_mp_pp_avx2(&x10, &x13, c32_q15);
    btf_c32_mp_pp_avx2(&x11, &x12, c32_q15);

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

  // Sub-problem C: x1[16..31] (x_odd[k] == output[ODD_SLOT_LO(k)],
  // x_odd[15-k] == output[ODD_SLOT_HI(k)])
  {
    const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
    __m256i x20 = output[ODD_SLOT_LO(4)];
    __m256i x21 = output[ODD_SLOT_LO(5)];
    __m256i x22 = output[ODD_SLOT_LO(6)];
    __m256i x23 = output[ODD_SLOT_LO(7)];
    __m256i x24 = output[ODD_SLOT_HI(7)];
    __m256i x25 = output[ODD_SLOT_HI(6)];
    __m256i x26 = output[ODD_SLOT_HI(5)];
    __m256i x27 = output[ODD_SLOT_HI(4)];

    btf_c32_mp_pp_avx2(&x20, &x27, c32_q15);
    btf_c32_mp_pp_avx2(&x21, &x26, c32_q15);
    btf_c32_mp_pp_avx2(&x22, &x25, c32_q15);
    btf_c32_mp_pp_avx2(&x23, &x24, c32_q15);

    // Stage 3 (odd half): replace x20..x27 in-place with x16..x19, x28..x31
    const __m256i o16 = output[ODD_SLOT_LO(0)];
    x_eo[3] = _mm256_subs_epi16(o16, x23);
    __m256i x16 = _mm256_adds_epi16(o16, x23);
    const __m256i o17 = output[ODD_SLOT_LO(1)];
    x_eo[2] = _mm256_subs_epi16(o17, x22);
    __m256i x17 = _mm256_adds_epi16(o17, x22);
    const __m256i o18 = output[ODD_SLOT_LO(2)];
    x_eo[1] = _mm256_subs_epi16(o18, x21);
    __m256i x18 = _mm256_adds_epi16(o18, x21);
    const __m256i o19 = output[ODD_SLOT_LO(3)];
    x_eo[0] = _mm256_subs_epi16(o19, x20);
    __m256i x19 = _mm256_adds_epi16(o19, x20);
    const __m256i o31 = output[ODD_SLOT_HI(0)];
    x_eo[4] = _mm256_subs_epi16(o31, x24);
    __m256i x31 = _mm256_adds_epi16(o31, x24);
    const __m256i o30 = output[ODD_SLOT_HI(1)];
    x_eo[5] = _mm256_subs_epi16(o30, x25);
    __m256i x30 = _mm256_adds_epi16(o30, x25);
    const __m256i o29 = output[ODD_SLOT_HI(2)];
    x_eo[6] = _mm256_subs_epi16(o29, x26);
    __m256i x29 = _mm256_adds_epi16(o29, x26);
    const __m256i o28 = output[ODD_SLOT_HI(3)];
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
#undef ODD_SLOT_LO
#undef ODD_SLOT_HI
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void fdct16x32_cb12_avx2(const __m256i *input, __m256i *output) {
  fdct16x32_core_avx2(input, output, 12);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void fdct16x32_cb13_avx2(const __m256i *input, __m256i *output) {
  fdct16x32_core_avx2(input, output, 13);
}

static void fdct16x32_avx2(const __m256i *input, __m256i *output,
                           int8_t cos_bit) {
  if (cos_bit == 13) {
    fdct16x32_cb13_avx2(input, output);
  } else {
    assert(cos_bit == 12);
    fdct16x32_cb12_avx2(input, output);
  }
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void fdct16x32_col_sll2_avx2(const int16_t *in, int stride,
                                    __m256i *output) {
  const int8_t cos_bit = 12;
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m256i c32_q15 = _mm256_set1_epi16((int16_t)(cospi[32] << 3));

  __m256i x_eo[8];
  __m256i x0, x1, x2, x3, x4, x5, x6, x7;

#define LOAD_SLL2_ROW(r) \
  _mm256_slli_epi16(load_16bit_to_16bit_avx2(in + (r) * stride), 2)
#define STAGE12_LOAD(k, dst_even)                           \
  do {                                                      \
    const __m256i a0 = LOAD_SLL2_ROW(k);                    \
    const __m256i b0 = LOAD_SLL2_ROW(31 - (k));             \
    const __m256i a1 = LOAD_SLL2_ROW(15 - (k));             \
    const __m256i b1 = LOAD_SLL2_ROW(16 + (k));             \
    const __m256i s0 = _mm256_adds_epi16(a0, b0);           \
    const __m256i s1 = _mm256_adds_epi16(a1, b1);           \
    output[2 * (15 - (k)) + 1] = _mm256_subs_epi16(a0, b0); \
    output[2 * (k) + 1] = _mm256_subs_epi16(a1, b1);        \
    dst_even = _mm256_adds_epi16(s0, s1);                   \
    x_eo[7 - (k)] = _mm256_subs_epi16(s0, s1);              \
  } while (0)

  STAGE12_LOAD(0, x0);
  STAGE12_LOAD(1, x1);
  STAGE12_LOAD(2, x2);
  STAGE12_LOAD(3, x3);
  STAGE12_LOAD(4, x4);
  STAGE12_LOAD(5, x5);
  STAGE12_LOAD(6, x6);
  STAGE12_LOAD(7, x7);
#undef STAGE12_LOAD
#undef LOAD_SLL2_ROW

  {
    const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
    btf_16_adds_subs_avx2(&x0, &x7);
    btf_16_adds_subs_avx2(&x1, &x6);
    btf_16_adds_subs_avx2(&x2, &x5);
    btf_16_adds_subs_avx2(&x3, &x4);

    btf_16_adds_subs_avx2(&x0, &x3);
    btf_16_adds_subs_avx2(&x1, &x2);
    btf_c32_mp_pp_avx2(&x5, &x6, c32_q15);
    // In column pass |x0 +/- x1| <= 32 * 1020 = 32640 <= 32767 (exact).
    btf_c32_pp_pm_avx2(&x0, &x1, c32_q15);
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

  {
    const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
    __m256i x8 = x_eo[0], x9 = x_eo[1], x10 = x_eo[2], x11 = x_eo[3];
    __m256i x12 = x_eo[4], x13 = x_eo[5], x14 = x_eo[6], x15 = x_eo[7];

    btf_c32_mp_pp_avx2(&x10, &x13, c32_q15);
    btf_c32_mp_pp_avx2(&x11, &x12, c32_q15);

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

  {
    const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
    __m256i x20 = output[9], x21 = output[11], x22 = output[13],
            x23 = output[15];
    __m256i x24 = output[17], x25 = output[19], x26 = output[21],
            x27 = output[23];

    btf_c32_mp_pp_avx2(&x20, &x27, c32_q15);
    btf_c32_mp_pp_avx2(&x21, &x26, c32_q15);
    btf_c32_mp_pp_avx2(&x22, &x25, c32_q15);
    btf_c32_mp_pp_avx2(&x23, &x24, c32_q15);

    const __m256i o16 = output[1];
    x_eo[3] = _mm256_subs_epi16(o16, x23);
    __m256i x16 = _mm256_adds_epi16(o16, x23);
    const __m256i o17 = output[3];
    x_eo[2] = _mm256_subs_epi16(o17, x22);
    __m256i x17 = _mm256_adds_epi16(o17, x22);
    const __m256i o18 = output[5];
    x_eo[1] = _mm256_subs_epi16(o18, x21);
    __m256i x18 = _mm256_adds_epi16(o18, x21);
    const __m256i o19 = output[7];
    x_eo[0] = _mm256_subs_epi16(o19, x20);
    __m256i x19 = _mm256_adds_epi16(o19, x20);
    const __m256i o31 = output[31];
    x_eo[4] = _mm256_subs_epi16(o31, x24);
    __m256i x31 = _mm256_adds_epi16(o31, x24);
    const __m256i o30 = output[29];
    x_eo[5] = _mm256_subs_epi16(o30, x25);
    __m256i x30 = _mm256_adds_epi16(o30, x25);
    const __m256i o29 = output[27];
    x_eo[6] = _mm256_subs_epi16(o29, x26);
    __m256i x29 = _mm256_adds_epi16(o29, x26);
    const __m256i o28 = output[25];
    x_eo[7] = _mm256_subs_epi16(o28, x27);
    __m256i x28 = _mm256_adds_epi16(o28, x27);

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

  {
    const __m256i _r = _mm256_set1_epi32(1 << (cos_bit - 1));
    __m256i x20 = x_eo[0], x21 = x_eo[1], x22 = x_eo[2], x23 = x_eo[3];
    __m256i x24 = x_eo[4], x25 = x_eo[5], x26 = x_eo[6], x27 = x_eo[7];

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

static AOM_FORCE_INLINE void fdct16x16_load_sll_avx2(const int16_t *input,
                                                     int stride,
                                                     __m256i *output,
                                                     int8_t cos_bit,
                                                     int sll_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m256i c32_q15 =
      _mm256_set1_epi16((int16_t)(cospi[32] << (15 - cos_bit)));

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
    btf_c32_mp_pp_avx2(&x0_5, &x0_6, c32_q15);

    btf_c32_pp_pm_avx2(&x0_0, &x0_1, c32_q15);
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
    btf_c32_mp_pp_avx2(&x1_2, &x1_5, c32_q15);
    btf_c32_mp_pp_avx2(&x1_3, &x1_4, c32_q15);

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
    const __m256i c32_q15 = _mm256_set1_epi16((int16_t)(cospi[32] << 3));
    btf_16_adds_subs_avx2(&x0_0, &x0_7);
    btf_16_adds_subs_avx2(&x0_1, &x0_6);
    btf_16_adds_subs_avx2(&x0_2, &x0_5);
    btf_16_adds_subs_avx2(&x0_3, &x0_4);

    btf_16_adds_subs_avx2(&x0_0, &x0_3);
    btf_16_adds_subs_avx2(&x0_1, &x0_2);
    btf_c32_mp_pp_avx2(&x0_5, &x0_6, c32_q15);

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
    const __m256i c32_q15 = _mm256_set1_epi16((int16_t)(cospi[32] << 3));
    __m256i x1_0 = x1[0], x1_1 = x1[1], x1_2 = x1[2], x1_3 = x1[3];
    __m256i x1_4 = x1[4], x1_5 = x1[5], x1_6 = x1[6], x1_7 = x1[7];
    btf_c32_mp_pp_avx2(&x1_2, &x1_5, c32_q15);
    btf_c32_mp_pp_avx2(&x1_3, &x1_4, c32_q15);

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

  const __m256i c32_p =
      _mm256_set1_epi16((int16_t)(cospi[32] << (15 - cos_bit)));
  const __m256i c32_n =
      _mm256_set1_epi16((int16_t)(-(cospi[32] << (15 - cos_bit))));

  // Step 1: Load {x6, x7, x14, x15} and run Stage 2 (cospi[32])
  const __m256i in4 = input[4], in11 = input[11];
  const __m256i in5 = input[5], in10 = input[10];
  __m256i x6 = _mm256_mulhrs_epi16(_mm256_sub_epi16(in4, in11), c32_p);
  __m256i x7 = _mm256_mulhrs_epi16(_mm256_add_epi16(in4, in11), c32_p);
  __m256i x14 = _mm256_mulhrs_epi16(_mm256_sub_epi16(in10, in5), c32_p);
  __m256i x15 = _mm256_mulhrs_epi16(_mm256_add_epi16(in5, in10), c32_n);

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
  const __m256i in7 = input[7], in8 = input[8];
  const __m256i in6 = input[6], in9 = input[9];
  __m256i x2 = _mm256_mulhrs_epi16(_mm256_sub_epi16(in8, in7), c32_p);
  __m256i x3 = _mm256_mulhrs_epi16(_mm256_add_epi16(in7, in8), c32_n);
  __m256i x10 = _mm256_mulhrs_epi16(_mm256_sub_epi16(in6, in9), c32_p);
  __m256i x11 = _mm256_mulhrs_epi16(_mm256_add_epi16(in6, in9), c32_p);

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
  const __m256i c32_p = _mm256_set1_epi16((int16_t)(cospi[32] << 2));
  const __m256i c32_n = _mm256_set1_epi16((int16_t)(-(cospi[32] << 2)));
  const __m256i in4 = LOAD_SLL2(4), in11 = LOAD_SLL2(11);
  const __m256i in5 = LOAD_SLL2(5), in10 = LOAD_SLL2(10);
  __m256i x6 = _mm256_mulhrs_epi16(_mm256_sub_epi16(in4, in11), c32_p);
  __m256i x7 = _mm256_mulhrs_epi16(_mm256_add_epi16(in4, in11), c32_p);
  __m256i x14 = _mm256_mulhrs_epi16(_mm256_sub_epi16(in10, in5), c32_p);
  __m256i x15 = _mm256_mulhrs_epi16(_mm256_add_epi16(in5, in10), c32_n);

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
  const __m256i in7 = LOAD_SLL2(7), in8 = LOAD_SLL2(8);
  const __m256i in6 = LOAD_SLL2(6), in9 = LOAD_SLL2(9);
  __m256i x2 = _mm256_mulhrs_epi16(_mm256_sub_epi16(in8, in7), c32_p);
  __m256i x3 = _mm256_mulhrs_epi16(_mm256_add_epi16(in7, in8), c32_n);
  __m256i x10 = _mm256_mulhrs_epi16(_mm256_sub_epi16(in6, in9), c32_p);
  __m256i x11 = _mm256_mulhrs_epi16(_mm256_add_epi16(in6, in9), c32_p);

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

  const __m256i c32_p =
      _mm256_set1_epi16((int16_t)(cospi[32] << (15 - cos_bit)));
  const __m256i c32_n =
      _mm256_set1_epi16((int16_t)(-(cospi[32] << (15 - cos_bit))));

#define IN_ROW(idx) input[idx]
  // Step 1: Load {x6, x7, x14, x15} and run Stage 2 (cospi[32])
  const __m256i in4 = IN_ROW(4), in11 = IN_ROW(11);
  const __m256i in5 = IN_ROW(5), in10 = IN_ROW(10);
  __m256i x6 = _mm256_mulhrs_epi16(_mm256_sub_epi16(in4, in11), c32_p);
  __m256i x7 = _mm256_mulhrs_epi16(_mm256_add_epi16(in4, in11), c32_p);
  __m256i x14 = _mm256_mulhrs_epi16(_mm256_sub_epi16(in10, in5), c32_p);
  __m256i x15 = _mm256_mulhrs_epi16(_mm256_add_epi16(in5, in10), c32_n);

  // Step 2: Load {x4, x5, x12, x13} and run Stage 3 + Stage 4 (cospi[16, 48])
  __m256i x4 = _mm256_subs_epi16(__zero, IN_ROW(3));
  __m256i x5 = IN_ROW(12);
  __m256i x12 = IN_ROW(2);
  __m256i x13 = _mm256_subs_epi16(__zero, IN_ROW(13));
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
  const __m256i in7 = IN_ROW(7), in8 = IN_ROW(8);
  const __m256i in6 = IN_ROW(6), in9 = IN_ROW(9);
  __m256i x2 = _mm256_mulhrs_epi16(_mm256_sub_epi16(in8, in7), c32_p);
  __m256i x3 = _mm256_mulhrs_epi16(_mm256_add_epi16(in7, in8), c32_n);
  __m256i x10 = _mm256_mulhrs_epi16(_mm256_sub_epi16(in6, in9), c32_p);
  __m256i x11 = _mm256_mulhrs_epi16(_mm256_add_epi16(in6, in9), c32_p);

  // Step 4: Load {x0, x8, x1, x9} and run Stage 3 + Stage 5
  __m256i x0 = IN_ROW(0);
  __m256i x8 = _mm256_subs_epi16(__zero, IN_ROW(1));
  btf_16_adds_subs_avx2(&x0, &x2);
  btf_16_adds_subs_avx2(&x8, &x10);
  btf_16_adds_subs_avx2(&x0, &x4);
  btf_16_adds_subs_avx2(&x2, &x6);
  btf_16_adds_subs_avx2(&x8, &x12);
  btf_16_adds_subs_avx2(&x10, &x14);

  __m256i x1 = _mm256_subs_epi16(__zero, IN_ROW(15));
  __m256i x9 = IN_ROW(14);
#undef IN_ROW
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
  const __m256i scale = _mm256_set1_epi16(2 * NewSqrt2 * 8 - 65536);
  for (int i = 0; i < 16; ++i) {
    const __m256i v = input[i];
    const __m256i t = _mm256_mulhrs_epi16(v, scale);
    output[i] = _mm256_adds_epi16(_mm256_adds_epi16(v, v), t);
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
  // Exact for |a| <= 23168 (global max across all 8-bit rect txfms <= 16339).
  const __m256i scale = _mm256_set1_epi16(NewSqrt2 * 8 - 32768);
  const __m256i s = _mm256_add_epi16(a, _mm256_mulhrs_epi16(a, scale));
  _mm256_store_si256((__m256i *)b,
                     _mm256_cvtepi16_epi32(_mm256_castsi256_si128(s)));
  _mm256_store_si256((__m256i *)(b + 8),
                     _mm256_cvtepi16_epi32(_mm256_extracti128_si256(s, 1)));
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

// fdct8 on mirrored input p[0..3]. Outputs (o = output row):
// q[0] = [o0 | o2], q[1] = [o4 | o6], q[2] = [o1 | o5], q[3] = [o7 | o3].
static AOM_FORCE_INLINE void fdct8x8_dual_avx2(const __m256i *p, __m256i *q) {
  const int32_t *cospi = cospi_arr(13);
  const __m256i r = _mm256_set1_epi32(1 << 12);
  const __m256i c32_q15 = _mm256_set1_epi16((int16_t)(cospi[32] << 2));
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
  const __m256i d65_sub = _mm256_sub_epi16(d76, d45);
  const __m256i d65_add = _mm256_add_epi16(d76, d45);
  const __m256i x56 = _mm256_mulhrs_epi16(
      _mm256_permute2x128_si256(d65_sub, d65_add, 0x31), c32_q15);

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
  const int16_t c32 = (int16_t)(cospi[32] << 2);
  const __m256i c32_pm = pair_set_w16x2_epi16(c32, c32, -c32, -c32);
  const __m256i c32_mp = pair_set_w16x2_epi16(-c32, -c32, c32, c32);
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

  // Fused stage 1 + stage 2 (c32_pm/c32_mp double as lane sign masks)
  const __m256i a04 = _mm256_sign_epi16(p[0], c32_pm);
  const __m256i a15 = _mm256_sign_epi16(p[3], c32_mp);
  const __m256i a26 = _mm256_mulhrs_epi16(_mm256_sub_epi16(p[2], p[1]), c32_pm);
  const __m256i a37 = _mm256_mulhrs_epi16(_mm256_add_epi16(p[1], p[2]), c32_mp);

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
  __m256i b0, b1, b2, b3;
  if (bit == 1) {
    const __m256i neg_one = _mm256_set1_epi16(-1);
    b0 = _mm256_srai_epi16(_mm256_sub_epi16(b[0], neg_one), 1);
    b1 = _mm256_srai_epi16(_mm256_sub_epi16(b[1], neg_one), 1);
    b2 = _mm256_srai_epi16(_mm256_sub_epi16(b[2], neg_one), 1);
    b3 = _mm256_srai_epi16(_mm256_sub_epi16(b[3], neg_one), 1);
  } else {
    const __m256i rounding = _mm256_set1_epi16(1 << (bit - 1));
    b0 = _mm256_srai_epi16(_mm256_adds_epi16(b[0], rounding), bit);
    b1 = _mm256_srai_epi16(_mm256_adds_epi16(b[1], rounding), bit);
    b2 = _mm256_srai_epi16(_mm256_adds_epi16(b[2], rounding), bit);
    b3 = _mm256_srai_epi16(_mm256_adds_epi16(b[3], rounding), bit);
  }
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
    case FLIPADST_1D: {
      __m256i pf[4];
      const __m256i lr_mask = _mm256_set1_epi32(-(int)(type == FLIPADST_1D));
      const __m256i t03 =
          _mm256_and_si256(_mm256_xor_si256(p[0], p[3]), lr_mask);
      const __m256i t12 =
          _mm256_and_si256(_mm256_xor_si256(p[1], p[2]), lr_mask);
      pf[0] = _mm256_xor_si256(p[0], t03);
      pf[1] = _mm256_xor_si256(p[1], t12);
      pf[2] = _mm256_xor_si256(p[2], t12);
      pf[3] = _mm256_xor_si256(p[3], t03);
      fadst8x8_dual_avx2(pf, q);
      store_row_pair_16bit_to_32bit_avx2(output, q[0], 0, 2);
      store_row_pair_16bit_to_32bit_avx2(output, q[1], 4, 6);
      store_row_pair_16bit_to_32bit_avx2(output, q[2], 3, 1);
      store_row_pair_16bit_to_32bit_avx2(output, q[3], 7, 5);
      break;
    }
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
  const int16_t *in_ptr = input;
  int in_stride = stride;
  if (UNLIKELY((0x4150 >> tx_type) & 1)) {
    in_ptr = input + 7 * stride;
    in_stride = -stride;
  }
  assert(shift[0] > 0);
  __m256i p[4], b[4];
  load_8x8_dual_sll_avx2(in_ptr, in_stride, shift[0], p);
  fwd_txfm8x8_col_dual_avx2(vtx_tab[tx_type], p, b);
  assert(shift[1] < 0);
  round_shift_transpose_8x8_dual_avx2(b, -shift[1], p);
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

#define LOADL_RS(idx)                                                          \
  do {                                                                         \
    const __m256i v = _mm256_permute2x128_si256(in[idx], in[(idx) + 8], 0x20); \
    t[idx] = _mm256_srai_epi16(_mm256_adds_epi16(v, round), rshift);           \
  } while (0)

#define LOADR_RS(idx)                                                          \
  do {                                                                         \
    const __m256i v = _mm256_permute2x128_si256(in[idx], in[(idx) + 8], 0x31); \
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
  const int16_t *in_ptr = input;
  int in_stride = stride;
  if (UNLIKELY((0x4150 >> tx_type) & 1)) {
    in_ptr = input + 15 * stride;
    in_stride = -stride;
  }
  col_txfm(in_ptr, buf0, in_stride);
  transpose_round_shift_16bit_16x16_avx2(buf0, buf1, 2);
  __m256i *buf;
  if (UNLIKELY((0x80e0 >> tx_type) & 1)) {
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
#define LOADL_RS2(idx)                                               \
  do {                                                               \
    const __m256i v =                                                \
        _mm256_permute2x128_si256(buf0[idx], buf0[(idx) + 8], 0x20); \
    t[idx] = _mm256_srai_epi16(_mm256_adds_epi16(v, round), 2);      \
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
#define LOADR_RS2(idx, dst)                                          \
  do {                                                               \
    const __m256i v =                                                \
        _mm256_permute2x128_si256(buf0[idx], buf0[(idx) + 8], 0x31); \
    dst = _mm256_srai_epi16(_mm256_adds_epi16(v, round), 2);         \
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
    const __m256i c32_q15 = _mm256_set1_epi16((int16_t)(cospi12[32] << 3));
    btf_16_adds_subs_avx2(&x0_0, &x0_7);
    btf_16_adds_subs_avx2(&x0_1, &x0_6);
    btf_16_adds_subs_avx2(&x0_2, &x0_5);
    btf_16_adds_subs_avx2(&x0_3, &x0_4);

    btf_16_adds_subs_avx2(&x0_0, &x0_3);
    btf_16_adds_subs_avx2(&x0_1, &x0_2);
    btf_c32_mp_pp_avx2(&x0_5, &x0_6, c32_q15);

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
    const __m256i c32_q15 = _mm256_set1_epi16((int16_t)(cospi12[32] << 3));
    __m256i x1_0 = x1[0], x1_1 = x1[1], x1_2 = x1[2], x1_3 = x1[3];
    __m256i x1_4 = x1[4], x1_5 = x1[5], x1_6 = x1[6], x1_7 = x1[7];
    btf_c32_mp_pp_avx2(&x1_2, &x1_5, c32_q15);
    btf_c32_mp_pp_avx2(&x1_3, &x1_4, c32_q15);

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
    fdct16x32_col_sll2_avx2(input + 16 * i, stride, buf0);
    transpose_round_shift_16bit_16x16_avx2(buf0 + 0 * 16,
                                           buf1 + 0 * 32 + 16 * i, 4);
    transpose_round_shift_16bit_16x16_avx2(buf0 + 1 * 16,
                                           buf1 + 1 * 32 + 16 * i, 4);
  }
  for (int i = 0; i < 2; i++) {
    __m256i *buf = buf1 + 32 * i;
    fdct16x32_cb12_avx2(buf, buf);
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

  fdct16x32_col_sll2_avx2(input, stride, buf0);
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
  const transform_1d_avx2 row_txfm = row_txfm16x32_arr[tx_type];

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

  fdct16x32_cb13_avx2(buf1, buf1);
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
  const __m256i scale = _mm256_set1_epi16((int16_t)(NewSqrt2 * 8 - 32768));
  const __m256i s = _mm256_add_epi16(a, _mm256_mulhrs_epi16(a, scale));
  _mm256_store_si256((__m256i *)(out + 8 * row_lo),
                     _mm256_cvtepi16_epi32(_mm256_castsi256_si128(s)));
  _mm256_store_si256((__m256i *)(out + 8 * row_hi),
                     _mm256_cvtepi16_epi32(_mm256_extracti128_si256(s, 1)));
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
  const __m256i c32_q15 = _mm256_set1_epi16((int16_t)(cospi[32] << 2));
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

  btf_c32_mp_pp_avx2(&d1, &d2, c32_q15);

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

static AOM_FORCE_INLINE void fdct8x8_reg_avx2(__m256i *b0, __m256i *b1,
                                              __m256i *b2, __m256i *b3,
                                              __m256i *b4, __m256i *b5,
                                              __m256i *b6, __m256i *b7) {
  const int8_t cos_bit = 13;
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m256i rounding = _mm256_set1_epi32(1 << (cos_bit - 1));
  const __m256i c32_q15 = _mm256_set1_epi16((int16_t)(cospi[32] << 2));

  __m256i x0 = _mm256_adds_epi16(*b0, *b7);
  __m256i x7 = _mm256_subs_epi16(*b0, *b7);
  __m256i x1 = _mm256_adds_epi16(*b1, *b6);
  __m256i x6 = _mm256_subs_epi16(*b1, *b6);
  __m256i x2 = _mm256_adds_epi16(*b2, *b5);
  __m256i x5 = _mm256_subs_epi16(*b2, *b5);
  __m256i x3 = _mm256_adds_epi16(*b3, *b4);
  __m256i x4 = _mm256_subs_epi16(*b3, *b4);

  btf_16_adds_subs_avx2(&x0, &x3);
  btf_16_adds_subs_avx2(&x1, &x2);
  btf_c32_mp_pp_avx2(&x5, &x6, c32_q15);

  btf_c32_pp_pm_avx2(&x0, &x1, c32_q15);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[48], cospi[16]),
                  pair_set_w16_epi16(-cospi[16], cospi[48]), &x2, &x3, rounding,
                  cos_bit);
  btf_16_adds_subs_avx2(&x4, &x5);
  btf_16_adds_subs_avx2(&x7, &x6);

  btf_16_w16_avx2(pair_set_w16_epi16(cospi[56], cospi[8]),
                  pair_set_w16_epi16(-cospi[8], cospi[56]), &x4, &x7, rounding,
                  cos_bit);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[24], cospi[40]),
                  pair_set_w16_epi16(-cospi[40], cospi[24]), &x5, &x6, rounding,
                  cos_bit);

  *b0 = x0;
  *b1 = x4;
  *b2 = x2;
  *b3 = x6;
  *b4 = x1;
  *b5 = x5;
  *b6 = x3;
  *b7 = x7;
}

static AOM_FORCE_INLINE void fadst8x8_reg_avx2(__m256i *b0, __m256i *b1,
                                               __m256i *b2, __m256i *b3,
                                               __m256i *b4, __m256i *b5,
                                               __m256i *b6, __m256i *b7) {
  const int8_t cos_bit = 13;
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m256i zero = _mm256_setzero_si256();
  const __m256i rounding = _mm256_set1_epi32(1 << (cos_bit - 1));
  const __m256i c32_p = _mm256_set1_epi16((int16_t)(cospi[32] << 2));
  const __m256i c32_n = _mm256_set1_epi16((int16_t)(-(cospi[32] << 2)));

  const __m256i in2 = *b2, in3 = *b3, in4 = *b4, in5 = *b5;
  const __m256i x2_0 = *b0;
  const __m256i x2_1 = _mm256_subs_epi16(zero, *b7);
  const __m256i x2_2 = _mm256_mulhrs_epi16(_mm256_sub_epi16(in4, in3), c32_p);
  const __m256i x2_3 = _mm256_mulhrs_epi16(_mm256_add_epi16(in3, in4), c32_n);
  const __m256i x2_4 = _mm256_subs_epi16(zero, *b1);
  const __m256i x2_5 = *b6;
  const __m256i x2_6 = _mm256_mulhrs_epi16(_mm256_sub_epi16(in2, in5), c32_p);
  const __m256i x2_7 = _mm256_mulhrs_epi16(_mm256_add_epi16(in2, in5), c32_p);

  __m256i x3_0 = _mm256_adds_epi16(x2_0, x2_2);
  __m256i x3_2 = _mm256_subs_epi16(x2_0, x2_2);
  __m256i x3_1 = _mm256_adds_epi16(x2_1, x2_3);
  __m256i x3_3 = _mm256_subs_epi16(x2_1, x2_3);
  __m256i x3_4 = _mm256_adds_epi16(x2_4, x2_6);
  __m256i x3_6 = _mm256_subs_epi16(x2_4, x2_6);
  __m256i x3_5 = _mm256_adds_epi16(x2_5, x2_7);
  __m256i x3_7 = _mm256_subs_epi16(x2_5, x2_7);

  {
    const __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
    btf_16_w16_avx2(cospi_p16_p48, pair_set_w16_epi16(cospi[48], -cospi[16]),
                    &x3_4, &x3_5, rounding, cos_bit);
    btf_16_w16_avx2(pair_set_w16_epi16(-cospi[48], cospi[16]), cospi_p16_p48,
                    &x3_6, &x3_7, rounding, cos_bit);
  }

  __m256i x5_0 = _mm256_adds_epi16(x3_0, x3_4);
  __m256i x5_4 = _mm256_subs_epi16(x3_0, x3_4);
  __m256i x5_1 = _mm256_adds_epi16(x3_1, x3_5);
  __m256i x5_5 = _mm256_subs_epi16(x3_1, x3_5);
  __m256i x5_2 = _mm256_adds_epi16(x3_2, x3_6);
  __m256i x5_6 = _mm256_subs_epi16(x3_2, x3_6);
  __m256i x5_3 = _mm256_adds_epi16(x3_3, x3_7);
  __m256i x5_7 = _mm256_subs_epi16(x3_3, x3_7);

  btf_16_w16_avx2(pair_set_w16_epi16(cospi[4], cospi[60]),
                  pair_set_w16_epi16(cospi[60], -cospi[4]), &x5_0, &x5_1,
                  rounding, cos_bit);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[20], cospi[44]),
                  pair_set_w16_epi16(cospi[44], -cospi[20]), &x5_2, &x5_3,
                  rounding, cos_bit);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[36], cospi[28]),
                  pair_set_w16_epi16(cospi[28], -cospi[36]), &x5_4, &x5_5,
                  rounding, cos_bit);
  btf_16_w16_avx2(pair_set_w16_epi16(cospi[52], cospi[12]),
                  pair_set_w16_epi16(cospi[12], -cospi[52]), &x5_6, &x5_7,
                  rounding, cos_bit);

  *b0 = x5_1;
  *b1 = x5_6;
  *b2 = x5_3;
  *b3 = x5_4;
  *b4 = x5_5;
  *b5 = x5_2;
  *b6 = x5_7;
  *b7 = x5_0;
}

static AOM_FORCE_INLINE void transpose2_8x8_ssa_avx2(__m256i *b0, __m256i *b1,
                                                     __m256i *b2, __m256i *b3,
                                                     __m256i *b4, __m256i *b5,
                                                     __m256i *b6, __m256i *b7) {
  const __m256i t0 = _mm256_unpacklo_epi16(*b0, *b1);
  const __m256i t1 = _mm256_unpackhi_epi16(*b0, *b1);
  const __m256i t2 = _mm256_unpacklo_epi16(*b2, *b3);
  const __m256i t3 = _mm256_unpackhi_epi16(*b2, *b3);
  const __m256i t4 = _mm256_unpacklo_epi16(*b4, *b5);
  const __m256i t5 = _mm256_unpackhi_epi16(*b4, *b5);
  const __m256i t6 = _mm256_unpacklo_epi16(*b6, *b7);
  const __m256i t7 = _mm256_unpackhi_epi16(*b6, *b7);

  const __m256i u0 = _mm256_unpacklo_epi32(t0, t2);
  const __m256i u2 = _mm256_unpackhi_epi32(t0, t2);
  const __m256i u1 = _mm256_unpacklo_epi32(t1, t3);
  const __m256i u3 = _mm256_unpackhi_epi32(t1, t3);
  const __m256i u4 = _mm256_unpacklo_epi32(t4, t6);
  const __m256i u6 = _mm256_unpackhi_epi32(t4, t6);
  const __m256i u5 = _mm256_unpacklo_epi32(t5, t7);
  const __m256i u7 = _mm256_unpackhi_epi32(t5, t7);

  *b0 = _mm256_unpacklo_epi64(u0, u4);
  *b1 = _mm256_unpackhi_epi64(u0, u4);
  *b2 = _mm256_unpacklo_epi64(u2, u6);
  *b3 = _mm256_unpackhi_epi64(u2, u6);
  *b4 = _mm256_unpacklo_epi64(u1, u5);
  *b5 = _mm256_unpackhi_epi64(u1, u5);
  *b6 = _mm256_unpacklo_epi64(u3, u7);
  *b7 = _mm256_unpackhi_epi64(u3, u7);
}

static AOM_FORCE_INLINE void fadst8x16_stages2_8_avx2(
    __m256i g0, __m256i g1, __m256i g2, __m256i g3, __m256i h0, __m256i h1,
    __m256i h2, __m256i h3, __m256i *s0, __m256i *s1, __m256i *s2, __m256i *s3,
    __m256i *s4, __m256i *s5, __m256i *s6, __m256i *s7) {
  const int32_t *cospi = cospi_arr(13);
  const __m256i r = _mm256_set1_epi32(1 << 12);
  const __m256i c32_q15 = _mm256_set1_epi16((int16_t)(cospi[32] << 2));
  const __m256i w0_s4 =
      pair_set_w16x2_epi16(cospi[16], cospi[48], -cospi[48], cospi[16]);
  const __m256i w1_s4 =
      pair_set_w16x2_epi16(cospi[48], -cospi[16], cospi[16], cospi[48]);

  btf_c32_pp_pm_avx2(&g2, &g3, c32_q15);
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

  btf_c32_pp_pm_avx2(&h2, &h3, c32_q15);
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

  {
    const __m256i w0_s6a =
        pair_set_w16x2_epi16(cospi[8], cospi[56], cospi[40], cospi[24]);
    btf_16_w16_avx2(
        w0_s6a,
        pair_set_w16x2_epi16(cospi[56], -cospi[8], cospi[24], -cospi[40]),
        &b5_02, &b5_13, r, 13);
    btf_16_w16_avx2(
        pair_set_w16x2_epi16(-cospi[56], cospi[8], -cospi[24], cospi[40]),
        w0_s6a, &b5_46, &b5_57, r, 13);
  }

  __m256i t0 = _mm256_adds_epi16(a5_02, b5_02);
  __m256i t4 = _mm256_subs_epi16(a5_02, b5_02);
  __m256i t1 = _mm256_adds_epi16(a5_13, b5_13);
  __m256i t5 = _mm256_subs_epi16(a5_13, b5_13);
  __m256i t2 = _mm256_adds_epi16(a5_46, b5_46);
  __m256i t6 = _mm256_subs_epi16(a5_46, b5_46);
  __m256i t3 = _mm256_adds_epi16(a5_57, b5_57);
  __m256i t7 = _mm256_subs_epi16(a5_57, b5_57);

  btf_16_w16_avx2(
      pair_set_w16x2_epi16(cospi[2], cospi[62], cospi[10], cospi[54]),
      pair_set_w16x2_epi16(cospi[62], -cospi[2], cospi[54], -cospi[10]), &t0,
      &t1, r, 13);
  btf_16_w16_avx2(
      pair_set_w16x2_epi16(cospi[18], cospi[46], cospi[26], cospi[38]),
      pair_set_w16x2_epi16(cospi[46], -cospi[18], cospi[38], -cospi[26]), &t2,
      &t3, r, 13);
  btf_16_w16_avx2(
      pair_set_w16x2_epi16(cospi[34], cospi[30], cospi[42], cospi[22]),
      pair_set_w16x2_epi16(cospi[30], -cospi[34], cospi[22], -cospi[42]), &t4,
      &t5, r, 13);
  btf_16_w16_avx2(
      pair_set_w16x2_epi16(cospi[50], cospi[14], cospi[58], cospi[6]),
      pair_set_w16x2_epi16(cospi[14], -cospi[50], cospi[6], -cospi[58]), &t6,
      &t7, r, 13);

  *s0 = t0;
  *s1 = t1;
  *s2 = t2;
  *s3 = t3;
  *s4 = t4;
  *s5 = t5;
  *s6 = t6;
  *s7 = t7;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void lowbd_fwd_txfm2d_8x16_non_dct_avx2(const int16_t *input,
                                               int32_t *output, int stride,
                                               TX_TYPE tx_type, int bd) {
  (void)bd;
  const int vtx = vtx_tab[tx_type];
  const int htx = htx_tab[tx_type];
  const ptrdiff_t s_in = stride;
  __m256i b0, b1, b2, b3, b4, b5, b6, b7;

  if (LIKELY(vtx == DCT_1D)) {
    const ptrdiff_t s4 = s_in * 4;
    const int16_t *r0 = input;
    const int16_t *r4 = r0 + s4;
    const int16_t *r8 = r4 + s4;
    const int16_t *r12 = r8 + s4;
    const int16_t *r2 = r0 + 2 * s_in;
    const int16_t *r6 = r4 + 2 * s_in;
    const int16_t *r10 = r8 + 2 * s_in;
    const int16_t *r14 = r12 + 2 * s_in;
#define LOAD_PAIR_SLL2(p_lo, p_hi)                                          \
  _mm256_slli_epi16(                                                        \
      _mm256_inserti128_si256(                                              \
          _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *)(p_lo))), \
          _mm_loadu_si128((const __m128i *)(p_hi)), 1),                     \
      2)
    const __m256i x_0_1 = LOAD_PAIR_SLL2(r0, r0 + s_in);
    const __m256i x_15_14 = LOAD_PAIR_SLL2(r14 + s_in, r14);
    const __m256i x_3_2 = LOAD_PAIR_SLL2(r2 + s_in, r2);
    const __m256i x_12_13 = LOAD_PAIR_SLL2(r12, r12 + s_in);
    const __m256i x_4_5 = LOAD_PAIR_SLL2(r4, r4 + s_in);
    const __m256i x_11_10 = LOAD_PAIR_SLL2(r10 + s_in, r10);
    const __m256i x_7_6 = LOAD_PAIR_SLL2(r6 + s_in, r6);
    const __m256i x_8_9 = LOAD_PAIR_SLL2(r8, r8 + s_in);
#undef LOAD_PAIR_SLL2

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
    b0 = _mm256_srai_epi16(
        _mm256_adds_epi16(_mm256_permute2x128_si256(q_even[0], q_even[1], 0x20),
                          round2),
        2);
    b1 = _mm256_srai_epi16(_mm256_adds_epi16(q_odd[0], round2), 2);
    b2 = _mm256_srai_epi16(_mm256_adds_epi16(q_even[2], round2), 2);
    b3 = _mm256_srai_epi16(
        _mm256_adds_epi16(_mm256_permute4x64_epi64(q_odd[3], 0x4e), round2), 2);
    b4 = _mm256_srai_epi16(
        _mm256_adds_epi16(_mm256_permute2x128_si256(q_even[0], q_even[1], 0x31),
                          round2),
        2);
    b5 = _mm256_srai_epi16(_mm256_adds_epi16(q_odd[2], round2), 2);
    b6 = _mm256_srai_epi16(
        _mm256_adds_epi16(_mm256_permute4x64_epi64(q_even[3], 0x4e), round2),
        2);
    b7 = _mm256_srai_epi16(
        _mm256_adds_epi16(_mm256_permute4x64_epi64(q_odd[1], 0x4e), round2), 2);
  } else if (vtx == IDTX_1D) {
    const __m256i scale =
        _mm256_set1_epi16((int16_t)(2 * NewSqrt2 * 8 - 65536));
    const __m256i round2 = _mm256_set1_epi16(2);
#define IDTX_COL_ROW(i, dst)                                                   \
  do {                                                                         \
    const __m128i lo = _mm_loadu_si128((const __m128i *)(input + (i) * s_in)); \
    const __m128i hi =                                                         \
        _mm_loadu_si128((const __m128i *)(input + ((i) + 8) * s_in));          \
    const __m256i v = _mm256_slli_epi16(                                       \
        _mm256_inserti128_si256(_mm256_castsi128_si256(lo), hi, 1), 2);        \
    const __m256i o = _mm256_adds_epi16(_mm256_adds_epi16(v, v),               \
                                        _mm256_mulhrs_epi16(v, scale));        \
    dst = _mm256_srai_epi16(_mm256_adds_epi16(o, round2), 2);                  \
  } while (0)
    IDTX_COL_ROW(0, b0);
    IDTX_COL_ROW(1, b1);
    IDTX_COL_ROW(2, b2);
    IDTX_COL_ROW(3, b3);
    IDTX_COL_ROW(4, b4);
    IDTX_COL_ROW(5, b5);
    IDTX_COL_ROW(6, b6);
    IDTX_COL_ROW(7, b7);
#undef IDTX_COL_ROW
  } else {
    const int16_t *in_ptr = (vtx == FLIPADST_1D) ? (input + 15 * s_in) : input;
    const ptrdiff_t s = (vtx == FLIPADST_1D) ? -s_in : s_in;
    const __m128i z = _mm_setzero_si128();
#define LD_PN_SLL2(ia, ib)                                               \
  _mm256_slli_epi16(                                                     \
      _mm256_inserti128_si256(                                           \
          _mm256_castsi128_si256(                                        \
              _mm_loadu_si128((const __m128i *)(in_ptr + (ia) * s))),    \
          _mm_sub_epi16(                                                 \
              z, _mm_loadu_si128((const __m128i *)(in_ptr + (ib) * s))), \
          1),                                                            \
      2)
#define LD_NP_SLL2(ia, ib)                                                \
  _mm256_slli_epi16(                                                      \
      _mm256_inserti128_si256(                                            \
          _mm256_castsi128_si256(_mm_sub_epi16(                           \
              z, _mm_loadu_si128((const __m128i *)(in_ptr + (ia) * s)))), \
          _mm_loadu_si128((const __m128i *)(in_ptr + (ib) * s)), 1),      \
      2)
    const __m256i g0 = LD_PN_SLL2(0, 3);
    const __m256i g3 = LD_PN_SLL2(8, 11);
    const __m256i g1 = LD_NP_SLL2(15, 12);
    const __m256i g2 = LD_NP_SLL2(7, 4);
    const __m256i h0 = LD_NP_SLL2(1, 2);
    const __m256i h3 = LD_NP_SLL2(9, 10);
    const __m256i h1 = LD_PN_SLL2(14, 13);
    const __m256i h2 = LD_PN_SLL2(6, 5);
#undef LD_PN_SLL2
#undef LD_NP_SLL2
    __m256i s0, s1, s2, s3, s4, s5, s6, s7;
    fadst8x16_stages2_8_avx2(g0, g1, g2, g3, h0, h1, h2, h3, &s0, &s1, &s2, &s3,
                             &s4, &s5, &s6, &s7);
    const __m256i round2 = _mm256_set1_epi16(2);
    s0 = _mm256_srai_epi16(_mm256_adds_epi16(s0, round2), 2);
    s1 = _mm256_srai_epi16(_mm256_adds_epi16(s1, round2), 2);
    s2 = _mm256_srai_epi16(_mm256_adds_epi16(s2, round2), 2);
    s3 = _mm256_srai_epi16(_mm256_adds_epi16(s3, round2), 2);
    s4 = _mm256_srai_epi16(_mm256_adds_epi16(s4, round2), 2);
    s5 = _mm256_srai_epi16(_mm256_adds_epi16(s5, round2), 2);
    s6 = _mm256_srai_epi16(_mm256_adds_epi16(s6, round2), 2);
    s7 = _mm256_srai_epi16(_mm256_adds_epi16(s7, round2), 2);
    b0 = _mm256_permute2x128_si256(s1, s5, 0x20);
    b2 = _mm256_permute2x128_si256(s1, s5, 0x31);
    b4 = _mm256_permute2x128_si256(s3, s7, 0x20);
    b6 = _mm256_permute2x128_si256(s3, s7, 0x31);
    b3 = _mm256_permute2x128_si256(s6, s2, 0x20);
    b1 = _mm256_permute2x128_si256(s6, s2, 0x31);
    b7 = _mm256_permute2x128_si256(s4, s0, 0x20);
    b5 = _mm256_permute2x128_si256(s4, s0, 0x31);
  }

  transpose2_8x8_ssa_avx2(&b0, &b1, &b2, &b3, &b4, &b5, &b6, &b7);

  if (LIKELY(htx == DCT_1D)) {
    fdct8x8_reg_avx2(&b0, &b1, &b2, &b3, &b4, &b5, &b6, &b7);
  } else if (htx == IDTX_1D) {
    b0 = _mm256_adds_epi16(b0, b0);
    b1 = _mm256_adds_epi16(b1, b1);
    b2 = _mm256_adds_epi16(b2, b2);
    b3 = _mm256_adds_epi16(b3, b3);
    b4 = _mm256_adds_epi16(b4, b4);
    b5 = _mm256_adds_epi16(b5, b5);
    b6 = _mm256_adds_epi16(b6, b6);
    b7 = _mm256_adds_epi16(b7, b7);
  } else {
    if (htx == FLIPADST_1D) {
      __m256i t;
      t = b0;
      b0 = b7;
      b7 = t;
      t = b1;
      b1 = b6;
      b6 = t;
      t = b2;
      b2 = b5;
      b5 = t;
      t = b3;
      b3 = b4;
      b4 = t;
    }
    fadst8x8_reg_avx2(&b0, &b1, &b2, &b3, &b4, &b5, &b6, &b7);
  }

  store_rect_16bit_to_32bit_avx2(b0, output + 16 * 0);
  store_rect_16bit_to_32bit_avx2(b1, output + 16 * 1);
  store_rect_16bit_to_32bit_avx2(b2, output + 16 * 2);
  store_rect_16bit_to_32bit_avx2(b3, output + 16 * 3);
  store_rect_16bit_to_32bit_avx2(b4, output + 16 * 4);
  store_rect_16bit_to_32bit_avx2(b5, output + 16 * 5);
  store_rect_16bit_to_32bit_avx2(b6, output + 16 * 6);
  store_rect_16bit_to_32bit_avx2(b7, output + 16 * 7);
}

static void lowbd_fwd_txfm2d_8x16_avx2(const int16_t *input, int32_t *output,
                                       int stride, TX_TYPE tx_type, int bd) {
  if (UNLIKELY(tx_type != DCT_DCT)) {
    lowbd_fwd_txfm2d_8x16_non_dct_avx2(input, output, stride, tx_type, bd);
    return;
  }
  (void)bd;
  __m256i buf0[8], buf1[8];
  const ptrdiff_t s4 = (ptrdiff_t)stride * 4;
  const int16_t *r0 = input;
  const int16_t *r4 = r0 + s4;
  const int16_t *r8 = r4 + s4;
  const int16_t *r12 = r8 + s4;
  const int16_t *r2 = r0 + 2 * stride;
  const int16_t *r6 = r4 + 2 * stride;
  const int16_t *r10 = r8 + 2 * stride;
  const int16_t *r14 = r12 + 2 * stride;
#define LOAD_PAIR_SLL2(p_lo, p_hi)                                          \
  _mm256_slli_epi16(                                                        \
      _mm256_inserti128_si256(                                              \
          _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *)(p_lo))), \
          _mm_loadu_si128((const __m128i *)(p_hi)), 1),                     \
      2)
  const __m256i x_0_1 = LOAD_PAIR_SLL2(r0, r0 + stride);
  const __m256i x_15_14 = LOAD_PAIR_SLL2(r14 + stride, r14);
  const __m256i x_3_2 = LOAD_PAIR_SLL2(r2 + stride, r2);
  const __m256i x_12_13 = LOAD_PAIR_SLL2(r12, r12 + stride);
  const __m256i x_4_5 = LOAD_PAIR_SLL2(r4, r4 + stride);
  const __m256i x_11_10 = LOAD_PAIR_SLL2(r10 + stride, r10);
  const __m256i x_7_6 = LOAD_PAIR_SLL2(r6 + stride, r6);
  const __m256i x_8_9 = LOAD_PAIR_SLL2(r8, r8 + stride);
#undef LOAD_PAIR_SLL2

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
  fdct8x8_reg_avx2(&buf1[0], &buf1[1], &buf1[2], &buf1[3], &buf1[4], &buf1[5],
                   &buf1[6], &buf1[7]);
  store_rect_buffer_16bit_to_32bit_w16_avx2(buf1, output, 16, 8);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void lowbd_fwd_txfm2d_16x8_non_dct_avx2(const int16_t *input,
                                               int32_t *output, int stride,
                                               TX_TYPE tx_type, int bd) {
  (void)bd;
  const int vtx = vtx_tab[tx_type];
  const int htx = htx_tab[tx_type];
  const ptrdiff_t s_in = stride;
  __m256i b0, b1, b2, b3, b4, b5, b6, b7;

#define LOADU_SLL(ptr, bit) \
  _mm256_slli_epi16(_mm256_loadu_si256((const __m256i *)(ptr)), (bit))
  if (LIKELY(vtx == DCT_1D)) {
    b0 = LOADU_SLL(input + 0 * s_in, 2);
    b1 = LOADU_SLL(input + 1 * s_in, 2);
    b2 = LOADU_SLL(input + 2 * s_in, 2);
    b3 = LOADU_SLL(input + 3 * s_in, 2);
    b4 = LOADU_SLL(input + 4 * s_in, 2);
    b5 = LOADU_SLL(input + 5 * s_in, 2);
    b6 = LOADU_SLL(input + 6 * s_in, 2);
    b7 = LOADU_SLL(input + 7 * s_in, 2);
    fdct8x8_reg_avx2(&b0, &b1, &b2, &b3, &b4, &b5, &b6, &b7);
    const __m256i round2 = _mm256_set1_epi16(2);
    b0 = _mm256_srai_epi16(_mm256_adds_epi16(b0, round2), 2);
    b1 = _mm256_srai_epi16(_mm256_adds_epi16(b1, round2), 2);
    b2 = _mm256_srai_epi16(_mm256_adds_epi16(b2, round2), 2);
    b3 = _mm256_srai_epi16(_mm256_adds_epi16(b3, round2), 2);
    b4 = _mm256_srai_epi16(_mm256_adds_epi16(b4, round2), 2);
    b5 = _mm256_srai_epi16(_mm256_adds_epi16(b5, round2), 2);
    b6 = _mm256_srai_epi16(_mm256_adds_epi16(b6, round2), 2);
    b7 = _mm256_srai_epi16(_mm256_adds_epi16(b7, round2), 2);
  } else if (vtx == IDTX_1D) {
    b0 = LOADU_SLL(input + 0 * s_in, 1);
    b1 = LOADU_SLL(input + 1 * s_in, 1);
    b2 = LOADU_SLL(input + 2 * s_in, 1);
    b3 = LOADU_SLL(input + 3 * s_in, 1);
    b4 = LOADU_SLL(input + 4 * s_in, 1);
    b5 = LOADU_SLL(input + 5 * s_in, 1);
    b6 = LOADU_SLL(input + 6 * s_in, 1);
    b7 = LOADU_SLL(input + 7 * s_in, 1);
  } else {
    const int16_t *in_ptr = (vtx == FLIPADST_1D) ? (input + 7 * s_in) : input;
    const ptrdiff_t in_s = (vtx == FLIPADST_1D) ? -s_in : s_in;
    b0 = LOADU_SLL(in_ptr + 0 * in_s, 2);
    b1 = LOADU_SLL(in_ptr + 1 * in_s, 2);
    b2 = LOADU_SLL(in_ptr + 2 * in_s, 2);
    b3 = LOADU_SLL(in_ptr + 3 * in_s, 2);
    b4 = LOADU_SLL(in_ptr + 4 * in_s, 2);
    b5 = LOADU_SLL(in_ptr + 5 * in_s, 2);
    b6 = LOADU_SLL(in_ptr + 6 * in_s, 2);
    b7 = LOADU_SLL(in_ptr + 7 * in_s, 2);
    fadst8x8_reg_avx2(&b0, &b1, &b2, &b3, &b4, &b5, &b6, &b7);
    const __m256i round2 = _mm256_set1_epi16(2);
    b0 = _mm256_srai_epi16(_mm256_adds_epi16(b0, round2), 2);
    b1 = _mm256_srai_epi16(_mm256_adds_epi16(b1, round2), 2);
    b2 = _mm256_srai_epi16(_mm256_adds_epi16(b2, round2), 2);
    b3 = _mm256_srai_epi16(_mm256_adds_epi16(b3, round2), 2);
    b4 = _mm256_srai_epi16(_mm256_adds_epi16(b4, round2), 2);
    b5 = _mm256_srai_epi16(_mm256_adds_epi16(b5, round2), 2);
    b6 = _mm256_srai_epi16(_mm256_adds_epi16(b6, round2), 2);
    b7 = _mm256_srai_epi16(_mm256_adds_epi16(b7, round2), 2);
  }
#undef LOADU_SLL

  transpose2_8x8_ssa_avx2(&b0, &b1, &b2, &b3, &b4, &b5, &b6, &b7);

  if (LIKELY(htx == DCT_1D)) {
    const __m256i x_0_1 = _mm256_permute2x128_si256(b0, b1, 0x20);
    const __m256i x_8_9 = _mm256_permute2x128_si256(b0, b1, 0x31);
    const __m256i x_7_6 = _mm256_permute2x128_si256(b7, b6, 0x20);
    const __m256i x_15_14 = _mm256_permute2x128_si256(b7, b6, 0x31);
    const __m256i x_3_2 = _mm256_permute2x128_si256(b3, b2, 0x20);
    const __m256i x_11_10 = _mm256_permute2x128_si256(b3, b2, 0x31);
    const __m256i x_4_5 = _mm256_permute2x128_si256(b4, b5, 0x20);
    const __m256i x_12_13 = _mm256_permute2x128_si256(b4, b5, 0x31);

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
  } else if (htx == IDTX_1D) {
    const __m256i scale =
        _mm256_set1_epi16((int16_t)(2 * NewSqrt2 * 8 - 65536));
#define IDTX_ROW_STORE(reg, idx)                            \
  store_rect_row_pair_16bit_to_32bit_avx2(                  \
      _mm256_adds_epi16(_mm256_adds_epi16((reg), (reg)),    \
                        _mm256_mulhrs_epi16((reg), scale)), \
      output, (idx), (idx) + 8)
    IDTX_ROW_STORE(b0, 0);
    IDTX_ROW_STORE(b1, 1);
    IDTX_ROW_STORE(b2, 2);
    IDTX_ROW_STORE(b3, 3);
    IDTX_ROW_STORE(b4, 4);
    IDTX_ROW_STORE(b5, 5);
    IDTX_ROW_STORE(b6, 6);
    IDTX_ROW_STORE(b7, 7);
#undef IDTX_ROW_STORE
  } else {
    const __m256i z = _mm256_setzero_si256();
    __m256i g0, g1, g2, g3, h0, h1, h2, h3;
    if (htx == FLIPADST_1D) {
      const __m256i n0 = _mm256_subs_epi16(z, b0);
      const __m256i n2 = _mm256_subs_epi16(z, b2);
      const __m256i n4 = _mm256_subs_epi16(z, b4);
      const __m256i n6 = _mm256_subs_epi16(z, b6);
      g0 = _mm256_permute2x128_si256(b7, n4, 0x31);
      g3 = _mm256_permute2x128_si256(b7, n4, 0x20);
      g1 = _mm256_permute2x128_si256(n0, b3, 0x20);
      g2 = _mm256_permute2x128_si256(n0, b3, 0x31);
      h0 = _mm256_permute2x128_si256(n6, b5, 0x31);
      h3 = _mm256_permute2x128_si256(n6, b5, 0x20);
      h1 = _mm256_permute2x128_si256(b1, n2, 0x20);
      h2 = _mm256_permute2x128_si256(b1, n2, 0x31);
    } else {
      const __m256i n1 = _mm256_subs_epi16(z, b1);
      const __m256i n3 = _mm256_subs_epi16(z, b3);
      const __m256i n5 = _mm256_subs_epi16(z, b5);
      const __m256i n7 = _mm256_subs_epi16(z, b7);
      g0 = _mm256_permute2x128_si256(b0, n3, 0x20);
      g3 = _mm256_permute2x128_si256(b0, n3, 0x31);
      g1 = _mm256_permute2x128_si256(n7, b4, 0x31);
      g2 = _mm256_permute2x128_si256(n7, b4, 0x20);
      h0 = _mm256_permute2x128_si256(n1, b2, 0x20);
      h3 = _mm256_permute2x128_si256(n1, b2, 0x31);
      h1 = _mm256_permute2x128_si256(b6, n5, 0x31);
      h2 = _mm256_permute2x128_si256(b6, n5, 0x20);
    }
    __m256i s0, s1, s2, s3, s4, s5, s6, s7;
    fadst8x16_stages2_8_avx2(g0, g1, g2, g3, h0, h1, h2, h3, &s0, &s1, &s2, &s3,
                             &s4, &s5, &s6, &s7);
    store_rect_row_pair_16bit_to_32bit_avx2(s1, output, 0, 2);
    store_rect_row_pair_16bit_to_32bit_avx2(s5, output, 8, 10);
    store_rect_row_pair_16bit_to_32bit_avx2(s3, output, 4, 6);
    store_rect_row_pair_16bit_to_32bit_avx2(s7, output, 12, 14);
    store_rect_row_pair_16bit_to_32bit_avx2(s6, output, 3, 1);
    store_rect_row_pair_16bit_to_32bit_avx2(s2, output, 11, 9);
    store_rect_row_pair_16bit_to_32bit_avx2(s4, output, 7, 5);
    store_rect_row_pair_16bit_to_32bit_avx2(s0, output, 15, 13);
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
  fdct8x8_reg_avx2(&buf0[0], &buf0[1], &buf0[2], &buf0[3], &buf0[4], &buf0[5],
                   &buf0[6], &buf0[7]);
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

// 4-point 1-D DCT on unpacked pairs u0=(x0,x1), u1=(x3,x2); returns 32-bit
// u01=[out0|out1] and u23=[out2|out3].
static AOM_FORCE_INLINE void fdct4_256_avx2(__m128i u0, __m128i u1,
                                            __m256i *u01, __m256i *u23) {
  const int32_t *cospi = cospi_arr(13);
  const __m256i r256 = _mm256_set1_epi32(1 << 12);
  const __m128i v0 = _mm_add_epi16(u0, u1);
  const __m128i v1 = _mm_sub_epi16(u0, u1);
  const __m256i v01 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(v0), v1, 1);
  const __m256i w01 =
      pair_set_w16x2_epi16(cospi[32], cospi[32], cospi[16], cospi[48]);
  const __m256i w23 =
      pair_set_w16x2_epi16(cospi[32], -cospi[32], cospi[48], -cospi[16]);
  *u01 = _mm256_srai_epi32(_mm256_add_epi32(_mm256_madd_epi16(v01, w01), r256),
                           13);
  *u23 = _mm256_srai_epi32(_mm256_add_epi32(_mm256_madd_epi16(v01, w23), r256),
                           13);
}

// 4-point 1-D ADST on 4x int16 rows r0..r3 (low 64 bits); returns 32-bit
// u01=[out0|out1] and u23=[out2|out3].
static AOM_FORCE_INLINE void fadst4_256_avx2(__m128i r0, __m128i r1, __m128i r2,
                                             __m128i r3, __m256i *u01,
                                             __m256i *u23) {
  const int32_t *sinpi = sinpi_arr(13);
  const __m256i r256 = _mm256_set1_epi32(1 << 12);
  const __m128i in7 = _mm_add_epi16(r0, r1);
  const __m128i u0 = _mm_unpacklo_epi16(r0, r1);
  const __m128i u1 = _mm_unpacklo_epi16(r2, r3);
  const __m128i u2 = _mm_unpacklo_epi16(in7, r3);

  const __m256i u00 =
      _mm256_permute4x64_epi64(_mm256_castsi128_si256(u0), 0x44);
  const __m256i u11 =
      _mm256_permute4x64_epi64(_mm256_castsi128_si256(u1), 0x44);
  const __m256i u21 =
      _mm256_inserti128_si256(_mm256_castsi128_si256(u2), u1, 1);

  const __m256i v03 = _mm256_madd_epi16(
      u00, pair_set_w16x2_epi16(sinpi[1], sinpi[2], sinpi[4], -sinpi[1]));
  const __m256i v14 = _mm256_madd_epi16(
      u11, pair_set_w16x2_epi16(sinpi[3], sinpi[4], -sinpi[3], sinpi[2]));
  const __m256i s1_v5 = _mm256_madd_epi16(
      u21, pair_set_w16x2_epi16(sinpi[3], -sinpi[3], sinpi[3], 0));

  const __m256i s0_s2 = _mm256_add_epi32(v03, v14);
  const __m128i s0 = _mm256_castsi256_si128(s0_s2);
  const __m128i s2 = _mm256_extracti128_si256(s0_s2, 1);
  const __m128i v5 = _mm256_extracti128_si256(s1_v5, 1);
  const __m128i s3 = _mm_sub_epi32(s2, s0);
  const __m128i s5 = _mm_sub_epi32(_mm_slli_epi32(v5, 2), v5);
  const __m128i s6 = _mm_add_epi32(s3, s5);
  const __m256i s1_s6 = _mm256_inserti128_si256(s1_v5, s6, 1);

  const __m256i m01 = _mm256_permute2x128_si256(s0_s2, s1_s6, 0x20);
  const __m256i m23 = _mm256_permute2x128_si256(s0_s2, s1_s6, 0x31);
  *u01 = _mm256_srai_epi32(_mm256_add_epi32(m01, r256), 13);
  *u23 = _mm256_srai_epi32(_mm256_add_epi32(m23, r256), 13);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void lowbd_fwd_txfm2d_4x4_non_dct_avx2(const int16_t *input,
                                              int32_t *output, int stride,
                                              TX_TYPE tx_type) {
  const int vtx = vtx_tab[tx_type];
  const int htx = htx_tab[tx_type];
  const ptrdiff_t s_in = stride;
  const int16_t *in_ptr = (vtx == FLIPADST_1D) ? (input + 3 * s_in) : input;
  const ptrdiff_t s = (vtx == FLIPADST_1D) ? -s_in : s_in;
  const __m128i r0 =
      _mm_slli_epi16(_mm_loadl_epi64((const __m128i *)(in_ptr + 0 * s)), 2);
  const __m128i r1 =
      _mm_slli_epi16(_mm_loadl_epi64((const __m128i *)(in_ptr + 1 * s)), 2);
  const __m128i r2 =
      _mm_slli_epi16(_mm_loadl_epi64((const __m128i *)(in_ptr + 2 * s)), 2);
  const __m128i r3 =
      _mm_slli_epi16(_mm_loadl_epi64((const __m128i *)(in_ptr + 3 * s)), 2);

  // p holds [c0, c2 | c1, c3] (each c_i = 4x int16_t).
  __m256i p;
  switch (vtx) {
    case DCT_1D: {
      __m256i u01, u23;
      fdct4_256_avx2(_mm_unpacklo_epi16(r0, r1), _mm_unpacklo_epi16(r3, r2),
                     &u01, &u23);
      p = _mm256_packs_epi32(u01, u23);
      break;
    }
    case ADST_1D:
    case FLIPADST_1D: {
      __m256i u01, u23;
      fadst4_256_avx2(r0, r1, r2, r3, &u01, &u23);
      p = _mm256_packs_epi32(u01, u23);
      break;
    }
    default: {
      const __m128i r02 = _mm_unpacklo_epi64(r0, r2);
      const __m128i r13 = _mm_unpacklo_epi64(r1, r3);
      const __m256i r0213 =
          _mm256_inserti128_si256(_mm256_castsi128_si256(r02), r13, 1);
      const __m256i scale = _mm256_set1_epi16((int16_t)(NewSqrt2 * 8 - 32768));
      p = _mm256_add_epi16(r0213, _mm256_mulhrs_epi16(r0213, scale));
      break;
    }
  }

  // Transpose 4x4: p=[c0,c2 | c1,c3] -> t01=[t0,t1], t23=[t2,t3].
  const __m128i p02 = _mm256_castsi256_si128(p);
  const __m128i p13 = _mm256_extracti128_si256(p, 1);
  const __m128i a0 = _mm_unpacklo_epi16(p02, p13);
  const __m128i a1 = _mm_unpackhi_epi16(p02, p13);
  const __m128i t01 = _mm_unpacklo_epi32(a0, a1);
  const __m128i t23 = _mm_unpackhi_epi32(a0, a1);

  switch (htx) {
    case DCT_1D: {
      const __m128i t1 = _mm_srli_si128(t01, 8);
      const __m128i t3 = _mm_srli_si128(t23, 8);
      __m256i u01, u23;
      fdct4_256_avx2(_mm_unpacklo_epi16(t01, t1), _mm_unpacklo_epi16(t3, t23),
                     &u01, &u23);
      _mm256_storeu_si256((__m256i *)output, u01);
      _mm256_storeu_si256((__m256i *)(output + 8), u23);
      break;
    }
    case ADST_1D: {
      __m256i u01, u23;
      fadst4_256_avx2(t01, _mm_srli_si128(t01, 8), t23, _mm_srli_si128(t23, 8),
                      &u01, &u23);
      _mm256_storeu_si256((__m256i *)output, u01);
      _mm256_storeu_si256((__m256i *)(output + 8), u23);
      break;
    }
    case FLIPADST_1D: {
      __m256i u01, u23;
      fadst4_256_avx2(_mm_srli_si128(t23, 8), t23, _mm_srli_si128(t01, 8), t01,
                      &u01, &u23);
      _mm256_storeu_si256((__m256i *)output, u01);
      _mm256_storeu_si256((__m256i *)(output + 8), u23);
      break;
    }
    default: {
      const __m256i t =
          _mm256_inserti128_si256(_mm256_castsi128_si256(t01), t23, 1);
      const __m256i scale = _mm256_set1_epi16((int16_t)(NewSqrt2 * 8 - 32768));
      const __m256i o = _mm256_add_epi16(t, _mm256_mulhrs_epi16(t, scale));
      _mm256_storeu_si256((__m256i *)output,
                          _mm256_cvtepi16_epi32(_mm256_castsi256_si128(o)));
      _mm256_storeu_si256(
          (__m256i *)(output + 8),
          _mm256_cvtepi16_epi32(_mm256_extracti128_si256(o, 1)));
      break;
    }
  }
}

static void lowbd_fwd_txfm2d_4x4_avx2(const int16_t *input, int32_t *output,
                                      int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  if (UNLIKELY(tx_type != DCT_DCT)) {
    lowbd_fwd_txfm2d_4x4_non_dct_avx2(input, output, stride, tx_type);
    return;
  }
  const ptrdiff_t s_in = stride;
  const __m128i r0 = _mm_loadl_epi64((const __m128i *)(input + 0 * s_in));
  const __m128i r1 = _mm_loadl_epi64((const __m128i *)(input + 1 * s_in));
  const __m128i r2 = _mm_loadl_epi64((const __m128i *)(input + 2 * s_in));
  const __m128i r3 = _mm_loadl_epi64((const __m128i *)(input + 3 * s_in));
  const __m128i cu0 = _mm_slli_epi16(_mm_unpacklo_epi16(r0, r1), 2);
  const __m128i cu1 = _mm_slli_epi16(_mm_unpacklo_epi16(r3, r2), 2);
  __m256i u01, u23;
  fdct4_256_avx2(cu0, cu1, &u01, &u23);
  const __m256i p = _mm256_packs_epi32(u01, u23);
  const __m256i sh = _mm256_shuffle_epi32(p, 0xd8);
  const __m128i s0 = _mm256_castsi256_si128(sh);
  const __m128i s1 = _mm256_extracti128_si256(sh, 1);
  const __m128i tu0 = _mm_unpacklo_epi32(s0, s1);
  const __m128i tu1_23 = _mm_unpackhi_epi32(s0, s1);
  const __m128i swap16 =
      _mm_setr_epi8(2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13);
  const __m128i tu1 = _mm_shuffle_epi8(tu1_23, swap16);
  fdct4_256_avx2(tu0, tu1, &u01, &u23);
  _mm256_storeu_si256((__m256i *)output, u01);
  _mm256_storeu_si256((__m256i *)(output + 8), u23);
}

static const FwdTxfm2dFunc fwd_txfm2d_func_ls[TX_SIZES_ALL] = {
  lowbd_fwd_txfm2d_4x4_avx2,       // 4x4 transform
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

FwdTxfm2dFunc av1_lowbd_fwd_txfm2d_func_avx2(TX_SIZE tx_size) {
  assert((unsigned)tx_size < TX_SIZES_ALL);
  return fwd_txfm2d_func_ls[tx_size];
}

void av1_lowbd_fwd_txfm_nl_avx2(const int16_t *src_diff, tran_low_t *coeff,
                                int diff_stride, TX_SIZE tx_size,
                                TX_TYPE tx_type) {
  assert((unsigned)tx_size < TX_SIZES_ALL);
  assert((unsigned)tx_type < TX_TYPES);
  fwd_txfm2d_func_ls[tx_size](src_diff, coeff, diff_stride, tx_type, 8);
}

uint64_t av1_sub_sum_sse_lbd_avx2(const uint8_t *src, int src_stride,
                                  const uint8_t *dst, int dst_stride,
                                  int16_t *diff, uint32_t dims_pack) {
  const int diff_stride = (int)(dims_pack & 0xffffu);
  const int bsw = (int)((dims_pack >> 16) & 0xffu);
  const int bsh = (int)(dims_pack >> 24);
  assert(bsw == 4 || bsw == 8 || bsw == 16 || bsw == 32);
  assert(bsh == 4 || bsh == 8 || bsh == 16 || bsh == 32);
  assert(diff_stride >= bsw);
  if (bsw == 4) {
    const __m128i z = _mm_setzero_si128();
    const __m128i one = _mm_srli_epi16(_mm_cmpeq_epi16(z, z), 15);
    __m128i v_sum = z;
    __m128i v_sse = z;
    for (int r = 0; r < bsh; r += 4) {
      uint32_t s0, s1, s2, s3, d0, d1, d2, d3;
      memcpy(&s0, src, 4);
      memcpy(&s1, src + src_stride, 4);
      memcpy(&s2, src + 2 * src_stride, 4);
      memcpy(&s3, src + 3 * src_stride, 4);
      memcpy(&d0, dst, 4);
      memcpy(&d1, dst + dst_stride, 4);
      memcpy(&d2, dst + 2 * dst_stride, 4);
      memcpy(&d3, dst + 3 * dst_stride, 4);
      const __m128i vs = _mm_setr_epi32((int)s0, (int)s1, (int)s2, (int)s3);
      const __m128i vd = _mm_setr_epi32((int)d0, (int)d1, (int)d2, (int)d3);
      const __m128i r01 =
          _mm_sub_epi16(_mm_unpacklo_epi8(vs, z), _mm_unpacklo_epi8(vd, z));
      const __m128i r23 =
          _mm_sub_epi16(_mm_unpackhi_epi8(vs, z), _mm_unpackhi_epi8(vd, z));
      _mm_storel_epi64((__m128i *)diff, r01);
      _mm_storel_epi64((__m128i *)(diff + diff_stride), _mm_srli_si128(r01, 8));
      _mm_storel_epi64((__m128i *)(diff + 2 * diff_stride), r23);
      _mm_storel_epi64((__m128i *)(diff + 3 * diff_stride),
                       _mm_srli_si128(r23, 8));
      v_sum =
          _mm_add_epi32(v_sum, _mm_madd_epi16(_mm_add_epi16(r01, r23), one));
      v_sse = _mm_add_epi32(v_sse, _mm_add_epi32(_mm_madd_epi16(r01, r01),
                                                 _mm_madd_epi16(r23, r23)));
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      diff += 4 * diff_stride;
    }
    v_sum = _mm_add_epi32(v_sum, _mm_srli_si128(v_sum, 8));
    v_sum = _mm_add_epi32(v_sum, _mm_srli_si128(v_sum, 4));
    v_sse = _mm_add_epi32(v_sse, _mm_srli_si128(v_sse, 8));
    v_sse = _mm_add_epi32(v_sse, _mm_srli_si128(v_sse, 4));
    return ((uint64_t)(uint32_t)_mm_cvtsi128_si32(v_sum) << 32) |
           (uint32_t)_mm_cvtsi128_si32(v_sse);
  }

  const __m256i z = _mm256_setzero_si256();
  const __m256i one = _mm256_srli_epi16(_mm256_cmpeq_epi16(z, z), 15);
  __m256i v_sum = z;
  __m256i v_sse = z;
  if (bsw == 8) {
    for (int r = 0; r < bsh; r += 4) {
      const __m128i s01 = _mm_unpacklo_epi64(
          _mm_loadl_epi64((const __m128i *)src),
          _mm_loadl_epi64((const __m128i *)(src + src_stride)));
      const __m128i d01 = _mm_unpacklo_epi64(
          _mm_loadl_epi64((const __m128i *)dst),
          _mm_loadl_epi64((const __m128i *)(dst + dst_stride)));
      const __m128i s23 = _mm_unpacklo_epi64(
          _mm_loadl_epi64((const __m128i *)(src + 2 * src_stride)),
          _mm_loadl_epi64((const __m128i *)(src + 3 * src_stride)));
      const __m128i d23 = _mm_unpacklo_epi64(
          _mm_loadl_epi64((const __m128i *)(dst + 2 * dst_stride)),
          _mm_loadl_epi64((const __m128i *)(dst + 3 * dst_stride)));
      const __m256i r01 = _mm256_sub_epi16(_mm256_cvtepu8_epi16(s01),
                                           _mm256_cvtepu8_epi16(d01));
      const __m256i r23 = _mm256_sub_epi16(_mm256_cvtepu8_epi16(s23),
                                           _mm256_cvtepu8_epi16(d23));
      _mm_storeu_si128((__m128i *)diff, _mm256_castsi256_si128(r01));
      _mm_storeu_si128((__m128i *)(diff + diff_stride),
                       _mm256_extracti128_si256(r01, 1));
      _mm_storeu_si128((__m128i *)(diff + 2 * diff_stride),
                       _mm256_castsi256_si128(r23));
      _mm_storeu_si128((__m128i *)(diff + 3 * diff_stride),
                       _mm256_extracti128_si256(r23, 1));
      v_sum = _mm256_add_epi32(
          v_sum, _mm256_madd_epi16(_mm256_add_epi16(r01, r23), one));
      v_sse = _mm256_add_epi32(v_sse,
                               _mm256_add_epi32(_mm256_madd_epi16(r01, r01),
                                                _mm256_madd_epi16(r23, r23)));
      src += 4 * src_stride;
      dst += 4 * dst_stride;
      diff += 4 * diff_stride;
    }
  } else if (bsw == 16) {
    for (int r = 0; r < bsh; r += 2) {
      const __m256i r0 = _mm256_sub_epi16(
          _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)src)),
          _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)dst)));
      const __m256i r1 = _mm256_sub_epi16(
          _mm256_cvtepu8_epi16(
              _mm_loadu_si128((const __m128i *)(src + src_stride))),
          _mm256_cvtepu8_epi16(
              _mm_loadu_si128((const __m128i *)(dst + dst_stride))));
      _mm256_storeu_si256((__m256i *)diff, r0);
      _mm256_storeu_si256((__m256i *)(diff + diff_stride), r1);
      v_sum = _mm256_add_epi32(
          v_sum, _mm256_madd_epi16(_mm256_add_epi16(r0, r1), one));
      v_sse =
          _mm256_add_epi32(v_sse, _mm256_add_epi32(_mm256_madd_epi16(r0, r0),
                                                   _mm256_madd_epi16(r1, r1)));
      src += 2 * src_stride;
      dst += 2 * dst_stride;
      diff += 2 * diff_stride;
    }
  } else {
    for (int r = 0; r < bsh; ++r) {
      const __m256i r0 = _mm256_sub_epi16(
          _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)src)),
          _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)dst)));
      const __m256i r1 = _mm256_sub_epi16(
          _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)(src + 16))),
          _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)(dst + 16))));
      _mm256_storeu_si256((__m256i *)diff, r0);
      _mm256_storeu_si256((__m256i *)(diff + 16), r1);
      v_sum = _mm256_add_epi32(
          v_sum, _mm256_madd_epi16(_mm256_add_epi16(r0, r1), one));
      v_sse =
          _mm256_add_epi32(v_sse, _mm256_add_epi32(_mm256_madd_epi16(r0, r0),
                                                   _mm256_madd_epi16(r1, r1)));
      src += src_stride;
      dst += dst_stride;
      diff += diff_stride;
    }
  }
  __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(v_sum),
                                 _mm256_extracti128_si256(v_sum, 1));
  __m128i sse128 = _mm_add_epi32(_mm256_castsi256_si128(v_sse),
                                 _mm256_extracti128_si256(v_sse, 1));
  sum128 = _mm_add_epi32(sum128, _mm_srli_si128(sum128, 8));
  sum128 = _mm_add_epi32(sum128, _mm_srli_si128(sum128, 4));
  sse128 = _mm_add_epi32(sse128, _mm_srli_si128(sse128, 8));
  sse128 = _mm_add_epi32(sse128, _mm_srli_si128(sse128, 4));
  return ((uint64_t)(uint32_t)_mm_cvtsi128_si32(sum128) << 32) |
         (uint32_t)_mm_cvtsi128_si32(sse128);
}

int64_t av1_sse_dc_add_8bit_avx2(const uint8_t *src, int src_stride,
                                 const uint8_t *dst, int dst_stride,
                                 uint32_t dims_dc) {
  const int bsw = (int)(dims_dc & 0xffu);
  const int bsh = (int)((dims_dc >> 8) & 0xffu);
  assert(bsw == 4 || bsw == 8 || bsw == 16 || bsw == 32 || bsw == 64);
  assert(bsh == 4 || bsh == 8 || bsh == 16 || bsh == 32 || bsh == 64);
  if (bsw < 8) {
    const __m128i v_pos = _mm_set1_epi8((char)(uint8_t)(dims_dc >> 16));
    const __m128i v_neg = _mm_set1_epi8((char)(uint8_t)(dims_dc >> 24));
    const __m128i zero = _mm_setzero_si128();
    __m128i v_acc = zero;
    for (int row = 0; row < bsh; row += 4) {
      uint32_t s0, s1, s2, s3, d0, d1, d2, d3;
      memcpy(&s0, src, 4);
      memcpy(&s1, src + src_stride, 4);
      memcpy(&s2, src + 2 * src_stride, 4);
      memcpy(&s3, src + 3 * src_stride, 4);
      memcpy(&d0, dst, 4);
      memcpy(&d1, dst + dst_stride, 4);
      memcpy(&d2, dst + 2 * dst_stride, 4);
      memcpy(&d3, dst + 3 * dst_stride, 4);
      const __m128i vs = _mm_setr_epi32((int)s0, (int)s1, (int)s2, (int)s3);
      const __m128i vd = _mm_setr_epi32((int)d0, (int)d1, (int)d2, (int)d3);
      const __m128i vr = _mm_subs_epu8(_mm_adds_epu8(vd, v_pos), v_neg);
      const __m128i va =
          _mm_or_si128(_mm_subs_epu8(vs, vr), _mm_subs_epu8(vr, vs));
      const __m128i vlo = _mm_unpacklo_epi8(va, zero);
      const __m128i vhi = _mm_unpackhi_epi8(va, zero);
      v_acc = _mm_add_epi32(v_acc, _mm_add_epi32(_mm_madd_epi16(vlo, vlo),
                                                 _mm_madd_epi16(vhi, vhi)));
      src += 4 * src_stride;
      dst += 4 * dst_stride;
    }
    v_acc = _mm_add_epi32(v_acc, _mm_srli_si128(v_acc, 8));
    v_acc = _mm_add_epi32(v_acc, _mm_srli_si128(v_acc, 4));
    return 16 * (int64_t)(uint32_t)_mm_cvtsi128_si32(v_acc);
  }

  const __m256i v_pos = _mm256_set1_epi8((char)(uint8_t)(dims_dc >> 16));
  const __m256i v_neg = _mm256_set1_epi8((char)(uint8_t)(dims_dc >> 24));
  const __m256i zero = _mm256_setzero_si256();
  __m256i v_acc = zero;
  if (bsw == 8) {
    for (int row = 0; row < bsh; row += 4) {
      const __m128i s01 = _mm_unpacklo_epi64(
          _mm_loadl_epi64((const __m128i *)src),
          _mm_loadl_epi64((const __m128i *)(src + src_stride)));
      const __m128i s23 = _mm_unpacklo_epi64(
          _mm_loadl_epi64((const __m128i *)(src + 2 * src_stride)),
          _mm_loadl_epi64((const __m128i *)(src + 3 * src_stride)));
      const __m128i d01 = _mm_unpacklo_epi64(
          _mm_loadl_epi64((const __m128i *)dst),
          _mm_loadl_epi64((const __m128i *)(dst + dst_stride)));
      const __m128i d23 = _mm_unpacklo_epi64(
          _mm_loadl_epi64((const __m128i *)(dst + 2 * dst_stride)),
          _mm_loadl_epi64((const __m128i *)(dst + 3 * dst_stride)));
      const __m256i vs =
          _mm256_inserti128_si256(_mm256_castsi128_si256(s01), s23, 1);
      const __m256i vd =
          _mm256_inserti128_si256(_mm256_castsi128_si256(d01), d23, 1);
      const __m256i vr = _mm256_subs_epu8(_mm256_adds_epu8(vd, v_pos), v_neg);
      const __m256i va =
          _mm256_or_si256(_mm256_subs_epu8(vs, vr), _mm256_subs_epu8(vr, vs));
      const __m256i vlo = _mm256_unpacklo_epi8(va, zero);
      const __m256i vhi = _mm256_unpackhi_epi8(va, zero);
      v_acc = _mm256_add_epi32(v_acc,
                               _mm256_add_epi32(_mm256_madd_epi16(vlo, vlo),
                                                _mm256_madd_epi16(vhi, vhi)));
      src += 4 * src_stride;
      dst += 4 * dst_stride;
    }
  } else if (bsw == 16) {
    for (int row = 0; row < bsh; row += 2) {
      const __m256i vs = _mm256_inserti128_si256(
          _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *)src)),
          _mm_loadu_si128((const __m128i *)(src + src_stride)), 1);
      const __m256i vd = _mm256_inserti128_si256(
          _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *)dst)),
          _mm_loadu_si128((const __m128i *)(dst + dst_stride)), 1);
      const __m256i vr = _mm256_subs_epu8(_mm256_adds_epu8(vd, v_pos), v_neg);
      const __m256i va =
          _mm256_or_si256(_mm256_subs_epu8(vs, vr), _mm256_subs_epu8(vr, vs));
      const __m256i vlo = _mm256_unpacklo_epi8(va, zero);
      const __m256i vhi = _mm256_unpackhi_epi8(va, zero);
      v_acc = _mm256_add_epi32(v_acc,
                               _mm256_add_epi32(_mm256_madd_epi16(vlo, vlo),
                                                _mm256_madd_epi16(vhi, vhi)));
      src += 2 * src_stride;
      dst += 2 * dst_stride;
    }
  } else {
    for (int row = 0; row < bsh; ++row) {
      for (int col = 0; col < bsw; col += 32) {
        const __m256i vs = _mm256_loadu_si256((const __m256i *)(src + col));
        const __m256i vd = _mm256_loadu_si256((const __m256i *)(dst + col));
        const __m256i vr = _mm256_subs_epu8(_mm256_adds_epu8(vd, v_pos), v_neg);
        const __m256i va =
            _mm256_or_si256(_mm256_subs_epu8(vs, vr), _mm256_subs_epu8(vr, vs));
        const __m256i vlo = _mm256_unpacklo_epi8(va, zero);
        const __m256i vhi = _mm256_unpackhi_epi8(va, zero);
        v_acc = _mm256_add_epi32(v_acc,
                                 _mm256_add_epi32(_mm256_madd_epi16(vlo, vlo),
                                                  _mm256_madd_epi16(vhi, vhi)));
      }
      src += src_stride;
      dst += dst_stride;
    }
  }
  __m128i acc128 = _mm_add_epi32(_mm256_castsi256_si128(v_acc),
                                 _mm256_extracti128_si256(v_acc, 1));
  acc128 = _mm_add_epi32(acc128, _mm_srli_si128(acc128, 8));
  acc128 = _mm_add_epi32(acc128, _mm_srli_si128(acc128, 4));
  return 16 * (int64_t)(uint32_t)_mm_cvtsi128_si32(acc128);
}

int64_t av1_dist_block_px_lbd_recon_avx2(const uint8_t *src, int src_stride,
                                         const uint8_t *dst, int dst_stride,
                                         const tran_low_t *dqcoeff,
                                         uint32_t tx_info) {
  const TX_SIZE tx_size = (TX_SIZE)(tx_info & 0x1fu);
  const TX_TYPE tx_type = (TX_TYPE)((tx_info >> 5) & 0xfu);
  const int eob = (int)((tx_info >> 9) & 0x7ffu);
  const int bsw = (int)((tx_info >> 20) & 0x3fu);
  const int bsh = (int)(tx_info >> 26);
  assert((unsigned)tx_size < TX_SIZES_ALL);
  assert((unsigned)tx_type < TX_TYPES);
  assert(eob > 0 && eob <= 1024);
  assert(bsw == tx_size_wide[tx_size] && bsh == tx_size_high[tx_size]);
  assert((bsw == 4 || bsw == 8 || bsw == 16 || bsw == 32) &&
         (bsh == 4 || bsh == 8 || bsh == 16 || bsh == 32));
  DECLARE_ALIGNED(32, uint8_t, recon[32 * 32]);
  if (bsw == 8) {
    for (int r = 0; r < bsh; r += 4) {
      const __m128i r01 = _mm_unpacklo_epi64(
          _mm_loadl_epi64((const __m128i *)(dst + r * dst_stride)),
          _mm_loadl_epi64((const __m128i *)(dst + (r + 1) * dst_stride)));
      const __m128i r23 = _mm_unpacklo_epi64(
          _mm_loadl_epi64((const __m128i *)(dst + (r + 2) * dst_stride)),
          _mm_loadl_epi64((const __m128i *)(dst + (r + 3) * dst_stride)));
      _mm256_store_si256(
          (__m256i *)(recon + r * 8),
          _mm256_inserti128_si256(_mm256_castsi128_si256(r01), r23, 1));
    }
  } else if (bsw == 4) {
    for (int r = 0; r < bsh; r += 4) {
      uint32_t d0, d1, d2, d3;
      memcpy(&d0, dst + r * dst_stride, 4);
      memcpy(&d1, dst + (r + 1) * dst_stride, 4);
      memcpy(&d2, dst + (r + 2) * dst_stride, 4);
      memcpy(&d3, dst + (r + 3) * dst_stride, 4);
      _mm_store_si128((__m128i *)(recon + r * 4),
                      _mm_setr_epi32((int)d0, (int)d1, (int)d2, (int)d3));
    }
  } else if (bsw == 16) {
    for (int r = 0; r < bsh; r += 4) {
      const __m256i r01 = _mm256_inserti128_si256(
          _mm256_castsi128_si256(
              _mm_loadu_si128((const __m128i *)(dst + r * dst_stride))),
          _mm_loadu_si128((const __m128i *)(dst + (r + 1) * dst_stride)), 1);
      const __m256i r23 = _mm256_inserti128_si256(
          _mm256_castsi128_si256(
              _mm_loadu_si128((const __m128i *)(dst + (r + 2) * dst_stride))),
          _mm_loadu_si128((const __m128i *)(dst + (r + 3) * dst_stride)), 1);
      _mm256_store_si256((__m256i *)(recon + r * 16), r01);
      _mm256_store_si256((__m256i *)(recon + (r + 2) * 16), r23);
    }
  } else {
    for (int r = 0; r < bsh; r += 2) {
      const uint8_t *const d0 = dst + r * dst_stride;
      const __m256i v0 = _mm256_loadu_si256((const __m256i *)d0);
      const __m256i v1 = _mm256_loadu_si256((const __m256i *)(d0 + dst_stride));
      _mm256_store_si256((__m256i *)(recon + r * 32), v0);
      _mm256_store_si256((__m256i *)(recon + (r + 1) * 32), v1);
    }
  }
  av1_lowbd_inv_txfm2d_add_avx2(dqcoeff, recon, bsw, tx_type, tx_size, eob);
  return 16 *
         (int64_t)(uint32_t)aom_sse_avx2(src, src_stride, recon, bsw, bsw, bsh);
}
