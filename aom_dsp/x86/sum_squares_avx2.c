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

#include <immintrin.h>
#include <smmintrin.h>

#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/synonyms_avx2.h"
#include "aom_dsp/x86/sum_squares_sse2.h"
#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

static uint64_t aom_sum_squares_2d_i16_nxn_avx2(const int16_t *src, int stride,
                                                int width, int height) {
  const __m256i zero = _mm256_setzero_si256();
  __m256i v_acc_q = zero;
  int col = 0;
  do {
    const int16_t *s = src + col;
    int r = 0;
    do {
      const int r_limit = r + 64 < height ? r + 64 : height;
      __m256i v_acc_d = zero;
      do {
        const __m256i v0 =
            _mm256_loadu_si256((const __m256i *)(s + 0 * stride));
        const __m256i v1 =
            _mm256_loadu_si256((const __m256i *)(s + 1 * stride));
        const __m256i v2 =
            _mm256_loadu_si256((const __m256i *)(s + 2 * stride));
        const __m256i v3 =
            _mm256_loadu_si256((const __m256i *)(s + 3 * stride));
        const __m256i q01 = _mm256_add_epi32(_mm256_madd_epi16(v0, v0),
                                             _mm256_madd_epi16(v1, v1));
        const __m256i q23 = _mm256_add_epi32(_mm256_madd_epi16(v2, v2),
                                             _mm256_madd_epi16(v3, v3));
        v_acc_d = _mm256_add_epi32(v_acc_d, _mm256_add_epi32(q01, q23));
        s += 4 * stride;
        r += 4;
      } while (r < r_limit);
      v_acc_q = _mm256_add_epi64(
          v_acc_q, _mm256_add_epi64(_mm256_unpacklo_epi32(v_acc_d, zero),
                                    _mm256_unpackhi_epi32(v_acc_d, zero)));
    } while (r < height);
    col += 16;
  } while (col < width);
  __m128i res = _mm_add_epi64(_mm256_castsi256_si128(v_acc_q),
                              _mm256_extracti128_si256(v_acc_q, 1));
  res = _mm_add_epi64(res, _mm_unpackhi_epi64(res, res));
  uint64_t result;
  xx_storel_64(&result, res);
  return result;
}

uint64_t aom_sum_squares_2d_i16_avx2(const int16_t *src, int stride, int width,
                                     int height) {
  if (width == 4 && height == 4) {
    return aom_sum_squares_2d_i16_4x4_sse2(src, stride);
  } else if (width == 4 && (height & 3) == 0) {
    return aom_sum_squares_2d_i16_4xn_sse2(src, stride, height);
  } else if (LIKELY(width == 8 && (height & 3) == 0)) {
    const __m128i zero = _mm_setzero_si128();
    __m128i v_acc_d = zero;
    int r = 0;
    do {
      const __m128i r0 = _mm_loadu_si128((const __m128i *)(src + 0 * stride));
      const __m128i r1 = _mm_loadu_si128((const __m128i *)(src + 1 * stride));
      const __m128i r2 = _mm_loadu_si128((const __m128i *)(src + 2 * stride));
      const __m128i r3 = _mm_loadu_si128((const __m128i *)(src + 3 * stride));
      const __m128i q01 =
          _mm_add_epi32(_mm_madd_epi16(r0, r0), _mm_madd_epi16(r1, r1));
      const __m128i q23 =
          _mm_add_epi32(_mm_madd_epi16(r2, r2), _mm_madd_epi16(r3, r3));
      v_acc_d = _mm_add_epi32(v_acc_d, _mm_add_epi32(q01, q23));
      src += 4 * stride;
      r += 4;
    } while (r < height);
    __m128i acc64 = _mm_add_epi64(_mm_unpacklo_epi32(v_acc_d, zero),
                                  _mm_unpackhi_epi32(v_acc_d, zero));
    acc64 = _mm_add_epi64(acc64, _mm_srli_si128(acc64, 8));
    uint64_t result;
    xx_storel_64(&result, acc64);
    return result;
  } else if (LIKELY(((width & 15) == 0) && ((height & 3) == 0))) {
    return aom_sum_squares_2d_i16_nxn_avx2(src, stride, width, height);
  } else {
    return aom_sum_squares_2d_i16_c(src, stride, width, height);
  }
}

