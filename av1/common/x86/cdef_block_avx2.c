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
#include <immintrin.h>
#include <stddef.h>
#include <string.h>

#include "aom_dsp/aom_simd.h"
#include "aom_ports/bitops.h"
#define SIMD_FUNC(name) name##_avx2
#include "av1/common/cdef_block_simd.h"

static inline int cdef_msb_nz(unsigned int n) { return n ? get_msb(n) : 0; }

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

static inline __m256i finish_dir_costs_avx2(
    __m256i partial4a, __m256i partial4b, __m256i partial5a, __m256i partial5b,
    __m256i partial6, __m256i partial7a, __m256i partial7b) {
  const __m256i const_reg_1 =
      _mm256_set_epi32(210, 280, 420, 840, 210, 280, 420, 840);
  const __m256i const_reg_2 =
      _mm256_set_epi32(105, 120, 140, 168, 105, 120, 140, 168);
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

static inline __m256i load_dir_line_avx2(const uint16_t *img1,
                                         const uint16_t *img2, int idx,
                                         int stride, int coeff_shift,
                                         __m128i shift_reg, __m256i c128) {
  const ptrdiff_t off = (ptrdiff_t)idx * stride;
  const __m128i s1 = _mm_loadu_si128((const __m128i *)(img1 + off));
  const __m128i s2 = _mm_loadu_si128((const __m128i *)(img2 + off));
  __m256i v = _mm256_inserti128_si256(_mm256_castsi128_si256(s1), s2, 1);
  if (coeff_shift != 0) {
    v = _mm256_sra_epi16(v, shift_reg);
  }
  return _mm256_sub_epi16(v, c128);
}

static inline void dir_accum_01_avx2(__m256i l0, __m256i l1, __m256i *p4a,
                                     __m256i *p4b, __m256i *p5a, __m256i *p5b,
                                     __m256i *p6, __m256i *p7a, __m256i *p7b) {
  *p4a = _mm256_add_epi16(_mm256_slli_si256(l0, 14), _mm256_slli_si256(l1, 12));
  *p4b = _mm256_add_epi16(_mm256_srli_si256(l0, 2), _mm256_srli_si256(l1, 4));
  const __m256i tmp = _mm256_add_epi16(l0, l1);
  *p5a = _mm256_slli_si256(tmp, 10);
  *p5b = _mm256_srli_si256(tmp, 6);
  *p7a = _mm256_slli_si256(tmp, 4);
  *p7b = _mm256_srli_si256(tmp, 12);
  *p6 = tmp;
}

static inline void dir_accum_23_avx2(__m256i l2, __m256i l3, __m256i *p4a,
                                     __m256i *p4b, __m256i *p5a, __m256i *p5b,
                                     __m256i *p6, __m256i *p7a, __m256i *p7b) {
  *p4a = _mm256_add_epi16(*p4a, _mm256_slli_si256(l2, 10));
  *p4b = _mm256_add_epi16(*p4b, _mm256_srli_si256(l2, 6));
  *p4a = _mm256_add_epi16(*p4a, _mm256_slli_si256(l3, 8));
  *p4b = _mm256_add_epi16(*p4b, _mm256_srli_si256(l3, 8));
  const __m256i tmp = _mm256_add_epi16(l2, l3);
  *p5a = _mm256_add_epi16(*p5a, _mm256_slli_si256(tmp, 8));
  *p5b = _mm256_add_epi16(*p5b, _mm256_srli_si256(tmp, 8));
  *p7a = _mm256_add_epi16(*p7a, _mm256_slli_si256(tmp, 6));
  *p7b = _mm256_add_epi16(*p7b, _mm256_srli_si256(tmp, 10));
  *p6 = _mm256_add_epi16(*p6, tmp);
}

static inline void dir_accum_45_avx2(__m256i l4, __m256i l5, __m256i *p4a,
                                     __m256i *p4b, __m256i *p5a, __m256i *p5b,
                                     __m256i *p6, __m256i *p7a, __m256i *p7b) {
  *p4a = _mm256_add_epi16(*p4a, _mm256_slli_si256(l4, 6));
  *p4b = _mm256_add_epi16(*p4b, _mm256_srli_si256(l4, 10));
  *p4a = _mm256_add_epi16(*p4a, _mm256_slli_si256(l5, 4));
  *p4b = _mm256_add_epi16(*p4b, _mm256_srli_si256(l5, 12));
  const __m256i tmp = _mm256_add_epi16(l4, l5);
  *p5a = _mm256_add_epi16(*p5a, _mm256_slli_si256(tmp, 6));
  *p5b = _mm256_add_epi16(*p5b, _mm256_srli_si256(tmp, 10));
  *p7a = _mm256_add_epi16(*p7a, _mm256_slli_si256(tmp, 8));
  *p7b = _mm256_add_epi16(*p7b, _mm256_srli_si256(tmp, 8));
  *p6 = _mm256_add_epi16(*p6, tmp);
}

static inline void dir_accum_67_avx2(__m256i l6, __m256i l7, __m256i *p4a,
                                     __m256i *p4b, __m256i *p5a, __m256i *p5b,
                                     __m256i *p6, __m256i *p7a, __m256i *p7b) {
  *p4a = _mm256_add_epi16(*p4a, _mm256_slli_si256(l6, 2));
  *p4b = _mm256_add_epi16(*p4b, _mm256_srli_si256(l6, 14));
  *p4a = _mm256_add_epi16(*p4a, l7);
  const __m256i tmp = _mm256_add_epi16(l6, l7);
  *p5a = _mm256_add_epi16(*p5a, _mm256_slli_si256(tmp, 4));
  *p5b = _mm256_add_epi16(*p5b, _mm256_srli_si256(tmp, 12));
  *p7a = _mm256_add_epi16(*p7a, _mm256_slli_si256(tmp, 10));
  *p7b = _mm256_add_epi16(*p7b, _mm256_srli_si256(tmp, 6));
  *p6 = _mm256_add_epi16(*p6, tmp);
}

void cdef_find_dir_dual_avx2(const uint16_t *img1, const uint16_t *img2,
                             int stride, int32_t *var_out_1st,
                             int32_t *var_out_2nd, int coeff_shift,
                             int *out_dir_1st_8x8, int *out_dir_2nd_8x8) {
  const __m256i c128 = _mm256_set1_epi16(128);
  const __m128i sh = _mm_cvtsi32_si128(coeff_shift);
  DECLARE_ALIGNED(32, uint8_t, t0_buf[8 * 32]);
  DECLARE_ALIGNED(32, int32_t, costs[16]);

  /* Pass 1: load 8 row pairs once, pre-interleave 16-bit transpose stage to
     stack buffer, and accumulate dir47 costs sequentially. */
  __m256i p4a, p4b, p5a, p5b, p6, p7a, p7b;
  {
    const __m256i a0 =
        load_dir_line_avx2(img1, img2, 0, stride, coeff_shift, sh, c128);
    const __m256i a1 =
        load_dir_line_avx2(img1, img2, 1, stride, coeff_shift, sh, c128);
    _mm256_store_si256((__m256i *)(t0_buf + 0 * 32),
                       _mm256_unpacklo_epi16(a0, a1));
    _mm256_store_si256((__m256i *)(t0_buf + 2 * 32),
                       _mm256_unpackhi_epi16(a0, a1));
    dir_accum_01_avx2(a0, a1, &p4a, &p4b, &p5a, &p5b, &p6, &p7a, &p7b);

    const __m256i a2 =
        load_dir_line_avx2(img1, img2, 2, stride, coeff_shift, sh, c128);
    const __m256i a3 =
        load_dir_line_avx2(img1, img2, 3, stride, coeff_shift, sh, c128);
    _mm256_store_si256((__m256i *)(t0_buf + 1 * 32),
                       _mm256_unpacklo_epi16(a2, a3));
    _mm256_store_si256((__m256i *)(t0_buf + 3 * 32),
                       _mm256_unpackhi_epi16(a2, a3));
    dir_accum_23_avx2(a2, a3, &p4a, &p4b, &p5a, &p5b, &p6, &p7a, &p7b);

    const __m256i a4 =
        load_dir_line_avx2(img1, img2, 4, stride, coeff_shift, sh, c128);
    const __m256i a5 =
        load_dir_line_avx2(img1, img2, 5, stride, coeff_shift, sh, c128);
    _mm256_store_si256((__m256i *)(t0_buf + 4 * 32),
                       _mm256_unpacklo_epi16(a4, a5));
    _mm256_store_si256((__m256i *)(t0_buf + 6 * 32),
                       _mm256_unpackhi_epi16(a4, a5));
    dir_accum_45_avx2(a4, a5, &p4a, &p4b, &p5a, &p5b, &p6, &p7a, &p7b);

    const __m256i a6 =
        load_dir_line_avx2(img1, img2, 6, stride, coeff_shift, sh, c128);
    const __m256i a7 =
        load_dir_line_avx2(img1, img2, 7, stride, coeff_shift, sh, c128);
    _mm256_store_si256((__m256i *)(t0_buf + 5 * 32),
                       _mm256_unpacklo_epi16(a6, a7));
    _mm256_store_si256((__m256i *)(t0_buf + 7 * 32),
                       _mm256_unpackhi_epi16(a6, a7));
    dir_accum_67_avx2(a6, a7, &p4a, &p4b, &p5a, &p5b, &p6, &p7a, &p7b);
  }
  const __m256i v_dir47 =
      finish_dir_costs_avx2(p4a, p4b, p5a, p5b, p6, p7a, p7b);
  _mm256_store_si256((__m256i *)(costs + 8), v_dir47);

  /* Pass 2: finish 32-bit/64-bit transpose in two 4-row halves into dir03. */
  {
    const __m256i u2 = _mm256_load_si256((const __m256i *)(t0_buf + 2 * 32));
    const __m256i u3 = _mm256_load_si256((const __m256i *)(t0_buf + 3 * 32));
    const __m256i u6 = _mm256_load_si256((const __m256i *)(t0_buf + 6 * 32));
    const __m256i u7 = _mm256_load_si256((const __m256i *)(t0_buf + 7 * 32));
    const __m256i tr1_4 = _mm256_unpacklo_epi32(u2, u3);
    const __m256i tr1_6 = _mm256_unpackhi_epi32(u2, u3);
    const __m256i tr1_5 = _mm256_unpacklo_epi32(u6, u7);
    const __m256i tr1_7 = _mm256_unpackhi_epi32(u6, u7);
    const __m256i r0 = _mm256_unpackhi_epi64(tr1_6, tr1_7);
    const __m256i r1 = _mm256_unpacklo_epi64(tr1_6, tr1_7);
    const __m256i r2 = _mm256_unpackhi_epi64(tr1_4, tr1_5);
    const __m256i r3 = _mm256_unpacklo_epi64(tr1_4, tr1_5);
    dir_accum_01_avx2(r0, r1, &p4a, &p4b, &p5a, &p5b, &p6, &p7a, &p7b);
    dir_accum_23_avx2(r2, r3, &p4a, &p4b, &p5a, &p5b, &p6, &p7a, &p7b);
  }
  {
    const __m256i u0 = _mm256_load_si256((const __m256i *)(t0_buf + 0 * 32));
    const __m256i u1 = _mm256_load_si256((const __m256i *)(t0_buf + 1 * 32));
    const __m256i u4 = _mm256_load_si256((const __m256i *)(t0_buf + 4 * 32));
    const __m256i u5 = _mm256_load_si256((const __m256i *)(t0_buf + 5 * 32));
    const __m256i tr1_0 = _mm256_unpacklo_epi32(u0, u1);
    const __m256i tr1_2 = _mm256_unpackhi_epi32(u0, u1);
    const __m256i tr1_1 = _mm256_unpacklo_epi32(u4, u5);
    const __m256i tr1_3 = _mm256_unpackhi_epi32(u4, u5);
    const __m256i r4 = _mm256_unpackhi_epi64(tr1_2, tr1_3);
    const __m256i r5 = _mm256_unpacklo_epi64(tr1_2, tr1_3);
    const __m256i r6 = _mm256_unpackhi_epi64(tr1_0, tr1_1);
    const __m256i r7 = _mm256_unpacklo_epi64(tr1_0, tr1_1);
    dir_accum_45_avx2(r4, r5, &p4a, &p4b, &p5a, &p5b, &p6, &p7a, &p7b);
    dir_accum_67_avx2(r6, r7, &p4a, &p4b, &p5a, &p5b, &p6, &p7a, &p7b);
  }
  const __m256i dir03 = finish_dir_costs_avx2(p4a, p4b, p5a, p5b, p6, p7a, p7b);
  _mm256_store_si256((__m256i *)costs, dir03);
  const __m256i dir47 = _mm256_load_si256((const __m256i *)(costs + 8));

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

  const uint32_t m0 = eq_mask & 0xffU;
  const uint32_t m1 = (eq_mask >> 16) & 0xffU;
  assert(m0 != 0 && m1 != 0);
  const int dir0 = cdef_msb_nz(m0 ^ (m0 - 1));
  const int dir1 = cdef_msb_nz(m1 ^ (m1 - 1));

  const int32_t best0 = _mm_cvtsi128_si32(_mm256_castsi256_si128(max));
  const int32_t best1 = _mm_cvtsi128_si32(_mm256_extracti128_si256(max, 1));
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
  assert(height % 2 == 0);
  assert(height > 0);
  assert(width > 0);

  const ptrdiff_t ss = sstride;
  const ptrdiff_t ds = dstride;
  const ptrdiff_t ss2 = 2 * ss;
  const ptrdiff_t ds2 = 2 * ds;
  int j = 0;

  if (width >= 64) {
    do {
      const uint8_t *s = src + j;
      uint16_t *d = dst + j;
      int h = height;
      do {
        const __m128i r00 = _mm_loadu_si128((const __m128i *)(s + 0));
        const __m128i r01 = _mm_loadu_si128((const __m128i *)(s + 16));
        const __m128i r02 = _mm_loadu_si128((const __m128i *)(s + 32));
        const __m128i r03 = _mm_loadu_si128((const __m128i *)(s + 48));
        const __m128i r10 = _mm_loadu_si128((const __m128i *)(s + ss + 0));
        const __m128i r11 = _mm_loadu_si128((const __m128i *)(s + ss + 16));
        const __m128i r12 = _mm_loadu_si128((const __m128i *)(s + ss + 32));
        const __m128i r13 = _mm_loadu_si128((const __m128i *)(s + ss + 48));
        _mm256_storeu_si256((__m256i *)(d + 0), _mm256_cvtepu8_epi16(r00));
        _mm256_storeu_si256((__m256i *)(d + 16), _mm256_cvtepu8_epi16(r01));
        _mm256_storeu_si256((__m256i *)(d + 32), _mm256_cvtepu8_epi16(r02));
        _mm256_storeu_si256((__m256i *)(d + 48), _mm256_cvtepu8_epi16(r03));
        _mm256_storeu_si256((__m256i *)(d + ds + 0), _mm256_cvtepu8_epi16(r10));
        _mm256_storeu_si256((__m256i *)(d + ds + 16),
                            _mm256_cvtepu8_epi16(r11));
        _mm256_storeu_si256((__m256i *)(d + ds + 32),
                            _mm256_cvtepu8_epi16(r12));
        _mm256_storeu_si256((__m256i *)(d + ds + 48),
                            _mm256_cvtepu8_epi16(r13));
        s += ss2;
        d += ds2;
        h -= 2;
      } while (h > 0);
      j += 64;
    } while (j <= width - 64);
  }

  if (width & 32) {
    const uint8_t *s = src + j;
    uint16_t *d = dst + j;
    int h = height;
    do {
      const __m128i r00 = _mm_loadu_si128((const __m128i *)(s + 0));
      const __m128i r01 = _mm_loadu_si128((const __m128i *)(s + 16));
      const __m128i r10 = _mm_loadu_si128((const __m128i *)(s + ss + 0));
      const __m128i r11 = _mm_loadu_si128((const __m128i *)(s + ss + 16));
      _mm256_storeu_si256((__m256i *)(d + 0), _mm256_cvtepu8_epi16(r00));
      _mm256_storeu_si256((__m256i *)(d + 16), _mm256_cvtepu8_epi16(r01));
      _mm256_storeu_si256((__m256i *)(d + ds + 0), _mm256_cvtepu8_epi16(r10));
      _mm256_storeu_si256((__m256i *)(d + ds + 16), _mm256_cvtepu8_epi16(r11));
      s += ss2;
      d += ds2;
      h -= 2;
    } while (h > 0);
    j += 32;
  }

  if (width & 16) {
    const uint8_t *s = src + j;
    uint16_t *d = dst + j;
    int h = height;
    do {
      const __m128i r0 = _mm_loadu_si128((const __m128i *)s);
      const __m128i r1 = _mm_loadu_si128((const __m128i *)(s + ss));
      _mm256_storeu_si256((__m256i *)d, _mm256_cvtepu8_epi16(r0));
      _mm256_storeu_si256((__m256i *)(d + ds), _mm256_cvtepu8_epi16(r1));
      s += ss2;
      d += ds2;
      h -= 2;
    } while (h > 0);
    j += 16;
  }

  if (width & 8) {
    const uint8_t *s = src + j;
    uint16_t *d = dst + j;
    int h = height;
    do {
      const __m128i r0 = _mm_loadl_epi64((const __m128i *)s);
      const __m128i r1 = _mm_loadl_epi64((const __m128i *)(s + ss));
      _mm_storeu_si128((__m128i *)d, _mm_cvtepu8_epi16(r0));
      _mm_storeu_si128((__m128i *)(d + ds), _mm_cvtepu8_epi16(r1));
      s += ss2;
      d += ds2;
      h -= 2;
    } while (h > 0);
    j += 8;
  }

  if (width & 4) {
    const uint8_t *s = src + j;
    uint16_t *d = dst + j;
    int h = height;
    do {
      int32_t v0, v1;
      memcpy(&v0, s, 4);
      memcpy(&v1, s + ss, 4);
      _mm_storel_epi64((__m128i *)d, _mm_cvtepu8_epi16(_mm_cvtsi32_si128(v0)));
      _mm_storel_epi64((__m128i *)(d + ds),
                       _mm_cvtepu8_epi16(_mm_cvtsi32_si128(v1)));
      s += ss2;
      d += ds2;
      h -= 2;
    } while (h > 0);
    j += 4;
  }

  if (width & 2) {
    const uint8_t *s = src + j;
    uint16_t *d = dst + j;
    int h = height;
    do {
      uint16_t v0, v1;
      memcpy(&v0, s, 2);
      memcpy(&v1, s + ss, 2);
      const uint32_t u0 = (v0 & 0xffu) | ((uint32_t)(v0 & 0xff00u) << 8);
      const uint32_t u1 = (v1 & 0xffu) | ((uint32_t)(v1 & 0xff00u) << 8);
      memcpy(d, &u0, 4);
      memcpy(d + ds, &u1, 4);
      s += ss2;
      d += ds2;
      h -= 2;
    } while (h > 0);
    j += 2;
  }

  if (width & 1) {
    const uint8_t *s = src + j;
    uint16_t *d = dst + j;
    int h = height;
    do {
      d[0] = s[0];
      d[ds] = s[ss];
      s += ss2;
      d += ds2;
      h -= 2;
    } while (h > 0);
  }
}

#if CONFIG_AV1_ENCODER
/* For damping <= 4, pri_damp <= 4 fits k_shmask_u8_avx2[0..4] and every
   primary tap is <= 7, keeping int8_t accumulators in [-84, 84] (<= 127). */
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
  const __m128i lo = _mm_unpacklo_epi64(
      _mm_loadl_epi64((const __m128i *)&p[0 * CDEF_BSTRIDE]),
      _mm_loadl_epi64((const __m128i *)&p[1 * CDEF_BSTRIDE]));
  const __m128i hi = _mm_unpacklo_epi64(
      _mm_loadl_epi64((const __m128i *)&p[2 * CDEF_BSTRIDE]),
      _mm_loadl_epi64((const __m128i *)&p[3 * CDEF_BSTRIDE]));
  return _mm256_inserti128_si256(_mm256_castsi128_si256(lo), hi, 1);
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
      _mm_set_epi32(load_s32_unaligned(&ref_row[3 * ref_stride]),
                    load_s32_unaligned(&ref_row[2 * ref_stride]),
                    load_s32_unaligned(&ref_row[1 * ref_stride]),
                    load_s32_unaligned(&ref_row[0 * ref_stride])));
}

