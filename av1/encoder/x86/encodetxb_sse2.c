/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <emmintrin.h>  // SSE2
#include <string.h>

#include "aom/aom_integer.h"
#include "aom_dsp/x86/mem_sse2.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/txb_common.h"

static inline uint16_t loadu_u16_sse2(const void *src) {
  uint16_t v;
  memcpy(&v, src, sizeof(v));
  return v;
}

static inline __m128i finish_nz_ctx_sse2(const __m128i sum,
                                         const __m128i const_4,
                                         const __m128i zero,
                                         const __m128i pos_to_offset) {
  __m128i count = _mm_avg_epu8(sum, zero);
  count = _mm_min_epu8(count, const_4);
  return _mm_add_epi8(count, pos_to_offset);
}

static inline void get_4x4_2d_sse2(const uint8_t *const levels,
                                   int8_t *const coeff_contexts) {
  const __m128i const_3 = _mm_set1_epi8(3);
  const __m128i const_4 = _mm_set1_epi8(4);
  const __m128i zero = _mm_setzero_si128();
  const __m128i pos_to_offset =
      _mm_setr_epi8(0, 1, 6, 6, 1, 6, 6, 21, 6, 6, 21, 21, 6, 21, 21, 21);

  const __m128i c01 =
      _mm_min_epu8(_mm_loadu_si128((const __m128i *)(levels + 0)), const_3);
  const __m128i c23 =
      _mm_min_epu8(_mm_loadu_si128((const __m128i *)(levels + 16)), const_3);

  const __m128i cur = _mm_castps_si128(
      _mm_shuffle_ps(_mm_castsi128_ps(c01), _mm_castsi128_ps(c23), 0x88));

  const __m128i pair = _mm_add_epi8(cur, _mm_srli_epi32(cur, 8));
  const __m128i pair_down1 = _mm_srli_epi32(pair, 8);

  __m128i sum = _mm_add_epi8(pair_down1, _mm_srli_si128(pair, 4));
  sum = _mm_add_epi8(sum, _mm_srli_si128(cur, 8));

  _mm_store_si128((__m128i *)coeff_contexts,
                  finish_nz_ctx_sse2(sum, const_4, zero, pos_to_offset));
  coeff_contexts[0] = 0;
}

static inline void get_8x8_2d_sse2(const uint8_t *const levels,
                                   const int last_idx,
                                   int8_t *const coeff_contexts) {
  const int stride = 8 + TX_PAD_HOR;
  const __m128i const_3 = _mm_set1_epi8(3);
  const __m128i const_4 = _mm_set1_epi8(4);
  const __m128i zero = _mm_setzero_si128();
  const __m128i pos_to_offset_0 =
      _mm_setr_epi8(0, 1, 6, 6, 21, 21, 21, 21, 1, 6, 6, 21, 21, 21, 21, 21);

  const __m128i c01 =
      _mm_min_epu8(load_8bit_8x2_to_1_reg_sse2(levels, stride), const_3);
  const __m128i c23 = _mm_min_epu8(
      load_8bit_8x2_to_1_reg_sse2(levels + 2 * stride, stride), const_3);

  const __m128i p01 = _mm_add_epi8(c01, _mm_srli_epi64(c01, 8));
  const __m128i p23 = _mm_add_epi8(c23, _mm_srli_epi64(c23, 8));
  const __m128i p12 = _mm_castps_si128(
      _mm_shuffle_ps(_mm_castsi128_ps(p01), _mm_castsi128_ps(p23), 0x4e));

  __m128i sum = _mm_add_epi8(_mm_srli_epi64(p01, 8), p12);
  sum = _mm_add_epi8(sum, c23);
  _mm_store_si128((__m128i *)coeff_contexts,
                  finish_nz_ctx_sse2(sum, const_4, zero, pos_to_offset_0));

  if (last_idx > 3) {
    const __m128i pos_to_offset_1 = _mm_setr_epi8(
        6, 6, 21, 21, 21, 21, 21, 21, 6, 21, 21, 21, 21, 21, 21, 21);
    const __m128i c45 = _mm_min_epu8(
        load_8bit_8x2_to_1_reg_sse2(levels + 4 * stride, stride), const_3);
    const __m128i p45 = _mm_add_epi8(c45, _mm_srli_epi64(c45, 8));
    const __m128i p34 = _mm_castps_si128(
        _mm_shuffle_ps(_mm_castsi128_ps(p23), _mm_castsi128_ps(p45), 0x4e));

    sum = _mm_add_epi8(_mm_srli_epi64(p23, 8), p34);
    sum = _mm_add_epi8(sum, c45);
    _mm_store_si128((__m128i *)(coeff_contexts + 16),
                    finish_nz_ctx_sse2(sum, const_4, zero, pos_to_offset_1));

    if (last_idx > 10) {
      const __m128i pos_to_offset_2 = _mm_set1_epi8(21);
      const __m128i c67 = _mm_min_epu8(
          load_8bit_8x2_to_1_reg_sse2(levels + 6 * stride, stride), const_3);
      const __m128i p67 = _mm_add_epi8(c67, _mm_srli_epi64(c67, 8));
      const __m128i p56 = _mm_castps_si128(
          _mm_shuffle_ps(_mm_castsi128_ps(p45), _mm_castsi128_ps(p67), 0x4e));

      sum = _mm_add_epi8(_mm_srli_epi64(p45, 8), p56);
      sum = _mm_add_epi8(sum, c67);
      _mm_store_si128((__m128i *)(coeff_contexts + 32),
                      finish_nz_ctx_sse2(sum, const_4, zero, pos_to_offset_2));

      if (last_idx > 21) {
        sum = _mm_add_epi8(_mm_srli_epi64(p67, 8), _mm_srli_si128(p67, 8));
        _mm_store_si128(
            (__m128i *)(coeff_contexts + 48),
            finish_nz_ctx_sse2(sum, const_4, zero, pos_to_offset_2));
      }
    }
  }

  coeff_contexts[0] = 0;
}

