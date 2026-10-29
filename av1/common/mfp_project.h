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

#ifndef AOM_AV1_COMMON_MFP_PROJECT_H_
#define AOM_AV1_COMMON_MFP_PROJECT_H_

#include <stddef.h>
#include <stdlib.h>

#include "av1/common/mvref_common.h"

// (1 << 14) / den for den in 0..MAX_FRAME_DISTANCE. Defined in mvref_common.c.
extern const int av1_mfp_div_mult[32];

// get_block_position() window. Height is 0, so a projected MV must stay in
// the same 8-row band. Width is 64 pixels (8 blocks): columns
// [base_blk_col - 8, base_blk_col + 16).
#define MAX_OFFSET_WIDTH 64
#define MAX_OFFSET_HEIGHT 0

// Same arithmetic as the former static get_block_position(). mi_rows_half and
// mi_cols_half are cm->mi_params.mi_rows/cols >> 1.
static inline int av1_mfp_block_position(int mi_rows_half, int mi_cols_half,
                                         int *mi_r, int *mi_c, int blk_row,
                                         int blk_col, MV mv, int sign_bias) {
  const int base_blk_row = (blk_row >> 3) << 3;
  const int base_blk_col = (blk_col >> 3) << 3;

  const int row_offset = (mv.row >= 0) ? (mv.row >> (4 + MI_SIZE_LOG2))
                                       : -((-mv.row) >> (4 + MI_SIZE_LOG2));

  const int col_offset = (mv.col >= 0) ? (mv.col >> (4 + MI_SIZE_LOG2))
                                       : -((-mv.col) >> (4 + MI_SIZE_LOG2));

  const int row =
      (sign_bias == 1) ? blk_row - row_offset : blk_row + row_offset;
  const int col =
      (sign_bias == 1) ? blk_col - col_offset : blk_col + col_offset;

  if (row < 0 || row >= mi_rows_half || col < 0 || col >= mi_cols_half)
    return 0;

  if (row < base_blk_row - (MAX_OFFSET_HEIGHT >> 3) ||
      row >= base_blk_row + 8 + (MAX_OFFSET_HEIGHT >> 3) ||
      col < base_blk_col - (MAX_OFFSET_WIDTH >> 3) ||
      col >= base_blk_col + 8 + (MAX_OFFSET_WIDTH >> 3))
    return 0;

  *mi_r = row;
  *mi_c = col;
  return 1;
}

// One 8x8 source MV. Identical control flow to the old blk_col loop body.
// Later columns overwrite earlier ones, so callers must walk blk_col upward.
static inline void av1_mfp_project_one(const MV_REF *src_row, TPL_MV_REF *tpl,
                                       int tpl_stride, int blk_row, int blk_col,
                                       int mi_rows_half, int mi_cols_half,
                                       const int *ref_offset, int cur_offset,
                                       int dir) {
  const MV_REF *mv_ref = &src_row[blk_col];
  const MV fwd_mv = mv_ref->mv.as_mv;

  if (mv_ref->ref_frame > INTRA_FRAME) {
    int_mv this_mv;
    int mi_r, mi_c;
    const int ref_frame_offset = ref_offset[mv_ref->ref_frame];

    int pos_valid = abs(ref_frame_offset) <= MAX_FRAME_DISTANCE &&
                    ref_frame_offset > 0 &&
                    abs(cur_offset) <= MAX_FRAME_DISTANCE;

    if (pos_valid) {
      av1_get_mv_projection(&this_mv.as_mv, fwd_mv, cur_offset,
                            ref_frame_offset);
      pos_valid =
          av1_mfp_block_position(mi_rows_half, mi_cols_half, &mi_r, &mi_c,
                                 blk_row, blk_col, this_mv.as_mv, dir >> 1);
    }

    if (pos_valid) {
      const int mi_offset = mi_r * tpl_stride + mi_c;
      tpl[mi_offset].mfmv0.as_mv.row = fwd_mv.row;
      tpl[mi_offset].mfmv0.as_mv.col = fwd_mv.col;
      tpl[mi_offset].ref_frame_offset = ref_frame_offset;
    }
  }
}

// Per-reference scale used by the SIMD kernels. Matches av1_get_mv_projection
// for every ref the scalar path will actually project (dist in 1..31 and
// |cur_offset| <= MAX_FRAME_DISTANCE). Invalid refs get scale 0.
static inline void av1_mfp_prepare_scale(const int *ref_offset, int cur_offset,
                                         int scale[REF_FRAMES],
                                         int off[REF_FRAMES],
                                         int ok[REF_FRAMES]) {
  const int num = cur_offset > 0 ? AOMMIN(cur_offset, MAX_FRAME_DISTANCE)
                                 : AOMMAX(cur_offset, -MAX_FRAME_DISTANCE);
  for (int rf = 0; rf < REF_FRAMES; ++rf) {
    const int dist = ref_offset[rf];
    const int valid =
        rf > INTRA_FRAME && dist > 0 && abs(dist) <= MAX_FRAME_DISTANCE;
    const int den = AOMMIN(dist > 0 ? dist : 0, MAX_FRAME_DISTANCE);
    ok[rf] = valid;
    off[rf] = dist;
    scale[rf] = valid ? num * av1_mfp_div_mult[den] : 0;
  }
}

// The SIMD kernels load MV_REF as 8-byte records: int16 row, int16 col, and
// the int8 ref_frame at byte 4.
typedef char av1_mfp_mv_ref_layout_check[(sizeof(MV_REF) == 8 &&
                                          offsetof(MV_REF, ref_frame) == 4 &&
                                          REF_FRAMES == 8 && sizeof(int) == 4)
                                             ? 1
                                             : -1];

static inline void av1_mfp_store(TPL_MV_REF *tpl, int tpl_stride, int mi_r,
                                 int mi_c, int row, int col, int ref_off) {
  const int mi_offset = mi_r * tpl_stride + mi_c;
  tpl[mi_offset].mfmv0.as_mv.row = (int16_t)row;
  tpl[mi_offset].mfmv0.as_mv.col = (int16_t)col;
  tpl[mi_offset].ref_frame_offset = (uint8_t)ref_off;
}

#endif  // AOM_AV1_COMMON_MFP_PROJECT_H_
