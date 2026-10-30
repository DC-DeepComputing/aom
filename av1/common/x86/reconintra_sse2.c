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

#include "aom_ports/mem.h"
#include "av1/common/blockd.h"
#include "av1/common/common_data.h"
#include "av1/common/enums.h"

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

static AOM_FORCE_INLINE uint32_t sum_left_col_lbd(const uint8_t *left_ref,
                                                  int ref_stride, int txhpx) {
  const int s2 = ref_stride * 2;
  const int s3 = ref_stride * 3;
  const int s4 = ref_stride * 4;
  uint32_t sum0 = 0, sum1 = 0;
  for (int i = 0; i < txhpx; i += 4) {
    sum0 += (uint32_t)left_ref[0] + (uint32_t)left_ref[ref_stride];
    sum1 += (uint32_t)left_ref[s2] + (uint32_t)left_ref[s3];
    left_ref += s4;
  }
  return sum0 + sum1;
}

static AOM_FORCE_INLINE void fill_block_lbd(uint8_t *dst, int dst_stride,
                                            uint8_t val, int txwpx, int txhpx) {
  if (txwpx == 4) {
    for (int i = 0; i < txhpx; ++i) {
      memset(dst, val, 4);
      dst += dst_stride;
    }
  } else if (txwpx == 8) {
    for (int i = 0; i < txhpx; ++i) {
      memset(dst, val, 8);
      dst += dst_stride;
    }
  } else if (txwpx == 16) {
    for (int i = 0; i < txhpx; ++i) {
      memset(dst, val, 16);
      dst += dst_stride;
    }
  } else if (txwpx == 32) {
    for (int i = 0; i < txhpx; ++i) {
      memset(dst, val, 32);
      dst += dst_stride;
    }
  } else {
    for (int i = 0; i < txhpx; ++i) {
      memset(dst, val, 64);
      dst += dst_stride;
    }
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
      sum_left = sum_left_col_lbd(left_ref, ref_stride, txhpx);
    } else {
      for (int i = 0; i < n_left_px; ++i) sum_left += left_ref[i * ref_stride];
      sum_left += (uint32_t)left_ref[(n_left_px - 1) * ref_stride] *
                  (uint32_t)(txhpx - n_left_px);
    }
  }

  uint8_t dc_val;
  if (n_top_px > 0 && n_left_px > 0) {
    // Reciprocal multiplication tables to divide by (txwpx + txhpx) for square,
    // 1:2 / 2:1, and 1:4 / 4:1 transform blocks.
    static const uint8_t dc_shift1_tab[TX_SIZES_ALL] = {
      2, 3, 4, 5, 6,  // 4x4, 8x8, 16x16, 32x32, 64x64
      2, 2, 3, 3, 4, 4,
      5, 5,             // 4x8, 8x4, 8x16, 16x8, 16x32, 32x16, 32x64, 64x32
      2, 2, 3, 3, 4, 4  // 4x16, 16x4, 8x32, 32x8, 16x64, 64x16
    };
    static const uint16_t dc_mult_tab[TX_SIZES_ALL] = {
      0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x5556, 0x5556,
      0x5556, 0x5556, 0x5556, 0x5556, 0x5556, 0x5556, 0x3334,
      0x3334, 0x3334, 0x3334, 0x3334, 0x3334
    };
    const uint32_t total =
        sum_top + sum_left + (uint32_t)((txwpx + txhpx) >> 1);
    dc_val =
        (uint8_t)(((total >> dc_shift1_tab[tx_size]) * dc_mult_tab[tx_size]) >>
                  16);
  } else if (n_top_px > 0) {
    const int log2_w = tx_size_wide_log2[tx_size];
    dc_val = (uint8_t)((sum_top + (uint32_t)(txwpx >> 1)) >> log2_w);
  } else if (n_left_px > 0) {
    const int log2_h = tx_size_high_log2[tx_size];
    dc_val = (uint8_t)((sum_left + (uint32_t)(txhpx >> 1)) >> log2_h);
  } else {
    dc_val = 128;
  }
  fill_block_lbd(dst, dst_stride, dc_val, txwpx, txhpx);
}