static const uint8_t kLastIdxToCols16n[37] = { 1, 1, 2, 2, 3, 3, 3, 4, 4, 4,
                                               4, 5, 5, 5, 5, 5, 6, 6, 6, 6,
                                               6, 6, 7, 7, 7, 7, 7, 7, 7, 8,
                                               8, 8, 8, 8, 8, 8, 8 };

static inline int get_cols_16n_2d(const int width, const int last_idx) {
  int col;
  if (last_idx <= 36) {
    col = kLastIdxToCols16n[last_idx];
  } else if (last_idx <= 120) {
    col = 9 + (((last_idx - 25) * 21) >> 8);
  } else if (last_idx <= 275) {
    col = 16 + ((last_idx - 120) >> 4);
  } else {
    col = width;
  }
  return AOMMIN(width, col);
}

static inline void get_16x16_2d_sse2(const uint8_t *levels, const int last_idx,
                                     int8_t *coeff_contexts) {
  const int stride = 16 + TX_PAD_HOR;
  int col;
  if (last_idx <= 36) {
    col = kLastIdxToCols16n[last_idx];
  } else if (last_idx <= 120) {
    if (last_idx <= 66) {
      col = (last_idx <= 45) ? 9 : 11;
    } else {
      col = (last_idx <= 91) ? 13 : ((last_idx <= 105) ? 14 : 15);
    }
  } else {
    col = 16;
  }

  const __m128i const_3 = _mm_set1_epi8(3);
  const __m128i const_4 = _mm_set1_epi8(4);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top_21 = _mm_slli_si128(_mm_cvtsi32_si128(21), 15);
  __m128i pos =
      _mm_setr_epi8(0, 1, 6, 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21);

  const __m128i c0 =
      _mm_min_epu8(_mm_loadu_si128((const __m128i *)(levels + 0)), const_3);
  const __m128i c1 = _mm_min_epu8(
      _mm_loadu_si128((const __m128i *)(levels + stride)), const_3);
  __m128i p0 = _mm_add_epi8(c0, _mm_srli_si128(c0, 1));
  __m128i p1 = _mm_add_epi8(c1, _mm_srli_si128(c1, 1));

  int8_t *cc = coeff_contexts;
  const uint8_t *lv = levels + 2 * stride;
  do {
    const __m128i c2 =
        _mm_min_epu8(_mm_loadu_si128((const __m128i *)lv), const_3);
    const __m128i p2 = _mm_add_epi8(c2, _mm_srli_si128(c2, 1));
    __m128i sum = _mm_add_epi8(_mm_srli_si128(p0, 1), p1);
    sum = _mm_add_epi8(sum, c2);
    _mm_store_si128((__m128i *)cc, finish_nz_ctx_sse2(sum, const_4, zero, pos));
    pos = _mm_or_si128(_mm_srli_si128(pos, 1), top_21);
    p0 = p1;
    p1 = p2;
    lv += stride;
    cc += 16;
  } while (--col);

  coeff_contexts[0] = 0;
}