static inline uint64_t hsum_u32_avx2(__m256i v) {
  __m128i sum128 =
      _mm_add_epi32(_mm256_castsi256_si128(v), _mm256_extracti128_si256(v, 1));
  sum128 = _mm_add_epi32(sum128, _mm_srli_si128(sum128, 8));
  sum128 = _mm_add_epi32(sum128, _mm_srli_si128(sum128, 4));
  return (uint32_t)_mm_cvtsi128_si32(sum128);
}

static inline void cdef_find_dir_list_avx2(
    const uint16_t *in, const cdef_list *dlist, int cdef_count,
    int dir[CDEF_NBLOCKS][CDEF_NBLOCKS], int var[CDEF_NBLOCKS][CDEF_NBLOCKS]) {
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
    int32_t dummy_var = 0;
    int dummy_dir = 0;
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
  assert(pli == 1 || pli == 2);
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
    const intptr_t s1o1 = cdef_directions_padded_inline[d + 4][0];
    const intptr_t s1o2 = cdef_directions_padded_inline[d + 4][1];
    const intptr_t s2o1 = cdef_directions_padded_inline[d][0];
    const intptr_t s2o2 = cdef_directions_padded_inline[d][1];

    const __m256i r = load_tap_4x4(in_row);
    const __m256i diff0 =
        _mm256_sub_epi16(r, load_ref16_4x4(ref_row, ref_stride));
    v_sse0 = _mm256_add_epi32(v_sse0, _mm256_madd_epi16(diff0, diff0));

    __m256i max_u8 = r;
    __m256i min_s16 = r;
    const __m256i snd1 = tap_pair_sum_minmax_4x4_avx2(
        in_row, s1o1, r, v_sec_thresh, v_sec_damp, &max_u8, &min_s16);
    const __m256i snd2 = tap_pair_sum_minmax_4x4_avx2(
        in_row, s2o1, r, v_sec_thresh, v_sec_damp, &max_u8, &min_s16);
    const __m256i two_snd = _mm256_slli_epi16(_mm256_add_epi16(snd1, snd2), 1);
    __m256i sum_sec3 = _mm256_add_epi16(
        two_snd, tap_pair_sum_minmax_4x4_avx2(in_row, s1o2, r, v_sec_thresh,
                                              v_sec_damp, &max_u8, &min_s16));
    sum_sec3 = _mm256_add_epi16(
        sum_sec3, tap_pair_sum_minmax_4x4_avx2(in_row, s2o2, r, v_sec_thresh,
                                               v_sec_damp, &max_u8, &min_s16));

    __m256i sum_sec0 = sum_sec3;
    if ((d & 3) != 0) {
      sum_sec0 = (d & 1)
                     ? two_snd
                     : _mm256_slli_epi16(
                           _mm256_add_epi16(
                               tap_pair_sum_4x4_avx2(in_row, 1, r, v_sec_thresh,
                                                     v_sec_damp),
                               tap_pair_sum_4x4_avx2(in_row, CDEF_BSTRIDE, r,
                                                     v_sec_thresh, v_sec_damp)),
                           1);
      sum_sec0 = _mm256_add_epi16(
          sum_sec0,
          tap_pair_sum_4x4_avx2(in_row, 2, r, v_sec_thresh, v_sec_damp));
      sum_sec0 = _mm256_add_epi16(
          sum_sec0, tap_pair_sum_4x4_avx2(in_row, 2 * CDEF_BSTRIDE, r,
                                          v_sec_thresh, v_sec_damp));
    }
    const __m256i diff1 = _mm256_add_epi16(diff0, finish_delta_fast(sum_sec0));
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
        r, diff0, _mm256_add_epi16(sum_sec3, sum_pri), max_u8, min_s16);
    v_sse3 = _mm256_add_epi32(v_sse3, _mm256_madd_epi16(diff3, diff3));
  }

  if (pli <= 1) {
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

static inline __m256i unpack_i8_lo_s16(__m256i v8) {
  return _mm256_srai_epi16(_mm256_unpacklo_epi8(v8, v8), 8);
}

static inline __m256i unpack_i8_hi_s16(__m256i v8) {
  return _mm256_srai_epi16(_mm256_unpackhi_epi8(v8, v8), 8);
}

static inline __m256i unpack_u8_lo_u16(__m256i v8) {
  return _mm256_srli_epi16(_mm256_unpacklo_epi8(v8, v8), 8);
}

static inline __m256i unpack_u8_hi_u16(__m256i v8) {
  return _mm256_srli_epi16(_mm256_unpackhi_epi8(v8, v8), 8);
}

/* For damping <= 4, sec_damp <= 3 so |diff| >= 16 maps to 0 in vpshufb;
   int8_t accumulators stay within [-24, 24] (sec) and [-84, 84] (pri). */
static inline __m256i make_sec_lut_avx2(int s) {
  int d0 = 0x00000100;
  int d1 = 0;
  int d2 = 0;
  if (s == 1) {
    d0 = 0x01010100;
  } else if (s == 2) {
    d0 = 0x02020100;
    d1 = 0x01010101;
  } else if (s >= 3) {
    d0 = 0x02020100;
    d1 = 0x02020202;
    d2 = 0x01010101;
  }
  return _mm256_broadcastsi128_si256(_mm_setr_epi32(d0, d1, d2, d2));
}

static inline __m256i make_pri11_d0_lut_avx2(void) {
  return _mm256_broadcastsi128_si256(
      _mm_setr_epi32(0x03020100, 0x04050504, 0x00010203, 0));
}

static inline __m256i constrain_lut_i8(__m256i raw8, __m256i v_lut,
                                       __m256i v_112) {
  const __m256i idx = _mm256_adds_epu8(_mm256_abs_epi8(raw8), v_112);
  return _mm256_sign_epi8(_mm256_shuffle_epi8(v_lut, idx), raw8);
}

static inline __m256i constrain_hoist_i8(__m256i raw8, __m256i v_t, __m128i v_s,
                                         const uint8_t *sh_mask_ptr) {
  const __m256i ad = _mm256_abs_epi8(raw8);
  const __m256i mask = _mm256_load_si256((const __m256i *)sh_mask_ptr);
  const __m256i sh = _mm256_and_si256(_mm256_srl_epi16(ad, v_s), mask);
  return _mm256_sign_epi8(_mm256_min_epu8(ad, _mm256_subs_epu8(v_t, sh)), raw8);
}

static inline __m256i raw8_seq_4r(const uint16_t *p_tap, __m256i r_01,
                                  __m256i r_23) {
  return _mm256_packs_epi16(
      _mm256_sub_epi16(load_u16_8x2(p_tap), r_01),
      _mm256_sub_epi16(load_u16_8x2(p_tap + 2 * CDEF_BSTRIDE), r_23));
}

static inline __m256i raw8_seq_mm_4r(const uint16_t *p_tap, __m256i r_01,
                                     __m256i r_23, __m256i *max_u8,
                                     __m256i *min_u8) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  const __m256i t_01 = load_u16_8x2(p_tap);
  const __m256i t_23 = load_u16_8x2(p_tap + 2 * CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(t_01, t_23));
  const __m256i raw = _mm256_packs_epi16(_mm256_sub_epi16(t_01, r_01),
                                         _mm256_sub_epi16(t_23, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(t_01, mask_00ff),
                                   _mm256_and_si256(t_23, mask_00ff)));
  return raw;
}

