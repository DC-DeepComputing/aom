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
#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "config/aom_dsp_rtcd.h"
#include "aom_dsp/x86/synonyms.h"

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
  _mm256_storeu_si256((__m256i *)diff_ptr, _mm256_sub_epi16(s0, p0));
  _mm256_storeu_si256((__m256i *)(diff_ptr + 16), _mm256_sub_epi16(s1, p1));
}

static void sub_4xn_avx2(int rows, int cols, int16_t *diff_ptr,
                         ptrdiff_t diff_stride, const uint8_t *src_ptr,
                         ptrdiff_t src_stride, const uint8_t *pred_ptr,
                         ptrdiff_t pred_stride) {
  assert(cols == 4 && rows >= 4 && (rows & 1) == 0);
  (void)cols;
  int32_t j = rows >> 1;
  do {
    const __m128i s0 = xx_loadl_32(src_ptr);
    const __m128i s1 = xx_loadl_32(src_ptr + src_stride);
    const __m128i p0 = xx_loadl_32(pred_ptr);
    const __m128i p1 = xx_loadl_32(pred_ptr + pred_stride);
    const __m128i d =
        _mm_sub_epi16(_mm_cvtepu8_epi16(_mm_unpacklo_epi32(s0, s1)),
                      _mm_cvtepu8_epi16(_mm_unpacklo_epi32(p0, p1)));
    _mm_storel_epi64((__m128i *)diff_ptr, d);
    _mm_storel_epi64((__m128i *)(diff_ptr + diff_stride), _mm_srli_si128(d, 8));
    src_ptr += 2 * src_stride;
    pred_ptr += 2 * pred_stride;
    diff_ptr += 2 * diff_stride;
  } while (--j != 0);
}

static void sub_8xn_avx2(int rows, int cols, int16_t *diff_ptr,
                         ptrdiff_t diff_stride, const uint8_t *src_ptr,
                         ptrdiff_t src_stride, const uint8_t *pred_ptr,
                         ptrdiff_t pred_stride) {
  assert(cols == 8 && rows >= 4 && (rows & 3) == 0);
  (void)cols;
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

    _mm_storeu_si128((__m128i *)diff_ptr, _mm_sub_epi16(s0, p0));
    _mm_storeu_si128((__m128i *)(diff_ptr + diff_stride),
                     _mm_sub_epi16(s1, p1));
    _mm_storeu_si128((__m128i *)(diff_ptr + 2 * diff_stride),
                     _mm_sub_epi16(s2, p2));
    _mm_storeu_si128((__m128i *)(diff_ptr + 3 * diff_stride),
                     _mm_sub_epi16(s3, p3));
    diff_ptr += 4 * diff_stride;
  } while (--j != 0);
}

static void sub_ge16_avx2(int rows, int cols, int16_t *diff_ptr,
                          ptrdiff_t diff_stride, const uint8_t *src_ptr,
                          ptrdiff_t src_stride, const uint8_t *pred_ptr,
                          ptrdiff_t pred_stride) {
  if (cols > 16) {
    if (cols == 32) {
      assert(rows >= 4 && (rows & 1) == 0);
      int32_t j = rows >> 1;
      do {
        subtract32_avx2(diff_ptr, src_ptr, pred_ptr);
        subtract32_avx2(diff_ptr + diff_stride, src_ptr + src_stride,
                        pred_ptr + pred_stride);
        src_ptr += 2 * src_stride;
        pred_ptr += 2 * pred_stride;
        diff_ptr += 2 * diff_stride;
      } while (--j != 0);
    } else if (cols == 64) {
      assert(rows >= 4);
      int32_t j = rows;
      do {
        subtract32_avx2(diff_ptr, src_ptr, pred_ptr);
        subtract32_avx2(diff_ptr + 32, src_ptr + 32, pred_ptr + 32);
        src_ptr += src_stride;
        pred_ptr += pred_stride;
        diff_ptr += diff_stride;
      } while (--j != 0);
    } else {
      assert(rows >= 4 && cols > 64 && (cols & 63) == 0);
      int32_t j = rows;
      do {
        ptrdiff_t x = 0;
        do {
          subtract32_avx2(diff_ptr + x, src_ptr + x, pred_ptr + x);
          subtract32_avx2(diff_ptr + x + 32, src_ptr + x + 32,
                          pred_ptr + x + 32);
          x += 64;
        } while (x < (ptrdiff_t)cols);
        src_ptr += src_stride;
        pred_ptr += pred_stride;
        diff_ptr += diff_stride;
      } while (--j != 0);
    }
    return;
  }
  assert(cols == 16 && rows >= 4 && (rows & 3) == 0);
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

    _mm256_storeu_si256((__m256i *)diff_ptr, _mm256_sub_epi16(s0, p0));
    _mm256_storeu_si256((__m256i *)(diff_ptr + diff_stride),
                        _mm256_sub_epi16(s1, p1));
    _mm256_storeu_si256((__m256i *)(diff_ptr + 2 * diff_stride),
                        _mm256_sub_epi16(s2, p2));
    _mm256_storeu_si256((__m256i *)(diff_ptr + 3 * diff_stride),
                        _mm256_sub_epi16(s3, p3));
    diff_ptr += 4 * diff_stride;
  } while (--j != 0);
}

void aom_subtract_block_avx2(int rows, int cols, int16_t *diff_ptr,
                             ptrdiff_t diff_stride, const uint8_t *src_ptr,
                             ptrdiff_t src_stride, const uint8_t *pred_ptr,
                             ptrdiff_t pred_stride) {
  if (cols == 8) {
    sub_8xn_avx2(rows, cols, diff_ptr, diff_stride, src_ptr, src_stride,
                 pred_ptr, pred_stride);
    return;
  }
  if (cols < 8) {
    sub_4xn_avx2(rows, cols, diff_ptr, diff_stride, src_ptr, src_stride,
                 pred_ptr, pred_stride);
    return;
  }
  sub_ge16_avx2(rows, cols, diff_ptr, diff_stride, src_ptr, src_stride,
                pred_ptr, pred_stride);
}