static inline void get_4_nz_map_contexts_2d(const uint8_t *levels,
                                            const int width, const int last_idx,
                                            int8_t *const coeff_contexts) {
  const int stride = 4 + TX_PAD_HOR;
  const __m128i const_3 = _mm_set1_epi8(3);
  const __m128i const_4 = _mm_set1_epi8(4);
  const __m128i zero = _mm_setzero_si128();
  const __m128i pos_large = _mm_set1_epi8(21);
  __m128i pos_to_offset =
      _mm_setr_epi8(0, 16, 16, 16, 16, 16, 16, 16, 6, 6, 21, 21, 6, 21, 21, 21);

  int col = AOMMIN(width, (((last_idx + 2) >> 4) + 1) << 2);

  const __m128i c01 =
      _mm_min_epu8(_mm_loadu_si128((const __m128i *)(levels + 0)), const_3);
  const __m128i c23 =
      _mm_min_epu8(_mm_loadu_si128((const __m128i *)(levels + 16)), const_3);
  __m128i cur = _mm_castps_si128(
      _mm_shuffle_ps(_mm_castsi128_ps(c01), _mm_castsi128_ps(c23), 0x88));
  __m128i cur_p = _mm_add_epi8(cur, _mm_srli_epi32(cur, 8));
  __m128i cur_ps1 = _mm_srli_epi32(cur_p, 8);

  int8_t *cc = coeff_contexts;
  const uint8_t *lv = levels + 4 * stride;
  do {
    const __m128i n01 =
        _mm_min_epu8(_mm_loadu_si128((const __m128i *)(lv + 0)), const_3);
    const __m128i n23 =
        _mm_min_epu8(_mm_loadu_si128((const __m128i *)(lv + 16)), const_3);
    const __m128i nxt_c = _mm_castps_si128(
        _mm_shuffle_ps(_mm_castsi128_ps(n01), _mm_castsi128_ps(n23), 0x88));
    const __m128i nxt_p = _mm_add_epi8(nxt_c, _mm_srli_epi32(nxt_c, 8));
    const __m128i nxt_ps1 = _mm_srli_epi32(nxt_p, 8);

    const __m128i nxt1_p =
        _mm_or_si128(_mm_srli_si128(cur_p, 4), _mm_slli_si128(nxt_p, 12));
    const __m128i nxt2_c = _mm_castps_si128(
        _mm_shuffle_ps(_mm_castsi128_ps(cur), _mm_castsi128_ps(nxt_c), 0x4e));

    __m128i sum = _mm_add_epi8(cur_ps1, nxt1_p);
    sum = _mm_add_epi8(sum, nxt2_c);
    _mm_store_si128((__m128i *)cc,
                    finish_nz_ctx_sse2(sum, const_4, zero, pos_to_offset));

    pos_to_offset = pos_large;
    cur = nxt_c;
    cur_p = nxt_p;
    cur_ps1 = nxt_ps1;
    lv += 4 * stride;
    cc += 16;
    col -= 4;
  } while (col);

  coeff_contexts[0] = 0;
}

static inline void get_8_coeff_contexts_2d(const uint8_t *levels,
                                           const int width, const int last_idx,
                                           int8_t *const coeff_contexts) {
  const int stride = 8 + TX_PAD_HOR;
  const __m128i const_3 = _mm_set1_epi8(3);
  const __m128i const_4 = _mm_set1_epi8(4);
  const __m128i zero = _mm_setzero_si128();

  int col;
  if (last_idx <= 10) {
    col = (last_idx <= 3) ? 2 : 4;
  } else if (last_idx <= 43) {
    col = AOMMIN(width, (last_idx <= 21) ? 6 : 8);
  } else {
    col = AOMMIN(width, 10 + (((last_idx - 44) >> 4) << 1));
  }

  __m128i pos0, pos1;
  const __m128i pos2 = _mm_set1_epi8(21);
  if (width < 8) {
    pos0 = _mm_setr_epi8(0, 11, 6, 6, 21, 21, 21, 21, 11, 11, 6, 21, 21, 21, 21,
                         21);
    pos1 = _mm_setr_epi8(11, 11, 21, 21, 21, 21, 21, 21, 11, 11, 21, 21, 21, 21,
                         21, 21);
  } else {
    pos0 = _mm_set1_epi8(16);
    pos1 = _mm_setr_epi8(6, 6, 21, 21, 21, 21, 21, 21, 6, 21, 21, 21, 21, 21,
                         21, 21);
  }

  const __m128i c01 =
      _mm_min_epu8(load_8bit_8x2_to_1_reg_sse2(levels, stride), const_3);
  __m128i p01 = _mm_add_epi8(c01, _mm_srli_epi64(c01, 8));

  int8_t *cc = coeff_contexts;
  const uint8_t *lv = levels + 2 * stride;
  do {
    const __m128i c23 =
        _mm_min_epu8(load_8bit_8x2_to_1_reg_sse2(lv, stride), const_3);
    const __m128i p23 = _mm_add_epi8(c23, _mm_srli_epi64(c23, 8));
    const __m128i p12 = _mm_castps_si128(
        _mm_shuffle_ps(_mm_castsi128_ps(p01), _mm_castsi128_ps(p23), 0x4e));

    __m128i sum = _mm_add_epi8(_mm_srli_epi64(p01, 8), p12);
    sum = _mm_add_epi8(sum, c23);
    _mm_store_si128((__m128i *)cc,
                    finish_nz_ctx_sse2(sum, const_4, zero, pos0));

    pos0 = pos1;
    pos1 = pos2;
    p01 = p23;
    lv += 2 * stride;
    cc += 16;
    col -= 2;
  } while (col);

  coeff_contexts[0] = 0;
}