static uint64_t aom_sum_sse_2d_i16_nxn_avx2(const int16_t *src, int stride,
                                            int width, int height, int *sum) {
  const __m256i zero_reg = _mm256_setzero_si256();
  const __m256i one_reg =
      _mm256_srli_epi16(_mm256_cmpeq_epi16(zero_reg, zero_reg), 15);
  __m256i v_sse_total = zero_reg;
  __m256i v_sum_total = zero_reg;

  int col = 0;
  do {
    const int16_t *s = src + col;
    int r = 0;
    do {
      const int r_limit = r + 64 < height ? r + 64 : height;
      __m256i v_sse_d = zero_reg;
      do {
        const __m256i v0 =
            _mm256_loadu_si256((const __m256i *)(s + 0 * stride));
        const __m256i v1 =
            _mm256_loadu_si256((const __m256i *)(s + 1 * stride));
        const __m256i v2 =
            _mm256_loadu_si256((const __m256i *)(s + 2 * stride));
        const __m256i v3 =
            _mm256_loadu_si256((const __m256i *)(s + 3 * stride));

        const __m256i s01 = _mm256_add_epi16(v0, v1);
        const __m256i s23 = _mm256_add_epi16(v2, v3);
        v_sum_total = _mm256_add_epi32(
            v_sum_total,
            _mm256_madd_epi16(_mm256_add_epi16(s01, s23), one_reg));

        const __m256i q01 = _mm256_add_epi32(_mm256_madd_epi16(v0, v0),
                                             _mm256_madd_epi16(v1, v1));
        const __m256i q23 = _mm256_add_epi32(_mm256_madd_epi16(v2, v2),
                                             _mm256_madd_epi16(v3, v3));
        v_sse_d = _mm256_add_epi32(v_sse_d, _mm256_add_epi32(q01, q23));
        s += 4 * stride;
        r += 4;
      } while (r < r_limit);
      v_sse_total = _mm256_add_epi64(
          v_sse_total,
          _mm256_add_epi64(_mm256_unpacklo_epi32(v_sse_d, zero_reg),
                           _mm256_unpackhi_epi32(v_sse_d, zero_reg)));
    } while (r < height);
    col += 16;
  } while (col < width);

  __m128i sum_128bit = _mm_add_epi32(_mm256_castsi256_si128(v_sum_total),
                                     _mm256_extracti128_si256(v_sum_total, 1));
  sum_128bit = _mm_add_epi32(sum_128bit, _mm_srli_si128(sum_128bit, 8));
  sum_128bit = _mm_add_epi32(sum_128bit, _mm_srli_si128(sum_128bit, 4));
  *sum += _mm_cvtsi128_si32(sum_128bit);

  __m128i sse_128bit = _mm_add_epi64(_mm256_castsi256_si128(v_sse_total),
                                     _mm256_extracti128_si256(v_sse_total, 1));
  sse_128bit =
      _mm_add_epi64(sse_128bit, _mm_unpackhi_epi64(sse_128bit, sse_128bit));
  uint64_t result;
  xx_storel_64(&result, sse_128bit);
  return result;
}

