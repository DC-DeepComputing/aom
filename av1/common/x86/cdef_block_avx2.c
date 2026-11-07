/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "aom_dsp/aom_simd.h"
#define SIMD_FUNC(name) name##_avx2
#include "av1/common/cdef_block_simd.h"

/* partial A is a 16-bit vector of the form:
[x8 - - x1 | x16 - - x9] and partial B has the form:
[0  y1 - y7 | 0 y9 - y15].
This function computes (x1^2+y1^2)*C1 + (x2^2+y2^2)*C2 + ...
(x7^2+y2^7)*C7 + (x8^2+0^2)*C8 on each 128-bit lane. Here the C1..C8 constants
are in const1 and const2. */
static inline __m256i fold_mul_and_sum_avx2(__m256i *partiala,
                                            __m256i *partialb,
                                            const __m256i *const1,
                                            const __m256i *const2) {
  // Mask used to shuffle the elements present in 256bit register.
  static const int shuffle_reg_256bit[8] = { 0x0b0a0d0c, 0x07060908, 0x03020504,
                                             0x0f0e0100, 0x0b0a0d0c, 0x07060908,
                                             0x03020504, 0x0f0e0100 };
  __m256i tmp;
  /* Reverse partial B. */
  *partialb = _mm256_shuffle_epi8(
      *partialb, _mm256_loadu_si256((const __m256i *)shuffle_reg_256bit));

  /* Interleave the x and y values of identical indices and pair x8 with 0. */
  tmp = *partiala;
  *partiala = _mm256_unpacklo_epi16(*partiala, *partialb);
  *partialb = _mm256_unpackhi_epi16(tmp, *partialb);

  /* Square and add the corresponding x and y values. */
  *partiala = _mm256_madd_epi16(*partiala, *partiala);
  *partialb = _mm256_madd_epi16(*partialb, *partialb);
  /* Multiply by constant. */
  *partiala = _mm256_mullo_epi32(*partiala, *const1);
  *partialb = _mm256_mullo_epi32(*partialb, *const2);
  /* Sum all results. */
  *partiala = _mm256_add_epi32(*partiala, *partialb);
  return *partiala;
}

static inline __m256i hsum4_avx2(__m256i *x0, __m256i *x1, __m256i *x2,
                                 __m256i *x3) {
  const __m256i t0 = _mm256_unpacklo_epi32(*x0, *x1);
  const __m256i t1 = _mm256_unpacklo_epi32(*x2, *x3);
  const __m256i t2 = _mm256_unpackhi_epi32(*x0, *x1);
  const __m256i t3 = _mm256_unpackhi_epi32(*x2, *x3);

  *x0 = _mm256_unpacklo_epi64(t0, t1);
  *x1 = _mm256_unpackhi_epi64(t0, t1);
  *x2 = _mm256_unpacklo_epi64(t2, t3);
  *x3 = _mm256_unpackhi_epi64(t2, t3);
  return _mm256_add_epi32(_mm256_add_epi32(*x0, *x1),
                          _mm256_add_epi32(*x2, *x3));
}

/* Computes cost for directions 0, 5, 6 and 7. We can call this function again
to compute the remaining directions. */
static inline __m256i compute_directions_avx2(__m256i *lines,
                                              int32_t cost_frist_8x8[4],
                                              int32_t cost_second_8x8[4]) {
  __m256i partial4a, partial4b, partial5a, partial5b, partial7a, partial7b;
  __m256i partial6;
  __m256i tmp;
  /* Partial sums for lines 0 and 1. */
  const __m256i l0 = lines[0];
  const __m256i l1 = lines[1];
  partial4a = _mm256_slli_si256(l0, 14);
  partial4b = _mm256_srli_si256(l0, 2);
  partial4a = _mm256_add_epi16(partial4a, _mm256_slli_si256(l1, 12));
  partial4b = _mm256_add_epi16(partial4b, _mm256_srli_si256(l1, 4));
  tmp = _mm256_add_epi16(l0, l1);
  partial5a = _mm256_slli_si256(tmp, 10);
  partial5b = _mm256_srli_si256(tmp, 6);
  partial7a = _mm256_slli_si256(tmp, 4);
  partial7b = _mm256_srli_si256(tmp, 12);
  partial6 = tmp;

  /* Partial sums for lines 2 and 3. */
  const __m256i l2 = lines[2];
  const __m256i l3 = lines[3];
  partial4a = _mm256_add_epi16(partial4a, _mm256_slli_si256(l2, 10));
  partial4b = _mm256_add_epi16(partial4b, _mm256_srli_si256(l2, 6));
  partial4a = _mm256_add_epi16(partial4a, _mm256_slli_si256(l3, 8));
  partial4b = _mm256_add_epi16(partial4b, _mm256_srli_si256(l3, 8));
  tmp = _mm256_add_epi16(l2, l3);
  partial5a = _mm256_add_epi16(partial5a, _mm256_slli_si256(tmp, 8));
  partial5b = _mm256_add_epi16(partial5b, _mm256_srli_si256(tmp, 8));
  partial7a = _mm256_add_epi16(partial7a, _mm256_slli_si256(tmp, 6));
  partial7b = _mm256_add_epi16(partial7b, _mm256_srli_si256(tmp, 10));
  partial6 = _mm256_add_epi16(partial6, tmp);

  /* Partial sums for lines 4 and 5. */
  const __m256i l4 = lines[4];
  const __m256i l5 = lines[5];
  partial4a = _mm256_add_epi16(partial4a, _mm256_slli_si256(l4, 6));
  partial4b = _mm256_add_epi16(partial4b, _mm256_srli_si256(l4, 10));
  partial4a = _mm256_add_epi16(partial4a, _mm256_slli_si256(l5, 4));
  partial4b = _mm256_add_epi16(partial4b, _mm256_srli_si256(l5, 12));
  tmp = _mm256_add_epi16(l4, l5);
  partial5a = _mm256_add_epi16(partial5a, _mm256_slli_si256(tmp, 6));
  partial5b = _mm256_add_epi16(partial5b, _mm256_srli_si256(tmp, 10));
  partial7a = _mm256_add_epi16(partial7a, _mm256_slli_si256(tmp, 8));
  partial7b = _mm256_add_epi16(partial7b, _mm256_srli_si256(tmp, 8));
  partial6 = _mm256_add_epi16(partial6, tmp);

  /* Partial sums for lines 6 and 7. */
  const __m256i l6 = lines[6];
  const __m256i l7 = lines[7];
  partial4a = _mm256_add_epi16(partial4a, _mm256_slli_si256(l6, 2));
  partial4b = _mm256_add_epi16(partial4b, _mm256_srli_si256(l6, 14));
  partial4a = _mm256_add_epi16(partial4a, l7);
  tmp = _mm256_add_epi16(l6, l7);
  partial5a = _mm256_add_epi16(partial5a, _mm256_slli_si256(tmp, 4));
  partial5b = _mm256_add_epi16(partial5b, _mm256_srli_si256(tmp, 12));
  partial7a = _mm256_add_epi16(partial7a, _mm256_slli_si256(tmp, 10));
  partial7b = _mm256_add_epi16(partial7b, _mm256_srli_si256(tmp, 6));
  partial6 = _mm256_add_epi16(partial6, tmp);

  const __m256i const_reg_1 =
      _mm256_set_epi32(210, 280, 420, 840, 210, 280, 420, 840);
  const __m256i const_reg_2 =
      _mm256_set_epi32(105, 120, 140, 168, 105, 120, 140, 168);

  /* Compute costs in terms of partial sums. */
  partial4a =
      fold_mul_and_sum_avx2(&partial4a, &partial4b, &const_reg_1, &const_reg_2);
  const __m256i const_reg_3 = _mm256_set_epi32(210, 420, 0, 0, 210, 420, 0, 0);
  const __m256i const_reg_4 =
      _mm256_set_epi32(105, 105, 105, 140, 105, 105, 105, 140);
  partial7a =
      fold_mul_and_sum_avx2(&partial7a, &partial7b, &const_reg_3, &const_reg_4);
  partial5a =
      fold_mul_and_sum_avx2(&partial5a, &partial5b, &const_reg_3, &const_reg_4);
  partial6 = _mm256_madd_epi16(partial6, partial6);
  partial6 = _mm256_mullo_epi32(partial6, _mm256_set1_epi32(105));

  partial4a = hsum4_avx2(&partial4a, &partial5a, &partial6, &partial7a);
  _mm_storeu_si128((__m128i *)cost_frist_8x8,
                   _mm256_castsi256_si128(partial4a));
  _mm_storeu_si128((__m128i *)cost_second_8x8,
                   _mm256_extractf128_si256(partial4a, 1));

  return partial4a;
}