static void run_2d_band16_sse2(const uint8_t *lv_band, const int stride,
                               int col, const int cc_stride, const int mode,
                               int8_t *cc_band) {
  const __m128i const_3 = _mm_set1_epi8(3);
  const __m128i const_4 = _mm_set1_epi8(4);
  const __m128i zero = _mm_setzero_si128();
  const __m128i v21 = _mm_set1_epi8(21);
  __m128i pos0 = v21, pos1 = v21, pos2 = v21, pos3 = v21, pos_tail = v21;

  if (mode == 2 || mode == 3) {
    const __m128i p0_t = _mm_setr_epi8(0, 1, 6, 6, 21, 21, 21, 21, 21, 21, 21,
                                       21, 21, 21, 21, 21);
    const __m128i top_21 = _mm_slli_si128(_mm_cvtsi32_si128(21), 15);
    const __m128i p1_t = _mm_or_si128(_mm_srli_si128(p0_t, 1), top_21);
    pos2 = _mm_or_si128(_mm_srli_si128(p1_t, 1), top_21);
    pos3 = _mm_or_si128(_mm_srli_si128(pos2, 1), top_21);
    if (mode == 3) {
      pos0 = p0_t;
      pos1 = p1_t;
    } else {
      pos0 = pos1 = _mm_set1_epi8(16);
    }
  } else if (mode == 4) {
    pos0 = _mm_setr_epi8(0, 11, 6, 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
                         21, 21);
    pos1 = _mm_setr_epi8(11, 11, 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
                         21, 21);
    pos2 = pos3 = pos_tail = _mm_setr_epi8(11, 11, 21, 21, 21, 21, 21, 21, 21,
                                           21, 21, 21, 21, 21, 21, 21);
  } else if (mode == 1) {
    pos0 = pos1 = _mm_set1_epi8(16);
  }

  const __m128i cl0 =
      _mm_min_epu8(_mm_loadu_si128((const __m128i *)(lv_band + 0)), const_3);
  const __m128i t0 = _mm_min_epu8(
      _mm_cvtsi32_si128((int)loadu_u16_sse2(lv_band + 16)), const_3);
  const __m128i p0 = _mm_add_epi8(
      cl0, _mm_or_si128(_mm_srli_si128(cl0, 1), _mm_slli_si128(t0, 15)));
  const __m128i pt0 = _mm_add_epi8(t0, _mm_srli_si128(t0, 1));
  __m128i p0_s1 = _mm_or_si128(_mm_srli_si128(p0, 1), _mm_slli_si128(pt0, 15));

  const __m128i cl1 = _mm_min_epu8(
      _mm_loadu_si128((const __m128i *)(lv_band + stride)), const_3);
  const __m128i t1 = _mm_min_epu8(
      _mm_cvtsi32_si128((int)loadu_u16_sse2(lv_band + stride + 16)), const_3);
  __m128i p1 = _mm_add_epi8(
      cl1, _mm_or_si128(_mm_srli_si128(cl1, 1), _mm_slli_si128(t1, 15)));
  const __m128i pt1 = _mm_add_epi8(t1, _mm_srli_si128(t1, 1));
  __m128i p1_s1 = _mm_or_si128(_mm_srli_si128(p1, 1), _mm_slli_si128(pt1, 15));

  const uint8_t *lv = lv_band + 2 * stride;
  do {
    const __m128i cl2 =
        _mm_min_epu8(_mm_loadu_si128((const __m128i *)lv), const_3);
    const __m128i t2 =
        _mm_min_epu8(_mm_cvtsi32_si128((int)loadu_u16_sse2(lv + 16)), const_3);
    const __m128i p2 = _mm_add_epi8(
        cl2, _mm_or_si128(_mm_srli_si128(cl2, 1), _mm_slli_si128(t2, 15)));
    const __m128i pt2 = _mm_add_epi8(t2, _mm_srli_si128(t2, 1));
    const __m128i p2_s1 =
        _mm_or_si128(_mm_srli_si128(p2, 1), _mm_slli_si128(pt2, 15));

    __m128i sum = _mm_add_epi8(p0_s1, p1);
    sum = _mm_add_epi8(sum, cl2);
    _mm_store_si128((__m128i *)cc_band,
                    finish_nz_ctx_sse2(sum, const_4, zero, pos0));
    pos0 = pos1;
    pos1 = pos2;
    pos2 = pos3;
    pos3 = pos_tail;
    p0_s1 = p1_s1;
    p1 = p2;
    p1_s1 = p2_s1;
    lv += stride;
    cc_band += cc_stride;
  } while (--col);
}