static inline void acc_sec_pair_4r(__m256i *acc, const uint16_t *p,
                                   intptr_t off, __m256i r_01, __m256i r_23,
                                   __m256i v_sec_lut, __m256i v_112) {
  *acc = _mm256_add_epi8(
      *acc,
      constrain_lut_i8(raw8_seq_4r(p + off, r_01, r_23), v_sec_lut, v_112));
  *acc = _mm256_add_epi8(
      *acc,
      constrain_lut_i8(raw8_seq_4r(p - off, r_01, r_23), v_sec_lut, v_112));
}

static inline __m256i pair_lut_mm_4r(const uint16_t *p, intptr_t off,
                                     __m256i r_01, __m256i r_23, __m256i v_lut,
                                     __m256i v_112, __m256i *max_u8,
                                     __m256i *min_u8) {
  const __m256i rp = raw8_seq_mm_4r(p + off, r_01, r_23, max_u8, min_u8);
  const __m256i cp = constrain_lut_i8(rp, v_lut, v_112);
  const __m256i rm = raw8_seq_mm_4r(p - off, r_01, r_23, max_u8, min_u8);
  return _mm256_add_epi8(cp, constrain_lut_i8(rm, v_lut, v_112));
}

static inline void acc_sec_pair_mm_4r(__m256i *acc, const uint16_t *p,
                                      intptr_t off, __m256i r_01, __m256i r_23,
                                      __m256i v_sec_lut, __m256i v_112,
                                      __m256i *max_u8, __m256i *min_u8) {
  const __m256i rp = raw8_seq_mm_4r(p + off, r_01, r_23, max_u8, min_u8);
  *acc = _mm256_add_epi8(*acc, constrain_lut_i8(rp, v_sec_lut, v_112));
  const __m256i rm = raw8_seq_mm_4r(p - off, r_01, r_23, max_u8, min_u8);
  *acc = _mm256_add_epi8(*acc, constrain_lut_i8(rm, v_sec_lut, v_112));
}

static inline void acc_sec_vert1_4r(__m256i *acc, const uint16_t *p,
                                    __m256i r_01, __m256i r_23,
                                    __m256i v_sec_lut, __m256i v_112) {
  const __m256i r_12 = load_u16_8x2(p + CDEF_BSTRIDE);
  const __m256i r_34 = load_u16_8x2(p + 3 * CDEF_BSTRIDE);
  const __m256i rp = _mm256_packs_epi16(_mm256_sub_epi16(r_12, r_01),
                                        _mm256_sub_epi16(r_34, r_23));
  *acc = _mm256_add_epi8(*acc, constrain_lut_i8(rp, v_sec_lut, v_112));
  const __m256i r_m10 = load_u16_8x2(p - CDEF_BSTRIDE);
  const __m256i rm = _mm256_packs_epi16(_mm256_sub_epi16(r_m10, r_01),
                                        _mm256_sub_epi16(r_12, r_23));
  *acc = _mm256_add_epi8(*acc, constrain_lut_i8(rm, v_sec_lut, v_112));
}

static inline __m256i init_sec_vert1_4r(const uint16_t *p, __m256i r_01,
                                        __m256i r_23, __m256i v_sec_lut,
                                        __m256i v_112) {
  const __m256i r_12 = load_u16_8x2(p + CDEF_BSTRIDE);
  const __m256i r_34 = load_u16_8x2(p + 3 * CDEF_BSTRIDE);
  const __m256i rp = _mm256_packs_epi16(_mm256_sub_epi16(r_12, r_01),
                                        _mm256_sub_epi16(r_34, r_23));
  const __m256i acc = constrain_lut_i8(rp, v_sec_lut, v_112);
  const __m256i r_m10 = load_u16_8x2(p - CDEF_BSTRIDE);
  const __m256i rm = _mm256_packs_epi16(_mm256_sub_epi16(r_m10, r_01),
                                        _mm256_sub_epi16(r_12, r_23));
  return _mm256_add_epi8(acc, constrain_lut_i8(rm, v_sec_lut, v_112));
}

static inline void acc_sec_vert2_4r(__m256i *acc, const uint16_t *p,
                                    __m256i r_01, __m256i r_23,
                                    __m256i v_sec_lut, __m256i v_112) {
  const __m256i r_45 = load_u16_8x2(p + 4 * CDEF_BSTRIDE);
  const __m256i rp = _mm256_packs_epi16(_mm256_sub_epi16(r_23, r_01),
                                        _mm256_sub_epi16(r_45, r_23));
  *acc = _mm256_add_epi8(*acc, constrain_lut_i8(rp, v_sec_lut, v_112));
  const __m256i r_m2m1 = load_u16_8x2(p - 2 * CDEF_BSTRIDE);
  const __m256i rm = _mm256_packs_epi16(_mm256_sub_epi16(r_m2m1, r_01),
                                        _mm256_sub_epi16(r_01, r_23));
  *acc = _mm256_add_epi8(*acc, constrain_lut_i8(rm, v_sec_lut, v_112));
}

static inline void acc_sec_vert1_mm_4r(__m256i *acc, const uint16_t *p,
                                       __m256i r_01, __m256i r_23,
                                       __m256i v_sec_lut, __m256i v_112,
                                       __m256i *max_u8, __m256i *min_u8) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  const __m256i r_12 = load_u16_8x2(p + CDEF_BSTRIDE);
  const __m256i r_34 = load_u16_8x2(p + 3 * CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_12, r_34));
  const __m256i rp = _mm256_packs_epi16(_mm256_sub_epi16(r_12, r_01),
                                        _mm256_sub_epi16(r_34, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(r_12, mask_00ff),
                                   _mm256_and_si256(r_34, mask_00ff)));
  *acc = _mm256_add_epi8(*acc, constrain_lut_i8(rp, v_sec_lut, v_112));
  const __m256i r_m10 = load_u16_8x2(p - CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_m10, r_12));
  const __m256i rm = _mm256_packs_epi16(_mm256_sub_epi16(r_m10, r_01),
                                        _mm256_sub_epi16(r_12, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(r_m10, mask_00ff),
                                   _mm256_and_si256(r_12, mask_00ff)));
  *acc = _mm256_add_epi8(*acc, constrain_lut_i8(rm, v_sec_lut, v_112));
}

static inline __m256i init_sec_vert1_mm_4r(const uint16_t *p, __m256i r_01,
                                           __m256i r_23, __m256i v_sec_lut,
                                           __m256i v_112, __m256i *max_u8,
                                           __m256i *min_u8) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  const __m256i r_12 = load_u16_8x2(p + CDEF_BSTRIDE);
  const __m256i r_34 = load_u16_8x2(p + 3 * CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_12, r_34));
  const __m256i rp = _mm256_packs_epi16(_mm256_sub_epi16(r_12, r_01),
                                        _mm256_sub_epi16(r_34, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(r_12, mask_00ff),
                                   _mm256_and_si256(r_34, mask_00ff)));
  const __m256i acc = constrain_lut_i8(rp, v_sec_lut, v_112);
  const __m256i r_m10 = load_u16_8x2(p - CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_m10, r_12));
  const __m256i rm = _mm256_packs_epi16(_mm256_sub_epi16(r_m10, r_01),
                                        _mm256_sub_epi16(r_12, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(r_m10, mask_00ff),
                                   _mm256_and_si256(r_12, mask_00ff)));
  return _mm256_add_epi8(acc, constrain_lut_i8(rm, v_sec_lut, v_112));
}

