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
#include <string.h>

#include "config/aom_dsp_rtcd.h"

static inline void subtract_block_4xn_avx2(
    int rows, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr,
    ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride) {
  int32_t j = rows >> 1;
  do {
    const __m128i s0 = _mm_castps_si128(_mm_load_ss((const float *)src_ptr));
    src_ptr += src_stride;
    const __m128i s1 = _mm_castps_si128(_mm_load_ss((const float *)src_ptr));
    src_ptr += src_stride;
    const __m128i p0 = _mm_castps_si128(_mm_load_ss((const float *)pred_ptr));
    pred_ptr += pred_stride;
    const __m128i p1 = _mm_castps_si128(_mm_load_ss((const float *)pred_ptr));
    pred_ptr += pred_stride;
    const __m128i d =
        _mm_sub_epi16(_mm_cvtepu8_epi16(_mm_unpacklo_epi32(s0, s1)),
                      _mm_cvtepu8_epi16(_mm_unpacklo_epi32(p0, p1)));
    _mm_storel_epi64((__m128i *)diff_ptr, d);
    diff_ptr += diff_stride;
    _mm_storeh_pi((__m64 *)diff_ptr, _mm_castsi128_ps(d));
    diff_ptr += diff_stride;
  } while (--j != 0);
}

static inline void subtract_block_8xn_avx2(
    int rows, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr,
    ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride) {
  int32_t j = rows >> 2;
  do {
    const __m128i s0 =
        _mm_cvtepu8_epi16(_mm_loadl_epi64((const __m128i *)src_ptr));
    const __m128i s1 = _mm_cvtepu8_epi16(
        _mm_loadl_epi64((const __m128i *)(src_ptr + src_stride)));
    const __m128i s2 = _mm_cvtepu8_epi16(
        _mm_loadl_epi64((const __m128i *)(src_ptr + 2 * src_stride)));
    const __m128i s3 = _mm_cvtepu8_epi16(
        _mm_loadl_epi64((const __m128i *)(src_ptr + 3 * src_stride)));
    src_ptr += 4 * src_stride;

    const __m128i p0 =
        _mm_cvtepu8_epi16(_mm_loadl_epi64((const __m128i *)pred_ptr));
    const __m128i p1 = _mm_cvtepu8_epi16(
        _mm_loadl_epi64((const __m128i *)(pred_ptr + pred_stride)));
    const __m128i p2 = _mm_cvtepu8_epi16(
        _mm_loadl_epi64((const __m128i *)(pred_ptr + 2 * pred_stride)));
    const __m128i p3 = _mm_cvtepu8_epi16(
        _mm_loadl_epi64((const __m128i *)(pred_ptr + 3 * pred_stride)));
    pred_ptr += 4 * pred_stride;

    _mm_store_si128((__m128i *)diff_ptr, _mm_sub_epi16(s0, p0));
    _mm_storeu_si128((__m128i *)(diff_ptr + diff_stride),
                     _mm_sub_epi16(s1, p1));
    _mm_storeu_si128((__m128i *)(diff_ptr + 2 * diff_stride),
                     _mm_sub_epi16(s2, p2));
    _mm_storeu_si128((__m128i *)(diff_ptr + 3 * diff_stride),
                     _mm_sub_epi16(s3, p3));
    diff_ptr += 4 * diff_stride;
  } while (--j != 0);
}

static inline void subtract32_avx2(int16_t *diff_ptr, const uint8_t *src_ptr,
                                   const uint8_t *pred_ptr) {
  const __m256i s0 =
      _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)src_ptr));
  const __m256i s1 =
      _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)(src_ptr + 16)));
  const __m256i p0 =
      _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)pred_ptr));
  const __m256i p1 =
      _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)(pred_ptr + 16)));
  _mm256_store_si256((__m256i *)diff_ptr, _mm256_sub_epi16(s0, p0));
  _mm256_store_si256((__m256i *)(diff_ptr + 16), _mm256_sub_epi16(s1, p1));
}