static inline void get_16n_all_coeff_contexts_2d(
    const uint8_t *levels, const int real_width, const int real_height,
    const int width, const int height, const int last_idx,
    int8_t *const coeff_contexts) {
  const int stride = height + TX_PAD_HOR;
  const int col = get_cols_16n_2d(width, last_idx);
  const int mode =
      (real_width == real_height) ? 3 : (real_width < real_height ? 4 : 2);
  run_2d_band16_sse2(levels, stride, col, height, mode, coeff_contexts);
  if (height == 32 && last_idx > 105) {
    run_2d_band16_sse2(levels + 16, stride, col, 32,
                       (real_width > real_height) ? 1 : 0, coeff_contexts + 16);
  }
  coeff_contexts[0] = 0;
}

static inline void get_4_nz_map_contexts_hv(const uint8_t *levels,
                                            const int width, const int last_idx,
                                            const int is_hor,
                                            int8_t *coeff_contexts) {
  const int stride = 4 + TX_PAD_HOR;
  const __m128i const_3 = _mm_set1_epi8(3);
  const __m128i const_4 = _mm_set1_epi8(4);
  const __m128i zero = _mm_setzero_si128();
  __m128i pos_to_offset =
      is_hor
          ? _mm_setr_epi8(SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 0,
                          SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 0,
                          SIG_COEF_CONTEXTS_2D + 5, SIG_COEF_CONTEXTS_2D + 5,
                          SIG_COEF_CONTEXTS_2D + 5, SIG_COEF_CONTEXTS_2D + 5,
                          SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                          SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                          SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                          SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10)
          : _mm_setr_epi8(SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 5,
                          SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                          SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 5,
                          SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                          SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 5,
                          SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                          SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 5,
                          SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10);
  const __m128i pos_large =
      is_hor ? _mm_set1_epi8(SIG_COEF_CONTEXTS_2D + 10) : pos_to_offset;
  int col = is_hor ? AOMMIN(width, ((last_idx + 15) >> 4) << 2) : width;

  const __m128i c01 =
      _mm_min_epu8(_mm_loadu_si128((const __m128i *)(levels + 0)), const_3);
  const __m128i c23 =
      _mm_min_epu8(_mm_loadu_si128((const __m128i *)(levels + 16)), const_3);
  __m128i cur = _mm_castps_si128(
      _mm_shuffle_ps(_mm_castsi128_ps(c01), _mm_castsi128_ps(c23), 0x88));

  levels += 4 * stride;
  do {
    const __m128i n01 =
        _mm_min_epu8(_mm_loadu_si128((const __m128i *)(levels + 0)), const_3);
    const __m128i n23 =
        _mm_min_epu8(_mm_loadu_si128((const __m128i *)(levels + 16)), const_3);
    const __m128i nxt = _mm_castps_si128(
        _mm_shuffle_ps(_mm_castsi128_ps(n01), _mm_castsi128_ps(n23), 0x88));
    const __m128i sh1 =
        _mm_or_si128(_mm_srli_si128(cur, 4), _mm_slli_si128(nxt, 12));
    __m128i cur_acc;
    __m128i sum = sh1;
    if (is_hor) {
      cur_acc = _mm_srli_epi32(cur, 8);
      const __m128i sh2 = _mm_castps_si128(
          _mm_shuffle_ps(_mm_castsi128_ps(cur), _mm_castsi128_ps(nxt), 0x4e));
      const __m128i sh3 =
          _mm_or_si128(_mm_srli_si128(cur, 12), _mm_slli_si128(nxt, 4));
      sum = _mm_add_epi8(sum, sh2);
      sum = _mm_add_epi8(sum, sh3);
      sum = _mm_add_epi8(sum, nxt);
    } else {
      const __m128i p = _mm_add_epi8(cur, _mm_srli_epi32(cur, 8));
      const __m128i q = _mm_add_epi8(p, _mm_srli_epi32(p, 16));
      cur_acc = _mm_srli_epi32(q, 8);
    }
    sum = _mm_add_epi8(sum, cur_acc);
    _mm_store_si128((__m128i *)coeff_contexts,
                    finish_nz_ctx_sse2(sum, const_4, zero, pos_to_offset));
    pos_to_offset = pos_large;
    cur = nxt;
    levels += 4 * stride;
    coeff_contexts += 16;
    col -= 4;
  } while (col);
}

