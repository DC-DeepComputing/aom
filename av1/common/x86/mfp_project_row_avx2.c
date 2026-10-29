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

#include <immintrin.h>

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

static inline void load8(const MV_REF *src, __m256i *rows, __m256i *cols,
                         __m256i *refs) {
  __m128i r0, c0, f0, r1, c1, f1;
  load4(src, &r0, &c0, &f0);
  load4(src + 4, &r1, &c1, &f1);
  *rows = _mm256_insertf128_si256(_mm256_castsi128_si256(r0), r1, 1);
  *cols = _mm256_insertf128_si256(_mm256_castsi128_si256(c0), c1, 1);
  *refs = _mm256_insertf128_si256(_mm256_castsi128_si256(f0), f1, 1);
}

// ROUND_POWER_OF_TWO_SIGNED(v, 14). The range note in av1_get_mv_projection()
// keeps |v| + (1 << 13) within int32_t.
static inline __m256i round14(__m256i v) {
  const __m256i neg = _mm256_cmpgt_epi32(_mm256_setzero_si256(), v);
  const __m256i rounded = _mm256_srai_epi32(
      _mm256_add_epi32(_mm256_abs_epi32(v), _mm256_set1_epi32(1 << 13)), 14);
  return _mm256_sub_epi32(_mm256_xor_si256(rounded, neg), neg);
}

static inline __m256i clamp_proj_mv(__m256i v) {
  v = _mm256_max_epi32(v, _mm256_set1_epi32(MV_LOW + 1));
  return _mm256_min_epi32(v, _mm256_set1_epi32(MV_UPP - 1));
}

// Truncate toward zero, matching `v >= 0 ? v >> 6 : -((-v) >> 6)`.
static inline __m256i trunc_div64(__m256i v) {
  const __m256i neg = _mm256_cmpgt_epi32(_mm256_setzero_si256(), v);
  const __m256i q = _mm256_srli_epi32(_mm256_abs_epi32(v), 6);
  return _mm256_sub_epi32(_mm256_xor_si256(q, neg), neg);
}

// lo <= v < hi.
static inline __m256i in_range(__m256i v, int lo, int hi) {
  const __m256i ge = _mm256_cmpgt_epi32(v, _mm256_set1_epi32(lo - 1));
  const __m256i lt = _mm256_cmpgt_epi32(_mm256_set1_epi32(hi), v);
  return _mm256_and_si256(ge, lt);
}