uint64_t aom_sum_sse_2d_i16_avx2(const int16_t *src, int src_stride, int width,
                                 int height, int *sum) {
  if (width == 4 && (height & 3) == 0) {
    const __m128i zero = _mm_setzero_si128();
    const __m128i one = _mm_srli_epi16(_mm_cmpeq_epi16(zero, zero), 15);
    __m128i v_sum = zero;
    __m128i v_sse = zero;
    int r = 0;
    do {
      const __m128i r0 =
          _mm_loadl_epi64((const __m128i *)(src + 0 * src_stride));
      const __m128i r1 =
          _mm_loadl_epi64((const __m128i *)(src + 1 * src_stride));
      const __m128i r2 =
          _mm_loadl_epi64((const __m128i *)(src + 2 * src_stride));
      const __m128i r3 =
          _mm_loadl_epi64((const __m128i *)(src + 3 * src_stride));
      const __m128i r01 = _mm_unpacklo_epi64(r0, r1);
      const __m128i r23 = _mm_unpacklo_epi64(r2, r3);
      v_sum =
          _mm_add_epi32(v_sum, _mm_madd_epi16(_mm_add_epi16(r01, r23), one));
      v_sse = _mm_add_epi32(v_sse, _mm_add_epi32(_mm_madd_epi16(r01, r01),
                                                 _mm_madd_epi16(r23, r23)));
      src += 4 * src_stride;
      r += 4;
    } while (r < height);
    v_sum = _mm_add_epi32(v_sum, _mm_srli_si128(v_sum, 8));
    v_sum = _mm_add_epi32(v_sum, _mm_srli_si128(v_sum, 4));
    *sum += _mm_cvtsi128_si32(v_sum);
    __m128i sse64 = _mm_add_epi64(_mm_unpacklo_epi32(v_sse, zero),
                                  _mm_unpackhi_epi32(v_sse, zero));
    sse64 = _mm_add_epi64(sse64, _mm_srli_si128(sse64, 8));
    uint64_t result;
    xx_storel_64(&result, sse64);
    return result;
  } else if (LIKELY(width == 8 && (height & 3) == 0)) {
    const __m128i zero = _mm_setzero_si128();
    const __m128i one = _mm_srli_epi16(_mm_cmpeq_epi16(zero, zero), 15);
    __m128i v_sum = zero;
    __m128i v_sse = zero;
    int r = 0;
    do {
      const __m128i r0 =
          _mm_loadu_si128((const __m128i *)(src + 0 * src_stride));
      const __m128i r1 =
          _mm_loadu_si128((const __m128i *)(src + 1 * src_stride));
      const __m128i r2 =
          _mm_loadu_si128((const __m128i *)(src + 2 * src_stride));
      const __m128i r3 =
          _mm_loadu_si128((const __m128i *)(src + 3 * src_stride));
      const __m128i s01 = _mm_add_epi16(r0, r1);
      const __m128i s23 = _mm_add_epi16(r2, r3);
      v_sum =
          _mm_add_epi32(v_sum, _mm_madd_epi16(_mm_add_epi16(s01, s23), one));
      const __m128i q01 =
          _mm_add_epi32(_mm_madd_epi16(r0, r0), _mm_madd_epi16(r1, r1));
      const __m128i q23 =
          _mm_add_epi32(_mm_madd_epi16(r2, r2), _mm_madd_epi16(r3, r3));
      v_sse = _mm_add_epi32(v_sse, _mm_add_epi32(q01, q23));
      src += 4 * src_stride;
      r += 4;
    } while (r < height);
    v_sum = _mm_add_epi32(v_sum, _mm_srli_si128(v_sum, 8));
    v_sum = _mm_add_epi32(v_sum, _mm_srli_si128(v_sum, 4));
    *sum += _mm_cvtsi128_si32(v_sum);
    __m128i sse64 = _mm_add_epi64(_mm_unpacklo_epi32(v_sse, zero),
                                  _mm_unpackhi_epi32(v_sse, zero));
    sse64 = _mm_add_epi64(sse64, _mm_srli_si128(sse64, 8));
    uint64_t result;
    xx_storel_64(&result, sse64);
    return result;
  } else if (LIKELY(((width & 15) == 0) && ((height & 3) == 0))) {
    return aom_sum_sse_2d_i16_nxn_avx2(src, src_stride, width, height, sum);
  } else {
    return aom_sum_sse_2d_i16_c(src, src_stride, width, height, sum);
  }
}