static inline void get_8_coeff_contexts_hv(const uint8_t *levels,
                                           const int width, const int last_idx,
                                           const int is_hor,
                                           int8_t *coeff_contexts) {
  const int stride = 8 + TX_PAD_HOR;
  const __m128i const_3 = _mm_set1_epi8(3);
  const __m128i const_4 = _mm_set1_epi8(4);
  const __m128i zero = _mm_setzero_si128();
  __m128i pos_to_offset =
      is_hor
          ? _mm_setr_epi8(SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 0,
                          SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 0,
                          SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 0,
                          SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 0,
                          SIG_COEF_CONTEXTS_2D + 5, SIG_COEF_CONTEXTS_2D + 5,
                          SIG_COEF_CONTEXTS_2D + 5, SIG_COEF_CONTEXTS_2D + 5,
                          SIG_COEF_CONTEXTS_2D + 5, SIG_COEF_CONTEXTS_2D + 5,
                          SIG_COEF_CONTEXTS_2D + 5, SIG_COEF_CONTEXTS_2D + 5)
          : _mm_setr_epi8(SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 5,
                          SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                          SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                          SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                          SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 5,
                          SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                          SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                          SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10);
  const __m128i pos_large =
      is_hor ? _mm_set1_epi8(SIG_COEF_CONTEXTS_2D + 10) : pos_to_offset;
  int col = is_hor ? AOMMIN(width, ((last_idx + 15) >> 4) << 1) : width;

  __m128i c01 =
      _mm_min_epu8(load_8bit_8x2_to_1_reg_sse2(levels, stride), const_3);

  if (is_hor) {
    __m128i c23 = _mm_min_epu8(
        load_8bit_8x2_to_1_reg_sse2(levels + 2 * stride, stride), const_3);
    __m128i s01 = _mm_add_epi8(c01, c23);
    levels += 4 * stride;
    do {
      const __m128i c45 =
          _mm_min_epu8(load_8bit_8x2_to_1_reg_sse2(levels, stride), const_3);
      const __m128i s23 = _mm_add_epi8(c23, c45);
      const __m128i mid = _mm_castps_si128(
          _mm_shuffle_ps(_mm_castsi128_ps(s01), _mm_castsi128_ps(s23), 0x4e));
      __m128i sum = _mm_add_epi8(_mm_srli_epi64(c01, 8), mid);
      sum = _mm_add_epi8(sum, s23);
      _mm_store_si128((__m128i *)coeff_contexts,
                      finish_nz_ctx_sse2(sum, const_4, zero, pos_to_offset));
      pos_to_offset = pos_large;
      c01 = c23;
      c23 = c45;
      s01 = s23;
      levels += 2 * stride;
      coeff_contexts += 16;
      col -= 2;
    } while (col);
  } else {
    levels += 2 * stride;
    do {
      const __m128i c23 =
          _mm_min_epu8(load_8bit_8x2_to_1_reg_sse2(levels, stride), const_3);
      const __m128i c12 = _mm_castps_si128(
          _mm_shuffle_ps(_mm_castsi128_ps(c01), _mm_castsi128_ps(c23), 0x4e));
      const __m128i p = _mm_add_epi8(c01, _mm_srli_epi64(c01, 8));
      const __m128i q = _mm_add_epi8(p, _mm_srli_epi64(p, 16));
      const __m128i sum = _mm_add_epi8(_mm_srli_epi64(q, 8), c12);
      _mm_store_si128((__m128i *)coeff_contexts,
                      finish_nz_ctx_sse2(sum, const_4, zero, pos_to_offset));
      c01 = c23;
      levels += 2 * stride;
      coeff_contexts += 16;
      col -= 2;
    } while (col);
  }
}

