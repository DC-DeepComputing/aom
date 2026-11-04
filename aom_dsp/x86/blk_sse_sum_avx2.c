/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>

#include "config/aom_dsp_rtcd.h"

static inline void sse_sum_wd4_avx2(const int16_t *data, int stride, int bh,
                                    int *x_sum, int64_t *x2_sum) {
  __m128i r01 =
      _mm_unpacklo_epi64(_mm_loadl_epi64((const __m128i *)data),
                         _mm_loadl_epi64((const __m128i *)(data + stride)));
  __m128i r23 =
      _mm_unpacklo_epi64(_mm_loadl_epi64((const __m128i *)(data + 2 * stride)),
                         _mm_loadl_epi64((const __m128i *)(data + 3 * stride)));
  const __m128i one = _mm_srli_epi16(_mm_cmpeq_epi16(r01, r01), 15);
  __m128i sum32 = _mm_madd_epi16(_mm_add_epi16(r01, r23), one);
  __m128i sse32 =
      _mm_add_epi32(_mm_madd_epi16(r01, r01), _mm_madd_epi16(r23, r23));
  if (bh > 4) {
    do {
      data += 4 * stride;
      r01 =
          _mm_unpacklo_epi64(_mm_loadl_epi64((const __m128i *)data),
                             _mm_loadl_epi64((const __m128i *)(data + stride)));
      r23 = _mm_unpacklo_epi64(
          _mm_loadl_epi64((const __m128i *)(data + 2 * stride)),
          _mm_loadl_epi64((const __m128i *)(data + 3 * stride)));
      sum32 =
          _mm_add_epi32(sum32, _mm_madd_epi16(_mm_add_epi16(r01, r23), one));
      sse32 = _mm_add_epi32(sse32, _mm_add_epi32(_mm_madd_epi16(r01, r01),
                                                 _mm_madd_epi16(r23, r23)));
    } while ((bh -= 4) > 4);
  }
  sum32 = _mm_add_epi32(sum32, _mm_srli_si128(sum32, 8));
  sum32 = _mm_add_epi32(sum32, _mm_srli_si128(sum32, 4));
  *x_sum = _mm_cvtsi128_si32(sum32);
  const __m128i zero = _mm_setzero_si128();
  __m128i sse64 = _mm_add_epi64(_mm_unpacklo_epi32(sse32, zero),
                                _mm_unpackhi_epi32(sse32, zero));
  sse64 = _mm_add_epi64(sse64, _mm_srli_si128(sse64, 8));
  _mm_storel_epi64((__m128i *)x2_sum, sse64);
}

static inline void sse_sum_wd8_avx2(const int16_t *data, int stride, int bh,
                                    int *x_sum, int64_t *x2_sum) {
  __m128i r0 = _mm_loadu_si128((const __m128i *)data);
  __m128i r1 = _mm_loadu_si128((const __m128i *)(data + stride));
  __m128i r2 = _mm_loadu_si128((const __m128i *)(data + 2 * stride));
  __m128i r3 = _mm_loadu_si128((const __m128i *)(data + 3 * stride));
  const __m128i one = _mm_srli_epi16(_mm_cmpeq_epi16(r0, r0), 15);
  __m128i sum32 = _mm_madd_epi16(
      _mm_add_epi16(_mm_add_epi16(r0, r1), _mm_add_epi16(r2, r3)), one);
  __m128i sse32 = _mm_add_epi32(
      _mm_add_epi32(_mm_madd_epi16(r0, r0), _mm_madd_epi16(r1, r1)),
      _mm_add_epi32(_mm_madd_epi16(r2, r2), _mm_madd_epi16(r3, r3)));
  if (bh > 4) {
    do {
      data += 4 * stride;
      r0 = _mm_loadu_si128((const __m128i *)data);
      r1 = _mm_loadu_si128((const __m128i *)(data + stride));
      r2 = _mm_loadu_si128((const __m128i *)(data + 2 * stride));
      r3 = _mm_loadu_si128((const __m128i *)(data + 3 * stride));
      sum32 = _mm_add_epi32(sum32,
                            _mm_madd_epi16(_mm_add_epi16(_mm_add_epi16(r0, r1),
                                                         _mm_add_epi16(r2, r3)),
                                           one));
      sse32 = _mm_add_epi32(
          sse32,
          _mm_add_epi32(
              _mm_add_epi32(_mm_madd_epi16(r0, r0), _mm_madd_epi16(r1, r1)),
              _mm_add_epi32(_mm_madd_epi16(r2, r2), _mm_madd_epi16(r3, r3))));
    } while ((bh -= 4) > 4);
  }
  sum32 = _mm_add_epi32(sum32, _mm_srli_si128(sum32, 8));
  sum32 = _mm_add_epi32(sum32, _mm_srli_si128(sum32, 4));
  *x_sum = _mm_cvtsi128_si32(sum32);
  const __m128i zero = _mm_setzero_si128();
  __m128i sse64 = _mm_add_epi64(_mm_unpacklo_epi32(sse32, zero),
                                _mm_unpackhi_epi32(sse32, zero));
  sse64 = _mm_add_epi64(sse64, _mm_srli_si128(sse64, 8));
  _mm_storel_epi64((__m128i *)x2_sum, sse64);
}

