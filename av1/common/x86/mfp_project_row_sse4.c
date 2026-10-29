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

#include <smmintrin.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_ports/mem.h"
#include "av1/common/mfp_project.h"

// Four MV_REF records occupy 32 bytes: int16 row, int16 col, int8 ref.
static inline void load4(const MV_REF *src, __m128i *rows, __m128i *cols,
                         __m128i *refs) {
  const __m128i lo = _mm_loadu_si128((const __m128i *)src);
  const __m128i hi = _mm_loadu_si128((const __m128i *)(src + 2));
  const __m128i rc_shuf =
      _mm_setr_epi8(-1, -1, 0, 1, -1, -1, 8, 9, -1, -1, 2, 3, -1, -1, 10, 11);
  const __m128i ref_shuf = _mm_setr_epi8(-1, -1, -1, 4, -1, -1, -1, 12, -1, -1,
                                         -1, -1, -1, -1, -1, -1);
  const __m128i packed_lo = _mm_srai_epi32(_mm_shuffle_epi8(lo, rc_shuf), 16);
  const __m128i packed_hi = _mm_srai_epi32(_mm_shuffle_epi8(hi, rc_shuf), 16);
  *rows = _mm_unpacklo_epi64(packed_lo, packed_hi);
  *cols = _mm_unpackhi_epi64(packed_lo, packed_hi);
  const __m128i f_lo = _mm_srai_epi32(_mm_shuffle_epi8(lo, ref_shuf), 24);
  const __m128i f_hi = _mm_srai_epi32(_mm_shuffle_epi8(hi, ref_shuf), 24);
  *refs = _mm_unpacklo_epi64(f_lo, f_hi);
}

// ROUND_POWER_OF_TWO_SIGNED(v, 14). The range note in av1_get_mv_projection()
// keeps |v| + (1 << 13) within int32_t.
static inline __m128i round14(__m128i v) {
  const __m128i neg = _mm_cmpgt_epi32(_mm_setzero_si128(), v);
  const __m128i rounded = _mm_srai_epi32(
      _mm_add_epi32(_mm_abs_epi32(v), _mm_set1_epi32(1 << 13)), 14);
  return _mm_sub_epi32(_mm_xor_si128(rounded, neg), neg);
}

static inline __m128i clamp_proj_mv(__m128i v) {
  v = _mm_max_epi32(v, _mm_set1_epi32(MV_LOW + 1));
  return _mm_min_epi32(v, _mm_set1_epi32(MV_UPP - 1));
}

// Truncate toward zero, matching `v >= 0 ? v >> 6 : -((-v) >> 6)`.
static inline __m128i trunc_div64(__m128i v) {
  const __m128i neg = _mm_cmpgt_epi32(_mm_setzero_si128(), v);
  const __m128i q = _mm_srli_epi32(_mm_abs_epi32(v), 6);
  return _mm_sub_epi32(_mm_xor_si128(q, neg), neg);
}

// lo <= v < hi.
static inline __m128i in_range(__m128i v, int lo, int hi) {
  const __m128i ge = _mm_cmpgt_epi32(v, _mm_set1_epi32(lo - 1));
  const __m128i lt = _mm_cmpgt_epi32(_mm_set1_epi32(hi), v);
  return _mm_and_si128(ge, lt);
}