// Accumulate sum of 16-bit elements in the vector
static inline int32_t mm256_accumulate_epi16(__m256i vec_a) {
  __m128i vtmp1 = _mm256_extracti128_si256(vec_a, 1);
  __m128i vtmp2 = _mm256_castsi256_si128(vec_a);
  vtmp1 = _mm_add_epi16(vtmp1, vtmp2);
  vtmp2 = _mm_srli_si128(vtmp1, 8);
  vtmp1 = _mm_add_epi16(vtmp1, vtmp2);
  vtmp2 = _mm_srli_si128(vtmp1, 4);
  vtmp1 = _mm_add_epi16(vtmp1, vtmp2);
  vtmp2 = _mm_srli_si128(vtmp1, 2);
  vtmp1 = _mm_add_epi16(vtmp1, vtmp2);
  return _mm_extract_epi16(vtmp1, 0);
}

// Accumulate sum of 32-bit elements in the vector
static inline int32_t mm256_accumulate_epi32(__m256i vec_a) {
  __m128i vtmp1 = _mm256_extracti128_si256(vec_a, 1);
  __m128i vtmp2 = _mm256_castsi256_si128(vec_a);
  vtmp1 = _mm_add_epi32(vtmp1, vtmp2);
  vtmp2 = _mm_srli_si128(vtmp1, 8);
  vtmp1 = _mm_add_epi32(vtmp1, vtmp2);
  vtmp2 = _mm_srli_si128(vtmp1, 4);
  vtmp1 = _mm_add_epi32(vtmp1, vtmp2);
  return _mm_cvtsi128_si32(vtmp1);
}

uint64_t aom_var_2d_u8_avx2(uint8_t *src, int src_stride, int width,
                            int height) {
  uint8_t *srcp;
  uint64_t s = 0, ss = 0;
  __m256i vzero = _mm256_setzero_si256();
  __m256i v_acc_sum = vzero;
  __m256i v_acc_sqs = vzero;
  int i, j;

  // Process 32 elements in a row
  for (i = 0; i < width - 31; i += 32) {
    srcp = src + i;
    // Process 8 columns at a time
    for (j = 0; j < height - 7; j += 8) {
      for (int k = 0; k < 8; k++) {
        __m256i vsrc = _mm256_loadu_si256((__m256i *)srcp);
        srcp += src_stride;
        __m256i vsrc0 = _mm256_unpacklo_epi8(vsrc, vzero);
        __m256i vsrc1 = _mm256_unpackhi_epi8(vsrc, vzero);
        v_acc_sum = _mm256_add_epi16(v_acc_sum, vsrc0);
        v_acc_sum = _mm256_add_epi16(v_acc_sum, vsrc1);

        __m256i vsqs0 = _mm256_madd_epi16(vsrc0, vsrc0);
        __m256i vsqs1 = _mm256_madd_epi16(vsrc1, vsrc1);
        v_acc_sqs = _mm256_add_epi32(v_acc_sqs, vsqs0);
        v_acc_sqs = _mm256_add_epi32(v_acc_sqs, vsqs1);
      }

      // Update total sum and clear the vectors
      s += mm256_accumulate_epi16(v_acc_sum);
      ss += mm256_accumulate_epi32(v_acc_sqs);
      v_acc_sum = vzero;
      v_acc_sqs = vzero;
    }

    // Process remaining rows (height not a multiple of 8)
    for (; j < height; j++) {
      __m256i vsrc = _mm256_loadu_si256((__m256i *)srcp);
      __m256i vsrc0 = _mm256_unpacklo_epi8(vsrc, vzero);
      __m256i vsrc1 = _mm256_unpackhi_epi8(vsrc, vzero);
      v_acc_sum = _mm256_add_epi16(v_acc_sum, vsrc0);
      v_acc_sum = _mm256_add_epi16(v_acc_sum, vsrc1);

      __m256i vsqs0 = _mm256_madd_epi16(vsrc0, vsrc0);
      __m256i vsqs1 = _mm256_madd_epi16(vsrc1, vsrc1);
      v_acc_sqs = _mm256_add_epi32(v_acc_sqs, vsqs0);
      v_acc_sqs = _mm256_add_epi32(v_acc_sqs, vsqs1);

      srcp += src_stride;
    }

    // Update total sum and clear the vectors
    s += mm256_accumulate_epi16(v_acc_sum);
    ss += mm256_accumulate_epi32(v_acc_sqs);
    v_acc_sum = vzero;
    v_acc_sqs = vzero;
  }

  // Process the remaining area using C
  srcp = src;
  for (int k = 0; k < height; k++) {
    for (int m = i; m < width; m++) {
      uint8_t val = srcp[m];
      s += val;
      ss += val * val;
    }
    srcp += src_stride;
  }
  return (ss - s * s / (width * height));
}