/* transpose and reverse the order of the lines -- equivalent to a 90-degree
counter-clockwise rotation of the pixels. */
static inline void array_reverse_transpose_8x8_avx2(__m256i *in, __m256i *res) {
  const __m256i tr0_0 = _mm256_unpacklo_epi16(in[0], in[1]);
  const __m256i tr0_1 = _mm256_unpacklo_epi16(in[2], in[3]);
  const __m256i tr0_2 = _mm256_unpackhi_epi16(in[0], in[1]);
  const __m256i tr0_3 = _mm256_unpackhi_epi16(in[2], in[3]);
  const __m256i tr0_4 = _mm256_unpacklo_epi16(in[4], in[5]);
  const __m256i tr0_5 = _mm256_unpacklo_epi16(in[6], in[7]);
  const __m256i tr0_6 = _mm256_unpackhi_epi16(in[4], in[5]);
  const __m256i tr0_7 = _mm256_unpackhi_epi16(in[6], in[7]);

  const __m256i tr1_0 = _mm256_unpacklo_epi32(tr0_0, tr0_1);
  const __m256i tr1_1 = _mm256_unpacklo_epi32(tr0_4, tr0_5);
  const __m256i tr1_2 = _mm256_unpackhi_epi32(tr0_0, tr0_1);
  const __m256i tr1_3 = _mm256_unpackhi_epi32(tr0_4, tr0_5);
  const __m256i tr1_4 = _mm256_unpacklo_epi32(tr0_2, tr0_3);
  const __m256i tr1_5 = _mm256_unpacklo_epi32(tr0_6, tr0_7);
  const __m256i tr1_6 = _mm256_unpackhi_epi32(tr0_2, tr0_3);
  const __m256i tr1_7 = _mm256_unpackhi_epi32(tr0_6, tr0_7);

  res[7] = _mm256_unpacklo_epi64(tr1_0, tr1_1);
  res[6] = _mm256_unpackhi_epi64(tr1_0, tr1_1);
  res[5] = _mm256_unpacklo_epi64(tr1_2, tr1_3);
  res[4] = _mm256_unpackhi_epi64(tr1_2, tr1_3);
  res[3] = _mm256_unpacklo_epi64(tr1_4, tr1_5);
  res[2] = _mm256_unpackhi_epi64(tr1_4, tr1_5);
  res[1] = _mm256_unpacklo_epi64(tr1_6, tr1_7);
  res[0] = _mm256_unpackhi_epi64(tr1_6, tr1_7);
}

void cdef_find_dir_dual_avx2(const uint16_t *img1, const uint16_t *img2,
                             int stride, int32_t *var_out_1st,
                             int32_t *var_out_2nd, int coeff_shift,
                             int *out_dir_1st_8x8, int *out_dir_2nd_8x8) {
  int32_t cost_first_8x8[8];
  int32_t cost_second_8x8[8];
  // Used to store the best cost for 2 8x8's.
  int32_t best_cost[2] = { 0 };
  // Best direction for 2 8x8's.
  int best_dir[2] = { 0 };

  const __m128i const_coeff_shift_reg = _mm_cvtsi32_si128(coeff_shift);
  const __m256i const_128_reg = _mm256_set1_epi16(128);
  __m256i lines[8];
  for (int i = 0; i < 8; i++) {
    const __m128i src_1 = _mm_loadu_si128((const __m128i *)&img1[i * stride]);
    const __m128i src_2 = _mm_loadu_si128((const __m128i *)&img2[i * stride]);

    __m256i line =
        _mm256_insertf128_si256(_mm256_castsi128_si256(src_1), src_2, 1);
    lines[i] = _mm256_sub_epi16(_mm256_sra_epi16(line, const_coeff_shift_reg),
                                const_128_reg);
  }

  /* Compute "mostly vertical" directions. */
  const __m256i dir47 =
      compute_directions_avx2(lines, cost_first_8x8 + 4, cost_second_8x8 + 4);

  /* Transpose and reverse the order of the lines. */
  array_reverse_transpose_8x8_avx2(lines, lines);

  /* Compute "mostly horizontal" directions. */
  const __m256i dir03 =
      compute_directions_avx2(lines, cost_first_8x8, cost_second_8x8);

  __m256i max = _mm256_max_epi32(dir03, dir47);
  max =
      _mm256_max_epi32(max, _mm256_or_si256(_mm256_srli_si256(max, 8),
                                            _mm256_slli_si256(max, 16 - (8))));
  max =
      _mm256_max_epi32(max, _mm256_or_si256(_mm256_srli_si256(max, 4),
                                            _mm256_slli_si256(max, 16 - (4))));

  const __m128i first_8x8_output = _mm256_castsi256_si128(max);
  const __m128i second_8x8_output = _mm256_extractf128_si256(max, 1);
  const __m128i cmpeg_res_00 =
      _mm_cmpeq_epi32(first_8x8_output, _mm256_castsi256_si128(dir47));
  const __m128i cmpeg_res_01 =
      _mm_cmpeq_epi32(first_8x8_output, _mm256_castsi256_si128(dir03));
  const __m128i cmpeg_res_10 =
      _mm_cmpeq_epi32(second_8x8_output, _mm256_extractf128_si256(dir47, 1));
  const __m128i cmpeg_res_11 =
      _mm_cmpeq_epi32(second_8x8_output, _mm256_extractf128_si256(dir03, 1));
  const __m128i t_first_8x8 = _mm_packs_epi32(cmpeg_res_01, cmpeg_res_00);
  const __m128i t_second_8x8 = _mm_packs_epi32(cmpeg_res_11, cmpeg_res_10);

  best_cost[0] = _mm_cvtsi128_si32(_mm256_castsi256_si128(max));
  best_cost[1] = _mm_cvtsi128_si32(second_8x8_output);
  best_dir[0] = _mm_movemask_epi8(_mm_packs_epi16(t_first_8x8, t_first_8x8));
  best_dir[1] = _mm_movemask_epi8(_mm_packs_epi16(t_second_8x8, t_second_8x8));
#if defined(__GNUC__) || defined(__clang__)
  best_dir[0] = __builtin_ctz(best_dir[0]);
  best_dir[1] = __builtin_ctz(best_dir[1]);
#else
  best_dir[0] =
      get_msb(best_dir[0] ^ (best_dir[0] - 1));  // Count trailing zeros
  best_dir[1] =
      get_msb(best_dir[1] ^ (best_dir[1] - 1));  // Count trailing zeros
#endif

  /* Difference between the optimal variance and the variance along the
     orthogonal direction. Again, the sum(x^2) terms cancel out. */
  *var_out_1st = best_cost[0] - cost_first_8x8[(best_dir[0] + 4) & 7];
  *var_out_2nd = best_cost[1] - cost_second_8x8[(best_dir[1] + 4) & 7];

  /* We'd normally divide by 840, but dividing by 1024 is close enough
  for what we're going to do with this. */
  *var_out_1st >>= 10;
  *var_out_2nd >>= 10;
  *out_dir_1st_8x8 = best_dir[0];
  *out_dir_2nd_8x8 = best_dir[1];
}