static void get_32h_coeff_contexts_ver(const uint8_t *levels, const int stride,
                                       int col, const int h_rows,
                                       int8_t *coeff_contexts) {
  const __m128i const_3 = _mm_set1_epi8(3);
  const __m128i const_4 = _mm_set1_epi8(4);
  const __m128i zero = _mm_setzero_si128();
  const __m128i pos_lo =
      _mm_setr_epi8(SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 5,
                    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10);
  const __m128i pos_hi = _mm_set1_epi8(SIG_COEF_CONTEXTS_2D + 10);
  do {
    int h = h_rows;
    __m128i pos = pos_lo;
    const uint8_t *lv = levels;
    int8_t *cc = coeff_contexts;
    do {
      const __m128i m1 =
          _mm_min_epu8(_mm_loadu_si128((const __m128i *)(lv + 1)), const_3);
      const __m128i m2 =
          _mm_min_epu8(_mm_loadu_si128((const __m128i *)(lv + 2)), const_3);
      const __m128i m3 =
          _mm_min_epu8(_mm_loadu_si128((const __m128i *)(lv + 3)), const_3);
      const __m128i m4 =
          _mm_min_epu8(_mm_loadu_si128((const __m128i *)(lv + 4)), const_3);
      const __m128i nxt = _mm_min_epu8(
          _mm_loadu_si128((const __m128i *)(lv + stride)), const_3);
      const __m128i sum = _mm_add_epi8(
          _mm_add_epi8(_mm_add_epi8(m1, m2), _mm_add_epi8(m3, m4)), nxt);
      _mm_store_si128((__m128i *)cc,
                      finish_nz_ctx_sse2(sum, const_4, zero, pos));
      pos = pos_hi;
      lv += 16;
      cc += 16;
      h -= 16;
    } while (h);
    levels += stride;
    coeff_contexts += 32;
  } while (--col);
}

static inline void get_16n_coeff_contexts_ver(const uint8_t *levels,
                                              const int width, const int height,
                                              const int last_idx,
                                              int8_t *coeff_contexts) {
  const int stride = height + TX_PAD_HOR;
  int col = AOMMIN(width, last_idx);
  if (height == 32) {
    const int h_rows = (last_idx > (width << 4)) ? 32 : 16;
    get_32h_coeff_contexts_ver(levels, stride, col, h_rows, coeff_contexts);
    return;
  }

  const __m128i const_3 = _mm_set1_epi8(3);
  const __m128i const_4 = _mm_set1_epi8(4);
  const __m128i zero = _mm_setzero_si128();
  const __m128i pos_lo =
      _mm_setr_epi8(SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 5,
                    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
                    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10);
  do {
    const __m128i cur =
        _mm_min_epu8(_mm_loadu_si128((const __m128i *)levels), const_3);
    const __m128i nxt = _mm_min_epu8(
        _mm_loadu_si128((const __m128i *)(levels + stride)), const_3);
    const __m128i p = _mm_add_epi8(cur, _mm_srli_si128(cur, 1));
    const __m128i q = _mm_add_epi8(p, _mm_srli_si128(p, 2));
    const __m128i sum = _mm_add_epi8(_mm_srli_si128(q, 1), nxt);
    _mm_store_si128((__m128i *)coeff_contexts,
                    finish_nz_ctx_sse2(sum, const_4, zero, pos_lo));
    levels += stride;
    coeff_contexts += 16;
  } while (--col);
}

static void get_16n_hor_band_sse2(const uint8_t *lv_b, const int stride,
                                  int col, const int height, int8_t *cc) {
  const __m128i const_3 = _mm_set1_epi8(3);
  const __m128i const_4 = _mm_set1_epi8(4);
  const __m128i zero = _mm_setzero_si128();
  const __m128i pos2 = _mm_set1_epi8(SIG_COEF_CONTEXTS_2D + 10);
  __m128i pos0 = _mm_set1_epi8(SIG_COEF_CONTEXTS_2D + 0);
  __m128i pos1 = _mm_set1_epi8(SIG_COEF_CONTEXTS_2D + 5);
  __m128i c1 =
      _mm_min_epu8(_mm_loadu_si128((const __m128i *)(lv_b + stride)), const_3);
  __m128i c2 = _mm_min_epu8(
      _mm_loadu_si128((const __m128i *)(lv_b + 2 * stride)), const_3);
  __m128i c3 = _mm_min_epu8(
      _mm_loadu_si128((const __m128i *)(lv_b + 3 * stride)), const_3);
  __m128i p12 = _mm_add_epi8(c1, c2);
  __m128i p23 = _mm_add_epi8(c2, c3);
  const uint8_t *lv0 = lv_b + 1;
  const uint8_t *lv4 = lv_b + 4 * stride;
  do {
    const __m128i c4 =
        _mm_min_epu8(_mm_loadu_si128((const __m128i *)lv4), const_3);
    const __m128i p34 = _mm_add_epi8(c3, c4);
    const __m128i l0 =
        _mm_min_epu8(_mm_loadu_si128((const __m128i *)lv0), const_3);
    __m128i sum = _mm_add_epi8(l0, p12);
    sum = _mm_add_epi8(sum, p34);
    _mm_store_si128((__m128i *)cc,
                    finish_nz_ctx_sse2(sum, const_4, zero, pos0));
    pos0 = pos1;
    pos1 = pos2;
    c3 = c4;
    p12 = p23;
    p23 = p34;
    lv0 += stride;
    lv4 += stride;
    cc += height;
  } while (--col);
}