static inline void sse_sum_wd16_avx2(const int16_t *data, int stride, int bh,
                                     int *x_sum, int64_t *x2_sum,
                                     int loop_count) {
  __m256i sum_total = _mm256_setzero_si256();
  __m256i sse_total = _mm256_setzero_si256();
  const __m256i zero = _mm256_setzero_si256();
  const __m256i one = _mm256_srli_epi16(_mm256_cmpeq_epi16(zero, zero), 15);

  for (int i = 0; i < loop_count; ++i) {
    const int16_t *data_tmp = data + 16 * i;
    __m256i sse_strip = _mm256_setzero_si256();
    for (int j = 0; j < bh; j += 2) {
      const __m256i v0 = _mm256_loadu_si256((const __m256i *)data_tmp);
      const __m256i v1 =
          _mm256_loadu_si256((const __m256i *)(data_tmp + stride));
      sum_total = _mm256_add_epi32(
          sum_total, _mm256_madd_epi16(_mm256_add_epi16(v0, v1), one));
      sse_strip = _mm256_add_epi32(sse_strip,
                                   _mm256_add_epi32(_mm256_madd_epi16(v0, v0),
                                                    _mm256_madd_epi16(v1, v1)));
      data_tmp += 2 * stride;
    }
    sse_total = _mm256_add_epi64(
        sse_total, _mm256_add_epi64(_mm256_unpacklo_epi32(sse_strip, zero),
                                    _mm256_unpackhi_epi32(sse_strip, zero)));
  }

  __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(sum_total),
                                 _mm256_extracti128_si256(sum_total, 1));
  sum128 = _mm_add_epi32(sum128, _mm_srli_si128(sum128, 8));
  sum128 = _mm_add_epi32(sum128, _mm_srli_si128(sum128, 4));
  *x_sum = _mm_cvtsi128_si32(sum128);

  __m128i sse128 = _mm_add_epi64(_mm256_castsi256_si128(sse_total),
                                 _mm256_extracti128_si256(sse_total, 1));
  sse128 = _mm_add_epi64(sse128, _mm_srli_si128(sse128, 8));
  _mm_storel_epi64((__m128i *)x2_sum, sse128);
}

void aom_get_blk_sse_sum_avx2(const int16_t *data, int stride, int bw, int bh,
                              int *x_sum, int64_t *x2_sum) {
  if ((bh & 3) == 0 && bh > 0) {
    switch (bw) {
      case 4: sse_sum_wd4_avx2(data, stride, bh, x_sum, x2_sum); break;
      case 8: sse_sum_wd8_avx2(data, stride, bh, x_sum, x2_sum); break;
      case 16:
      case 32:
      case 64:
        sse_sum_wd16_avx2(data, stride, bh, x_sum, x2_sum, bw >> 4);
        break;
      default: aom_get_blk_sse_sum_c(data, stride, bw, bh, x_sum, x2_sum);
    }
  } else {
    aom_get_blk_sse_sum_c(data, stride, bw, bh, x_sum, x2_sum);
  }
}
