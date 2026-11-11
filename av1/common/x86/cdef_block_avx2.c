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

#include <assert.h>

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
static inline __m256i compute_directions_avx2(__m256i *lines) {
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

  return hsum4_avx2(&partial4a, &partial5a, &partial6, &partial7a);
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
  const __m256i const_128_reg = _mm256_set1_epi16(128);
  __m256i lines[8];
  if (coeff_shift == 0) {
    for (int i = 0; i < 8; i++) {
      const __m128i src_1 = _mm_loadu_si128((const __m128i *)&img1[i * stride]);
      const __m128i src_2 = _mm_loadu_si128((const __m128i *)&img2[i * stride]);
      const __m256i line =
          _mm256_insertf128_si256(_mm256_castsi128_si256(src_1), src_2, 1);
      lines[i] = _mm256_sub_epi16(line, const_128_reg);
    }
  } else {
    const __m128i const_coeff_shift_reg = _mm_cvtsi32_si128(coeff_shift);
    for (int i = 0; i < 8; i++) {
      const __m128i src_1 = _mm_loadu_si128((const __m128i *)&img1[i * stride]);
      const __m128i src_2 = _mm_loadu_si128((const __m128i *)&img2[i * stride]);
      const __m256i line =
          _mm256_insertf128_si256(_mm256_castsi128_si256(src_1), src_2, 1);
      lines[i] = _mm256_sub_epi16(_mm256_sra_epi16(line, const_coeff_shift_reg),
                                  const_128_reg);
    }
  }

  /* Compute "mostly vertical" directions. */
  const __m256i dir47 = compute_directions_avx2(lines);

  /* Transpose and reverse the order of the lines. */
  array_reverse_transpose_8x8_avx2(lines, lines);

  /* Compute "mostly horizontal" directions. */
  const __m256i dir03 = compute_directions_avx2(lines);

  int32_t costs[16];
  _mm256_storeu_si256((__m256i *)costs, dir03);
  _mm256_storeu_si256((__m256i *)(costs + 8), dir47);

  __m256i max = _mm256_max_epi32(dir03, dir47);
  max =
      _mm256_max_epi32(max, _mm256_shuffle_epi32(max, _MM_SHUFFLE(1, 0, 3, 2)));
  max =
      _mm256_max_epi32(max, _mm256_shuffle_epi32(max, _MM_SHUFFLE(2, 3, 0, 1)));

  const __m256i eq03 = _mm256_cmpeq_epi32(max, dir03);
  const __m256i eq47 = _mm256_cmpeq_epi32(max, dir47);
  const __m256i packed16 = _mm256_packs_epi32(eq03, eq47);
  const uint32_t eq_mask =
      (uint32_t)_mm256_movemask_epi8(_mm256_packs_epi16(packed16, packed16));

  int dir0, dir1;
#if defined(__GNUC__) || defined(__clang__)
  dir0 = __builtin_ctz(eq_mask);
  dir1 = __builtin_ctz(eq_mask >> 16);
#else
  const uint32_t m0 = eq_mask & 0xffU;
  const uint32_t m1 = (eq_mask >> 16) & 0xffU;
  dir0 = get_msb(m0 ^ (m0 - 1));  // Count trailing zeros
  dir1 = get_msb(m1 ^ (m1 - 1));  // Count trailing zeros
#endif

  const int32_t best0 = _mm_cvtsi128_si32(_mm256_castsi256_si128(max));
  const int32_t best1 = _mm_cvtsi128_si32(_mm256_extractf128_si256(max, 1));
  const int32_t *orth_costs0 = (dir0 & 4) ? costs : costs + 8;
  const int32_t *orth_costs1 = (dir1 & 4) ? costs + 4 : costs + 12;

  /* Difference between the optimal variance and the variance along the
     orthogonal direction. Again, the sum(x^2) terms cancel out. */
  *var_out_1st = (best0 - orth_costs0[dir0 & 3]) >> 10;
  *var_out_2nd = (best1 - orth_costs1[dir1 & 3]) >> 10;
  *out_dir_1st_8x8 = dir0;
  *out_dir_2nd_8x8 = dir1;
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

#if CONFIG_AV1_ENCODER
DECLARE_ALIGNED(32, static const uint8_t, k_shmask_u8_avx2[5][32]) = {
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
  { 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f },
  { 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
    0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
    0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f },
  { 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
    0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
    0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f },
  { 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
    0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
    0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f },
};

static inline __m256i constrain16_vecs(__m256i a, __m256i b, __m256i v_thresh,
                                       __m128i v_damp) {
  const __m256i raw = _mm256_sub_epi16(a, b);
  const __m256i ad = _mm256_abs_epi16(raw);
  const __m256i s = _mm256_subs_epu16(v_thresh, _mm256_srl_epi16(ad, v_damp));
  return _mm256_sign_epi16(_mm256_min_epi16(ad, s), raw);
}

static inline __m256i finish_delta_fast(__m256i sum) {
  sum = _mm256_add_epi16(sum, _mm256_srai_epi16(sum, 15));
  return _mm256_srai_epi16(_mm256_add_epi16(sum, _mm256_set1_epi16(8)), 4);
}

static inline __m256i finish_clamped_diff3(__m256i r, __m256i diff0,
                                           __m256i sum_tot, __m256i max_u8,
                                           __m256i min_s16) {
  const __m256i max_s16 = _mm256_srli_epi16(_mm256_slli_epi16(max_u8, 2), 2);
  __m256i res3 = _mm256_add_epi16(r, finish_delta_fast(sum_tot));
  res3 = _mm256_min_epi16(_mm256_max_epi16(res3, min_s16), max_s16);
  return _mm256_add_epi16(_mm256_sub_epi16(res3, r), diff0);
}

static inline __m256i load_tap_4x4(const uint16_t *p) {
  return v256_from_v64(v64_load_unaligned(&p[0 * CDEF_BSTRIDE]),
                       v64_load_unaligned(&p[1 * CDEF_BSTRIDE]),
                       v64_load_unaligned(&p[2 * CDEF_BSTRIDE]),
                       v64_load_unaligned(&p[3 * CDEF_BSTRIDE]));
}

static inline __m256i tap_pair_sum_4x4_avx2(const uint16_t *in_row,
                                            intptr_t off, __m256i r,
                                            __m256i v_thresh, __m128i v_damp) {
  const __m256i c0 =
      constrain16_vecs(load_tap_4x4(in_row + off), r, v_thresh, v_damp);
  const __m256i c1 =
      constrain16_vecs(load_tap_4x4(in_row - off), r, v_thresh, v_damp);
  return _mm256_add_epi16(c0, c1);
}

static inline __m256i tap_pair_sum_minmax_4x4_avx2(
    const uint16_t *in_row, intptr_t off, __m256i r, __m256i v_thresh,
    __m128i v_damp, __m256i *max_u8, __m256i *min_s16) {
  const __m256i tap0 = load_tap_4x4(in_row + off);
  *max_u8 = _mm256_max_epu8(*max_u8, tap0);
  *min_s16 = _mm256_min_epi16(*min_s16, tap0);
  const __m256i c0 = constrain16_vecs(tap0, r, v_thresh, v_damp);
  const __m256i tap1 = load_tap_4x4(in_row - off);
  *max_u8 = _mm256_max_epu8(*max_u8, tap1);
  *min_s16 = _mm256_min_epi16(*min_s16, tap1);
  const __m256i c1 = constrain16_vecs(tap1, r, v_thresh, v_damp);
  return _mm256_add_epi16(c0, c1);
}

static inline int load_s32_unaligned(const uint8_t *p) {
  int v;
  memcpy(&v, p, sizeof(v));
  return v;
}

static inline __m256i load_ref16_4x4(const uint8_t *ref_row,
                                     intptr_t ref_stride) {
  return _mm256_cvtepu8_epi16(
      _mm_set_epi32(load_s32_unaligned(&ref_row[0 * ref_stride]),
                    load_s32_unaligned(&ref_row[1 * ref_stride]),
                    load_s32_unaligned(&ref_row[2 * ref_stride]),
                    load_s32_unaligned(&ref_row[3 * ref_stride])));
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
    const uint16_t *p = &in[8 * by * CDEF_BSTRIDE + 8 * bx];
    int32_t dummy_var;
    int dummy_dir;
    cdef_find_dir_dual_avx2(p, p, CDEF_BSTRIDE, &var[by][bx], &dummy_var, 0,
                            &dir[by][bx], &dummy_dir);
  }
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void cdef_mse_calc_block_lvl4_chroma_4x4_avx2(
    const uint16_t *in, const uint8_t *ref_base, int ref_stride,
    const cdef_list *dlist, int cdef_count,
    const int dir[CDEF_NBLOCKS][CDEF_NBLOCKS], int damping, int pli,
    uint64_t mse[4]) {
  __m256i v_sse0 = _mm256_setzero_si256();
  __m256i v_sse1 = _mm256_setzero_si256();
  __m256i v_sse2 = _mm256_setzero_si256();
  __m256i v_sse3 = _mm256_setzero_si256();

  const __m256i v_sec_thresh = _mm256_set1_epi16(2);
  const __m256i v_pri_thresh = _mm256_set1_epi16(11);
  const __m128i v_sec_damp = _mm_cvtsi32_si128(AOMMAX(0, damping - 2));
  const __m128i v_pri_damp = _mm_cvtsi32_si128(AOMMAX(0, damping - 4));

  for (int bi = 0; bi < cdef_count; ++bi) {
    const int by = dlist[bi].by;
    const int bx = dlist[bi].bx;
    const int d = dir[by][bx];
    const uint16_t *in_row = &in[(by * CDEF_BSTRIDE << 2) + (bx << 2)];
    const uint8_t *ref_row =
        &ref_base[(ptrdiff_t)(by << 2) * ref_stride + (bx << 2)];
    const intptr_t po1 = cdef_directions_padded_inline[d + 2][0];
    const intptr_t po2 = cdef_directions_padded_inline[d + 2][1];

    const __m256i r = load_tap_4x4(in_row);
    const __m256i diff0 =
        _mm256_sub_epi16(r, load_ref16_4x4(ref_row, ref_stride));
    v_sse0 = _mm256_add_epi32(v_sse0, _mm256_madd_epi16(diff0, diff0));

    if ((d & 3) == 0) {
      __m256i max_u8 = r;
      __m256i min_s16 = r;

      const __m256i sn1 = tap_pair_sum_minmax_4x4_avx2(
          in_row, 1, r, v_sec_thresh, v_sec_damp, &max_u8, &min_s16);
      const __m256i sn2 = tap_pair_sum_minmax_4x4_avx2(
          in_row, CDEF_BSTRIDE, r, v_sec_thresh, v_sec_damp, &max_u8, &min_s16);
      __m256i sum_sec = _mm256_slli_epi16(_mm256_add_epi16(sn1, sn2), 1);
      sum_sec = _mm256_add_epi16(
          sum_sec, tap_pair_sum_minmax_4x4_avx2(in_row, 2, r, v_sec_thresh,
                                                v_sec_damp, &max_u8, &min_s16));
      sum_sec = _mm256_add_epi16(
          sum_sec, tap_pair_sum_minmax_4x4_avx2(in_row, 2 * CDEF_BSTRIDE, r,
                                                v_sec_thresh, v_sec_damp,
                                                &max_u8, &min_s16));
      const __m256i diff1 = _mm256_add_epi16(diff0, finish_delta_fast(sum_sec));
      v_sse1 = _mm256_add_epi32(v_sse1, _mm256_madd_epi16(diff1, diff1));

      const __m256i pn = tap_pair_sum_minmax_4x4_avx2(
          in_row, po1, r, v_pri_thresh, v_pri_damp, &max_u8, &min_s16);
      const __m256i pf = tap_pair_sum_minmax_4x4_avx2(
          in_row, po2, r, v_pri_thresh, v_pri_damp, &max_u8, &min_s16);
      const __m256i p_sum = _mm256_add_epi16(pn, pf);
      const __m256i sum_pri =
          _mm256_add_epi16(_mm256_slli_epi16(p_sum, 1), p_sum);
      const __m256i diff2 = _mm256_add_epi16(diff0, finish_delta_fast(sum_pri));
      v_sse2 = _mm256_add_epi32(v_sse2, _mm256_madd_epi16(diff2, diff2));

      const __m256i diff3 = finish_clamped_diff3(
          r, diff0, _mm256_add_epi16(sum_sec, sum_pri), max_u8, min_s16);
      v_sse3 = _mm256_add_epi32(v_sse3, _mm256_madd_epi16(diff3, diff3));
    } else {
      const intptr_t s1o1 = cdef_directions_padded_inline[d + 4][0];
      const intptr_t s1o2 = cdef_directions_padded_inline[d + 4][1];
      const intptr_t s2o1 = cdef_directions_padded_inline[d][0];
      const intptr_t s2o2 = cdef_directions_padded_inline[d][1];

      const __m256i sn1 =
          tap_pair_sum_4x4_avx2(in_row, 1, r, v_sec_thresh, v_sec_damp);
      const __m256i sn2 = tap_pair_sum_4x4_avx2(in_row, CDEF_BSTRIDE, r,
                                                v_sec_thresh, v_sec_damp);
      __m256i sum_sec0 = _mm256_slli_epi16(_mm256_add_epi16(sn1, sn2), 1);
      sum_sec0 = _mm256_add_epi16(
          sum_sec0,
          tap_pair_sum_4x4_avx2(in_row, 2, r, v_sec_thresh, v_sec_damp));
      sum_sec0 = _mm256_add_epi16(
          sum_sec0, tap_pair_sum_4x4_avx2(in_row, 2 * CDEF_BSTRIDE, r,
                                          v_sec_thresh, v_sec_damp));
      const __m256i diff1 =
          _mm256_add_epi16(diff0, finish_delta_fast(sum_sec0));
      v_sse1 = _mm256_add_epi32(v_sse1, _mm256_madd_epi16(diff1, diff1));

      __m256i max_u8 = r;
      __m256i min_s16 = r;
      const __m256i snd1 = tap_pair_sum_minmax_4x4_avx2(
          in_row, s1o1, r, v_sec_thresh, v_sec_damp, &max_u8, &min_s16);
      const __m256i snd2 = tap_pair_sum_minmax_4x4_avx2(
          in_row, s2o1, r, v_sec_thresh, v_sec_damp, &max_u8, &min_s16);
      __m256i sum_sec3 = _mm256_slli_epi16(_mm256_add_epi16(snd1, snd2), 1);
      sum_sec3 = _mm256_add_epi16(sum_sec3, tap_pair_sum_minmax_4x4_avx2(
                                                in_row, s1o2, r, v_sec_thresh,
                                                v_sec_damp, &max_u8, &min_s16));
      sum_sec3 = _mm256_add_epi16(sum_sec3, tap_pair_sum_minmax_4x4_avx2(
                                                in_row, s2o2, r, v_sec_thresh,
                                                v_sec_damp, &max_u8, &min_s16));

      const __m256i pn = tap_pair_sum_minmax_4x4_avx2(
          in_row, po1, r, v_pri_thresh, v_pri_damp, &max_u8, &min_s16);
      const __m256i pf = tap_pair_sum_minmax_4x4_avx2(
          in_row, po2, r, v_pri_thresh, v_pri_damp, &max_u8, &min_s16);
      const __m256i p_sum = _mm256_add_epi16(pn, pf);
      const __m256i sum_pri =
          _mm256_add_epi16(_mm256_slli_epi16(p_sum, 1), p_sum);
      const __m256i diff2 = _mm256_add_epi16(diff0, finish_delta_fast(sum_pri));
      v_sse2 = _mm256_add_epi32(v_sse2, _mm256_madd_epi16(diff2, diff2));

      const __m256i diff3 = finish_clamped_diff3(
          r, diff0, _mm256_add_epi16(sum_sec3, sum_pri), max_u8, min_s16);
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

static inline __m256i load_u16_8x2(const uint16_t *p) {
  const __m128i lo = _mm_loadu_si128((const __m128i *)(p + CDEF_BSTRIDE));
  return _mm256_inserti128_si256(_mm256_castsi128_si256(lo),
                                 _mm_loadu_si128((const __m128i *)p), 1);
}

static inline __m256i load_ref16_8x2(const uint8_t *ref_row,
                                     intptr_t ref_stride) {
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

static inline __m256i tap_pair_sum_off(const uint16_t *in_row, intptr_t off,
                                       __m256i r, __m256i v_thresh,
                                       __m128i v_damp) {
  const __m256i t0 = load_u16_8x2(in_row + off);
  const __m256i t1 = load_u16_8x2(in_row - off);
  return tap_pair_sum_fast(t0, t1, r, v_thresh, v_damp);
}

static inline __m256i tap_pair_sum_minmax_off(const uint16_t *in_row,
                                              intptr_t off, __m256i r,
                                              __m256i v_thresh, __m128i v_damp,
                                              __m256i *max_u8,
                                              __m256i *min_s16) {
  const __m256i t0 = load_u16_8x2(in_row + off);
  const __m256i t1 = load_u16_8x2(in_row - off);
  *max_u8 = _mm256_max_epu8(*max_u8, _mm256_max_epu8(t0, t1));
  *min_s16 = _mm256_min_epi16(*min_s16, _mm256_min_epi16(t0, t1));
  return tap_pair_sum_fast(t0, t1, r, v_thresh, v_damp);
}

static inline __m256i combine_pri_branchless(__m256i pn, __m256i pf,
                                             __m256i v_pri_thresh) {
  const __m256i odd_mask =
      _mm256_srai_epi16(_mm256_slli_epi16(v_pri_thresh, 15), 15);
  const __m256i base = _mm256_add_epi16(
      _mm256_slli_epi16(_mm256_add_epi16(pn, pf), 1), _mm256_slli_epi16(pn, 1));
  const __m256i corr = _mm256_and_si256(_mm256_sub_epi16(pf, pn), odd_mask);
  return _mm256_add_epi16(base, corr);
}

static inline __m256i load_ref8_8x4(const uint8_t *rr, intptr_t ref_stride) {
  const __m128i r0 = _mm_loadl_epi64((const __m128i *)rr);
  const __m128i r1 = _mm_loadl_epi64((const __m128i *)(rr + ref_stride));
  const __m128i r2 = _mm_loadl_epi64((const __m128i *)(rr + 2 * ref_stride));
  const __m128i r3 = _mm_loadl_epi64((const __m128i *)(rr + 3 * ref_stride));
  const __m128i lo = _mm_unpacklo_epi64(r1, r3);
  const __m128i hi = _mm_unpacklo_epi64(r0, r2);
  return _mm256_inserti128_si256(_mm256_castsi128_si256(lo), hi, 1);
}

static inline __m256i unpack_i8_lo_s16(__m256i v8) {
  return _mm256_srai_epi16(_mm256_unpacklo_epi8(v8, v8), 8);
}

static inline __m256i unpack_i8_hi_s16(__m256i v8) {
  return _mm256_srai_epi16(_mm256_unpackhi_epi8(v8, v8), 8);
}

static inline __m256i constrain_tap_4x8_i8(__m256i raw8, __m256i v_thresh,
                                           __m128i v_shift, __m256i v_shmask) {
  const __m256i ad = _mm256_abs_epi8(raw8);
  const __m256i sh = _mm256_and_si256(_mm256_srl_epi16(ad, v_shift), v_shmask);
  const __m256i sv = _mm256_subs_epu8(v_thresh, sh);
  return _mm256_sign_epi8(_mm256_min_epu8(ad, sv), raw8);
}

static inline __m256i tap_1_4r_i8(const uint16_t *p_tap, __m256i r_01,
                                  __m256i r_23, __m256i v_thresh,
                                  __m128i v_shift, __m256i v_shmask) {
  const __m256i d01 = _mm256_sub_epi16(load_u16_8x2(p_tap), r_01);
  const __m256i d23 =
      _mm256_sub_epi16(load_u16_8x2(p_tap + 2 * CDEF_BSTRIDE), r_23);
  return constrain_tap_4x8_i8(_mm256_packs_epi16(d01, d23), v_thresh, v_shift,
                              v_shmask);
}

static inline __m256i tap_1_4r_minmax_i8(const uint16_t *p_tap, __m256i r_01,
                                         __m256i r_23, __m256i v_thresh,
                                         __m128i v_shift, __m256i v_shmask,
                                         __m256i *max_u8, __m256i *min_u8) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  const __m256i t_01 = load_u16_8x2(p_tap);
  const __m256i t_23 = load_u16_8x2(p_tap + 2 * CDEF_BSTRIDE);
  const __m256i raw8 = _mm256_packs_epi16(_mm256_sub_epi16(t_01, r_01),
                                          _mm256_sub_epi16(t_23, r_23));
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(t_01, t_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(t_01, mask_00ff),
                                   _mm256_and_si256(t_23, mask_00ff)));
  return constrain_tap_4x8_i8(raw8, v_thresh, v_shift, v_shmask);
}

static inline __m256i tap_pair_4r_i8(const uint16_t *p, intptr_t off,
                                     __m256i r_01, __m256i r_23,
                                     __m256i v_thresh, __m128i v_shift,
                                     __m256i v_shmask) {
  const __m256i c0 =
      tap_1_4r_i8(p + off, r_01, r_23, v_thresh, v_shift, v_shmask);
  const __m256i c1 =
      tap_1_4r_i8(p - off, r_01, r_23, v_thresh, v_shift, v_shmask);
  return _mm256_add_epi8(c0, c1);
}

static inline __m256i tap_pair_4r_acc_i8(const uint16_t *p, intptr_t off,
                                         __m256i r_01, __m256i r_23,
                                         __m256i v_thresh, __m128i v_shift,
                                         __m256i v_shmask, __m256i acc) {
  const __m256i sum = _mm256_add_epi8(
      acc, tap_1_4r_i8(p + off, r_01, r_23, v_thresh, v_shift, v_shmask));
  return _mm256_add_epi8(
      sum, tap_1_4r_i8(p - off, r_01, r_23, v_thresh, v_shift, v_shmask));
}

static inline __m256i tap_pair_4r_minmax_i8(const uint16_t *p, intptr_t off,
                                            __m256i r_01, __m256i r_23,
                                            __m256i v_thresh, __m128i v_shift,
                                            __m256i v_shmask, __m256i *max_u8,
                                            __m256i *min_u8) {
  const __m256i c0 = tap_1_4r_minmax_i8(p + off, r_01, r_23, v_thresh, v_shift,
                                        v_shmask, max_u8, min_u8);
  const __m256i c1 = tap_1_4r_minmax_i8(p - off, r_01, r_23, v_thresh, v_shift,
                                        v_shmask, max_u8, min_u8);
  return _mm256_add_epi8(c0, c1);
}

static inline __m256i tap_pair_4r_acc_minmax_i8(
    const uint16_t *p, intptr_t off, __m256i r_01, __m256i r_23,
    __m256i v_thresh, __m128i v_shift, __m256i v_shmask, __m256i *max_u8,
    __m256i *min_u8, __m256i acc) {
  const __m256i sum = _mm256_add_epi8(
      acc, tap_1_4r_minmax_i8(p + off, r_01, r_23, v_thresh, v_shift, v_shmask,
                              max_u8, min_u8));
  return _mm256_add_epi8(
      sum, tap_1_4r_minmax_i8(p - off, r_01, r_23, v_thresh, v_shift, v_shmask,
                              max_u8, min_u8));
}

static inline void finish_sec_4r_i8(__m256i r_01, __m256i r_23, __m256i ref8,
                                    __m256i sum_sec0, __m256i *v_sse1) {
  const __m256i zero = _mm256_setzero_si256();
  const __m256i res1_01 =
      _mm256_add_epi16(r_01, finish_delta_fast(unpack_i8_lo_s16(sum_sec0)));
  const __m256i diff1_01 =
      _mm256_sub_epi16(res1_01, _mm256_unpacklo_epi8(ref8, zero));
  *v_sse1 = _mm256_add_epi32(*v_sse1, _mm256_madd_epi16(diff1_01, diff1_01));
  const __m256i res1_23 =
      _mm256_add_epi16(r_23, finish_delta_fast(unpack_i8_hi_s16(sum_sec0)));
  const __m256i diff1_23 =
      _mm256_sub_epi16(res1_23, _mm256_unpackhi_epi8(ref8, zero));
  *v_sse1 = _mm256_add_epi32(*v_sse1, _mm256_madd_epi16(diff1_23, diff1_23));
}

static inline void finish_pri_tot_4r_i8(__m256i r_01, __m256i r_23,
                                        __m256i ref8, __m256i sum_pri,
                                        __m256i sum_sec3, __m256i max_u8,
                                        __m256i min_u8, __m256i *v_sse2,
                                        __m256i *v_sse3) {
  const __m256i zero = _mm256_setzero_si256();
  const __m256i ref16_01 = _mm256_unpacklo_epi8(ref8, zero);
  const __m256i pri_01 = unpack_i8_lo_s16(sum_pri);
  const __m256i diff2_01 = _mm256_sub_epi16(
      _mm256_add_epi16(r_01, finish_delta_fast(pri_01)), ref16_01);
  *v_sse2 = _mm256_add_epi32(*v_sse2, _mm256_madd_epi16(diff2_01, diff2_01));

  const __m256i tot_01 = _mm256_add_epi16(pri_01, unpack_i8_lo_s16(sum_sec3));
  __m256i res3_01 = _mm256_add_epi16(r_01, finish_delta_fast(tot_01));
  res3_01 = _mm256_min_epi16(
      _mm256_max_epi16(res3_01, _mm256_unpacklo_epi8(min_u8, zero)),
      _mm256_unpacklo_epi8(max_u8, zero));
  const __m256i diff3_01 = _mm256_sub_epi16(res3_01, ref16_01);
  *v_sse3 = _mm256_add_epi32(*v_sse3, _mm256_madd_epi16(diff3_01, diff3_01));

  const __m256i ref16_23 = _mm256_unpackhi_epi8(ref8, zero);
  const __m256i pri_23 = unpack_i8_hi_s16(sum_pri);
  const __m256i diff2_23 = _mm256_sub_epi16(
      _mm256_add_epi16(r_23, finish_delta_fast(pri_23)), ref16_23);
  *v_sse2 = _mm256_add_epi32(*v_sse2, _mm256_madd_epi16(diff2_23, diff2_23));

  const __m256i tot_23 = _mm256_add_epi16(pri_23, unpack_i8_hi_s16(sum_sec3));
  __m256i res3_23 = _mm256_add_epi16(r_23, finish_delta_fast(tot_23));
  res3_23 = _mm256_min_epi16(
      _mm256_max_epi16(res3_23, _mm256_unpackhi_epi8(min_u8, zero)),
      _mm256_unpackhi_epi8(max_u8, zero));
  const __m256i diff3_23 = _mm256_sub_epi16(res3_23, ref16_23);
  *v_sse3 = _mm256_add_epi32(*v_sse3, _mm256_madd_epi16(diff3_23, diff3_23));
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void cdef_mse_calc_block_lvl4_chroma_8x8_fast_avx2(
    const uint16_t *in, const uint8_t *ref_base, int ref_stride,
    const cdef_list *dlist, int cdef_count,
    const int dir[CDEF_NBLOCKS][CDEF_NBLOCKS], int sec_damp_val,
    int pri_damp_val, int pli, uint64_t mse[4]) {
  __m256i v_sse0 = _mm256_setzero_si256();
  __m256i v_sse1 = _mm256_setzero_si256();
  __m256i v_sse2 = _mm256_setzero_si256();
  __m256i v_sse3 = _mm256_setzero_si256();

  const __m256i v_sec_thresh = _mm256_set1_epi8(2);
  const __m256i v_pri_thresh = _mm256_set1_epi8(11);
  const __m256i v_sec_shmask =
      _mm256_load_si256((const __m256i *)k_shmask_u8_avx2[sec_damp_val]);
  const __m256i v_pri_shmask =
      _mm256_load_si256((const __m256i *)k_shmask_u8_avx2[pri_damp_val]);
  const __m128i v_sec_shift = _mm_cvtsi32_si128(sec_damp_val);
  const __m128i v_pri_shift = _mm_cvtsi32_si128(pri_damp_val);

  for (int bi = 0; bi < cdef_count; ++bi) {
    const int by = dlist[bi].by;
    const int bx = dlist[bi].bx;
    const int d = dir[by][bx];
    const uint16_t *p = &in[(by * CDEF_BSTRIDE << 3) + (bx << 3)];
    const uint8_t *rr =
        &ref_base[(ptrdiff_t)(by << 3) * ref_stride + (bx << 3)];
    const intptr_t po1 = cdef_directions_padded_inline[d + 2][0];
    const intptr_t po2 = cdef_directions_padded_inline[d + 2][1];
    if ((d & 1) != 0) {
      const intptr_t s1o2 = cdef_directions_padded_inline[d + 4][1];
      const intptr_t s2o2 = cdef_directions_padded_inline[d][1];
      for (int i = 0; i < 8;
           i += 4, p += 4 * CDEF_BSTRIDE, rr += 4 * ref_stride) {
        const __m256i zero = _mm256_setzero_si256();
        const __m256i r_01 = load_u16_8x2(p);
        const __m256i r_23 = load_u16_8x2(p + 2 * CDEF_BSTRIDE);
        const __m256i ref8 = load_ref8_8x4(rr, ref_stride);
        const __m256i d0_01 =
            _mm256_sub_epi16(r_01, _mm256_unpacklo_epi8(ref8, zero));
        v_sse0 = _mm256_add_epi32(v_sse0, _mm256_madd_epi16(d0_01, d0_01));
        const __m256i d0_23 =
            _mm256_sub_epi16(r_23, _mm256_unpackhi_epi8(ref8, zero));
        v_sse0 = _mm256_add_epi32(v_sse0, _mm256_madd_epi16(d0_23, d0_23));

        __m256i max_u8 = _mm256_packus_epi16(r_01, r_23);
        __m256i min_u8 = max_u8;
        const __m256i sn1 =
            tap_pair_4r_minmax_i8(p, 1, r_01, r_23, v_sec_thresh, v_sec_shift,
                                  v_sec_shmask, &max_u8, &min_u8);
        const __m256i sn = tap_pair_4r_acc_minmax_i8(
            p, CDEF_BSTRIDE, r_01, r_23, v_sec_thresh, v_sec_shift,
            v_sec_shmask, &max_u8, &min_u8, sn1);
        const __m256i two_sn = _mm256_add_epi8(sn, sn);

        __m256i sum_sec0 = tap_pair_4r_acc_i8(
            p, 2, r_01, r_23, v_sec_thresh, v_sec_shift, v_sec_shmask, two_sn);
        sum_sec0 =
            tap_pair_4r_acc_i8(p, 2 * CDEF_BSTRIDE, r_01, r_23, v_sec_thresh,
                               v_sec_shift, v_sec_shmask, sum_sec0);
        finish_sec_4r_i8(r_01, r_23, ref8, sum_sec0, &v_sse1);

        __m256i sum_sec3 = tap_pair_4r_acc_minmax_i8(
            p, s1o2, r_01, r_23, v_sec_thresh, v_sec_shift, v_sec_shmask,
            &max_u8, &min_u8, two_sn);
        sum_sec3 = tap_pair_4r_acc_minmax_i8(p, s2o2, r_01, r_23, v_sec_thresh,
                                             v_sec_shift, v_sec_shmask, &max_u8,
                                             &min_u8, sum_sec3);

        const __m256i pn =
            tap_pair_4r_minmax_i8(p, po1, r_01, r_23, v_pri_thresh, v_pri_shift,
                                  v_pri_shmask, &max_u8, &min_u8);
        const __m256i s = tap_pair_4r_acc_minmax_i8(
            p, po2, r_01, r_23, v_pri_thresh, v_pri_shift, v_pri_shmask,
            &max_u8, &min_u8, pn);
        const __m256i sum_pri = _mm256_add_epi8(_mm256_add_epi8(s, s), s);
        finish_pri_tot_4r_i8(r_01, r_23, ref8, sum_pri, sum_sec3, max_u8,
                             min_u8, &v_sse2, &v_sse3);
      }
    } else if ((d & 3) == 0) {
      for (int i = 0; i < 8;
           i += 4, p += 4 * CDEF_BSTRIDE, rr += 4 * ref_stride) {
        const __m256i zero = _mm256_setzero_si256();
        const __m256i r_01 = load_u16_8x2(p);
        const __m256i r_23 = load_u16_8x2(p + 2 * CDEF_BSTRIDE);
        const __m256i ref8 = load_ref8_8x4(rr, ref_stride);
        const __m256i d0_01 =
            _mm256_sub_epi16(r_01, _mm256_unpacklo_epi8(ref8, zero));
        v_sse0 = _mm256_add_epi32(v_sse0, _mm256_madd_epi16(d0_01, d0_01));
        const __m256i d0_23 =
            _mm256_sub_epi16(r_23, _mm256_unpackhi_epi8(ref8, zero));
        v_sse0 = _mm256_add_epi32(v_sse0, _mm256_madd_epi16(d0_23, d0_23));

        __m256i max_u8 = _mm256_packus_epi16(r_01, r_23);
        __m256i min_u8 = max_u8;
        const __m256i sn1 =
            tap_pair_4r_minmax_i8(p, 1, r_01, r_23, v_sec_thresh, v_sec_shift,
                                  v_sec_shmask, &max_u8, &min_u8);
        const __m256i sn = tap_pair_4r_acc_minmax_i8(
            p, CDEF_BSTRIDE, r_01, r_23, v_sec_thresh, v_sec_shift,
            v_sec_shmask, &max_u8, &min_u8, sn1);
        __m256i sum_sec = _mm256_add_epi8(sn, sn);
        sum_sec = tap_pair_4r_acc_minmax_i8(p, 2, r_01, r_23, v_sec_thresh,
                                            v_sec_shift, v_sec_shmask, &max_u8,
                                            &min_u8, sum_sec);
        sum_sec = tap_pair_4r_acc_minmax_i8(
            p, 2 * CDEF_BSTRIDE, r_01, r_23, v_sec_thresh, v_sec_shift,
            v_sec_shmask, &max_u8, &min_u8, sum_sec);
        finish_sec_4r_i8(r_01, r_23, ref8, sum_sec, &v_sse1);

        const __m256i pn =
            tap_pair_4r_minmax_i8(p, po1, r_01, r_23, v_pri_thresh, v_pri_shift,
                                  v_pri_shmask, &max_u8, &min_u8);
        const __m256i s = tap_pair_4r_acc_minmax_i8(
            p, po2, r_01, r_23, v_pri_thresh, v_pri_shift, v_pri_shmask,
            &max_u8, &min_u8, pn);
        const __m256i sum_pri = _mm256_add_epi8(_mm256_add_epi8(s, s), s);
        finish_pri_tot_4r_i8(r_01, r_23, ref8, sum_pri, sum_sec, max_u8, min_u8,
                             &v_sse2, &v_sse3);
      }
    } else {
      const intptr_t s1o1 = cdef_directions_padded_inline[d + 4][0];
      const intptr_t s1o2 = cdef_directions_padded_inline[d + 4][1];
      const intptr_t s2o1 = cdef_directions_padded_inline[d][0];
      const intptr_t s2o2 = cdef_directions_padded_inline[d][1];
      for (int i = 0; i < 8;
           i += 4, p += 4 * CDEF_BSTRIDE, rr += 4 * ref_stride) {
        const __m256i zero = _mm256_setzero_si256();
        const __m256i r_01 = load_u16_8x2(p);
        const __m256i r_23 = load_u16_8x2(p + 2 * CDEF_BSTRIDE);
        const __m256i ref8 = load_ref8_8x4(rr, ref_stride);
        const __m256i d0_01 =
            _mm256_sub_epi16(r_01, _mm256_unpacklo_epi8(ref8, zero));
        v_sse0 = _mm256_add_epi32(v_sse0, _mm256_madd_epi16(d0_01, d0_01));
        const __m256i d0_23 =
            _mm256_sub_epi16(r_23, _mm256_unpackhi_epi8(ref8, zero));
        v_sse0 = _mm256_add_epi32(v_sse0, _mm256_madd_epi16(d0_23, d0_23));

        const __m256i sn1 = tap_pair_4r_i8(p, 1, r_01, r_23, v_sec_thresh,
                                           v_sec_shift, v_sec_shmask);
        const __m256i sn =
            tap_pair_4r_acc_i8(p, CDEF_BSTRIDE, r_01, r_23, v_sec_thresh,
                               v_sec_shift, v_sec_shmask, sn1);
        __m256i sum_sec0 = _mm256_add_epi8(sn, sn);
        sum_sec0 = tap_pair_4r_acc_i8(p, 2, r_01, r_23, v_sec_thresh,
                                      v_sec_shift, v_sec_shmask, sum_sec0);
        sum_sec0 =
            tap_pair_4r_acc_i8(p, 2 * CDEF_BSTRIDE, r_01, r_23, v_sec_thresh,
                               v_sec_shift, v_sec_shmask, sum_sec0);
        finish_sec_4r_i8(r_01, r_23, ref8, sum_sec0, &v_sse1);

        __m256i max_u8 = _mm256_packus_epi16(r_01, r_23);
        __m256i min_u8 = max_u8;
        const __m256i snd1 =
            tap_pair_4r_minmax_i8(p, s1o1, r_01, r_23, v_sec_thresh,
                                  v_sec_shift, v_sec_shmask, &max_u8, &min_u8);
        const __m256i snd = tap_pair_4r_acc_minmax_i8(
            p, s2o1, r_01, r_23, v_sec_thresh, v_sec_shift, v_sec_shmask,
            &max_u8, &min_u8, snd1);
        __m256i sum_sec3 = _mm256_add_epi8(snd, snd);
        sum_sec3 = tap_pair_4r_acc_minmax_i8(p, s1o2, r_01, r_23, v_sec_thresh,
                                             v_sec_shift, v_sec_shmask, &max_u8,
                                             &min_u8, sum_sec3);
        sum_sec3 = tap_pair_4r_acc_minmax_i8(p, s2o2, r_01, r_23, v_sec_thresh,
                                             v_sec_shift, v_sec_shmask, &max_u8,
                                             &min_u8, sum_sec3);

        const __m256i pn =
            tap_pair_4r_minmax_i8(p, po1, r_01, r_23, v_pri_thresh, v_pri_shift,
                                  v_pri_shmask, &max_u8, &min_u8);
        const __m256i s = tap_pair_4r_acc_minmax_i8(
            p, po2, r_01, r_23, v_pri_thresh, v_pri_shift, v_pri_shmask,
            &max_u8, &min_u8, pn);
        const __m256i sum_pri = _mm256_add_epi8(_mm256_add_epi8(s, s), s);
        finish_pri_tot_4r_i8(r_01, r_23, ref8, sum_pri, sum_sec3, max_u8,
                             min_u8, &v_sse2, &v_sse3);
      }
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

static inline void cdef_mse_calc_block_lvl4_chroma_8x8_avx2(
    const uint16_t *in, const uint8_t *ref_base, int ref_stride,
    const cdef_list *dlist, int cdef_count,
    const int dir[CDEF_NBLOCKS][CDEF_NBLOCKS], int damping, int pli,
    uint64_t mse[4]) {
  assert(damping >= 3 && damping <= 6);
  const int damp_adj = damping - 1;
  const int sec_damp_val = AOMMAX(0, damp_adj - 1);
  const int pri_damp_val = AOMMAX(0, damp_adj - 3);
  assert(sec_damp_val <= 4 && pri_damp_val <= 2);
  cdef_mse_calc_block_lvl4_chroma_8x8_fast_avx2(in, ref_base, ref_stride, dlist,
                                                cdef_count, dir, sec_damp_val,
                                                pri_damp_val, pli, mse);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void cdef_mse_calc_block_lvl4_luma_core_avx2(
    const uint16_t *in, const uint8_t *ref_base, int ref_stride,
    const cdef_list *dlist, int cdef_count, int dir[CDEF_NBLOCKS][CDEF_NBLOCKS],
    int var[CDEF_NBLOCKS][CDEF_NBLOCKS], int damping, uint64_t mse[4]) {
  __m256i v_sse0 = _mm256_setzero_si256();
  __m256i v_sse1 = _mm256_setzero_si256();
  __m256i v_sse2 = _mm256_setzero_si256();
  __m256i v_sse3 = _mm256_setzero_si256();
  const __m256i v_sec_thresh = _mm256_set1_epi16(2);

  cdef_find_dir_list_avx2(in, dlist, cdef_count, dir, var);
  const __m128i v_sec_damp = _mm_cvtsi32_si128(AOMMAX(0, damping - 1));

  for (int bi = 0; bi < cdef_count; ++bi) {
    const int by = dlist[bi].by;
    const int bx = dlist[bi].bx;
    const int d = dir[by][bx];
    const int v = var[by][bx];
    const int t =
        v ? (11 * (4 + (v >> 6 ? AOMMIN(get_msb(v >> 6), 12) : 0)) + 8) >> 4
          : 0;
    const uint16_t *in_row = &in[(by * CDEF_BSTRIDE << 3) + (bx << 3)];
    const uint8_t *ref_row =
        &ref_base[(ptrdiff_t)(by << 3) * ref_stride + (bx << 3)];

    if (t == 0) {
      if ((d & 3) == 0) {
        for (int i = 0; i < 8;
             i += 2, in_row += 2 * CDEF_BSTRIDE, ref_row += 2 * ref_stride) {
          const __m256i r = load_u16_8x2(in_row);
          const __m256i diff0 =
              _mm256_sub_epi16(r, load_ref16_8x2(ref_row, ref_stride));
          const __m256i sse0 = _mm256_madd_epi16(diff0, diff0);
          v_sse0 = _mm256_add_epi32(v_sse0, sse0);
          v_sse2 = _mm256_add_epi32(v_sse2, sse0);

          const __m256i sn1 =
              tap_pair_sum_off(in_row, 1, r, v_sec_thresh, v_sec_damp);
          const __m256i sn2 = tap_pair_sum_off(in_row, CDEF_BSTRIDE, r,
                                               v_sec_thresh, v_sec_damp);
          __m256i sum_sec = _mm256_slli_epi16(_mm256_add_epi16(sn1, sn2), 1);
          sum_sec = _mm256_add_epi16(
              sum_sec,
              tap_pair_sum_off(in_row, 2, r, v_sec_thresh, v_sec_damp));
          sum_sec = _mm256_add_epi16(
              sum_sec, tap_pair_sum_off(in_row, 2 * CDEF_BSTRIDE, r,
                                        v_sec_thresh, v_sec_damp));

          const __m256i diff1 =
              _mm256_add_epi16(diff0, finish_delta_fast(sum_sec));
          const __m256i sse1 = _mm256_madd_epi16(diff1, diff1);
          v_sse1 = _mm256_add_epi32(v_sse1, sse1);
          v_sse3 = _mm256_add_epi32(v_sse3, sse1);
        }
      } else {
        const intptr_t s1o1 = cdef_directions_padded_inline[d + 4][0];
        const intptr_t s1o2 = cdef_directions_padded_inline[d + 4][1];
        const intptr_t s2o1 = cdef_directions_padded_inline[d][0];
        const intptr_t s2o2 = cdef_directions_padded_inline[d][1];
        for (int i = 0; i < 8;
             i += 2, in_row += 2 * CDEF_BSTRIDE, ref_row += 2 * ref_stride) {
          const __m256i r = load_u16_8x2(in_row);
          const __m256i diff0 =
              _mm256_sub_epi16(r, load_ref16_8x2(ref_row, ref_stride));
          const __m256i sse0 = _mm256_madd_epi16(diff0, diff0);
          v_sse0 = _mm256_add_epi32(v_sse0, sse0);
          v_sse2 = _mm256_add_epi32(v_sse2, sse0);

          const __m256i sn1 =
              tap_pair_sum_off(in_row, 1, r, v_sec_thresh, v_sec_damp);
          const __m256i sn2 = tap_pair_sum_off(in_row, CDEF_BSTRIDE, r,
                                               v_sec_thresh, v_sec_damp);
          const __m256i sec_near0 = _mm256_add_epi16(sn1, sn2);
          __m256i sum_sec0 = _mm256_slli_epi16(sec_near0, 1);
          sum_sec0 = _mm256_add_epi16(
              sum_sec0,
              tap_pair_sum_off(in_row, 2, r, v_sec_thresh, v_sec_damp));
          sum_sec0 = _mm256_add_epi16(
              sum_sec0, tap_pair_sum_off(in_row, 2 * CDEF_BSTRIDE, r,
                                         v_sec_thresh, v_sec_damp));

          const __m256i diff1 =
              _mm256_add_epi16(diff0, finish_delta_fast(sum_sec0));
          v_sse1 = _mm256_add_epi32(v_sse1, _mm256_madd_epi16(diff1, diff1));

          const __m256i sec_neard =
              (d & 1) ? sec_near0
                      : _mm256_add_epi16(
                            tap_pair_sum_off(in_row, s1o1, r, v_sec_thresh,
                                             v_sec_damp),
                            tap_pair_sum_off(in_row, s2o1, r, v_sec_thresh,
                                             v_sec_damp));
          __m256i sum_secd = _mm256_slli_epi16(sec_neard, 1);
          sum_secd = _mm256_add_epi16(
              sum_secd,
              tap_pair_sum_off(in_row, s1o2, r, v_sec_thresh, v_sec_damp));
          sum_secd = _mm256_add_epi16(
              sum_secd,
              tap_pair_sum_off(in_row, s2o2, r, v_sec_thresh, v_sec_damp));

          const __m256i diff3 =
              _mm256_add_epi16(diff0, finish_delta_fast(sum_secd));
          v_sse3 = _mm256_add_epi32(v_sse3, _mm256_madd_epi16(diff3, diff3));
        }
      }
      continue;
    }

    const int pri_damping = AOMMAX(0, damping - get_msb(t));
    const __m128i v_pri_damp = _mm_cvtsi32_si128(pri_damping);
    const __m256i v_pri_thresh = _mm256_set1_epi16((int16_t)t);
    const intptr_t po1 = cdef_directions_padded_inline[d + 2][0];
    const intptr_t po2 = cdef_directions_padded_inline[d + 2][1];

    if ((d & 3) == 0) {
      for (int i = 0; i < 8;
           i += 2, in_row += 2 * CDEF_BSTRIDE, ref_row += 2 * ref_stride) {
        const __m256i r = load_u16_8x2(in_row);
        const __m256i diff0 =
            _mm256_sub_epi16(r, load_ref16_8x2(ref_row, ref_stride));
        v_sse0 = _mm256_add_epi32(v_sse0, _mm256_madd_epi16(diff0, diff0));

        __m256i max_u8 = r;
        __m256i min_s16 = r;

        const __m256i sn1 = tap_pair_sum_minmax_off(
            in_row, 1, r, v_sec_thresh, v_sec_damp, &max_u8, &min_s16);
        const __m256i sn2 =
            tap_pair_sum_minmax_off(in_row, CDEF_BSTRIDE, r, v_sec_thresh,
                                    v_sec_damp, &max_u8, &min_s16);
        __m256i sum_sec = _mm256_slli_epi16(_mm256_add_epi16(sn1, sn2), 1);
        sum_sec = _mm256_add_epi16(
            sum_sec, tap_pair_sum_minmax_off(in_row, 2, r, v_sec_thresh,
                                             v_sec_damp, &max_u8, &min_s16));
        sum_sec = _mm256_add_epi16(
            sum_sec,
            tap_pair_sum_minmax_off(in_row, 2 * CDEF_BSTRIDE, r, v_sec_thresh,
                                    v_sec_damp, &max_u8, &min_s16));
        const __m256i diff1 =
            _mm256_add_epi16(diff0, finish_delta_fast(sum_sec));
        v_sse1 = _mm256_add_epi32(v_sse1, _mm256_madd_epi16(diff1, diff1));

        const __m256i pn = tap_pair_sum_minmax_off(
            in_row, po1, r, v_pri_thresh, v_pri_damp, &max_u8, &min_s16);
        const __m256i pf = tap_pair_sum_minmax_off(
            in_row, po2, r, v_pri_thresh, v_pri_damp, &max_u8, &min_s16);
        const __m256i sum_pri = combine_pri_branchless(pn, pf, v_pri_thresh);
        const __m256i diff2 =
            _mm256_add_epi16(diff0, finish_delta_fast(sum_pri));
        v_sse2 = _mm256_add_epi32(v_sse2, _mm256_madd_epi16(diff2, diff2));

        const __m256i diff3 = finish_clamped_diff3(
            r, diff0, _mm256_add_epi16(sum_pri, sum_sec), max_u8, min_s16);
        v_sse3 = _mm256_add_epi32(v_sse3, _mm256_madd_epi16(diff3, diff3));
      }
    } else if (d & 1) {
      const intptr_t s1o2 = cdef_directions_padded_inline[d + 4][1];
      const intptr_t s2o2 = cdef_directions_padded_inline[d][1];
      for (int i = 0; i < 8;
           i += 2, in_row += 2 * CDEF_BSTRIDE, ref_row += 2 * ref_stride) {
        const __m256i r = load_u16_8x2(in_row);
        const __m256i diff0 =
            _mm256_sub_epi16(r, load_ref16_8x2(ref_row, ref_stride));
        v_sse0 = _mm256_add_epi32(v_sse0, _mm256_madd_epi16(diff0, diff0));

        __m256i max_u8 = r;
        __m256i min_s16 = r;

        const __m256i sn1 = tap_pair_sum_minmax_off(
            in_row, 1, r, v_sec_thresh, v_sec_damp, &max_u8, &min_s16);
        const __m256i sn2 =
            tap_pair_sum_minmax_off(in_row, CDEF_BSTRIDE, r, v_sec_thresh,
                                    v_sec_damp, &max_u8, &min_s16);
        const __m256i two_sec_near =
            _mm256_slli_epi16(_mm256_add_epi16(sn1, sn2), 1);
        __m256i sum_sec0 = _mm256_add_epi16(
            two_sec_near,
            tap_pair_sum_off(in_row, 2, r, v_sec_thresh, v_sec_damp));
        sum_sec0 = _mm256_add_epi16(
            sum_sec0, tap_pair_sum_off(in_row, 2 * CDEF_BSTRIDE, r,
                                       v_sec_thresh, v_sec_damp));
        const __m256i diff1 =
            _mm256_add_epi16(diff0, finish_delta_fast(sum_sec0));
        v_sse1 = _mm256_add_epi32(v_sse1, _mm256_madd_epi16(diff1, diff1));

        // For odd d, {s1o1, s2o1} == {1, CDEF_BSTRIDE} and po1 is in that
        // set, so sn1/sn2 already folded them into max_u8/min_s16.
        const __m256i pn =
            tap_pair_sum_off(in_row, po1, r, v_pri_thresh, v_pri_damp);
        const __m256i pf = tap_pair_sum_minmax_off(
            in_row, po2, r, v_pri_thresh, v_pri_damp, &max_u8, &min_s16);
        const __m256i sum_pri = combine_pri_branchless(pn, pf, v_pri_thresh);
        const __m256i diff2 =
            _mm256_add_epi16(diff0, finish_delta_fast(sum_pri));
        v_sse2 = _mm256_add_epi32(v_sse2, _mm256_madd_epi16(diff2, diff2));

        __m256i sum_tot = _mm256_add_epi16(sum_pri, two_sec_near);
        sum_tot = _mm256_add_epi16(
            sum_tot, tap_pair_sum_minmax_off(in_row, s1o2, r, v_sec_thresh,
                                             v_sec_damp, &max_u8, &min_s16));
        sum_tot = _mm256_add_epi16(
            sum_tot, tap_pair_sum_minmax_off(in_row, s2o2, r, v_sec_thresh,
                                             v_sec_damp, &max_u8, &min_s16));
        const __m256i diff3 =
            finish_clamped_diff3(r, diff0, sum_tot, max_u8, min_s16);
        v_sse3 = _mm256_add_epi32(v_sse3, _mm256_madd_epi16(diff3, diff3));
      }
    } else {
      const intptr_t s1o1 = cdef_directions_padded_inline[d + 4][0];
      const intptr_t s1o2 = cdef_directions_padded_inline[d + 4][1];
      const intptr_t s2o1 = cdef_directions_padded_inline[d][0];
      const intptr_t s2o2 = cdef_directions_padded_inline[d][1];
      for (int i = 0; i < 8;
           i += 2, in_row += 2 * CDEF_BSTRIDE, ref_row += 2 * ref_stride) {
        const __m256i r = load_u16_8x2(in_row);
        const __m256i diff0 =
            _mm256_sub_epi16(r, load_ref16_8x2(ref_row, ref_stride));
        v_sse0 = _mm256_add_epi32(v_sse0, _mm256_madd_epi16(diff0, diff0));

        const __m256i sn1 =
            tap_pair_sum_off(in_row, 1, r, v_sec_thresh, v_sec_damp);
        const __m256i sn2 =
            tap_pair_sum_off(in_row, CDEF_BSTRIDE, r, v_sec_thresh, v_sec_damp);
        __m256i sum_sec0 = _mm256_slli_epi16(_mm256_add_epi16(sn1, sn2), 1);
        sum_sec0 = _mm256_add_epi16(
            sum_sec0, tap_pair_sum_off(in_row, 2, r, v_sec_thresh, v_sec_damp));
        sum_sec0 = _mm256_add_epi16(
            sum_sec0, tap_pair_sum_off(in_row, 2 * CDEF_BSTRIDE, r,
                                       v_sec_thresh, v_sec_damp));
        const __m256i diff1 =
            _mm256_add_epi16(diff0, finish_delta_fast(sum_sec0));
        v_sse1 = _mm256_add_epi32(v_sse1, _mm256_madd_epi16(diff1, diff1));

        __m256i max_u8 = r;
        __m256i min_s16 = r;
        const __m256i pn = tap_pair_sum_minmax_off(
            in_row, po1, r, v_pri_thresh, v_pri_damp, &max_u8, &min_s16);
        const __m256i pf = tap_pair_sum_minmax_off(
            in_row, po2, r, v_pri_thresh, v_pri_damp, &max_u8, &min_s16);
        const __m256i sum_pri = combine_pri_branchless(pn, pf, v_pri_thresh);
        const __m256i diff2 =
            _mm256_add_epi16(diff0, finish_delta_fast(sum_pri));
        v_sse2 = _mm256_add_epi32(v_sse2, _mm256_madd_epi16(diff2, diff2));

        const __m256i snd1 = tap_pair_sum_minmax_off(
            in_row, s1o1, r, v_sec_thresh, v_sec_damp, &max_u8, &min_s16);
        const __m256i snd2 = tap_pair_sum_minmax_off(
            in_row, s2o1, r, v_sec_thresh, v_sec_damp, &max_u8, &min_s16);
        __m256i sum_tot = _mm256_add_epi16(
            sum_pri, _mm256_slli_epi16(_mm256_add_epi16(snd1, snd2), 1));
        sum_tot = _mm256_add_epi16(
            sum_tot, tap_pair_sum_minmax_off(in_row, s1o2, r, v_sec_thresh,
                                             v_sec_damp, &max_u8, &min_s16));
        sum_tot = _mm256_add_epi16(
            sum_tot, tap_pair_sum_minmax_off(in_row, s2o2, r, v_sec_thresh,
                                             v_sec_damp, &max_u8, &min_s16));
        const __m256i diff3 =
            finish_clamped_diff3(r, diff0, sum_tot, max_u8, min_s16);
        v_sse3 = _mm256_add_epi32(v_sse3, _mm256_madd_epi16(diff3, diff3));
      }
    }
  }

  mse[0] = hsum_u32_avx2(v_sse0);
  mse[1] = hsum_u32_avx2(v_sse1);
  mse[2] = hsum_u32_avx2(v_sse2);
  mse[3] = hsum_u32_avx2(v_sse3);
}

void av1_cdef_mse_calc_block_lvl4_avx2(
    const uint16_t *in, const uint8_t *ref_buffer, int ref_stride, int row,
    int col, const cdef_list *dlist, int cdef_count,
    int dir[CDEF_NBLOCKS][CDEF_NBLOCKS], int var[CDEF_NBLOCKS][CDEF_NBLOCKS],
    int damping, int pli, int mi_wide_l2, uint64_t mse[4]) {
  assert(damping >= 3 && damping <= 6);
  const uint8_t *ref_base = &ref_buffer[(ptrdiff_t)row * ref_stride + col];
  if (pli > 0) {
    assert(pli == 1 || pli == 2);
    assert(mi_wide_l2 == 1 || mi_wide_l2 == 2);
    if (mi_wide_l2 != 2) {
      cdef_mse_calc_block_lvl4_chroma_4x4_avx2(
          in, ref_base, ref_stride, dlist, cdef_count, dir, damping, pli, mse);
    } else {
      cdef_mse_calc_block_lvl4_chroma_8x8_avx2(
          in, ref_base, ref_stride, dlist, cdef_count, dir, damping, pli, mse);
    }
    return;
  }
  assert(mi_wide_l2 == 2);
  cdef_mse_calc_block_lvl4_luma_core_avx2(in, ref_base, ref_stride, dlist,
                                          cdef_count, dir, var, damping, mse);
}
#endif  // CONFIG_AV1_ENCODER