void cdef_copy_rect8_8bit_to_16bit_avx2(uint16_t *dst, int dstride,
                                        const uint8_t *src, int sstride,
                                        int width, int height) {
  int j = 0;
  int remaining_width = width;
  assert(height % 2 == 0);
  assert(height > 0);
  assert(width > 0);

  // Process multiple 32 pixels at a time.
  if (remaining_width > 31) {
    int i = 0;
    do {
      j = 0;
      do {
        __m128i row00 =
            _mm_loadu_si128((const __m128i *)&src[(i + 0) * sstride + (j + 0)]);
        __m128i row01 = _mm_loadu_si128(
            (const __m128i *)&src[(i + 0) * sstride + (j + 16)]);
        __m128i row10 =
            _mm_loadu_si128((const __m128i *)&src[(i + 1) * sstride + (j + 0)]);
        __m128i row11 = _mm_loadu_si128(
            (const __m128i *)&src[(i + 1) * sstride + (j + 16)]);
        _mm256_storeu_si256((__m256i *)&dst[(i + 0) * dstride + (j + 0)],
                            _mm256_cvtepu8_epi16(row00));
        _mm256_storeu_si256((__m256i *)&dst[(i + 0) * dstride + (j + 16)],
                            _mm256_cvtepu8_epi16(row01));
        _mm256_storeu_si256((__m256i *)&dst[(i + 1) * dstride + (j + 0)],
                            _mm256_cvtepu8_epi16(row10));
        _mm256_storeu_si256((__m256i *)&dst[(i + 1) * dstride + (j + 16)],
                            _mm256_cvtepu8_epi16(row11));
        j += 32;
      } while (j <= width - 32);
      i += 2;
    } while (i < height);
    remaining_width = width & 31;
  }

  // Process 16 pixels at a time.
  if (remaining_width > 15) {
    int i = 0;
    do {
      __m128i row0 =
          _mm_loadu_si128((const __m128i *)&src[(i + 0) * sstride + j]);
      __m128i row1 =
          _mm_loadu_si128((const __m128i *)&src[(i + 1) * sstride + j]);
      _mm256_storeu_si256((__m256i *)&dst[(i + 0) * dstride + j],
                          _mm256_cvtepu8_epi16(row0));
      _mm256_storeu_si256((__m256i *)&dst[(i + 1) * dstride + j],
                          _mm256_cvtepu8_epi16(row1));
      i += 2;
    } while (i < height);
    remaining_width = width & 15;
    j += 16;
  }

  // Process 8 pixels at a time.
  if (remaining_width > 7) {
    int i = 0;
    do {
      __m128i row0 =
          _mm_loadl_epi64((const __m128i *)&src[(i + 0) * sstride + j]);
      __m128i row1 =
          _mm_loadl_epi64((const __m128i *)&src[(i + 1) * sstride + j]);
      _mm_storeu_si128((__m128i *)&dst[(i + 0) * dstride + j],
                       _mm_unpacklo_epi8(row0, _mm_setzero_si128()));
      _mm_storeu_si128((__m128i *)&dst[(i + 1) * dstride + j],
                       _mm_unpacklo_epi8(row1, _mm_setzero_si128()));
      i += 2;
    } while (i < height);
    remaining_width = width & 7;
    j += 8;
  }

  // Process 4 pixels at a time.
  if (remaining_width > 3) {
    int i = 0;
    do {
      __m128i row0 =
          _mm_cvtsi32_si128(*((const int32_t *)&src[(i + 0) * sstride + j]));
      __m128i row1 =
          _mm_cvtsi32_si128(*((const int32_t *)&src[(i + 1) * sstride + j]));
      _mm_storel_epi64((__m128i *)&dst[(i + 0) * dstride + j],
                       _mm_unpacklo_epi8(row0, _mm_setzero_si128()));
      _mm_storel_epi64((__m128i *)&dst[(i + 1) * dstride + j],
                       _mm_unpacklo_epi8(row1, _mm_setzero_si128()));
      i += 2;
    } while (i < height);
    remaining_width = width & 3;
    j += 4;
  }

  // Process the remaining pixels.
  if (remaining_width) {
    for (int i = 0; i < height; i++) {
      for (int k = j; k < width; k++) {
        dst[i * dstride + k] = src[i * sstride + k];
      }
    }
  }
}

static inline __m256i constrain16_avx2(__m256i a, __m256i b,
                                       unsigned int threshold,
                                       unsigned int adjdamp) {
  const __m256i raw_diff = _mm256_sub_epi16(a, b);
  const __m256i diff = _mm256_abs_epi16(raw_diff);
  const __m256i s = _mm256_subs_epu16(
      _mm256_set1_epi16((int16_t)threshold),
      _mm256_srl_epi16(diff, _mm_cvtsi32_si128((int)adjdamp)));
  return _mm256_sign_epi16(_mm256_min_epi16(diff, s), raw_diff);
}

static inline __m256i finish_filter_avx2(__m256i r, __m256i sum) {
  sum = _mm256_add_epi16(sum, _mm256_srai_epi16(sum, 15));
  sum = _mm256_srai_epi16(_mm256_add_epi16(sum, _mm256_set1_epi16(8)), 4);
  __m256i res = _mm256_add_epi16(r, sum);
  return res;
}