static inline void acc_sec_vert2_mm_4r(__m256i *acc, const uint16_t *p,
                                       __m256i r_01, __m256i r_23,
                                       __m256i v_sec_lut, __m256i v_112,
                                       __m256i *max_u8, __m256i *min_u8) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  const __m256i r_45 = load_u16_8x2(p + 4 * CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_23, r_45));
  const __m256i rp = _mm256_packs_epi16(_mm256_sub_epi16(r_23, r_01),
                                        _mm256_sub_epi16(r_45, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(r_23, _mm256_and_si256(r_45, mask_00ff)));
  *acc = _mm256_add_epi8(*acc, constrain_lut_i8(rp, v_sec_lut, v_112));
  const __m256i r_m2m1 = load_u16_8x2(p - 2 * CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_m2m1, r_01));
  const __m256i rm = _mm256_packs_epi16(_mm256_sub_epi16(r_m2m1, r_01),
                                        _mm256_sub_epi16(r_01, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(r_m2m1, mask_00ff), r_01));
  *acc = _mm256_add_epi8(*acc, constrain_lut_i8(rm, v_sec_lut, v_112));
}

/* Merged odd-d near secondary + primary. */
static inline __m256i odd_near_lut_mm_4r(__m256i *sec_out, const uint16_t *p,
                                         int d, __m256i r_01, __m256i r_23,
                                         __m256i v_sec_lut, __m256i v_pri_lut,
                                         __m256i v_112, __m256i *max_u8,
                                         __m256i *min_u8) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  __m256i pn = _mm256_setzero_si256();
  const __m256i rhp = raw8_seq_mm_4r(p + 1, r_01, r_23, max_u8, min_u8);
  const __m256i ihp = _mm256_adds_epu8(_mm256_abs_epi8(rhp), v_112);
  __m256i sn = _mm256_sign_epi8(_mm256_shuffle_epi8(v_sec_lut, ihp), rhp);
  if (!(d & 4)) {
    pn = _mm256_sign_epi8(_mm256_shuffle_epi8(v_pri_lut, ihp), rhp);
  }
  const __m256i rhm = raw8_seq_mm_4r(p - 1, r_01, r_23, max_u8, min_u8);
  const __m256i ihm = _mm256_adds_epu8(_mm256_abs_epi8(rhm), v_112);
  sn = _mm256_add_epi8(
      sn, _mm256_sign_epi8(_mm256_shuffle_epi8(v_sec_lut, ihm), rhm));
  if (!(d & 4)) {
    pn = _mm256_add_epi8(
        pn, _mm256_sign_epi8(_mm256_shuffle_epi8(v_pri_lut, ihm), rhm));
  }

  const __m256i r_12 = load_u16_8x2(p + CDEF_BSTRIDE);
  const __m256i r_34 = load_u16_8x2(p + 3 * CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_12, r_34));
  const __m256i rvp = _mm256_packs_epi16(_mm256_sub_epi16(r_12, r_01),
                                         _mm256_sub_epi16(r_34, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(r_12, mask_00ff),
                                   _mm256_and_si256(r_34, mask_00ff)));
  const __m256i ivp = _mm256_adds_epu8(_mm256_abs_epi8(rvp), v_112);
  sn = _mm256_add_epi8(
      sn, _mm256_sign_epi8(_mm256_shuffle_epi8(v_sec_lut, ivp), rvp));
  if (d & 4) {
    pn = _mm256_sign_epi8(_mm256_shuffle_epi8(v_pri_lut, ivp), rvp);
  }
  const __m256i r_m10 = load_u16_8x2(p - CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_m10, r_12));
  const __m256i rvm = _mm256_packs_epi16(_mm256_sub_epi16(r_m10, r_01),
                                         _mm256_sub_epi16(r_12, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(r_m10, mask_00ff),
                                   _mm256_and_si256(r_12, mask_00ff)));
  const __m256i ivm = _mm256_adds_epu8(_mm256_abs_epi8(rvm), v_112);
  *sec_out = _mm256_add_epi8(
      sn, _mm256_sign_epi8(_mm256_shuffle_epi8(v_sec_lut, ivm), rvm));
  if (d & 4) {
    pn = _mm256_add_epi8(
        pn, _mm256_sign_epi8(_mm256_shuffle_epi8(v_pri_lut, ivm), rvm));
  }
  return pn;
}

static inline __m256i odd_near_hoist_mm_4r(__m256i *sec_out, const uint16_t *p,
                                           int d, __m256i r_01, __m256i r_23,
                                           __m256i v_sec_lut, __m256i v_112,
                                           __m256i v_pt, __m128i v_ps,
                                           const uint8_t *sh_mask_ptr,
                                           __m256i *max_u8, __m256i *min_u8) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  __m256i pn = _mm256_setzero_si256();
  const __m256i rhp = raw8_seq_mm_4r(p + 1, r_01, r_23, max_u8, min_u8);
  __m256i sn = constrain_lut_i8(rhp, v_sec_lut, v_112);
  if (!(d & 4)) pn = constrain_hoist_i8(rhp, v_pt, v_ps, sh_mask_ptr);
  const __m256i rhm = raw8_seq_mm_4r(p - 1, r_01, r_23, max_u8, min_u8);
  sn = _mm256_add_epi8(sn, constrain_lut_i8(rhm, v_sec_lut, v_112));
  if (!(d & 4)) {
    pn = _mm256_add_epi8(pn, constrain_hoist_i8(rhm, v_pt, v_ps, sh_mask_ptr));
  }

  const __m256i r_12 = load_u16_8x2(p + CDEF_BSTRIDE);
  const __m256i r_34 = load_u16_8x2(p + 3 * CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_12, r_34));
  const __m256i rvp = _mm256_packs_epi16(_mm256_sub_epi16(r_12, r_01),
                                         _mm256_sub_epi16(r_34, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(r_12, mask_00ff),
                                   _mm256_and_si256(r_34, mask_00ff)));
  sn = _mm256_add_epi8(sn, constrain_lut_i8(rvp, v_sec_lut, v_112));
  if (d & 4) pn = constrain_hoist_i8(rvp, v_pt, v_ps, sh_mask_ptr);
  const __m256i r_m10 = load_u16_8x2(p - CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_m10, r_12));
  const __m256i rvm = _mm256_packs_epi16(_mm256_sub_epi16(r_m10, r_01),
                                         _mm256_sub_epi16(r_12, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(r_m10, mask_00ff),
                                   _mm256_and_si256(r_12, mask_00ff)));
  *sec_out = _mm256_add_epi8(sn, constrain_lut_i8(rvm, v_sec_lut, v_112));
  if (d & 4) {
    pn = _mm256_add_epi8(pn, constrain_hoist_i8(rvm, v_pt, v_ps, sh_mask_ptr));
  }
  return pn;
}

/* Dual-LUT primary+secondary init/accum (pri_t == 11, pri_damp == 0). */
static inline __m256i pri_sec_lut_init_mm_4r(__m256i *sec_out,
                                             const uint16_t *p, intptr_t off,
                                             __m256i r_01, __m256i r_23,
                                             __m256i v_sec_lut,
                                             __m256i v_pri_lut, __m256i v_112,
                                             __m256i *max_u8, __m256i *min_u8) {
  const __m256i rp = raw8_seq_mm_4r(p + off, r_01, r_23, max_u8, min_u8);
  const __m256i idx_p = _mm256_adds_epu8(_mm256_abs_epi8(rp), v_112);
  const __m256i s_acc =
      _mm256_sign_epi8(_mm256_shuffle_epi8(v_sec_lut, idx_p), rp);
  const __m256i p_acc =
      _mm256_sign_epi8(_mm256_shuffle_epi8(v_pri_lut, idx_p), rp);
  const __m256i rm = raw8_seq_mm_4r(p - off, r_01, r_23, max_u8, min_u8);
  const __m256i idx_m = _mm256_adds_epu8(_mm256_abs_epi8(rm), v_112);
  *sec_out = _mm256_add_epi8(
      s_acc, _mm256_sign_epi8(_mm256_shuffle_epi8(v_sec_lut, idx_m), rm));
  return _mm256_add_epi8(
      p_acc, _mm256_sign_epi8(_mm256_shuffle_epi8(v_pri_lut, idx_m), rm));
}

static inline __m256i pri_sec_lut_acc_mm_4r(__m256i *sec_acc, const uint16_t *p,
                                            intptr_t off, __m256i r_01,
                                            __m256i r_23, __m256i v_sec_lut,
                                            __m256i v_pri_lut, __m256i v_112,
                                            __m256i *max_u8, __m256i *min_u8) {
  const __m256i rp = raw8_seq_mm_4r(p + off, r_01, r_23, max_u8, min_u8);
  const __m256i idx_p = _mm256_adds_epu8(_mm256_abs_epi8(rp), v_112);
  *sec_acc = _mm256_add_epi8(
      *sec_acc, _mm256_sign_epi8(_mm256_shuffle_epi8(v_sec_lut, idx_p), rp));
  const __m256i p_acc =
      _mm256_sign_epi8(_mm256_shuffle_epi8(v_pri_lut, idx_p), rp);
  const __m256i rm = raw8_seq_mm_4r(p - off, r_01, r_23, max_u8, min_u8);
  const __m256i idx_m = _mm256_adds_epu8(_mm256_abs_epi8(rm), v_112);
  *sec_acc = _mm256_add_epi8(
      *sec_acc, _mm256_sign_epi8(_mm256_shuffle_epi8(v_sec_lut, idx_m), rm));
  return _mm256_add_epi8(
      p_acc, _mm256_sign_epi8(_mm256_shuffle_epi8(v_pri_lut, idx_m), rm));
}

static inline __m256i pri_sec_lut_vert1_init_mm_4r(
    __m256i *sec_out, const uint16_t *p, __m256i r_01, __m256i r_23,
    __m256i v_sec_lut, __m256i v_pri_lut, __m256i v_112, __m256i *max_u8,
    __m256i *min_u8) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  const __m256i r_12 = load_u16_8x2(p + CDEF_BSTRIDE);
  const __m256i r_34 = load_u16_8x2(p + 3 * CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_12, r_34));
  const __m256i rp = _mm256_packs_epi16(_mm256_sub_epi16(r_12, r_01),
                                        _mm256_sub_epi16(r_34, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(r_12, mask_00ff),
                                   _mm256_and_si256(r_34, mask_00ff)));
  const __m256i idx_p = _mm256_adds_epu8(_mm256_abs_epi8(rp), v_112);
  const __m256i s_acc =
      _mm256_sign_epi8(_mm256_shuffle_epi8(v_sec_lut, idx_p), rp);
  const __m256i p_acc =
      _mm256_sign_epi8(_mm256_shuffle_epi8(v_pri_lut, idx_p), rp);
  const __m256i r_m10 = load_u16_8x2(p - CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_m10, r_12));
  const __m256i rm = _mm256_packs_epi16(_mm256_sub_epi16(r_m10, r_01),
                                        _mm256_sub_epi16(r_12, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(r_m10, mask_00ff),
                                   _mm256_and_si256(r_12, mask_00ff)));
  const __m256i idx_m = _mm256_adds_epu8(_mm256_abs_epi8(rm), v_112);
  *sec_out = _mm256_add_epi8(
      s_acc, _mm256_sign_epi8(_mm256_shuffle_epi8(v_sec_lut, idx_m), rm));
  return _mm256_add_epi8(
      p_acc, _mm256_sign_epi8(_mm256_shuffle_epi8(v_pri_lut, idx_m), rm));
}