static inline void get_16n_coeff_contexts_hor(const uint8_t *levels,
                                              const int width, const int height,
                                              const int last_idx,
                                              int8_t *coeff_contexts) {
  const int stride = height + TX_PAD_HOR;
  const int shift = 4 + (height >> 5);
  const int col = AOMMIN(width, (last_idx + height - 1) >> shift);
  get_16n_hor_band_sse2(levels, stride, col, height, coeff_contexts);
  if (height == 32) {
    get_16n_hor_band_sse2(levels + 16, stride, col, height,
                          coeff_contexts + 16);
  }
}

static void get_nz_map_contexts_hv_sse2(const uint8_t *const levels,
                                        const int16_t *const scan,
                                        const int last_idx,
                                        const TX_SIZE tx_size,
                                        const TX_CLASS tx_class,
                                        int8_t *const coeff_contexts) {
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  const int is_hor = (tx_class == TX_CLASS_HORIZ);
  if (height == 4) {
    get_4_nz_map_contexts_hv(levels, width, last_idx, is_hor, coeff_contexts);
  } else if (height == 8) {
    get_8_coeff_contexts_hv(levels, width, last_idx, is_hor, coeff_contexts);
  } else if (is_hor) {
    get_16n_coeff_contexts_hor(levels, width, height, last_idx, coeff_contexts);
  } else {
    get_16n_coeff_contexts_ver(levels, width, height, last_idx, coeff_contexts);
  }
  const int n_coeffs = width * height;
  coeff_contexts[scan[last_idx]] =
      (int8_t)(1 + (last_idx > (n_coeffs >> 3)) + (last_idx > (n_coeffs >> 2)));
}

static void get_nz_map_contexts_rect_large_sse2(const uint8_t *const levels,
                                                const int16_t *const scan,
                                                const int last_idx,
                                                const TX_SIZE tx_size,
                                                int8_t *const coeff_contexts) {
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  if (height == 4) {
    get_4_nz_map_contexts_2d(levels, width, last_idx, coeff_contexts);
  } else if (height == 8) {
    get_8_coeff_contexts_2d(levels, width, last_idx, coeff_contexts);
  } else {
    const int real_width = tx_size_wide[tx_size];
    const int real_height = tx_size_high[tx_size];
    get_16n_all_coeff_contexts_2d(levels, real_width, real_height, width,
                                  height, last_idx, coeff_contexts);
  }
  const int n_coeffs = width * height;
  coeff_contexts[scan[last_idx]] =
      (int8_t)(1 + (last_idx > (n_coeffs >> 3)) + (last_idx > (n_coeffs >> 2)));
}

// Note: levels[] must be in the range [0, 127], inclusive.
void av1_get_nz_map_contexts_sse2(const uint8_t *const levels,
                                  const int16_t *const scan, const int eob,
                                  const TX_SIZE tx_size,
                                  const TX_CLASS tx_class,
                                  int8_t *const coeff_contexts) {
  const int last_idx = eob - 1;
  if (!last_idx || (last_idx == 1 && tx_class == TX_CLASS_2D)) {
    coeff_contexts[0] = 0;
    if (last_idx) coeff_contexts[scan[1]] = 1;
    return;
  }

  /* coeff_contexts must be 16 byte aligned. */
  assert(!((intptr_t)coeff_contexts & 0xf));

  if (UNLIKELY(tx_class != TX_CLASS_2D)) {
    get_nz_map_contexts_hv_sse2(levels, scan, last_idx, tx_size, tx_class,
                                coeff_contexts);
    return;
  }

  if (tx_size == TX_4X4) {
    get_4x4_2d_sse2(levels, coeff_contexts);
    coeff_contexts[scan[last_idx]] =
        (int8_t)(1 + (last_idx > 2) + (last_idx > 4));
  } else if (tx_size == TX_8X8) {
    get_8x8_2d_sse2(levels, last_idx, coeff_contexts);
    coeff_contexts[scan[last_idx]] =
        (int8_t)(1 + (last_idx > 8) + (last_idx > 16));
  } else if (tx_size == TX_16X16) {
    get_16x16_2d_sse2(levels, last_idx, coeff_contexts);
    coeff_contexts[scan[last_idx]] =
        (int8_t)(1 + (last_idx > 32) + (last_idx > 64));
  } else {
    get_nz_map_contexts_rect_large_sse2(levels, scan, last_idx, tx_size,
                                        coeff_contexts);
  }
}