// blk_col is a multiple of 4, so the four lanes share one 8-column base.
static inline void project_4(const MV_REF *src, TPL_MV_REF *tpl, int tpl_stride,
                             int blk_row, int blk_col, int mi_rows_half,
                             int mi_cols_half, const int *scale, const int *off,
                             const int *ok, int sign_bias) {
  __m128i rows, cols, refs;
  load4(src + blk_col, &rows, &cols, &refs);

  DECLARE_ALIGNED(16, int32_t, rf[4]);
  DECLARE_ALIGNED(16, int32_t, sc_a[4]);
  DECLARE_ALIGNED(16, int32_t, off_a[4]);
  DECLARE_ALIGNED(16, int32_t, ok_a[4]);
  _mm_store_si128((__m128i *)rf, refs);
  for (int i = 0; i < 4; ++i) {
    const int r = rf[i];
    const int valid = r > INTRA_FRAME && r < REF_FRAMES && ok[r];
    sc_a[i] = valid ? scale[r] : 0;
    off_a[i] = valid ? off[r] : 0;
    ok_a[i] = valid ? -1 : 0;
  }
  const __m128i sc = _mm_load_si128((const __m128i *)sc_a);
  const __m128i proj_r = clamp_proj_mv(round14(_mm_mullo_epi32(rows, sc)));
  const __m128i proj_c = clamp_proj_mv(round14(_mm_mullo_epi32(cols, sc)));
  const __m128i row_off = trunc_div64(proj_r);
  const __m128i col_off = trunc_div64(proj_c);
  const __m128i col_ids =
      _mm_add_epi32(_mm_set1_epi32(blk_col), _mm_setr_epi32(0, 1, 2, 3));
  const __m128i dst_r = (sign_bias == 1)
                            ? _mm_sub_epi32(_mm_set1_epi32(blk_row), row_off)
                            : _mm_add_epi32(_mm_set1_epi32(blk_row), row_off);
  const __m128i dst_c = (sign_bias == 1) ? _mm_sub_epi32(col_ids, col_off)
                                         : _mm_add_epi32(col_ids, col_off);

  const int base_blk_row = (blk_row >> 3) << 3;
  const int base_blk_col = (blk_col >> 3) << 3;
  const int row_lo = base_blk_row - (MAX_OFFSET_HEIGHT >> 3);
  const int row_hi = base_blk_row + 8 + (MAX_OFFSET_HEIGHT >> 3);
  const int col_lo = base_blk_col - (MAX_OFFSET_WIDTH >> 3);
  const int col_hi = base_blk_col + 8 + (MAX_OFFSET_WIDTH >> 3);
  __m128i mask = in_range(dst_r, 0, mi_rows_half);
  mask = _mm_and_si128(mask, in_range(dst_c, 0, mi_cols_half));
  mask = _mm_and_si128(mask, in_range(dst_r, row_lo, row_hi));
  mask = _mm_and_si128(mask, in_range(dst_c, col_lo, col_hi));
  mask = _mm_and_si128(mask, _mm_load_si128((const __m128i *)ok_a));
  if (!_mm_movemask_epi8(mask)) return;

  DECLARE_ALIGNED(16, int32_t, dr[4]);
  DECLARE_ALIGNED(16, int32_t, dc[4]);
  DECLARE_ALIGNED(16, int32_t, mm[4]);
  DECLARE_ALIGNED(16, int32_t, orig_r[4]);
  DECLARE_ALIGNED(16, int32_t, orig_c[4]);
  _mm_store_si128((__m128i *)dr, dst_r);
  _mm_store_si128((__m128i *)dc, dst_c);
  _mm_store_si128((__m128i *)mm, mask);
  _mm_store_si128((__m128i *)orig_r, rows);
  _mm_store_si128((__m128i *)orig_c, cols);
  // Lanes are stored in column order so later columns win, as in the C code.
  for (int i = 0; i < 4; ++i) {
    if (!mm[i]) continue;
    av1_mfp_store(tpl, tpl_stride, dr[i], dc[i], orig_r[i], orig_c[i],
                  off_a[i]);
  }
}

void av1_mfp_project_row_sse4_1(const MV_REF *src_row, TPL_MV_REF *tpl_mvs,
                                int tpl_stride, int blk_row, int mvs_cols,
                                int mi_rows_half, int mi_cols_half,
                                const int *ref_offset, int cur_offset,
                                int dir) {
  if (abs(cur_offset) > MAX_FRAME_DISTANCE) return;
  int scale[REF_FRAMES], off[REF_FRAMES], ok[REF_FRAMES];
  av1_mfp_prepare_scale(ref_offset, cur_offset, scale, off, ok);
  const int sign_bias = dir >> 1;
  int blk_col = 0;
  for (; blk_col + 4 <= mvs_cols; blk_col += 4) {
    project_4(src_row, tpl_mvs, tpl_stride, blk_row, blk_col, mi_rows_half,
              mi_cols_half, scale, off, ok, sign_bias);
  }
  for (; blk_col < mvs_cols; ++blk_col) {
    av1_mfp_project_one(src_row, tpl_mvs, tpl_stride, blk_row, blk_col,
                        mi_rows_half, mi_cols_half, ref_offset, cur_offset,
                        dir);
  }
}