static inline __m256i pri_sec_lut_vert2_acc_mm_4r(
    __m256i *sec_acc, const uint16_t *p, __m256i r_01, __m256i r_23,
    __m256i v_sec_lut, __m256i v_pri_lut, __m256i v_112, __m256i *max_u8,
    __m256i *min_u8) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  const __m256i r_45 = load_u16_8x2(p + 4 * CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_23, r_45));
  const __m256i rp = _mm256_packs_epi16(_mm256_sub_epi16(r_23, r_01),
                                        _mm256_sub_epi16(r_45, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(r_23, _mm256_and_si256(r_45, mask_00ff)));
  const __m256i idx_p = _mm256_adds_epu8(_mm256_abs_epi8(rp), v_112);
  *sec_acc = _mm256_add_epi8(
      *sec_acc, _mm256_sign_epi8(_mm256_shuffle_epi8(v_sec_lut, idx_p), rp));
  const __m256i p_acc =
      _mm256_sign_epi8(_mm256_shuffle_epi8(v_pri_lut, idx_p), rp);
  const __m256i r_m2m1 = load_u16_8x2(p - 2 * CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_m2m1, r_01));
  const __m256i rm = _mm256_packs_epi16(_mm256_sub_epi16(r_m2m1, r_01),
                                        _mm256_sub_epi16(r_01, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(r_m2m1, mask_00ff), r_01));
  const __m256i idx_m = _mm256_adds_epu8(_mm256_abs_epi8(rm), v_112);
  *sec_acc = _mm256_add_epi8(
      *sec_acc, _mm256_sign_epi8(_mm256_shuffle_epi8(v_sec_lut, idx_m), rm));
  return _mm256_add_epi8(
      p_acc, _mm256_sign_epi8(_mm256_shuffle_epi8(v_pri_lut, idx_m), rm));
}

static inline __m256i pair_hoist_mm_4r(const uint16_t *p, intptr_t off,
                                       __m256i r_01, __m256i r_23, __m256i v_t,
                                       __m128i v_s, const uint8_t *sh_mask_ptr,
                                       __m256i *max_u8, __m256i *min_u8) {
  const __m256i rp = raw8_seq_mm_4r(p + off, r_01, r_23, max_u8, min_u8);
  const __m256i cp = constrain_hoist_i8(rp, v_t, v_s, sh_mask_ptr);
  const __m256i rm = raw8_seq_mm_4r(p - off, r_01, r_23, max_u8, min_u8);
  return _mm256_add_epi8(cp, constrain_hoist_i8(rm, v_t, v_s, sh_mask_ptr));
}

static inline __m256i pri_sec_hoist_init_mm_4r(
    __m256i *sec_out, const uint16_t *p, intptr_t off, __m256i r_01,
    __m256i r_23, __m256i v_sec_lut, __m256i v_112, __m256i v_pt, __m128i v_ps,
    const uint8_t *sh_mask_ptr, __m256i *max_u8, __m256i *min_u8) {
  const __m256i rp = raw8_seq_mm_4r(p + off, r_01, r_23, max_u8, min_u8);
  const __m256i s_acc = constrain_lut_i8(rp, v_sec_lut, v_112);
  const __m256i p_acc = constrain_hoist_i8(rp, v_pt, v_ps, sh_mask_ptr);
  const __m256i rm = raw8_seq_mm_4r(p - off, r_01, r_23, max_u8, min_u8);
  *sec_out = _mm256_add_epi8(s_acc, constrain_lut_i8(rm, v_sec_lut, v_112));
  return _mm256_add_epi8(p_acc,
                         constrain_hoist_i8(rm, v_pt, v_ps, sh_mask_ptr));
}

static inline __m256i pri_sec_hoist_acc_mm_4r(
    __m256i *sec_acc, const uint16_t *p, intptr_t off, __m256i r_01,
    __m256i r_23, __m256i v_sec_lut, __m256i v_112, __m256i v_pt, __m128i v_ps,
    const uint8_t *sh_mask_ptr, __m256i *max_u8, __m256i *min_u8) {
  const __m256i rp = raw8_seq_mm_4r(p + off, r_01, r_23, max_u8, min_u8);
  *sec_acc = _mm256_add_epi8(*sec_acc, constrain_lut_i8(rp, v_sec_lut, v_112));
  const __m256i p_acc = constrain_hoist_i8(rp, v_pt, v_ps, sh_mask_ptr);
  const __m256i rm = raw8_seq_mm_4r(p - off, r_01, r_23, max_u8, min_u8);
  *sec_acc = _mm256_add_epi8(*sec_acc, constrain_lut_i8(rm, v_sec_lut, v_112));
  return _mm256_add_epi8(p_acc,
                         constrain_hoist_i8(rm, v_pt, v_ps, sh_mask_ptr));
}

static inline __m256i pri_sec_hoist_vert1_init_mm_4r(
    __m256i *sec_out, const uint16_t *p, __m256i r_01, __m256i r_23,
    __m256i v_sec_lut, __m256i v_112, __m256i v_pt, __m128i v_ps,
    const uint8_t *sh_mask_ptr, __m256i *max_u8, __m256i *min_u8) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  const __m256i r_12 = load_u16_8x2(p + CDEF_BSTRIDE);
  const __m256i r_34 = load_u16_8x2(p + 3 * CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_12, r_34));
  const __m256i rp = _mm256_packs_epi16(_mm256_sub_epi16(r_12, r_01),
                                        _mm256_sub_epi16(r_34, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(r_12, mask_00ff),
                                   _mm256_and_si256(r_34, mask_00ff)));
  const __m256i s_acc = constrain_lut_i8(rp, v_sec_lut, v_112);
  const __m256i p_acc = constrain_hoist_i8(rp, v_pt, v_ps, sh_mask_ptr);
  const __m256i r_m10 = load_u16_8x2(p - CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_m10, r_12));
  const __m256i rm = _mm256_packs_epi16(_mm256_sub_epi16(r_m10, r_01),
                                        _mm256_sub_epi16(r_12, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(r_m10, mask_00ff),
                                   _mm256_and_si256(r_12, mask_00ff)));
  *sec_out = _mm256_add_epi8(s_acc, constrain_lut_i8(rm, v_sec_lut, v_112));
  return _mm256_add_epi8(p_acc,
                         constrain_hoist_i8(rm, v_pt, v_ps, sh_mask_ptr));
}

static inline __m256i pri_sec_hoist_vert2_acc_mm_4r(
    __m256i *sec_acc, const uint16_t *p, __m256i r_01, __m256i r_23,
    __m256i v_sec_lut, __m256i v_112, __m256i v_pt, __m128i v_ps,
    const uint8_t *sh_mask_ptr, __m256i *max_u8, __m256i *min_u8) {
  const __m256i mask_00ff = _mm256_set1_epi16(0x00ff);
  const __m256i r_45 = load_u16_8x2(p + 4 * CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_23, r_45));
  const __m256i rp = _mm256_packs_epi16(_mm256_sub_epi16(r_23, r_01),
                                        _mm256_sub_epi16(r_45, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(r_23, _mm256_and_si256(r_45, mask_00ff)));
  *sec_acc = _mm256_add_epi8(*sec_acc, constrain_lut_i8(rp, v_sec_lut, v_112));
  const __m256i p_acc = constrain_hoist_i8(rp, v_pt, v_ps, sh_mask_ptr);
  const __m256i r_m2m1 = load_u16_8x2(p - 2 * CDEF_BSTRIDE);
  *min_u8 = _mm256_min_epu8(*min_u8, _mm256_packus_epi16(r_m2m1, r_01));
  const __m256i rm = _mm256_packs_epi16(_mm256_sub_epi16(r_m2m1, r_01),
                                        _mm256_sub_epi16(r_01, r_23));
  *max_u8 = _mm256_max_epu8(
      *max_u8, _mm256_packus_epi16(_mm256_and_si256(r_m2m1, mask_00ff), r_01));
  *sec_acc = _mm256_add_epi8(*sec_acc, constrain_lut_i8(rm, v_sec_lut, v_112));
  return _mm256_add_epi8(p_acc,
                         constrain_hoist_i8(rm, v_pt, v_ps, sh_mask_ptr));
}

static inline __m256i sec_diag26_lut_mm_4r(const uint16_t *p, __m256i r_01,
                                           __m256i r_23, __m256i v_sec_lut,
                                           __m256i v_112, __m256i *max_u8,
                                           __m256i *min_u8) {
  __m256i acc =
      init_sec_vert1_mm_4r(p + 1, r_01, r_23, v_sec_lut, v_112, max_u8, min_u8);
  acc_sec_vert1_mm_4r(&acc, p - 1, r_01, r_23, v_sec_lut, v_112, max_u8,
                      min_u8);
  acc = _mm256_add_epi8(acc, acc);
  acc_sec_pair_mm_4r(&acc, p, -2 * CDEF_BSTRIDE + 2, r_01, r_23, v_sec_lut,
                     v_112, max_u8, min_u8);
  acc_sec_pair_mm_4r(&acc, p, 2 * CDEF_BSTRIDE + 2, r_01, r_23, v_sec_lut,
                     v_112, max_u8, min_u8);
  return acc;
}

static inline __m256i sec_diag26_lut_4r(const uint16_t *p, __m256i r_01,
                                        __m256i r_23, __m256i v_sec_lut,
                                        __m256i v_112) {
  __m256i acc = init_sec_vert1_4r(p + 1, r_01, r_23, v_sec_lut, v_112);
  acc_sec_vert1_4r(&acc, p - 1, r_01, r_23, v_sec_lut, v_112);
  acc = _mm256_add_epi8(acc, acc);
  acc_sec_pair_4r(&acc, p, -2 * CDEF_BSTRIDE + 2, r_01, r_23, v_sec_lut, v_112);
  acc_sec_pair_4r(&acc, p, 2 * CDEF_BSTRIDE + 2, r_01, r_23, v_sec_lut, v_112);
  return acc;
}

static inline void finish_split_4r_i8(__m256i r_01, __m256i r_23,
                                      const uint8_t *rr, intptr_t ref_stride,
                                      __m256i sum_sec0, __m256i sum_pri,
                                      __m256i sum_sec3, __m256i max_u8,
                                      __m256i min_u8, __m256i *v_sse0,
                                      __m256i *v_sse1, __m256i *v_sse2,
                                      __m256i *v_sse3) {
  {
    const __m256i d0 = _mm256_sub_epi16(r_01, load_ref16_8x2(rr, ref_stride));
    *v_sse0 = _mm256_add_epi32(*v_sse0, _mm256_madd_epi16(d0, d0));
    const __m256i d1 =
        _mm256_add_epi16(d0, finish_delta_fast(unpack_i8_lo_s16(sum_sec0)));
    *v_sse1 = _mm256_add_epi32(*v_sse1, _mm256_madd_epi16(d1, d1));
    const __m256i pri16 = unpack_i8_lo_s16(sum_pri);
    const __m256i d2 = _mm256_add_epi16(d0, finish_delta_fast(pri16));
    *v_sse2 = _mm256_add_epi32(*v_sse2, _mm256_madd_epi16(d2, d2));
    const __m256i tot16 = _mm256_add_epi16(pri16, unpack_i8_lo_s16(sum_sec3));
    __m256i res3 = _mm256_add_epi16(r_01, finish_delta_fast(tot16));
    res3 = _mm256_min_epi16(_mm256_max_epi16(res3, unpack_u8_lo_u16(min_u8)),
                            unpack_u8_lo_u16(max_u8));
    const __m256i d3 = _mm256_add_epi16(d0, _mm256_sub_epi16(res3, r_01));
    *v_sse3 = _mm256_add_epi32(*v_sse3, _mm256_madd_epi16(d3, d3));
  }
  {
    const __m256i ref23 = load_ref16_8x2(rr + 2 * ref_stride, ref_stride);
    const __m256i d0 = _mm256_sub_epi16(r_23, ref23);
    *v_sse0 = _mm256_add_epi32(*v_sse0, _mm256_madd_epi16(d0, d0));
    const __m256i d1 =
        _mm256_add_epi16(d0, finish_delta_fast(unpack_i8_hi_s16(sum_sec0)));
    *v_sse1 = _mm256_add_epi32(*v_sse1, _mm256_madd_epi16(d1, d1));
    const __m256i pri16 = unpack_i8_hi_s16(sum_pri);
    const __m256i d2 = _mm256_add_epi16(d0, finish_delta_fast(pri16));
    *v_sse2 = _mm256_add_epi32(*v_sse2, _mm256_madd_epi16(d2, d2));
    const __m256i tot16 = _mm256_add_epi16(pri16, unpack_i8_hi_s16(sum_sec3));
    __m256i res3 = _mm256_add_epi16(r_23, finish_delta_fast(tot16));
    res3 = _mm256_min_epi16(_mm256_max_epi16(res3, unpack_u8_hi_u16(min_u8)),
                            unpack_u8_hi_u16(max_u8));
    const __m256i d3 = _mm256_sub_epi16(res3, ref23);
    *v_sse3 = _mm256_add_epi32(*v_sse3, _mm256_madd_epi16(d3, d3));
  }
}

