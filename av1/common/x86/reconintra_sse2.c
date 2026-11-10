/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <emmintrin.h>
#include <string.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "av1/common/blockd.h"
#include "av1/common/enums.h"
#include "av1/common/reconintra.h"

// Computes the horizontal sum of txwpx 8-bit pixels in above_ref using SSE2
// _mm_sad_epu8 against zero.
static AOM_FORCE_INLINE uint32_t
sum_above_row_lbd_sse2(const uint8_t *above_ref, int txwpx) {
  const __m128i zero = _mm_setzero_si128();
  if (txwpx == 4) {
    uint32_t v;
    memcpy(&v, above_ref, 4);
    const __m128i s = _mm_sad_epu8(_mm_cvtsi32_si128((int)v), zero);
    return (uint32_t)_mm_cvtsi128_si32(s);
  } else if (txwpx == 8) {
    const __m128i s =
        _mm_sad_epu8(_mm_loadl_epi64((const __m128i *)above_ref), zero);
    return (uint32_t)_mm_cvtsi128_si32(s);
  } else if (txwpx == 16) {
    const __m128i v = _mm_loadu_si128((const __m128i *)above_ref);
    const __m128i s = _mm_sad_epu8(v, zero);
    return (uint32_t)_mm_cvtsi128_si32(s) + (uint32_t)_mm_extract_epi16(s, 4);
  } else if (txwpx == 32) {
    const __m128i v0 = _mm_loadu_si128((const __m128i *)above_ref);
    const __m128i v1 = _mm_loadu_si128((const __m128i *)(above_ref + 16));
    const __m128i s =
        _mm_add_epi64(_mm_sad_epu8(v0, zero), _mm_sad_epu8(v1, zero));
    return (uint32_t)_mm_cvtsi128_si32(s) + (uint32_t)_mm_extract_epi16(s, 4);
  } else {
    const __m128i v0 = _mm_loadu_si128((const __m128i *)above_ref);
    const __m128i v1 = _mm_loadu_si128((const __m128i *)(above_ref + 16));
    const __m128i v2 = _mm_loadu_si128((const __m128i *)(above_ref + 32));
    const __m128i v3 = _mm_loadu_si128((const __m128i *)(above_ref + 48));

    const __m128i s01 =
        _mm_add_epi64(_mm_sad_epu8(v0, zero), _mm_sad_epu8(v1, zero));
    const __m128i s23 =
        _mm_add_epi64(_mm_sad_epu8(v2, zero), _mm_sad_epu8(v3, zero));
    const __m128i s = _mm_add_epi64(s01, s23);
    return (uint32_t)_mm_cvtsi128_si32(s) + (uint32_t)_mm_extract_epi16(s, 4);
  }
}

// Computes 8-bit DC_PRED directly from the reference boundary pixels without
// staging above_row and left_col buffers on the stack.
void av1_predict_dc_block_lbd_sse2(const uint8_t *ref, int ref_stride,
                                   uint8_t *dst, int dst_stride,
                                   TX_SIZE tx_size, int txwpx, int txhpx,
                                   int n_top_px, int n_left_px) {
  const uint8_t *above_ref = ref - ref_stride;
  const uint8_t *left_ref = ref - 1;
  uint32_t sum_top = 0;
  if (n_top_px > 0) {
    if (LIKELY(n_top_px == txwpx)) {
      sum_top = sum_above_row_lbd_sse2(above_ref, txwpx);
    } else {
      for (int i = 0; i < n_top_px; ++i) sum_top += above_ref[i];
      sum_top +=
          (uint32_t)above_ref[n_top_px - 1] * (uint32_t)(txwpx - n_top_px);
    }
  }

  uint32_t sum_left = 0;
  if (n_left_px > 0) {
    if (LIKELY(n_left_px == txhpx)) {
      sum_left = av1_sum_left_col_lbd(left_ref, ref_stride, txhpx);
    } else {
      for (int i = 0; i < n_left_px; ++i) sum_left += left_ref[i * ref_stride];
      sum_left += (uint32_t)left_ref[(n_left_px - 1) * ref_stride] *
                  (uint32_t)(txhpx - n_left_px);
    }
  }

  const uint8_t dc_val = av1_dc_val_from_sums_lbd(
      tx_size, txwpx, txhpx, sum_top, sum_left, n_top_px, n_left_px);
  av1_fill_block_lbd(dst, dst_stride, dc_val, txwpx, txhpx);
}