static inline __m256i load_tap_4x4(const uint16_t *p) {
  return v256_from_v64(v64_load_unaligned(&p[0 * CDEF_BSTRIDE]),
                       v64_load_unaligned(&p[1 * CDEF_BSTRIDE]),
                       v64_load_unaligned(&p[2 * CDEF_BSTRIDE]),
                       v64_load_unaligned(&p[3 * CDEF_BSTRIDE]));
}

static inline __m256i tap_pair_sum_4x4_avx2(const uint16_t **in_row_p,
                                            intptr_t offset, __m256i r,
                                            unsigned int threshold,
                                            unsigned int adjdamp) {
  const uint16_t *in_row = *in_row_p;
  const __m256i tap0 = load_tap_4x4(in_row + offset);
  const __m256i p0 = constrain16_avx2(tap0, r, threshold, adjdamp);
  const __m256i tap1 = load_tap_4x4(in_row - offset);
  const __m256i p1 = constrain16_avx2(tap1, r, threshold, adjdamp);
  __m256i sum = _mm256_add_epi16(p0, p1);
  return sum;
}

static inline __m256i tap_pair_sum_minmax_4x4_avx2(const uint16_t **in_row_p,
                                                   intptr_t offset, __m256i r,
                                                   unsigned int threshold,
                                                   unsigned int adjdamp,
                                                   __m256i *max_u8,
                                                   __m256i *min_s16) {
  const uint16_t *in_row = *in_row_p;
  const __m256i tap0 = load_tap_4x4(in_row + offset);
  *max_u8 = _mm256_max_epu8(*max_u8, tap0);
  *min_s16 = _mm256_min_epi16(*min_s16, tap0);
  const __m256i p0 = constrain16_avx2(tap0, r, threshold, adjdamp);
  const __m256i tap1 = load_tap_4x4(in_row - offset);
  *max_u8 = _mm256_max_epu8(*max_u8, tap1);
  *min_s16 = _mm256_min_epi16(*min_s16, tap1);
  const __m256i p1 = constrain16_avx2(tap1, r, threshold, adjdamp);
  __m256i sum = _mm256_add_epi16(p0, p1);
  return sum;
}

