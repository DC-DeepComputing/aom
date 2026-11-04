/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>

#include "av1/common/av1_common_int.h"
#include "av1/common/blockd.h"

PREDICTION_MODE av1_left_block_mode(const MB_MODE_INFO *left_mi) {
  if (!left_mi) return DC_PRED;
  assert(!is_inter_block(left_mi) || is_intrabc_block(left_mi));
  return left_mi->mode;
}

PREDICTION_MODE av1_above_block_mode(const MB_MODE_INFO *above_mi) {
  if (!above_mi) return DC_PRED;
  assert(!is_inter_block(above_mi) || is_intrabc_block(above_mi));
  return above_mi->mode;
}

void av1_reset_entropy_context(MACROBLOCKD *xd, BLOCK_SIZE bsize,
                               const int num_planes) {
  assert(bsize < BLOCK_SIZES_ALL);
  av1_fill_entropy_ctx_pow2(xd->plane[0].above_entropy_context, 0,
                            mi_size_wide[bsize]);
  av1_fill_entropy_ctx_pow2(xd->plane[0].left_entropy_context, 0,
                            mi_size_high[bsize]);
  if (xd->is_chroma_ref && num_planes > 1) {
    const struct macroblockd_plane *const pd1 = &xd->plane[1];
    const BLOCK_SIZE plane_bsize =
        get_plane_block_size(bsize, pd1->subsampling_x, pd1->subsampling_y);
    const int txs_wide = mi_size_wide[plane_bsize];
    const int txs_high = mi_size_high[plane_bsize];
    ENTROPY_CONTEXT *const a1 = pd1->above_entropy_context;
    ENTROPY_CONTEXT *const a2 = xd->plane[2].above_entropy_context;
    if (txs_wide <= 2) {
      a1[0] = 0;
      a1[txs_wide - 1] = 0;
      a2[0] = 0;
      a2[txs_wide - 1] = 0;
    } else if (txs_wide == 4) {
      const uint32_t z4 = 0;
      memcpy(a1, &z4, 4);
      memcpy(a2, &z4, 4);
    } else if (txs_wide <= 16) {
      const uint64_t z8 = 0;
      memcpy(a1, &z8, 8);
      memcpy(a1 + (txs_wide - 8), &z8, 8);
      memcpy(a2, &z8, 8);
      memcpy(a2 + (txs_wide - 8), &z8, 8);
    } else {
      memset(a1, 0, txs_wide * sizeof(*a1));
      memset(a2, 0, txs_wide * sizeof(*a2));
    }
    ENTROPY_CONTEXT *const l1 = pd1->left_entropy_context;
    ENTROPY_CONTEXT *const l2 = xd->plane[2].left_entropy_context;
    if (txs_high <= 2) {
      l1[0] = 0;
      l1[txs_high - 1] = 0;
      l2[0] = 0;
      l2[txs_high - 1] = 0;
    } else if (txs_high == 4) {
      const uint32_t z4 = 0;
      memcpy(l1, &z4, 4);
      memcpy(l2, &z4, 4);
    } else if (txs_high <= 16) {
      const uint64_t z8 = 0;
      memcpy(l1, &z8, 8);
      memcpy(l1 + (txs_high - 8), &z8, 8);
      memcpy(l2, &z8, 8);
      memcpy(l2 + (txs_high - 8), &z8, 8);
    } else {
      memset(l1, 0, txs_high * sizeof(*l1));
      memset(l2, 0, txs_high * sizeof(*l2));
    }
  }
}

void av1_reset_loop_filter_delta(MACROBLOCKD *xd, int num_planes) {
  xd->delta_lf_from_base = 0;
  const int frame_lf_count =
      num_planes > 1 ? FRAME_LF_COUNT : FRAME_LF_COUNT - 2;
  for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id) xd->delta_lf[lf_id] = 0;
}

void av1_reset_loop_restoration(MACROBLOCKD *xd, const int num_planes) {
  for (int p = 0; p < num_planes; ++p) {
    set_default_wiener(xd->wiener_info + p);
    set_default_sgrproj(xd->sgrproj_info + p);
  }
}

void av1_setup_block_planes(MACROBLOCKD *xd, int ss_x, int ss_y,
                            const int num_planes) {
  int i;

  for (i = 0; i < num_planes; i++) {
    xd->plane[i].plane_type = get_plane_type(i);
    xd->plane[i].subsampling_x = i ? ss_x : 0;
    xd->plane[i].subsampling_y = i ? ss_y : 0;
  }
  for (i = num_planes; i < MAX_MB_PLANE; i++) {
    xd->plane[i].subsampling_x = 1;
    xd->plane[i].subsampling_y = 1;
  }
}
