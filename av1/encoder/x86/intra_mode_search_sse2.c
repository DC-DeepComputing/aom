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

#include <assert.h>
#include <emmintrin.h>
#include <stdlib.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_ports/bitops.h"
#include "av1/encoder/intra_mode_search_utils.h"

#define BINS 32
#define FIX_PREC_BITS (16)

static inline int get_hist_bin_idx_sse2(int dx, int dy) {
  if (dy == 0) return 16;
  const int32_t ratio = (dy * (1 << FIX_PREC_BITS)) / dx;

  DECLARE_ALIGNED(64, static const int32_t, thresholds[BINS]) = {
    -1334015, -441798, -261605, -183158, -138560, -109331, -88359, -72303,
    -59392,   -48579,  -39272,  -30982,  -23445,  -16400,  -9715,  -3194,
    3227,     9748,    16433,   23478,   31015,   39305,   48611,  59425,
    72336,    88392,   109364,  138593,  183191,  261638,  441831, INT32_MAX
  };

  // Select the lower (0..15) or upper (16..31) half of thresholds, then
  // compare all 16 bins in parallel using SSE2.
  const int lo_idx = (ratio > -3194) << 4;
  const __m128i ratio_vec = _mm_set1_epi32(ratio);
  const __m128i c0 = _mm_cmpgt_epi32(
      ratio_vec, _mm_load_si128((const __m128i *)&thresholds[lo_idx]));
  const __m128i c1 = _mm_cmpgt_epi32(
      ratio_vec, _mm_load_si128((const __m128i *)&thresholds[lo_idx + 4]));
  const __m128i c2 = _mm_cmpgt_epi32(
      ratio_vec, _mm_load_si128((const __m128i *)&thresholds[lo_idx + 8]));
  const __m128i c3 = _mm_cmpgt_epi32(
      ratio_vec, _mm_load_si128((const __m128i *)&thresholds[lo_idx + 12]));

  // Pack 32-bit comparison masks down to 8-bit lanes and find the first bin
  // where ratio <= thresholds[idx].
  const __m128i p01 = _mm_packs_epi32(c0, c1);
  const __m128i p23 = _mm_packs_epi32(c2, c3);
  const __m128i p0123 = _mm_packs_epi16(p01, p23);
  const uint32_t inv_mask = ~(uint32_t)_mm_movemask_epi8(p0123);
  return lo_idx + get_msb(inv_mask ^ (inv_mask - 1));
}

#undef FIX_PREC_BITS

void av1_lowbd_generate_hog_sse2(const uint8_t *src, int stride, int rows,
                                 int cols, float *hist) {
  float total = 0.1f;
  src += stride;
  for (int r = 1; r < rows - 1; ++r) {
    const uint8_t *row_above = src - stride;
    const uint8_t *row_cur = src;
    const uint8_t *row_below = src + stride;

    // Prime the 3x3 sliding Sobel window with columns 0 and 1.
    int v0 = row_above[0] + 2 * row_cur[0] + row_below[0];
    int v1 = row_above[1] + 2 * row_cur[1] + row_below[1];
    int d0 = row_below[0] - row_above[0];
    int d1 = row_below[1] - row_above[1];

    for (int c = 1; c < cols - 1; ++c) {
      // Load the incoming right column (c + 1) and update Sobel gradients.
      const int above_r = row_above[c + 1];
      const int cur_r = row_cur[c + 1];
      const int below_r = row_below[c + 1];
      const int v2 = above_r + 2 * cur_r + below_r;
      const int d2 = below_r - above_r;
      const int dx = v2 - v0;
      const int dy = d0 + 2 * d1 + d2;
      v0 = v1;
      v1 = v2;
      d0 = d1;
      d1 = d2;

      if ((dx | dy) == 0) continue;
      const int temp = abs(dx) + abs(dy);
      total += temp;
      if (dx == 0) {
        hist[0] += temp / 2;
        hist[BINS - 1] += temp / 2;
      } else {
        const int idx = get_hist_bin_idx_sse2(dx, dy);
        assert(idx >= 0 && idx < BINS);
        hist[idx] += temp;
      }
    }
    src += stride;
  }

  normalize_hog(total, hist);
}