// blk_col is a multiple of 8, so the eight lanes share one 8-column base.
static inline void project_8(const MV_REF *src, TPL_MV_REF *tpl, int tpl_stride,
                             int blk_row, int blk_col, int mi_rows_half,
                             int mi_cols_half, const int *scale, const int *off,
                             const int *ok, int sign_bias) {
  __m256i rows, cols, refs;
  load8(src + blk_col, &rows, &cols, &refs);
  const __m256i idx = _mm256_and_si256(refs, _mm256_set1_epi32(7));
  const __m256i ref_valid =
      _mm256_and_si256(_mm256_cmpgt_epi32(refs, _mm256_setzero_si256()),
                       _mm256_cmpgt_epi32(_mm256_set1_epi32(REF_FRAMES), refs));
  const __m256i sc = _mm256_permutevar8x32_epi32(
      _mm256_loadu_si256((const __m256i *)scale), idx);
  const __m256i okp =
      _mm256_permutevar8x32_epi32(_mm256_loadu_si256((const __m256i *)ok), idx);
  const __m256i offs = _mm256_permutevar8x32_epi32(
      _mm256_loadu_si256((const __m256i *)off), idx);
  const __m256i valid = _mm256_and_si256(
      ref_valid, _mm256_cmpgt_epi32(okp, _mm256_setzero_si256()));

  const __m256i proj_r = clamp_proj_mv(round14(_mm256_mullo_epi32(rows, sc)));
  const __m256i proj_c = clamp_proj_mv(round14(_mm256_mullo_epi32(cols, sc)));
  const __m256i row_off = trunc_div64(proj_r);
  const __m256i col_off = trunc_div64(proj_c);
  const __m256i col_ids = _mm256_add_epi32(
      _mm256_set1_epi32(blk_col), _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7));
  const __m256i dst_r =
      (sign_bias == 1) ? _mm256_sub_epi32(_mm256_set1_epi32(blk_row), row_off)
                       : _mm256_add_epi32(_mm256_set1_epi32(blk_row), row_off);
  const __m256i dst_c = (sign_bias == 1) ? _mm256_sub_epi32(col_ids, col_off)
                                         : _mm256_add_epi32(col_ids, col_off);

  const int base_blk_row = (blk_row >> 3) << 3;
  const int base_blk_col = (blk_col >> 3) << 3;
  const int row_lo = base_blk_row - (MAX_OFFSET_HEIGHT >> 3);
  const int row_hi = base_blk_row + 8 + (MAX_OFFSET_HEIGHT >> 3);
  const int col_lo = base_blk_col - (MAX_OFFSET_WIDTH >> 3);
  const int col_hi = base_blk_col + 8 + (MAX_OFFSET_WIDTH >> 3);
  __m256i mask = in_range(dst_r, 0, mi_rows_half);
  mask = _mm256_and_si256(mask, in_range(dst_c, 0, mi_cols_half));
  mask = _mm256_and_si256(mask, in_range(dst_r, row_lo, row_hi));
  mask = _mm256_and_si256(mask, in_range(dst_c, col_lo, col_hi));
  mask = _mm256_and_si256(mask, valid);
  if (!_mm256_movemask_ps(_mm256_castsi256_ps(mask))) return;

  DECLARE_ALIGNED(32, int32_t, dr[8]);
  DECLARE_ALIGNED(32, int32_t, dc[8]);
  DECLARE_ALIGNED(32, int32_t, mm[8]);
  DECLARE_ALIGNED(32, int32_t, orig_r[8]);
  DECLARE_ALIGNED(32, int32_t, orig_c[8]);
  DECLARE_ALIGNED(32, int32_t, off_a[8]);
  _mm256_store_si256((__m256i *)dr, dst_r);
  _mm256_store_si256((__m256i *)dc, dst_c);
  _mm256_store_si256((__m256i *)mm, mask);
  _mm256_store_si256((__m256i *)orig_r, rows);
  _mm256_store_si256((__m256i *)orig_c, cols);
  _mm256_store_si256((__m256i *)off_a, offs);
  // Lanes are stored in column order so later columns win, as in the C code.
  for (int i = 0; i < 8; ++i) {
    if (!mm[i]) continue;
    av1_mfp_store(tpl, tpl_stride, dr[i], dc[i], orig_r[i], orig_c[i],
                  off_a[i]);
  }
}

void av1_mfp_project_row_avx2(const MV_REF *src_row, TPL_MV_REF *tpl_mvs,
                              int tpl_stride, int blk_row, int mvs_cols,
                              int mi_rows_half, int mi_cols_half,
                              const int *ref_offset, int cur_offset, int dir) {
  if (abs(cur_offset) > MAX_FRAME_DISTANCE) return;
  int scale[REF_FRAMES], off[REF_FRAMES], ok[REF_FRAMES];
  av1_mfp_prepare_scale(ref_offset, cur_offset, scale, off, ok);
  const int sign_bias = dir >> 1;
  int blk_col = 0;
  for (; blk_col + 8 <= mvs_cols; blk_col += 8) {
    project_8(src_row, tpl_mvs, tpl_stride, blk_row, blk_col, mi_rows_half,
              mi_cols_half, scale, off, ok, sign_bias);
  }
  for (; blk_col < mvs_cols; ++blk_col) {
    av1_mfp_project_one(src_row, tpl_mvs, tpl_stride, blk_row, blk_col,
                        mi_rows_half, mi_cols_half, ref_offset, cur_offset,
                        dir);
  }
}