static inline void subtract_block_16xn_avx2(
    int rows, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr,
    ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride) {
  int32_t j = rows >> 2;
  do {
    const __m256i s0 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)src_ptr));
    const __m256i s1 = _mm256_cvtepu8_epi16(
        _mm_loadu_si128((const __m128i *)(src_ptr + src_stride)));
    const __m256i s2 = _mm256_cvtepu8_epi16(
        _mm_loadu_si128((const __m128i *)(src_ptr + 2 * src_stride)));
    const __m256i s3 = _mm256_cvtepu8_epi16(
        _mm_loadu_si128((const __m128i *)(src_ptr + 3 * src_stride)));
    src_ptr += 4 * src_stride;

    const __m256i p0 =
        _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)pred_ptr));
    const __m256i p1 = _mm256_cvtepu8_epi16(
        _mm_loadu_si128((const __m128i *)(pred_ptr + pred_stride)));
    const __m256i p2 = _mm256_cvtepu8_epi16(
        _mm_loadu_si128((const __m128i *)(pred_ptr + 2 * pred_stride)));
    const __m256i p3 = _mm256_cvtepu8_epi16(
        _mm_loadu_si128((const __m128i *)(pred_ptr + 3 * pred_stride)));
    pred_ptr += 4 * pred_stride;

    _mm256_store_si256((__m256i *)diff_ptr, _mm256_sub_epi16(s0, p0));
    _mm256_store_si256((__m256i *)(diff_ptr + diff_stride),
                       _mm256_sub_epi16(s1, p1));
    _mm256_store_si256((__m256i *)(diff_ptr + 2 * diff_stride),
                       _mm256_sub_epi16(s2, p2));
    _mm256_store_si256((__m256i *)(diff_ptr + 3 * diff_stride),
                       _mm256_sub_epi16(s3, p3));
    diff_ptr += 4 * diff_stride;
  } while (--j != 0);
}

static inline void subtract_block_32xn_avx2(
    int rows, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr,
    ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride) {
  int32_t j = rows >> 1;
  do {
    subtract32_avx2(diff_ptr, src_ptr, pred_ptr);
    subtract32_avx2(diff_ptr + diff_stride, src_ptr + src_stride,
                    pred_ptr + pred_stride);
    src_ptr += 2 * src_stride;
    pred_ptr += 2 * pred_stride;
    diff_ptr += 2 * diff_stride;
  } while (--j != 0);
}

static inline void subtract_block_64xn_avx2(
    int rows, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr,
    ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride) {
  int32_t j = rows;
  do {
    subtract32_avx2(diff_ptr, src_ptr, pred_ptr);
    subtract32_avx2(diff_ptr + 32, src_ptr + 32, pred_ptr + 32);
    src_ptr += src_stride;
    pred_ptr += pred_stride;
    diff_ptr += diff_stride;
  } while (--j != 0);
}

static inline void subtract_block_128xn_avx2(
    int rows, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr,
    ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride) {
  int32_t j = rows;
  do {
    subtract32_avx2(diff_ptr, src_ptr, pred_ptr);
    subtract32_avx2(diff_ptr + 32, src_ptr + 32, pred_ptr + 32);
    subtract32_avx2(diff_ptr + 64, src_ptr + 64, pred_ptr + 64);
    subtract32_avx2(diff_ptr + 96, src_ptr + 96, pred_ptr + 96);
    src_ptr += src_stride;
    pred_ptr += pred_stride;
    diff_ptr += diff_stride;
  } while (--j != 0);
}

void aom_subtract_block_avx2(int rows, int cols, int16_t *diff_ptr,
                             ptrdiff_t diff_stride, const uint8_t *src_ptr,
                             ptrdiff_t src_stride, const uint8_t *pred_ptr,
                             ptrdiff_t pred_stride) {
  switch (cols) {
    case 4:
      subtract_block_4xn_avx2(rows, diff_ptr, diff_stride, src_ptr, src_stride,
                              pred_ptr, pred_stride);
      break;
    case 8:
      subtract_block_8xn_avx2(rows, diff_ptr, diff_stride, src_ptr, src_stride,
                              pred_ptr, pred_stride);
      break;
    case 16:
      subtract_block_16xn_avx2(rows, diff_ptr, diff_stride, src_ptr, src_stride,
                               pred_ptr, pred_stride);
      break;
    case 32:
      subtract_block_32xn_avx2(rows, diff_ptr, diff_stride, src_ptr, src_stride,
                               pred_ptr, pred_stride);
      break;
    case 64:
      subtract_block_64xn_avx2(rows, diff_ptr, diff_stride, src_ptr, src_stride,
                               pred_ptr, pred_stride);
      break;
    default:
      subtract_block_128xn_avx2(rows, diff_ptr, diff_stride, src_ptr,
                                src_stride, pred_ptr, pred_stride);
      break;
  }
}