static inline uint64_t hsum_u32_avx2(__m256i v) {
  __m128i sum128 =
      _mm_add_epi32(_mm256_castsi256_si128(v), _mm256_extracti128_si256(v, 1));
  sum128 = _mm_add_epi32(sum128, _mm_srli_si128(sum128, 8));
  sum128 = _mm_add_epi32(sum128, _mm_srli_si128(sum128, 4));
  return (uint32_t)_mm_cvtsi128_si32(sum128);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void cdef_find_dir_list_avx2(const uint16_t *in, const cdef_list *dlist,
                                    int cdef_count,
                                    int dir[CDEF_NBLOCKS][CDEF_NBLOCKS],
                                    int var[CDEF_NBLOCKS][CDEF_NBLOCKS]) {
  int bi;
  for (bi = 0; bi < cdef_count - 1; bi += 2) {
    const int by = dlist[bi].by;
    const int bx = dlist[bi].bx;
    const int by2 = dlist[bi + 1].by;
    const int bx2 = dlist[bi + 1].bx;
    cdef_find_dir_dual_avx2(&in[8 * by * CDEF_BSTRIDE + 8 * bx],
                            &in[8 * by2 * CDEF_BSTRIDE + 8 * bx2], CDEF_BSTRIDE,
                            &var[by][bx], &var[by2][bx2], 0, &dir[by][bx],
                            &dir[by2][bx2]);
  }
  if (cdef_count & 1) {
    const int by = dlist[bi].by;
    const int bx = dlist[bi].bx;
    dir[by][bx] = cdef_find_dir_avx2(&in[8 * by * CDEF_BSTRIDE + 8 * bx],
                                     CDEF_BSTRIDE, &var[by][bx], 0);
  }
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void cdef_mse_calc_block_lvl4_chroma_4x4_avx2(
    const uint16_t *in, const uint8_t *ref_buffer, int ref_stride, int row,
    int col, const cdef_list *dlist, int cdef_count,
    const int dir[CDEF_NBLOCKS][CDEF_NBLOCKS], int damping, int pli,
    uint64_t mse[4]) {
  __m256i v_sse0 = _mm256_setzero_si256();
  __m256i v_sse1 = _mm256_setzero_si256();
  __m256i v_sse2 = _mm256_setzero_si256();
  __m256i v_sse3 = _mm256_setzero_si256();

  const unsigned int pri_damping = AOMMAX(0, damping - 4);
  const unsigned int sec_damping = AOMMAX(0, damping - 2);

  for (int bi = 0; bi < cdef_count; ++bi) {
    unsigned int pri_d = pri_damping;
    unsigned int sec_d = sec_damping;
    const int by = dlist[bi].by;
    const int bx = dlist[bi].bx;
    const int d = dir[by][bx];
    const uint16_t *in_row = &in[(by * CDEF_BSTRIDE << 2) + (bx << 2)];
    const uint8_t *ref_row =
        &ref_buffer[(ptrdiff_t)(row + (by << 2)) * ref_stride +
                    (col + (bx << 2))];
    const intptr_t po1 = cdef_directions_padded_inline[d + 2][0];
    const intptr_t po2 = cdef_directions_padded_inline[d + 2][1];

    if ((d & 3) == 0) {
      const __m256i r = load_tap_4x4(in_row);
      const __m256i ref16 = _mm256_cvtepu8_epi16(
          v128_from_32(u32_load_aligned(&ref_row[0 * ref_stride]),
                       u32_load_aligned(&ref_row[1 * ref_stride]),
                       u32_load_aligned(&ref_row[2 * ref_stride]),
                       u32_load_aligned(&ref_row[3 * ref_stride])));
      const __m256i diff0 = _mm256_sub_epi16(r, ref16);
      v_sse0 = _mm256_add_epi32(v_sse0, _mm256_madd_epi16(diff0, diff0));

      __m256i max_u8 = r;
      __m256i min_s16 = r;
      const __m256i pn = tap_pair_sum_minmax_4x4_avx2(&in_row, po1, r, 11,
                                                      pri_d, &max_u8, &min_s16);
      const __m256i pf = tap_pair_sum_minmax_4x4_avx2(&in_row, po2, r, 11,
                                                      pri_d, &max_u8, &min_s16);
      const __m256i p_sum = _mm256_add_epi16(pn, pf);
      const __m256i sum_pri =
          _mm256_add_epi16(_mm256_slli_epi16(p_sum, 1), p_sum);

      const __m256i res2 = finish_filter_avx2(r, sum_pri);
      const __m256i diff2 = _mm256_sub_epi16(res2, ref16);
      v_sse2 = _mm256_add_epi32(v_sse2, _mm256_madd_epi16(diff2, diff2));

      const __m256i sn1 = tap_pair_sum_minmax_4x4_avx2(&in_row, 1, r, 2, sec_d,
                                                       &max_u8, &min_s16);
      const __m256i sn2 = tap_pair_sum_minmax_4x4_avx2(
          &in_row, CDEF_BSTRIDE, r, 2, sec_d, &max_u8, &min_s16);
      const __m256i sec_near = _mm256_add_epi16(sn1, sn2);
      __m256i sum_sec = _mm256_add_epi16(sec_near, sec_near);
      const __m256i sf1 = tap_pair_sum_minmax_4x4_avx2(&in_row, 2, r, 2, sec_d,
                                                       &max_u8, &min_s16);
      const __m256i sf2 = tap_pair_sum_minmax_4x4_avx2(
          &in_row, 2 * CDEF_BSTRIDE, r, 2, sec_d, &max_u8, &min_s16);
      sum_sec = _mm256_add_epi16(sum_sec, _mm256_add_epi16(sf1, sf2));

      const __m256i res1 = finish_filter_avx2(r, sum_sec);
      const __m256i diff1 = _mm256_sub_epi16(res1, ref16);
      v_sse1 = _mm256_add_epi32(v_sse1, _mm256_madd_epi16(diff1, diff1));

      const __m256i sum_tot = _mm256_add_epi16(sum_pri, sum_sec);
      const __m256i max_s16 = _mm256_and_si256(
          max_u8, _mm256_set1_epi16((int16_t)~CDEF_VERY_LARGE));
      __m256i res3 = finish_filter_avx2(r, sum_tot);
      res3 = _mm256_min_epi16(_mm256_max_epi16(res3, min_s16), max_s16);
      const __m256i diff3 = _mm256_sub_epi16(res3, ref16);
      v_sse3 = _mm256_add_epi32(v_sse3, _mm256_madd_epi16(diff3, diff3));
    } else {
      const intptr_t s1o1 = cdef_directions_padded_inline[d + 4][0];
      const intptr_t s1o2 = cdef_directions_padded_inline[d + 4][1];
      const intptr_t s2o1 = cdef_directions_padded_inline[d][0];
      const intptr_t s2o2 = cdef_directions_padded_inline[d][1];

      const __m256i r = load_tap_4x4(in_row);
      const __m256i ref16 = _mm256_cvtepu8_epi16(
          v128_from_32(u32_load_aligned(&ref_row[0 * ref_stride]),
                       u32_load_aligned(&ref_row[1 * ref_stride]),
                       u32_load_aligned(&ref_row[2 * ref_stride]),
                       u32_load_aligned(&ref_row[3 * ref_stride])));
      const __m256i diff0 = _mm256_sub_epi16(r, ref16);
      v_sse0 = _mm256_add_epi32(v_sse0, _mm256_madd_epi16(diff0, diff0));

      const __m256i sn1 = tap_pair_sum_4x4_avx2(&in_row, 1, r, 2, sec_d);
      const __m256i sn2 =
          tap_pair_sum_4x4_avx2(&in_row, CDEF_BSTRIDE, r, 2, sec_d);
      const __m256i sec_near0 = _mm256_add_epi16(sn1, sn2);
      __m256i sum_sec0 = _mm256_add_epi16(sec_near0, sec_near0);
      const __m256i sf1 = tap_pair_sum_4x4_avx2(&in_row, 2, r, 2, sec_d);
      const __m256i sf2 =
          tap_pair_sum_4x4_avx2(&in_row, 2 * CDEF_BSTRIDE, r, 2, sec_d);
      sum_sec0 = _mm256_add_epi16(sum_sec0, _mm256_add_epi16(sf1, sf2));

      const __m256i res1 = finish_filter_avx2(r, sum_sec0);
      const __m256i diff1 = _mm256_sub_epi16(res1, ref16);
      v_sse1 = _mm256_add_epi32(v_sse1, _mm256_madd_epi16(diff1, diff1));

      __m256i max_u8 = r;
      __m256i min_s16 = r;
      const __m256i pn = tap_pair_sum_minmax_4x4_avx2(&in_row, po1, r, 11,
                                                      pri_d, &max_u8, &min_s16);
      const __m256i pf = tap_pair_sum_minmax_4x4_avx2(&in_row, po2, r, 11,
                                                      pri_d, &max_u8, &min_s16);
      const __m256i p_sum = _mm256_add_epi16(pn, pf);
      const __m256i sum_pri =
          _mm256_add_epi16(_mm256_slli_epi16(p_sum, 1), p_sum);

      const __m256i res2 = finish_filter_avx2(r, sum_pri);
      const __m256i diff2 = _mm256_sub_epi16(res2, ref16);
      v_sse2 = _mm256_add_epi32(v_sse2, _mm256_madd_epi16(diff2, diff2));

      const __m256i snd1 = tap_pair_sum_minmax_4x4_avx2(
          &in_row, s1o1, r, 2, sec_d, &max_u8, &min_s16);
      const __m256i snd2 = tap_pair_sum_minmax_4x4_avx2(
          &in_row, s2o1, r, 2, sec_d, &max_u8, &min_s16);
      const __m256i sec_neard = _mm256_add_epi16(snd1, snd2);
      __m256i sum_secd = _mm256_add_epi16(sec_neard, sec_neard);
      const __m256i sfd1 = tap_pair_sum_minmax_4x4_avx2(
          &in_row, s1o2, r, 2, sec_d, &max_u8, &min_s16);
      const __m256i sfd2 = tap_pair_sum_minmax_4x4_avx2(
          &in_row, s2o2, r, 2, sec_d, &max_u8, &min_s16);
      sum_secd = _mm256_add_epi16(sum_secd, _mm256_add_epi16(sfd1, sfd2));

      const __m256i sum_tot = _mm256_add_epi16(sum_pri, sum_secd);
      const __m256i max_s16 = _mm256_and_si256(
          max_u8, _mm256_set1_epi16((int16_t)~CDEF_VERY_LARGE));
      __m256i res3 = finish_filter_avx2(r, sum_tot);
      res3 = _mm256_min_epi16(_mm256_max_epi16(res3, min_s16), max_s16);
      const __m256i diff3 = _mm256_sub_epi16(res3, ref16);
      v_sse3 = _mm256_add_epi32(v_sse3, _mm256_madd_epi16(diff3, diff3));
    }
  }

  if (pli == 1) {
    mse[0] = hsum_u32_avx2(v_sse0);
    mse[1] = hsum_u32_avx2(v_sse1);
    mse[2] = hsum_u32_avx2(v_sse2);
    mse[3] = hsum_u32_avx2(v_sse3);
  } else {
    mse[0] += hsum_u32_avx2(v_sse0);
    mse[1] += hsum_u32_avx2(v_sse1);
    mse[2] += hsum_u32_avx2(v_sse2);
    mse[3] += hsum_u32_avx2(v_sse3);
  }
}

void av1_cdef_mse_calc_block_lvl4_luma_avx2(
    const uint16_t *in, const uint8_t *ref_buffer, int ref_stride, int row,
    int col, const cdef_list *dlist, int cdef_count,
    int dir[CDEF_NBLOCKS][CDEF_NBLOCKS], int var[CDEF_NBLOCKS][CDEF_NBLOCKS],
    int damping, int pli, int xdec, uint64_t mse[4]);

static inline __m256i load_u16_8x2(const uint16_t *p) {
  const __m128i lo = _mm_loadu_si128((const __m128i *)(p + CDEF_BSTRIDE));
  return _mm256_inserti128_si256(_mm256_castsi128_si256(lo),
                                 _mm_loadu_si128((const __m128i *)p), 1);
}

static inline __m256i load_ref16_8x2(const uint8_t *ref_row, int ref_stride) {
  const __m128i r0 = _mm_loadl_epi64((const __m128i *)ref_row);
  const __m128i r1 = _mm_loadl_epi64((const __m128i *)(ref_row + ref_stride));
  return _mm256_cvtepu8_epi16(_mm_unpacklo_epi64(r1, r0));
}

static inline __m256i tap_pair_sum_fast(__m256i t0, __m256i t1, __m256i r,
                                        __m256i v_thresh, __m128i v_damp) {
  const __m256i raw0 = _mm256_sub_epi16(t0, r);
  const __m256i raw1 = _mm256_sub_epi16(t1, r);
  const __m256i diff0 = _mm256_abs_epi16(raw0);
  const __m256i diff1 = _mm256_abs_epi16(raw1);
  const __m256i s0 =
      _mm256_subs_epu16(v_thresh, _mm256_srl_epi16(diff0, v_damp));
  const __m256i s1 =
      _mm256_subs_epu16(v_thresh, _mm256_srl_epi16(diff1, v_damp));
  const __m256i c0 = _mm256_sign_epi16(_mm256_min_epi16(diff0, s0), raw0);
  const __m256i c1 = _mm256_sign_epi16(_mm256_min_epi16(diff1, s1), raw1);
  return _mm256_add_epi16(c0, c1);
}

static inline __m256i tap_pair_sum_minmax_fast(__m256i t0, __m256i t1,
                                               __m256i r, __m256i v_thresh,
                                               __m128i v_damp, __m256i *max_u8,
                                               __m256i *min_s16) {
  *max_u8 = _mm256_max_epu8(*max_u8, _mm256_max_epu8(t0, t1));
  *min_s16 = _mm256_min_epi16(*min_s16, _mm256_min_epi16(t0, t1));
  return tap_pair_sum_fast(t0, t1, r, v_thresh, v_damp);
}

static inline __m256i finish_delta_fast(__m256i sum, __m256i c8) {
  sum = _mm256_add_epi16(sum, _mm256_srai_epi16(sum, 15));
  return _mm256_srai_epi16(_mm256_add_epi16(sum, c8), 4);
}

static inline __m256i calc_sse_fast(__m256i res, __m256i ref16) {
  const __m256i diff = _mm256_sub_epi16(res, ref16);
  return _mm256_madd_epi16(diff, diff);
}

void av1_cdef_mse_calc_block_lvl4_luma_avx2(
    const uint16_t *in, const uint8_t *ref_buffer, int ref_stride, int row,
    int col, const cdef_list *dlist, int cdef_count,
    int dir[CDEF_NBLOCKS][CDEF_NBLOCKS], int var[CDEF_NBLOCKS][CDEF_NBLOCKS],
    int damping, int pli, int xdec, uint64_t mse[4]) {
  if (pli > 0) {
    if (xdec != 2) {
      cdef_mse_calc_block_lvl4_chroma_4x4_avx2(in, ref_buffer, ref_stride, row,
                                               col, dlist, cdef_count, dir,
                                               damping, pli, mse);
      return;
    }
    damping -= 1;
  } else {
    cdef_find_dir_list_avx2(in, dlist, cdef_count, dir, var);
  }

  __m256i v_sse0 = _mm256_setzero_si256();
  __m256i v_sse1 = _mm256_setzero_si256();
  __m256i v_sse2 = _mm256_setzero_si256();
  __m256i v_sse3 = _mm256_setzero_si256();

  const __m256i c8 = _mm256_set1_epi16(8);
  const __m256i v_sec_thresh = _mm256_set1_epi16(2);
  const unsigned int sec_damping = AOMMAX(0, damping - 1);
  const __m128i v_sec_damp = _mm_cvtsi32_si128((int)sec_damping);

  for (int bi = 0; bi < cdef_count; ++bi) {
    const int by = dlist[bi].by;
    const int bx = dlist[bi].bx;
    const int d = dir[by][bx];
    const int v = var[by][bx];
    const int t =
        pli ? 11
            : (v ? (11 * (4 + (v >> 6 ? AOMMIN(get_msb(v >> 6), 12) : 0)) +
                    8) >>
                       4
                 : 0);
    const uint16_t *in_row = &in[(by * CDEF_BSTRIDE << 3) + (bx << 3)];
    const uint8_t *ref_row =
        &ref_buffer[(ptrdiff_t)(row + (by << 3)) * ref_stride +
                    (col + (bx << 3))];

    if (t == 0) {
      if ((d & 3) == 0) {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 1
#endif
        for (int i = 0; i < 8;
             i += 2, in_row += 2 * CDEF_BSTRIDE, ref_row += 2 * ref_stride) {
          const __m256i r = load_u16_8x2(in_row);
          const __m256i ref16 = load_ref16_8x2(ref_row, ref_stride);
          const __m256i diff0 = _mm256_sub_epi16(r, ref16);
          const __m256i sse0 = _mm256_madd_epi16(diff0, diff0);
          v_sse0 = _mm256_add_epi32(v_sse0, sse0);
          v_sse2 = _mm256_add_epi32(v_sse2, sse0);

          const __m256i sn1 = tap_pair_sum_fast(load_u16_8x2(in_row + 1),
                                                load_u16_8x2(in_row - 1), r,
                                                v_sec_thresh, v_sec_damp);
          const __m256i sn2 = tap_pair_sum_fast(
              load_u16_8x2(in_row + CDEF_BSTRIDE),
              load_u16_8x2(in_row - CDEF_BSTRIDE), r, v_sec_thresh, v_sec_damp);
          const __m256i sec_near = _mm256_add_epi16(sn1, sn2);
          __m256i sum_sec = _mm256_add_epi16(sec_near, sec_near);

          const __m256i sf1 = tap_pair_sum_fast(load_u16_8x2(in_row + 2),
                                                load_u16_8x2(in_row - 2), r,
                                                v_sec_thresh, v_sec_damp);
          sum_sec = _mm256_add_epi16(sum_sec, sf1);

          const __m256i sf2 =
              tap_pair_sum_fast(load_u16_8x2(in_row + 2 * CDEF_BSTRIDE),
                                load_u16_8x2(in_row - 2 * CDEF_BSTRIDE), r,
                                v_sec_thresh, v_sec_damp);
          sum_sec = _mm256_add_epi16(sum_sec, sf2);

          const __m256i diff1 =
              _mm256_add_epi16(diff0, finish_delta_fast(sum_sec, c8));
          const __m256i sse1 = _mm256_madd_epi16(diff1, diff1);
          v_sse1 = _mm256_add_epi32(v_sse1, sse1);
          v_sse3 = _mm256_add_epi32(v_sse3, sse1);
        }
      } else {
        const intptr_t s1o1 = cdef_directions_padded_inline[d + 4][0];
        const intptr_t s1o2 = cdef_directions_padded_inline[d + 4][1];
        const intptr_t s2o1 = cdef_directions_padded_inline[d][0];
        const intptr_t s2o2 = cdef_directions_padded_inline[d][1];
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 1
#endif
        for (int i = 0; i < 8;
             i += 2, in_row += 2 * CDEF_BSTRIDE, ref_row += 2 * ref_stride) {
          const __m256i r = load_u16_8x2(in_row);
          const __m256i ref16 = load_ref16_8x2(ref_row, ref_stride);
          const __m256i diff0 = _mm256_sub_epi16(r, ref16);
          const __m256i sse0 = _mm256_madd_epi16(diff0, diff0);
          v_sse0 = _mm256_add_epi32(v_sse0, sse0);
          v_sse2 = _mm256_add_epi32(v_sse2, sse0);

          const __m256i sn1 = tap_pair_sum_fast(load_u16_8x2(in_row + 1),
                                                load_u16_8x2(in_row - 1), r,
                                                v_sec_thresh, v_sec_damp);
          const __m256i sn2 = tap_pair_sum_fast(
              load_u16_8x2(in_row + CDEF_BSTRIDE),
              load_u16_8x2(in_row - CDEF_BSTRIDE), r, v_sec_thresh, v_sec_damp);
          const __m256i sec_near0 = _mm256_add_epi16(sn1, sn2);
          __m256i sum_sec0 = _mm256_add_epi16(sec_near0, sec_near0);

          const __m256i sf1 = tap_pair_sum_fast(load_u16_8x2(in_row + 2),
                                                load_u16_8x2(in_row - 2), r,
                                                v_sec_thresh, v_sec_damp);
          sum_sec0 = _mm256_add_epi16(sum_sec0, sf1);

          const __m256i sf2 =
              tap_pair_sum_fast(load_u16_8x2(in_row + 2 * CDEF_BSTRIDE),
                                load_u16_8x2(in_row - 2 * CDEF_BSTRIDE), r,
                                v_sec_thresh, v_sec_damp);
          sum_sec0 = _mm256_add_epi16(sum_sec0, sf2);

          const __m256i diff1 =
              _mm256_add_epi16(diff0, finish_delta_fast(sum_sec0, c8));
          v_sse1 = _mm256_add_epi32(v_sse1, _mm256_madd_epi16(diff1, diff1));

          const __m256i snd1 = tap_pair_sum_fast(load_u16_8x2(in_row + s1o1),
                                                 load_u16_8x2(in_row - s1o1), r,
                                                 v_sec_thresh, v_sec_damp);
          const __m256i snd2 = tap_pair_sum_fast(load_u16_8x2(in_row + s2o1),
                                                 load_u16_8x2(in_row - s2o1), r,
                                                 v_sec_thresh, v_sec_damp);
          const __m256i sec_neard = _mm256_add_epi16(snd1, snd2);
          __m256i sum_secd = _mm256_add_epi16(sec_neard, sec_neard);

          const __m256i sfd1 = tap_pair_sum_fast(load_u16_8x2(in_row + s1o2),
                                                 load_u16_8x2(in_row - s1o2), r,
                                                 v_sec_thresh, v_sec_damp);
          sum_secd = _mm256_add_epi16(sum_secd, sfd1);

          const __m256i sfd2 = tap_pair_sum_fast(load_u16_8x2(in_row + s2o2),
                                                 load_u16_8x2(in_row - s2o2), r,
                                                 v_sec_thresh, v_sec_damp);
          sum_secd = _mm256_add_epi16(sum_secd, sfd2);

          const __m256i diff3 =
              _mm256_add_epi16(diff0, finish_delta_fast(sum_secd, c8));
          v_sse3 = _mm256_add_epi32(v_sse3, _mm256_madd_epi16(diff3, diff3));
        }
      }
      continue;
    }

    const unsigned int pri_damping = AOMMAX(0, damping - get_msb(t));
    const __m128i v_pri_damp = _mm_cvtsi32_si128((int)pri_damping);
    const __m256i v_pri_thresh = _mm256_set1_epi16((int16_t)t);
    const intptr_t po1 = cdef_directions_padded_inline[d + 2][0];
    const intptr_t po2 = cdef_directions_padded_inline[d + 2][1];
    const int t_odd = t & 1;

    if ((d & 3) == 0) {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 1
#endif
      for (int i = 0; i < 8;
           i += 2, in_row += 2 * CDEF_BSTRIDE, ref_row += 2 * ref_stride) {
        const __m256i r = load_u16_8x2(in_row);
        const __m256i ref16 = load_ref16_8x2(ref_row, ref_stride);
        const __m256i diff0 = _mm256_sub_epi16(r, ref16);
        v_sse0 = _mm256_add_epi32(v_sse0, _mm256_madd_epi16(diff0, diff0));

        // 1. Filter 1: orthogonal secondary
        __m256i max_u8 = r;
        __m256i min_s16 = r;

        const __m256i sn1 = tap_pair_sum_minmax_fast(
            load_u16_8x2(in_row + 1), load_u16_8x2(in_row - 1), r, v_sec_thresh,
            v_sec_damp, &max_u8, &min_s16);
        const __m256i sn2 = tap_pair_sum_minmax_fast(
            load_u16_8x2(in_row + CDEF_BSTRIDE),
            load_u16_8x2(in_row - CDEF_BSTRIDE), r, v_sec_thresh, v_sec_damp,
            &max_u8, &min_s16);
        const __m256i sec_near = _mm256_add_epi16(sn1, sn2);
        __m256i sum_sec = _mm256_add_epi16(sec_near, sec_near);

        const __m256i sf1 = tap_pair_sum_minmax_fast(
            load_u16_8x2(in_row + 2), load_u16_8x2(in_row - 2), r, v_sec_thresh,
            v_sec_damp, &max_u8, &min_s16);
        sum_sec = _mm256_add_epi16(sum_sec, sf1);

        const __m256i sf2 = tap_pair_sum_minmax_fast(
            load_u16_8x2(in_row + 2 * CDEF_BSTRIDE),
            load_u16_8x2(in_row - 2 * CDEF_BSTRIDE), r, v_sec_thresh,
            v_sec_damp, &max_u8, &min_s16);
        sum_sec = _mm256_add_epi16(sum_sec, sf2);

        const __m256i diff1 =
            _mm256_add_epi16(diff0, finish_delta_fast(sum_sec, c8));
        v_sse1 = _mm256_add_epi32(v_sse1, _mm256_madd_epi16(diff1, diff1));

        // 2. Filter 2: primary
        const __m256i pn = tap_pair_sum_minmax_fast(
            load_u16_8x2(in_row + po1), load_u16_8x2(in_row - po1), r,
            v_pri_thresh, v_pri_damp, &max_u8, &min_s16);
        const __m256i pf = tap_pair_sum_minmax_fast(
            load_u16_8x2(in_row + po2), load_u16_8x2(in_row - po2), r,
            v_pri_thresh, v_pri_damp, &max_u8, &min_s16);
        const __m256i p_sum = _mm256_add_epi16(pn, pf);
        const __m256i sum_pri =
            t_odd ? _mm256_add_epi16(_mm256_slli_epi16(p_sum, 1), p_sum)
                  : _mm256_add_epi16(_mm256_slli_epi16(p_sum, 1),
                                     _mm256_slli_epi16(pn, 1));

        const __m256i diff2 =
            _mm256_add_epi16(diff0, finish_delta_fast(sum_pri, c8));
        v_sse2 = _mm256_add_epi32(v_sse2, _mm256_madd_epi16(diff2, diff2));

        // 3. Combined
        const __m256i sum_tot = _mm256_add_epi16(sum_pri, sum_sec);
        const __m256i max_s16 =
            _mm256_srli_epi16(_mm256_slli_epi16(max_u8, 2), 2);
        __m256i res3 = _mm256_add_epi16(r, finish_delta_fast(sum_tot, c8));
        res3 = _mm256_min_epi16(_mm256_max_epi16(res3, min_s16), max_s16);
        v_sse3 = _mm256_add_epi32(v_sse3, calc_sse_fast(res3, ref16));
      }
    } else {
      const intptr_t s1o1 = cdef_directions_padded_inline[d + 4][0];
      const intptr_t s1o2 = cdef_directions_padded_inline[d + 4][1];
      const intptr_t s2o1 = cdef_directions_padded_inline[d][0];
      const intptr_t s2o2 = cdef_directions_padded_inline[d][1];
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 1
#endif
      for (int i = 0; i < 8;
           i += 2, in_row += 2 * CDEF_BSTRIDE, ref_row += 2 * ref_stride) {
        const __m256i r = load_u16_8x2(in_row);
        const __m256i ref16 = load_ref16_8x2(ref_row, ref_stride);
        const __m256i diff0 = _mm256_sub_epi16(r, ref16);
        v_sse0 = _mm256_add_epi32(v_sse0, _mm256_madd_epi16(diff0, diff0));

        // 1. Filter 1: orthogonal secondary
        const __m256i sn1 = tap_pair_sum_fast(load_u16_8x2(in_row + 1),
                                              load_u16_8x2(in_row - 1), r,
                                              v_sec_thresh, v_sec_damp);
        const __m256i sn2 = tap_pair_sum_fast(
            load_u16_8x2(in_row + CDEF_BSTRIDE),
            load_u16_8x2(in_row - CDEF_BSTRIDE), r, v_sec_thresh, v_sec_damp);
        const __m256i sec_near0 = _mm256_add_epi16(sn1, sn2);
        __m256i sum_sec0 = _mm256_add_epi16(sec_near0, sec_near0);

        const __m256i sf1 = tap_pair_sum_fast(load_u16_8x2(in_row + 2),
                                              load_u16_8x2(in_row - 2), r,
                                              v_sec_thresh, v_sec_damp);
        sum_sec0 = _mm256_add_epi16(sum_sec0, sf1);

        const __m256i sf2 =
            tap_pair_sum_fast(load_u16_8x2(in_row + 2 * CDEF_BSTRIDE),
                              load_u16_8x2(in_row - 2 * CDEF_BSTRIDE), r,
                              v_sec_thresh, v_sec_damp);
        sum_sec0 = _mm256_add_epi16(sum_sec0, sf2);

        const __m256i diff1 =
            _mm256_add_epi16(diff0, finish_delta_fast(sum_sec0, c8));
        v_sse1 = _mm256_add_epi32(v_sse1, _mm256_madd_epi16(diff1, diff1));

        // 2. Filter 2: primary
        __m256i max_u8 = r;
        __m256i min_s16 = r;

        const __m256i pn = tap_pair_sum_minmax_fast(
            load_u16_8x2(in_row + po1), load_u16_8x2(in_row - po1), r,
            v_pri_thresh, v_pri_damp, &max_u8, &min_s16);
        const __m256i pf = tap_pair_sum_minmax_fast(
            load_u16_8x2(in_row + po2), load_u16_8x2(in_row - po2), r,
            v_pri_thresh, v_pri_damp, &max_u8, &min_s16);
        const __m256i p_sum = _mm256_add_epi16(pn, pf);
        const __m256i sum_pri =
            t_odd ? _mm256_add_epi16(_mm256_slli_epi16(p_sum, 1), p_sum)
                  : _mm256_add_epi16(_mm256_slli_epi16(p_sum, 1),
                                     _mm256_slli_epi16(pn, 1));

        const __m256i diff2 =
            _mm256_add_epi16(diff0, finish_delta_fast(sum_pri, c8));
        v_sse2 = _mm256_add_epi32(v_sse2, _mm256_madd_epi16(diff2, diff2));

        // 3. Filter 3: directional secondary + combined
        const __m256i snd1 = tap_pair_sum_minmax_fast(
            load_u16_8x2(in_row + s1o1), load_u16_8x2(in_row - s1o1), r,
            v_sec_thresh, v_sec_damp, &max_u8, &min_s16);
        const __m256i snd2 = tap_pair_sum_minmax_fast(
            load_u16_8x2(in_row + s2o1), load_u16_8x2(in_row - s2o1), r,
            v_sec_thresh, v_sec_damp, &max_u8, &min_s16);
        const __m256i sec_neard = _mm256_add_epi16(snd1, snd2);
        __m256i sum_secd = _mm256_add_epi16(sec_neard, sec_neard);

        const __m256i sfd1 = tap_pair_sum_minmax_fast(
            load_u16_8x2(in_row + s1o2), load_u16_8x2(in_row - s1o2), r,
            v_sec_thresh, v_sec_damp, &max_u8, &min_s16);
        sum_secd = _mm256_add_epi16(sum_secd, sfd1);

        const __m256i sfd2 = tap_pair_sum_minmax_fast(
            load_u16_8x2(in_row + s2o2), load_u16_8x2(in_row - s2o2), r,
            v_sec_thresh, v_sec_damp, &max_u8, &min_s16);
        sum_secd = _mm256_add_epi16(sum_secd, sfd2);

        const __m256i sum_tot = _mm256_add_epi16(sum_pri, sum_secd);
        const __m256i max_s16 =
            _mm256_srli_epi16(_mm256_slli_epi16(max_u8, 2), 2);
        __m256i res3 = _mm256_add_epi16(r, finish_delta_fast(sum_tot, c8));
        res3 = _mm256_min_epi16(_mm256_max_epi16(res3, min_s16), max_s16);
        v_sse3 = _mm256_add_epi32(v_sse3, calc_sse_fast(res3, ref16));
      }
    }
  }

  if (pli < 2) {
    mse[0] = hsum_u32_avx2(v_sse0);
    mse[1] = hsum_u32_avx2(v_sse1);
    mse[2] = hsum_u32_avx2(v_sse2);
    mse[3] = hsum_u32_avx2(v_sse3);
  } else {
    mse[0] += hsum_u32_avx2(v_sse0);
    mse[1] += hsum_u32_avx2(v_sse1);
    mse[2] += hsum_u32_avx2(v_sse2);
    mse[3] += hsum_u32_avx2(v_sse3);
  }
}