static inline void finish_all_same_4r_i8(__m256i r_01, __m256i r_23,
                                         const uint8_t *rr, intptr_t ref_stride,
                                         __m256i sum_sec, __m256i sum_pri,
                                         __m256i max_u8, __m256i min_u8,
                                         __m256i *v_sse0, __m256i *v_sse1,
                                         __m256i *v_sse2, __m256i *v_sse3) {
  {
    const __m256i d0 = _mm256_sub_epi16(r_01, load_ref16_8x2(rr, ref_stride));
    *v_sse0 = _mm256_add_epi32(*v_sse0, _mm256_madd_epi16(d0, d0));
    const __m256i sec16 = unpack_i8_lo_s16(sum_sec);
    const __m256i d1 = _mm256_add_epi16(d0, finish_delta_fast(sec16));
    *v_sse1 = _mm256_add_epi32(*v_sse1, _mm256_madd_epi16(d1, d1));
    const __m256i pri16 = unpack_i8_lo_s16(sum_pri);
    const __m256i d2 = _mm256_add_epi16(d0, finish_delta_fast(pri16));
    *v_sse2 = _mm256_add_epi32(*v_sse2, _mm256_madd_epi16(d2, d2));
    const __m256i tot16 = _mm256_add_epi16(pri16, sec16);
    __m256i res3 = _mm256_add_epi16(r_01, finish_delta_fast(tot16));
    res3 = _mm256_min_epi16(_mm256_max_epi16(res3, unpack_u8_lo_u16(min_u8)),
                            unpack_u8_lo_u16(max_u8));
    const __m256i d3 = _mm256_add_epi16(d0, _mm256_sub_epi16(res3, r_01));
    *v_sse3 = _mm256_add_epi32(*v_sse3, _mm256_madd_epi16(d3, d3));
  }
  {
    const __m256i ref23 = load_ref16_8x2(rr + 2 * ref_stride, ref_stride);
    const __m256i d0 = _mm256_sub_epi16(r_23, ref23);
    *v_sse0 = _mm256_add_epi32(*v_sse0, _mm256_madd_epi16(d0, d0));
    const __m256i sec16 = unpack_i8_hi_s16(sum_sec);
    const __m256i d1 = _mm256_add_epi16(d0, finish_delta_fast(sec16));
    *v_sse1 = _mm256_add_epi32(*v_sse1, _mm256_madd_epi16(d1, d1));
    const __m256i pri16 = unpack_i8_hi_s16(sum_pri);
    const __m256i d2 = _mm256_add_epi16(d0, finish_delta_fast(pri16));
    *v_sse2 = _mm256_add_epi32(*v_sse2, _mm256_madd_epi16(d2, d2));
    const __m256i tot16 = _mm256_add_epi16(pri16, sec16);
    __m256i res3 = _mm256_add_epi16(r_23, finish_delta_fast(tot16));
    res3 = _mm256_min_epi16(_mm256_max_epi16(res3, unpack_u8_hi_u16(min_u8)),
                            unpack_u8_hi_u16(max_u8));
    const __m256i d3 = _mm256_sub_epi16(res3, ref23);
    *v_sse3 = _mm256_add_epi32(*v_sse3, _mm256_madd_epi16(d3, d3));
  }
}

static inline void finish_t0_same_4r_i8(__m256i r_01, __m256i r_23,
                                        const uint8_t *rr, intptr_t ref_stride,
                                        __m256i sum_sec0, __m256i *v_sse0,
                                        __m256i *v_sse1, __m256i *v_sse2,
                                        __m256i *v_sse3) {
  const __m256i d0_01 = _mm256_sub_epi16(r_01, load_ref16_8x2(rr, ref_stride));
  const __m256i d0_23 =
      _mm256_sub_epi16(r_23, load_ref16_8x2(rr + 2 * ref_stride, ref_stride));
  const __m256i s0 = _mm256_add_epi32(_mm256_madd_epi16(d0_01, d0_01),
                                      _mm256_madd_epi16(d0_23, d0_23));
  *v_sse0 = _mm256_add_epi32(*v_sse0, s0);
  *v_sse2 = _mm256_add_epi32(*v_sse2, s0);

  const __m256i d1_01 =
      _mm256_add_epi16(d0_01, finish_delta_fast(unpack_i8_lo_s16(sum_sec0)));
  const __m256i d1_23 =
      _mm256_add_epi16(d0_23, finish_delta_fast(unpack_i8_hi_s16(sum_sec0)));
  const __m256i s1 = _mm256_add_epi32(_mm256_madd_epi16(d1_01, d1_01),
                                      _mm256_madd_epi16(d1_23, d1_23));
  *v_sse1 = _mm256_add_epi32(*v_sse1, s1);
  *v_sse3 = _mm256_add_epi32(*v_sse3, s1);
}

static inline void finish_t0_split_4r_i8(__m256i r_01, __m256i r_23,
                                         const uint8_t *rr, intptr_t ref_stride,
                                         __m256i sum_sec0, __m256i sum_secd,
                                         __m256i *v_sse0, __m256i *v_sse1,
                                         __m256i *v_sse2, __m256i *v_sse3) {
  {
    const __m256i d0 = _mm256_sub_epi16(r_01, load_ref16_8x2(rr, ref_stride));
    const __m256i s0 = _mm256_madd_epi16(d0, d0);
    *v_sse0 = _mm256_add_epi32(*v_sse0, s0);
    *v_sse2 = _mm256_add_epi32(*v_sse2, s0);
    const __m256i d1 =
        _mm256_add_epi16(d0, finish_delta_fast(unpack_i8_lo_s16(sum_sec0)));
    *v_sse1 = _mm256_add_epi32(*v_sse1, _mm256_madd_epi16(d1, d1));
    const __m256i d3 =
        _mm256_add_epi16(d0, finish_delta_fast(unpack_i8_lo_s16(sum_secd)));
    *v_sse3 = _mm256_add_epi32(*v_sse3, _mm256_madd_epi16(d3, d3));
  }
  {
    const __m256i d0 =
        _mm256_sub_epi16(r_23, load_ref16_8x2(rr + 2 * ref_stride, ref_stride));
    const __m256i s0 = _mm256_madd_epi16(d0, d0);
    *v_sse0 = _mm256_add_epi32(*v_sse0, s0);
    *v_sse2 = _mm256_add_epi32(*v_sse2, s0);
    const __m256i d1 =
        _mm256_add_epi16(d0, finish_delta_fast(unpack_i8_hi_s16(sum_sec0)));
    *v_sse1 = _mm256_add_epi32(*v_sse1, _mm256_madd_epi16(d1, d1));
    const __m256i d3 =
        _mm256_add_epi16(d0, finish_delta_fast(unpack_i8_hi_s16(sum_secd)));
    *v_sse3 = _mm256_add_epi32(*v_sse3, _mm256_madd_epi16(d3, d3));
  }
}

static inline __m256i combine_pri_3_i8(__m256i pn, __m256i pf) {
  const __m256i s = _mm256_add_epi8(pn, pf);
  return _mm256_add_epi8(_mm256_add_epi8(s, s), s);
}