void av1_lowbd_compute_gradient_info_sb_sse2(MACROBLOCK *const x,
                                             BLOCK_SIZE sb_size,
                                             PLANE_TYPE plane) {
  PixelLevelGradientInfo *const grad_info_sb =
      x->pixel_gradient_info + plane * MAX_SB_SQUARE;
  const uint8_t *src = x->plane[plane].src.buf;
  const int stride = x->plane[plane].src.stride;
  const int ss_x = x->e_mbd.plane[plane].subsampling_x;
  const int ss_y = x->e_mbd.plane[plane].subsampling_y;
  const int sb_height = block_size_high[sb_size] >> ss_y;
  const int sb_width = block_size_wide[sb_size] >> ss_x;
  src += stride;
  for (int r = 1; r < sb_height - 1; ++r) {
    const uint8_t *row_above = src - stride;
    const uint8_t *row_cur = src;
    const uint8_t *row_below = src + stride;

    // Prime the 3x3 sliding Sobel window with columns 0 and 1.
    int v0 = row_above[0] + 2 * row_cur[0] + row_below[0];
    int v1 = row_above[1] + 2 * row_cur[1] + row_below[1];
    int d0 = row_below[0] - row_above[0];
    int d1 = row_below[1] - row_above[1];
    PixelLevelGradientInfo *const row_info = grad_info_sb + r * sb_width;

    for (int c = 1; c < sb_width - 1; ++c) {
      // Load the incoming right column (c + 1) and update Sobel gradients.
      const int above_r = row_above[c + 1];
      const int cur_r = row_cur[c + 1];
      const int below_r = row_below[c + 1];
      const int v2 = above_r + 2 * cur_r + below_r;
      const int d2 = below_r - above_r;
      const int dx = v2 - v0;
      const int dy = d0 + 2 * d1 + d2;
      v0 = v1;
      v1 = v2;
      d0 = d1;
      d1 = d2;

      row_info[c].abs_dx_abs_dy_sum = (uint16_t)(abs(dx) + abs(dy));
      row_info[c].hist_bin_idx =
          (dx == 0) ? BINS : (int8_t)get_hist_bin_idx_sse2(dx, dy);
    }
    src += stride;
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
void av1_highbd_generate_hog_sse2(const uint8_t *src8, int stride, int rows,
                                  int cols, float *hist) {
  float total = 0.1f;
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  src += stride;
  for (int r = 1; r < rows - 1; ++r) {
    const uint16_t *row_above = src - stride;
    const uint16_t *row_cur = src;
    const uint16_t *row_below = src + stride;

    // Prime the 3x3 sliding Sobel window with columns 0 and 1.
    int v0 = row_above[0] + 2 * row_cur[0] + row_below[0];
    int v1 = row_above[1] + 2 * row_cur[1] + row_below[1];
    int d0 = row_below[0] - row_above[0];
    int d1 = row_below[1] - row_above[1];

    for (int c = 1; c < cols - 1; ++c) {
      // Load the incoming right column (c + 1) and update Sobel gradients.
      const int above_r = row_above[c + 1];
      const int cur_r = row_cur[c + 1];
      const int below_r = row_below[c + 1];
      const int v2 = above_r + 2 * cur_r + below_r;
      const int d2 = below_r - above_r;
      const int dx = v2 - v0;
      const int dy = d0 + 2 * d1 + d2;
      v0 = v1;
      v1 = v2;
      d0 = d1;
      d1 = d2;

      if ((dx | dy) == 0) continue;
      const int temp = abs(dx) + abs(dy);
      total += temp;
      if (dx == 0) {
        hist[0] += temp / 2;
        hist[BINS - 1] += temp / 2;
      } else {
        const int idx = get_hist_bin_idx_sse2(dx, dy);
        assert(idx >= 0 && idx < BINS);
        hist[idx] += temp;
      }
    }
    src += stride;
  }

  normalize_hog(total, hist);
}

void av1_highbd_compute_gradient_info_sb_sse2(MACROBLOCK *const x,
                                              BLOCK_SIZE sb_size,
                                              PLANE_TYPE plane) {
  PixelLevelGradientInfo *const grad_info_sb =
      x->pixel_gradient_info + plane * MAX_SB_SQUARE;
  const uint16_t *src = CONVERT_TO_SHORTPTR(x->plane[plane].src.buf);
  const int stride = x->plane[plane].src.stride;
  const int ss_x = x->e_mbd.plane[plane].subsampling_x;
  const int ss_y = x->e_mbd.plane[plane].subsampling_y;
  const int sb_height = block_size_high[sb_size] >> ss_y;
  const int sb_width = block_size_wide[sb_size] >> ss_x;
  src += stride;
  for (int r = 1; r < sb_height - 1; ++r) {
    const uint16_t *row_above = src - stride;
    const uint16_t *row_cur = src;
    const uint16_t *row_below = src + stride;

    // Prime the 3x3 sliding Sobel window with columns 0 and 1.
    int v0 = row_above[0] + 2 * row_cur[0] + row_below[0];
    int v1 = row_above[1] + 2 * row_cur[1] + row_below[1];
    int d0 = row_below[0] - row_above[0];
    int d1 = row_below[1] - row_above[1];
    PixelLevelGradientInfo *const row_info = grad_info_sb + r * sb_width;

    for (int c = 1; c < sb_width - 1; ++c) {
      // Load the incoming right column (c + 1) and update Sobel gradients.
      const int above_r = row_above[c + 1];
      const int cur_r = row_cur[c + 1];
      const int below_r = row_below[c + 1];
      const int v2 = above_r + 2 * cur_r + below_r;
      const int d2 = below_r - above_r;
      const int dx = v2 - v0;
      const int dy = d0 + 2 * d1 + d2;
      v0 = v1;
      v1 = v2;
      d0 = d1;
      d1 = d2;

      row_info[c].abs_dx_abs_dy_sum = (uint16_t)(abs(dx) + abs(dy));
      row_info[c].hist_bin_idx =
          (dx == 0) ? BINS : (int8_t)get_hist_bin_idx_sse2(dx, dy);
    }
    src += stride;
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
#undef BINS