#if CONFIG_AV1_HIGHBITDEPTH
uint64_t aom_var_2d_u16_avx2(uint8_t *src, int src_stride, int width,
                             int height) {
  uint16_t *srcp1 = CONVERT_TO_SHORTPTR(src), *srcp;
  uint64_t s = 0, ss = 0;
  __m256i vzero = _mm256_setzero_si256();
  __m256i v_acc_sum = vzero;
  __m256i v_acc_sqs = vzero;
  int i, j;

  // Process 16 elements in a row
  for (i = 0; i < width - 15; i += 16) {
    srcp = srcp1 + i;
    // Process 8 columns at a time
    for (j = 0; j < height - 8; j += 8) {
      for (int k = 0; k < 8; k++) {
        __m256i vsrc = _mm256_loadu_si256((__m256i *)srcp);
        srcp += src_stride;
        __m256i vsrc0 = _mm256_unpacklo_epi16(vsrc, vzero);
        __m256i vsrc1 = _mm256_unpackhi_epi16(vsrc, vzero);
        v_acc_sum = _mm256_add_epi32(vsrc0, v_acc_sum);
        v_acc_sum = _mm256_add_epi32(vsrc1, v_acc_sum);

        __m256i vsqs0 = _mm256_madd_epi16(vsrc, vsrc);
        v_acc_sqs = _mm256_add_epi32(v_acc_sqs, vsqs0);
      }

      // Update total sum and clear the vectors
      s += mm256_accumulate_epi32(v_acc_sum);
      ss += mm256_accumulate_epi32(v_acc_sqs);
      v_acc_sum = vzero;
      v_acc_sqs = vzero;
    }

    // Process remaining rows (height not a multiple of 8)
    for (; j < height; j++) {
      __m256i vsrc = _mm256_loadu_si256((__m256i *)srcp);
      __m256i vsrc0 = _mm256_unpacklo_epi16(vsrc, vzero);
      __m256i vsrc1 = _mm256_unpackhi_epi16(vsrc, vzero);
      v_acc_sum = _mm256_add_epi32(vsrc0, v_acc_sum);
      v_acc_sum = _mm256_add_epi32(vsrc1, v_acc_sum);

      __m256i vsqs0 = _mm256_madd_epi16(vsrc, vsrc);
      v_acc_sqs = _mm256_add_epi32(v_acc_sqs, vsqs0);
      srcp += src_stride;
    }

    // Update total sum and clear the vectors
    s += mm256_accumulate_epi32(v_acc_sum);
    ss += mm256_accumulate_epi32(v_acc_sqs);
    v_acc_sum = vzero;
    v_acc_sqs = vzero;
  }

  // Process the remaining area using C
  srcp = srcp1;
  for (int k = 0; k < height; k++) {
    for (int m = i; m < width; m++) {
      uint16_t val = srcp[m];
      s += val;
      ss += val * val;
    }
    srcp += src_stride;
  }
  return (ss - s * s / (width * height));
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