static inline __m256i combine_pri_even_odd_i8(__m256i pn, __m256i pf,
                                              int is_odd) {
  if (is_odd) return combine_pri_3_i8(pn, pf);
  const __m256i s = _mm256_add_epi8(_mm256_add_epi8(pn, pn), pf);
  return _mm256_add_epi8(s, s);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void cdef_mse_calc_block_lvl4_chroma_8x8_fast_avx2(
    const uint16_t *in, const uint8_t *ref_base, int ref_stride,
    const cdef_list *dlist, int cdef_count,
    const int dir[CDEF_NBLOCKS][CDEF_NBLOCKS], int sec_damp_val, int pli,
    uint64_t mse[4]) {
  assert(pli == 1 || pli == 2);
  assert(sec_damp_val >= 1 && sec_damp_val <= 2);
  __m256i v_sse0 = _mm256_setzero_si256();
  __m256i v_sse1 = _mm256_setzero_si256();
  __m256i v_sse2 = _mm256_setzero_si256();
  __m256i v_sse3 = _mm256_setzero_si256();

  const __m256i v_sec_lut = make_sec_lut_avx2(sec_damp_val);
  const __m256i v_pri_lut = make_pri11_d0_lut_avx2();
  const __m256i v_112 = _mm256_set1_epi8(0x70);

  for (int bi = 0; bi < cdef_count; ++bi) {
    const int by = dlist[bi].by;
    const int bx = dlist[bi].bx;
    const int d = dir[by][bx];
    const uint16_t *p = &in[(by * CDEF_BSTRIDE << 3) + (bx << 3)];
    const uint8_t *rr =
        &ref_base[(ptrdiff_t)(by << 3) * ref_stride + (bx << 3)];

    if ((d & 1) != 0) {
      const intptr_t po2 = cdef_directions_padded_inline[d + 2][1];
      const intptr_t s1o2 = cdef_directions_padded_inline[d + 4][1];
      const intptr_t s2o2 = cdef_directions_padded_inline[d][1];
      for (int i = 0; i < 8;
           i += 4, p += 4 * CDEF_BSTRIDE, rr += 4 * ref_stride) {
        const __m256i r_01 = load_u16_8x2(p);
        const __m256i r_23 = load_u16_8x2(p + 2 * CDEF_BSTRIDE);
        __m256i max_u8 = _mm256_packus_epi16(r_01, r_23);
        __m256i min_u8 = max_u8;
        __m256i sn;
        const __m256i pn =
            odd_near_lut_mm_4r(&sn, p, d, r_01, r_23, v_sec_lut, v_pri_lut,
                               v_112, &max_u8, &min_u8);
        const __m256i pf = pair_lut_mm_4r(p, po2, r_01, r_23, v_pri_lut, v_112,
                                          &max_u8, &min_u8);
        const __m256i sum_pri = combine_pri_3_i8(pn, pf);
        __m256i sum_sec0 = _mm256_add_epi8(sn, sn);
        __m256i sum_sec3 = sum_sec0;
        acc_sec_pair_mm_4r(&sum_sec3, p, s1o2, r_01, r_23, v_sec_lut, v_112,
                           &max_u8, &min_u8);
        acc_sec_pair_mm_4r(&sum_sec3, p, s2o2, r_01, r_23, v_sec_lut, v_112,
                           &max_u8, &min_u8);
        acc_sec_pair_4r(&sum_sec0, p, 2, r_01, r_23, v_sec_lut, v_112);
        acc_sec_vert2_4r(&sum_sec0, p, r_01, r_23, v_sec_lut, v_112);
        finish_split_4r_i8(r_01, r_23, rr, ref_stride, sum_sec0, sum_pri,
                           sum_sec3, max_u8, min_u8, &v_sse0, &v_sse1, &v_sse2,
                           &v_sse3);
      }
    } else if ((d & 3) == 0) {
      const intptr_t po1 = cdef_directions_padded_inline[d + 2][0];
      const intptr_t po2 = 2 * po1;
      for (int i = 0; i < 8;
           i += 4, p += 4 * CDEF_BSTRIDE, rr += 4 * ref_stride) {
        const __m256i r_01 = load_u16_8x2(p);
        const __m256i r_23 = load_u16_8x2(p + 2 * CDEF_BSTRIDE);
        __m256i max_u8 = _mm256_packus_epi16(r_01, r_23);
        __m256i min_u8 = max_u8;
        __m256i sum_sec = init_sec_vert1_mm_4r(p, r_01, r_23, v_sec_lut, v_112,
                                               &max_u8, &min_u8);
        acc_sec_pair_mm_4r(&sum_sec, p, 1, r_01, r_23, v_sec_lut, v_112,
                           &max_u8, &min_u8);
        sum_sec = _mm256_add_epi8(sum_sec, sum_sec);
        acc_sec_pair_mm_4r(&sum_sec, p, 2, r_01, r_23, v_sec_lut, v_112,
                           &max_u8, &min_u8);
        acc_sec_vert2_mm_4r(&sum_sec, p, r_01, r_23, v_sec_lut, v_112, &max_u8,
                            &min_u8);

        const __m256i pn = pair_lut_mm_4r(p, po1, r_01, r_23, v_pri_lut, v_112,
                                          &max_u8, &min_u8);
        const __m256i pf = pair_lut_mm_4r(p, po2, r_01, r_23, v_pri_lut, v_112,
                                          &max_u8, &min_u8);
        const __m256i sum_pri = combine_pri_3_i8(pn, pf);
        finish_all_same_4r_i8(r_01, r_23, rr, ref_stride, sum_sec, sum_pri,
                              max_u8, min_u8, &v_sse0, &v_sse1, &v_sse2,
                              &v_sse3);
      }
    } else {
      for (int i = 0; i < 8;
           i += 4, p += 4 * CDEF_BSTRIDE, rr += 4 * ref_stride) {
        const __m256i r_01 = load_u16_8x2(p);
        const __m256i r_23 = load_u16_8x2(p + 2 * CDEF_BSTRIDE);
        __m256i max_u8 = _mm256_packus_epi16(r_01, r_23);
        __m256i min_u8 = max_u8;
        __m256i sum_sec0, sum_pri;
        if (d & 4) {
          const __m256i pn =
              pri_sec_lut_vert1_init_mm_4r(&sum_sec0, p, r_01, r_23, v_sec_lut,
                                           v_pri_lut, v_112, &max_u8, &min_u8);
          acc_sec_pair_4r(&sum_sec0, p, 1, r_01, r_23, v_sec_lut, v_112);
          sum_sec0 = _mm256_add_epi8(sum_sec0, sum_sec0);
          const __m256i pf =
              pri_sec_lut_vert2_acc_mm_4r(&sum_sec0, p, r_01, r_23, v_sec_lut,
                                          v_pri_lut, v_112, &max_u8, &min_u8);
          sum_pri = combine_pri_3_i8(pn, pf);
          acc_sec_pair_4r(&sum_sec0, p, 2, r_01, r_23, v_sec_lut, v_112);
        } else {
          const __m256i pn =
              pri_sec_lut_init_mm_4r(&sum_sec0, p, 1, r_01, r_23, v_sec_lut,
                                     v_pri_lut, v_112, &max_u8, &min_u8);
          acc_sec_vert1_4r(&sum_sec0, p, r_01, r_23, v_sec_lut, v_112);
          sum_sec0 = _mm256_add_epi8(sum_sec0, sum_sec0);
          const __m256i pf =
              pri_sec_lut_acc_mm_4r(&sum_sec0, p, 2, r_01, r_23, v_sec_lut,
                                    v_pri_lut, v_112, &max_u8, &min_u8);
          sum_pri = combine_pri_3_i8(pn, pf);
          acc_sec_vert2_4r(&sum_sec0, p, r_01, r_23, v_sec_lut, v_112);
        }
        const __m256i sum_sec3 = sec_diag26_lut_mm_4r(p, r_01, r_23, v_sec_lut,
                                                      v_112, &max_u8, &min_u8);
        finish_split_4r_i8(r_01, r_23, rr, ref_stride, sum_sec0, sum_pri,
                           sum_sec3, max_u8, min_u8, &v_sse0, &v_sse1, &v_sse2,
                           &v_sse3);
      }
    }
  }

  if (pli <= 1) {
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

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void cdef_mse_calc_block_lvl4_8x8_cold_avx2(
    const uint16_t *in, const uint8_t *ref_base, int ref_stride,
    const cdef_list *dlist, int cdef_count,
    const int dir[CDEF_NBLOCKS][CDEF_NBLOCKS],
    const int var[CDEF_NBLOCKS][CDEF_NBLOCKS], int damping, int pli,
    uint64_t mse[4]) {
  assert(pli >= 0 && pli <= 2);
  __m256i v_sse0 = _mm256_setzero_si256();
  __m256i v_sse1 = _mm256_setzero_si256();
  __m256i v_sse2 = _mm256_setzero_si256();
  __m256i v_sse3 = _mm256_setzero_si256();
  const int sec_damp = AOMMAX(0, damping - (pli ? 2 : 1));
  const __m128i v_sec_damp = _mm_cvtsi32_si128(sec_damp);
  const __m256i v_sec_thresh = _mm256_set1_epi16(2);
  const int chroma_pri_damp = AOMMAX(0, damping - 4);

  for (int bi = 0; bi < cdef_count; ++bi) {
    const int by = dlist[bi].by;
    const int bx = dlist[bi].bx;
    const int d = dir[by][bx];
    int t = 11;
    int pri_damp = chroma_pri_damp;
    if (pli == 0) {
      const int v = var[by][bx];
      const int lg = AOMMIN(cdef_msb_nz((unsigned int)(v >> 6)), 12);
      t = v ? ((11 * (4 + lg) + 8) >> 4) : 0;
      pri_damp = t ? AOMMAX(0, damping - cdef_msb_nz((unsigned int)t)) : 0;
    }
    const uint16_t *in_row = &in[(by * CDEF_BSTRIDE << 3) + (bx << 3)];
    const uint8_t *ref_row =
        &ref_base[(ptrdiff_t)(by << 3) * ref_stride + (bx << 3)];
    const intptr_t po1 = cdef_directions_padded_inline[d + 2][0];
    const intptr_t po2 = cdef_directions_padded_inline[d + 2][1];
    const intptr_t s1o1 = cdef_directions_padded_inline[d + 4][0];
    const intptr_t s1o2 = cdef_directions_padded_inline[d + 4][1];
    const intptr_t s2o1 = cdef_directions_padded_inline[d][0];
    const intptr_t s2o2 = cdef_directions_padded_inline[d][1];
    const __m128i v_pri_damp = _mm_cvtsi32_si128(pri_damp);
    const __m256i v_pri_thresh = _mm256_set1_epi16((int16_t)t);

    for (int i = 0; i < 8;
         i += 2, in_row += 2 * CDEF_BSTRIDE, ref_row += 2 * ref_stride) {
      const __m256i r = load_u16_8x2(in_row);
      const __m256i diff0 =
          _mm256_sub_epi16(r, load_ref16_8x2(ref_row, ref_stride));
      const __m256i sse0 = _mm256_madd_epi16(diff0, diff0);
      v_sse0 = _mm256_add_epi32(v_sse0, sse0);
      __m256i max_u8 = r;
      __m256i min_s16 = r;

      const __m256i snd1 = tap_pair_sum_minmax_off(
          in_row, s1o1, r, v_sec_thresh, v_sec_damp, &max_u8, &min_s16);
      const __m256i snd2 = tap_pair_sum_minmax_off(
          in_row, s2o1, r, v_sec_thresh, v_sec_damp, &max_u8, &min_s16);
      const __m256i two_snd =
          _mm256_slli_epi16(_mm256_add_epi16(snd1, snd2), 1);
      __m256i sum_sec3 = _mm256_add_epi16(
          two_snd, tap_pair_sum_minmax_off(in_row, s1o2, r, v_sec_thresh,
                                           v_sec_damp, &max_u8, &min_s16));
      sum_sec3 = _mm256_add_epi16(
          sum_sec3, tap_pair_sum_minmax_off(in_row, s2o2, r, v_sec_thresh,
                                            v_sec_damp, &max_u8, &min_s16));

      __m256i sum_sec0 = sum_sec3;
      if ((d & 3) != 0) {
        sum_sec0 = (d & 1)
                       ? two_snd
                       : _mm256_slli_epi16(
                             _mm256_add_epi16(
                                 tap_pair_sum_off(in_row, 1, r, v_sec_thresh,
                                                  v_sec_damp),
                                 tap_pair_sum_off(in_row, CDEF_BSTRIDE, r,
                                                  v_sec_thresh, v_sec_damp)),
                             1);
        sum_sec0 = _mm256_add_epi16(
            sum_sec0, tap_pair_sum_off(in_row, 2, r, v_sec_thresh, v_sec_damp));
        sum_sec0 = _mm256_add_epi16(
            sum_sec0, tap_pair_sum_off(in_row, 2 * CDEF_BSTRIDE, r,
                                       v_sec_thresh, v_sec_damp));
      }
      const __m256i diff1 =
          _mm256_add_epi16(diff0, finish_delta_fast(sum_sec0));
      v_sse1 = _mm256_add_epi32(v_sse1, _mm256_madd_epi16(diff1, diff1));

      if (t == 0) {
        v_sse2 = _mm256_add_epi32(v_sse2, sse0);
        const __m256i diff3 =
            _mm256_add_epi16(diff0, finish_delta_fast(sum_sec3));
        v_sse3 = _mm256_add_epi32(v_sse3, _mm256_madd_epi16(diff3, diff3));
      } else {
        const __m256i pn = tap_pair_sum_minmax_off(
            in_row, po1, r, v_pri_thresh, v_pri_damp, &max_u8, &min_s16);
        const __m256i pf = tap_pair_sum_minmax_off(
            in_row, po2, r, v_pri_thresh, v_pri_damp, &max_u8, &min_s16);
        const __m256i sum_pri = combine_pri_branchless(pn, pf, v_pri_thresh);
        const __m256i diff2 =
            _mm256_add_epi16(diff0, finish_delta_fast(sum_pri));
        v_sse2 = _mm256_add_epi32(v_sse2, _mm256_madd_epi16(diff2, diff2));
        const __m256i diff3 = finish_clamped_diff3(
            r, diff0, _mm256_add_epi16(sum_pri, sum_sec3), max_u8, min_s16);
        v_sse3 = _mm256_add_epi32(v_sse3, _mm256_madd_epi16(diff3, diff3));
      }
    }
  }

  if (pli <= 1) {
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

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void cdef_mse_calc_block_lvl4_luma_core_avx2(
    const uint16_t *in, const uint8_t *ref_base, int ref_stride,
    const cdef_list *dlist, int cdef_count,
    const int dir[CDEF_NBLOCKS][CDEF_NBLOCKS],
    const int var[CDEF_NBLOCKS][CDEF_NBLOCKS], int damping, uint64_t mse[4]) {
  assert(damping >= 3 && damping <= 4);
  __m256i v_sse0 = _mm256_setzero_si256();
  __m256i v_sse1 = _mm256_setzero_si256();
  __m256i v_sse2 = _mm256_setzero_si256();
  __m256i v_sse3 = _mm256_setzero_si256();
  const __m256i v_sec_lut = make_sec_lut_avx2(AOMMAX(0, damping - 1));
  const __m256i v_112 = _mm256_set1_epi8(0x70);

  for (int bi = 0; bi < cdef_count; ++bi) {
    const int by = dlist[bi].by;
    const int bx = dlist[bi].bx;
    const int d = dir[by][bx];
    const int v = var[by][bx];
    const int lg = AOMMIN(cdef_msb_nz((unsigned int)(v >> 6)), 12);
    const int t = v ? ((11 * (4 + lg) + 8) >> 4) : 0;
    const uint16_t *p = &in[(by * CDEF_BSTRIDE << 3) + (bx << 3)];
    const uint8_t *rr =
        &ref_base[(ptrdiff_t)(by << 3) * ref_stride + (bx << 3)];

    if (t == 0) {
      if ((d & 3) == 0) {
        for (int i = 0; i < 8;
             i += 4, p += 4 * CDEF_BSTRIDE, rr += 4 * ref_stride) {
          const __m256i r_01 = load_u16_8x2(p);
          const __m256i r_23 = load_u16_8x2(p + 2 * CDEF_BSTRIDE);
          __m256i sum_sec = init_sec_vert1_4r(p, r_01, r_23, v_sec_lut, v_112);
          acc_sec_pair_4r(&sum_sec, p, 1, r_01, r_23, v_sec_lut, v_112);
          sum_sec = _mm256_add_epi8(sum_sec, sum_sec);
          acc_sec_pair_4r(&sum_sec, p, 2, r_01, r_23, v_sec_lut, v_112);
          acc_sec_vert2_4r(&sum_sec, p, r_01, r_23, v_sec_lut, v_112);
          finish_t0_same_4r_i8(r_01, r_23, rr, ref_stride, sum_sec, &v_sse0,
                               &v_sse1, &v_sse2, &v_sse3);
        }
      } else {
        const intptr_t s1o2 = cdef_directions_padded_inline[d + 4][1];
        const intptr_t s2o2 = cdef_directions_padded_inline[d][1];
        for (int i = 0; i < 8;
             i += 4, p += 4 * CDEF_BSTRIDE, rr += 4 * ref_stride) {
          const __m256i r_01 = load_u16_8x2(p);
          const __m256i r_23 = load_u16_8x2(p + 2 * CDEF_BSTRIDE);
          __m256i sn = init_sec_vert1_4r(p, r_01, r_23, v_sec_lut, v_112);
          acc_sec_pair_4r(&sn, p, 1, r_01, r_23, v_sec_lut, v_112);
          const __m256i two_sn = _mm256_add_epi8(sn, sn);

          __m256i sum_sec0 = two_sn;
          acc_sec_pair_4r(&sum_sec0, p, 2, r_01, r_23, v_sec_lut, v_112);
          acc_sec_vert2_4r(&sum_sec0, p, r_01, r_23, v_sec_lut, v_112);

          __m256i sum_secd;
          if (d & 1) {
            sum_secd = two_sn;
            acc_sec_pair_4r(&sum_secd, p, s1o2, r_01, r_23, v_sec_lut, v_112);
            acc_sec_pair_4r(&sum_secd, p, s2o2, r_01, r_23, v_sec_lut, v_112);
          } else {
            sum_secd = sec_diag26_lut_4r(p, r_01, r_23, v_sec_lut, v_112);
          }
          finish_t0_split_4r_i8(r_01, r_23, rr, ref_stride, sum_sec0, sum_secd,
                                &v_sse0, &v_sse1, &v_sse2, &v_sse3);
        }
      }
      continue;
    }

    const int pri_damp = AOMMAX(0, damping - cdef_msb_nz((unsigned int)t));
    assert(pri_damp >= 0 && pri_damp < 5);
    const uint8_t *const pri_shmask = k_shmask_u8_avx2[pri_damp];
    const __m256i v_pt = _mm256_set1_epi8((char)t);
    const __m128i v_ps = _mm_cvtsi32_si128(pri_damp);
    const int is_odd = t & 1;

    if ((d & 1) != 0) {
      const intptr_t po2 = cdef_directions_padded_inline[d + 2][1];
      const intptr_t s1o2 = cdef_directions_padded_inline[d + 4][1];
      const intptr_t s2o2 = cdef_directions_padded_inline[d][1];
      for (int i = 0; i < 8;
           i += 4, p += 4 * CDEF_BSTRIDE, rr += 4 * ref_stride) {
        const __m256i r_01 = load_u16_8x2(p);
        const __m256i r_23 = load_u16_8x2(p + 2 * CDEF_BSTRIDE);
        __m256i max_u8 = _mm256_packus_epi16(r_01, r_23);
        __m256i min_u8 = max_u8;
        __m256i sn;
        const __m256i pn =
            odd_near_hoist_mm_4r(&sn, p, d, r_01, r_23, v_sec_lut, v_112, v_pt,
                                 v_ps, pri_shmask, &max_u8, &min_u8);
        const __m256i pf = pair_hoist_mm_4r(p, po2, r_01, r_23, v_pt, v_ps,
                                            pri_shmask, &max_u8, &min_u8);
        const __m256i sum_pri = combine_pri_even_odd_i8(pn, pf, is_odd);
        __m256i sum_sec0 = _mm256_add_epi8(sn, sn);
        __m256i sum_sec3 = sum_sec0;
        acc_sec_pair_mm_4r(&sum_sec3, p, s1o2, r_01, r_23, v_sec_lut, v_112,
                           &max_u8, &min_u8);
        acc_sec_pair_mm_4r(&sum_sec3, p, s2o2, r_01, r_23, v_sec_lut, v_112,
                           &max_u8, &min_u8);
        acc_sec_pair_4r(&sum_sec0, p, 2, r_01, r_23, v_sec_lut, v_112);
        acc_sec_vert2_4r(&sum_sec0, p, r_01, r_23, v_sec_lut, v_112);
        finish_split_4r_i8(r_01, r_23, rr, ref_stride, sum_sec0, sum_pri,
                           sum_sec3, max_u8, min_u8, &v_sse0, &v_sse1, &v_sse2,
                           &v_sse3);
      }
    } else if ((d & 3) == 0) {
      const intptr_t po1 = cdef_directions_padded_inline[d + 2][0];
      const intptr_t po2 = 2 * po1;
      for (int i = 0; i < 8;
           i += 4, p += 4 * CDEF_BSTRIDE, rr += 4 * ref_stride) {
        const __m256i r_01 = load_u16_8x2(p);
        const __m256i r_23 = load_u16_8x2(p + 2 * CDEF_BSTRIDE);
        __m256i max_u8 = _mm256_packus_epi16(r_01, r_23);
        __m256i min_u8 = max_u8;
        __m256i sum_sec = init_sec_vert1_mm_4r(p, r_01, r_23, v_sec_lut, v_112,
                                               &max_u8, &min_u8);
        acc_sec_pair_mm_4r(&sum_sec, p, 1, r_01, r_23, v_sec_lut, v_112,
                           &max_u8, &min_u8);
        sum_sec = _mm256_add_epi8(sum_sec, sum_sec);
        acc_sec_pair_mm_4r(&sum_sec, p, 2, r_01, r_23, v_sec_lut, v_112,
                           &max_u8, &min_u8);
        acc_sec_vert2_mm_4r(&sum_sec, p, r_01, r_23, v_sec_lut, v_112, &max_u8,
                            &min_u8);

        const __m256i pn = pair_hoist_mm_4r(p, po1, r_01, r_23, v_pt, v_ps,
                                            pri_shmask, &max_u8, &min_u8);
        const __m256i pf = pair_hoist_mm_4r(p, po2, r_01, r_23, v_pt, v_ps,
                                            pri_shmask, &max_u8, &min_u8);
        const __m256i sum_pri = combine_pri_even_odd_i8(pn, pf, is_odd);
        finish_all_same_4r_i8(r_01, r_23, rr, ref_stride, sum_sec, sum_pri,
                              max_u8, min_u8, &v_sse0, &v_sse1, &v_sse2,
                              &v_sse3);
      }
    } else {
      for (int i = 0; i < 8;
           i += 4, p += 4 * CDEF_BSTRIDE, rr += 4 * ref_stride) {
        const __m256i r_01 = load_u16_8x2(p);
        const __m256i r_23 = load_u16_8x2(p + 2 * CDEF_BSTRIDE);
        __m256i max_u8 = _mm256_packus_epi16(r_01, r_23);
        __m256i min_u8 = max_u8;
        __m256i sum_sec0, sum_pri;
        if (d & 4) {
          const __m256i pn = pri_sec_hoist_vert1_init_mm_4r(
              &sum_sec0, p, r_01, r_23, v_sec_lut, v_112, v_pt, v_ps,
              pri_shmask, &max_u8, &min_u8);
          acc_sec_pair_4r(&sum_sec0, p, 1, r_01, r_23, v_sec_lut, v_112);
          sum_sec0 = _mm256_add_epi8(sum_sec0, sum_sec0);
          const __m256i pf = pri_sec_hoist_vert2_acc_mm_4r(
              &sum_sec0, p, r_01, r_23, v_sec_lut, v_112, v_pt, v_ps,
              pri_shmask, &max_u8, &min_u8);
          sum_pri = combine_pri_even_odd_i8(pn, pf, is_odd);
          acc_sec_pair_4r(&sum_sec0, p, 2, r_01, r_23, v_sec_lut, v_112);
        } else {
          const __m256i pn = pri_sec_hoist_init_mm_4r(
              &sum_sec0, p, 1, r_01, r_23, v_sec_lut, v_112, v_pt, v_ps,
              pri_shmask, &max_u8, &min_u8);
          acc_sec_vert1_4r(&sum_sec0, p, r_01, r_23, v_sec_lut, v_112);
          sum_sec0 = _mm256_add_epi8(sum_sec0, sum_sec0);
          const __m256i pf = pri_sec_hoist_acc_mm_4r(
              &sum_sec0, p, 2, r_01, r_23, v_sec_lut, v_112, v_pt, v_ps,
              pri_shmask, &max_u8, &min_u8);
          sum_pri = combine_pri_even_odd_i8(pn, pf, is_odd);
          acc_sec_vert2_4r(&sum_sec0, p, r_01, r_23, v_sec_lut, v_112);
        }
        const __m256i sum_sec3 = sec_diag26_lut_mm_4r(p, r_01, r_23, v_sec_lut,
                                                      v_112, &max_u8, &min_u8);
        finish_split_4r_i8(r_01, r_23, rr, ref_stride, sum_sec0, sum_pri,
                           sum_sec3, max_u8, min_u8, &v_sse0, &v_sse1, &v_sse2,
                           &v_sse3);
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
    } else if (damping <= 4) {
      cdef_mse_calc_block_lvl4_chroma_8x8_fast_avx2(
          in, ref_base, ref_stride, dlist, cdef_count, dir,
          AOMMAX(0, damping - 2), pli, mse);
    } else {
      cdef_mse_calc_block_lvl4_8x8_cold_avx2(in, ref_base, ref_stride, dlist,
                                             cdef_count, dir, var, damping, pli,
                                             mse);
    }
    return;
  }
  assert(mi_wide_l2 == 2);
  cdef_find_dir_list_avx2(in, dlist, cdef_count, dir, var);
  if (damping <= 4) {
    cdef_mse_calc_block_lvl4_luma_core_avx2(in, ref_base, ref_stride, dlist,
                                            cdef_count, dir, var, damping, mse);
  } else {
    cdef_mse_calc_block_lvl4_8x8_cold_avx2(
        in, ref_base, ref_stride, dlist, cdef_count, dir, var, damping, 0, mse);
  }
}
#endif  // CONFIG_AV1_ENCODER
