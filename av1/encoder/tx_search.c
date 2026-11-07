/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <inttypes.h>

#include "config/aom_config.h"

#if HAVE_SSE2 && (defined(__SSE2__) || defined(_M_X64) || \
                  (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#include <emmintrin.h>
#define TX_SEARCH_SSE2 1
#else
#define TX_SEARCH_SSE2 0
#endif

#include "av1/common/av1_inv_txfm2d.h"
#include "av1/common/cfl.h"
#include "av1/common/reconintra.h"
#include "av1/encoder/block.h"
#include "av1/encoder/hybrid_fwd_txfm.h"
#include "av1/common/idct.h"
#include "av1/encoder/model_rd.h"
#include "av1/encoder/random.h"
#include "av1/encoder/rdopt_utils.h"
#include "av1/encoder/sorting_network.h"
#include "av1/encoder/tx_prune_model_weights.h"
#include "av1/encoder/tx_search.h"
#include "av1/encoder/txb_rdopt.h"

#define PROB_THRESH_OFFSET_TX_TYPE 100

struct rdcost_block_args {
  const AV1_COMP *cpi;
  MACROBLOCK *x;
  ENTROPY_CONTEXT t_above[MAX_MIB_SIZE];
  ENTROPY_CONTEXT t_left[MAX_MIB_SIZE];
  RD_STATS *rd_stats;
  int64_t current_rd;
  int64_t best_rd;
  int exit_early;
  int incomplete_exit;
  FAST_TX_SEARCH_MODE ftxs_mode;
  int skip_trellis;
};

typedef struct {
  int64_t rd;
  int txb_entropy_ctx;
  TX_TYPE tx_type;
} TxCandidateInfo;

// origin_threshold * 128 / 100
static const uint32_t skip_pred_threshold[3][BLOCK_SIZES_ALL] = {
  {
      64, 64, 64, 70, 60, 60, 68, 68, 68, 68, 68,
      68, 68, 68, 68, 68, 64, 64, 70, 70, 68, 68,
  },
  {
      88, 88, 88, 86, 87, 87, 68, 68, 68, 68, 68,
      68, 68, 68, 68, 68, 88, 88, 86, 86, 68, 68,
  },
  {
      90, 93, 93, 90, 93, 93, 74, 74, 74, 74, 74,
      74, 74, 74, 74, 74, 90, 90, 90, 90, 74, 74,
  },
};

// lookup table for predict_skip_txfm
// int max_tx_size = max_txsize_rect_lookup[bsize];
// if (tx_size_high[max_tx_size] > 16 || tx_size_wide[max_tx_size] > 16)
//   max_tx_size = AOMMIN(max_txsize_lookup[bsize], TX_16X16);
static const TX_SIZE max_predict_sf_tx_size[BLOCK_SIZES_ALL] = {
  TX_4X4,   TX_4X8,   TX_8X4,   TX_8X8,   TX_8X16,  TX_16X8,
  TX_16X16, TX_16X16, TX_16X16, TX_16X16, TX_16X16, TX_16X16,
  TX_16X16, TX_16X16, TX_16X16, TX_16X16, TX_4X16,  TX_16X4,
  TX_8X8,   TX_8X8,   TX_16X16, TX_16X16,
};

// look-up table for sqrt of number of pixels in a transform block
// rounded up to the nearest integer.
static const int sqrt_tx_pixels_2d[TX_SIZES_ALL] = { 4,  8,  16, 32, 32, 6,  6,
                                                     12, 12, 23, 23, 32, 32, 8,
                                                     8,  16, 16, 23, 23 };

// look-up table for number of top no-split RD Costs that should be considered
// based on prune_inter_tx_split_rd_eval_lvl speed feature.
static const int num_inter_tx_no_split_cand[2] = { 4, 3 };

static inline uint32_t get_block_residue_hash(MACROBLOCK *x, BLOCK_SIZE bsize) {
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];
  const int16_t *diff = x->plane[0].src_diff;
  const uint32_t hash =
      av1_get_crc32c_value(&x->txfm_search_info.mb_rd_record->crc_calculator,
                           (uint8_t *)diff, 2 * rows * cols);
  return (hash << 5) + bsize;
}

static inline int32_t find_mb_rd_info(const MB_RD_RECORD *const mb_rd_record,
                                      const int64_t ref_best_rd,
                                      const uint32_t hash) {
  int32_t match_index = -1;
  if (ref_best_rd != INT64_MAX) {
    for (int i = 0; i < mb_rd_record->num; ++i) {
      const int index = (mb_rd_record->index_start + i) % RD_RECORD_BUFFER_LEN;
      // If there is a match in the mb_rd_record, fetch the RD decision and
      // terminate early.
      if (mb_rd_record->mb_rd_info[index].hash_value == hash) {
        match_index = index;
        break;
      }
    }
  }
  return match_index;
}

static inline void fetch_mb_rd_info(int n4, const MB_RD_INFO *const mb_rd_info,
                                    RD_STATS *const rd_stats,
                                    MACROBLOCK *const x) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  mbmi->tx_size = mb_rd_info->tx_size;
  av1_copy(mbmi->inter_tx_size, mb_rd_info->inter_tx_size);
  av1_copy_tx_type_map(xd->tx_type_map, mb_rd_info->tx_type_map, n4);
  *rd_stats = mb_rd_info->rd_stats;
}

int64_t av1_pixel_diff_dist(const MACROBLOCK *x, int plane, int blk_row,
                            int blk_col, const BLOCK_SIZE plane_bsize,
                            const BLOCK_SIZE tx_bsize,
                            unsigned int *block_mse_q8) {
  int visible_rows, visible_cols;
  const int txb_cols = block_size_wide[tx_bsize];
  const int txb_rows = block_size_high[tx_bsize];
  const int is_border = get_visible_dimensions(
      x, plane, plane_bsize, blk_col, blk_row, txb_cols, txb_rows,
      /*clip_dims=*/true, &visible_cols, &visible_rows);

  uint64_t sse = 0;
  if (visible_cols > 0 && visible_rows > 0) {
    const int diff_stride = block_size_wide[plane_bsize];
    const int16_t *diff = x->plane[plane].src_diff;

    diff += ((blk_row * diff_stride + blk_col) << MI_SIZE_LOG2);
    sse = aom_sum_squares_2d_i16(diff, diff_stride, visible_cols, visible_rows);
    if (block_mse_q8 != NULL) {
      if (LIKELY(!is_border)) {
        *block_mse_q8 =
            (unsigned int)((sse << 8) >> num_pels_log2_lookup[tx_bsize]);
      } else {
        *block_mse_q8 =
            (unsigned int)((256 * sse) / (visible_cols * visible_rows));
      }
    }
  } else {
    if (block_mse_q8 != NULL) *block_mse_q8 = 0;
  }
  return sse;
}

// Uses simple features on top of DCT coefficients to quickly predict
// whether optimal RD decision is to skip encoding the residual.
// The sse value is stored in dist.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int predict_skip_txfm(const AV1_COMP *cpi, MACROBLOCK *x,
                             BLOCK_SIZE bsize, int64_t *dist,
                             int reduced_tx_set) {
  const MACROBLOCKD *xd = &x->e_mbd;
#if CONFIG_AV1_HIGHBITDEPTH
  if (xd->bd > 8 && cpi->oxcf.algo_cfg.sharpness == 3) {
    if (av1_is_skip_txfm_penalized(cpi, x, bsize)) return 0;
  }
#else
  (void)cpi;
#endif
  const TxfmSearchParams *txfm_params = &x->txfm_search_params;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const int16_t dc_q = av1_dc_quant_QTX(x->qindex, 0, xd->bd);

  *dist = av1_pixel_diff_dist(x, 0, 0, 0, bsize, bsize, NULL);

  const int64_t mse = *dist / bw / bh;
  // Normalized quantizer takes the transform upscaling factor (8 for tx size
  // smaller than 32) into account.
  const int16_t normalized_dc_q = dc_q >> 3;
  const int64_t mse_thresh = (int64_t)normalized_dc_q * normalized_dc_q / 8;
  // For faster early skip decision, use dist to compare against threshold so
  // that quality risk is less for the skip=1 decision. Otherwise, use mse
  // since the fwd_txfm coeff checks will take care of quality
  // TODO(any): Use dist to return 0 when skip_txfm_level is 1
  int64_t pred_err = (txfm_params->skip_txfm_level >= 2) ? *dist : mse;
  // Predict not to skip when error is larger than threshold.
  if (pred_err > mse_thresh) return 0;
  // Return as skip otherwise for aggressive early skip
  else if (txfm_params->skip_txfm_level >= 2)
    return 1;

  const int max_tx_size = max_predict_sf_tx_size[bsize];
  const int tx_h = tx_size_high[max_tx_size];
  const int tx_w = tx_size_wide[max_tx_size];
  DECLARE_ALIGNED(32, tran_low_t, coefs[32 * 32]);
  TxfmParam param;
  param.tx_type = DCT_DCT;
  param.tx_size = max_tx_size;
  param.bd = xd->bd;
  param.is_hbd = is_cur_buf_hbd(xd);
  param.lossless = 0;
  param.tx_set_type = av1_get_ext_tx_set_type(
      param.tx_size, is_inter_block(xd->mi[0]), reduced_tx_set);
  const int bd_idx = (xd->bd == 8) ? 0 : ((xd->bd == 10) ? 1 : 2);
  const uint32_t max_qcoef_thresh = skip_pred_threshold[bd_idx][bsize];
  const int16_t *src_diff = x->plane[0].src_diff;
  const int n_coeff = tx_w * tx_h;
  const int16_t ac_q = av1_ac_quant_QTX(x->qindex, 0, xd->bd);
  const uint32_t dc_thresh = max_qcoef_thresh * dc_q;
  const uint32_t ac_thresh = max_qcoef_thresh * ac_q;
  for (int row = 0; row < bh; row += tx_h) {
    for (int col = 0; col < bw; col += tx_w) {
      av1_fwd_txfm(src_diff + col, coefs, bw, &param);
      // Operating on TX domain, not pixels; we want the QTX quantizers
      const uint32_t dc_coef = (((uint32_t)abs(coefs[0])) << 7);
      if (dc_coef >= dc_thresh) return 0;
      for (int i = 1; i < n_coeff; ++i) {
        const uint32_t ac_coef = (((uint32_t)abs(coefs[i])) << 7);
        if (ac_coef >= ac_thresh) return 0;
      }
    }
    src_diff += tx_h * bw;
  }
  return 1;
}

// Used to set proper context for early termination with skip = 1.
static inline void set_skip_txfm(MACROBLOCK *x, RD_STATS *rd_stats,
                                 BLOCK_SIZE bsize, int64_t dist) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int n4 = bsize_to_num_blk(bsize);
  const TX_SIZE tx_size = max_txsize_rect_lookup[bsize];
  memset(xd->tx_type_map, DCT_DCT, sizeof(xd->tx_type_map[0]) * n4);
  memset(mbmi->inter_tx_size, tx_size, sizeof(mbmi->inter_tx_size));
  mbmi->tx_size = tx_size;
  rd_stats->skip_txfm = 1;
  if (is_cur_buf_hbd(xd)) dist = ROUND_POWER_OF_TWO(dist, (xd->bd - 8) * 2);
  rd_stats->dist = rd_stats->sse = (dist << 4);
  // Though decision is to make the block as skip based on luma stats,
  // it is possible that block becomes non skip after chroma rd. In addition
  // intermediate non skip costs calculated by caller function will be
  // incorrect, if rate is set as  zero (i.e., if zero_blk_rate is not
  // accounted). Hence intermediate rate is populated to code the luma tx blks
  // as skip, the caller function based on final rd decision (i.e., skip vs
  // non-skip) sets the final rate accordingly. Here the rate populated
  // corresponds to coding all the tx blocks with zero_blk_rate (based on max tx
  // size possible) in the current block. Eg: For 128*128 block, rate would be
  // 4 * zero_blk_rate where zero_blk_rate corresponds to coding of one 64x64 tx
  // block as 'all zeros'
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  TXB_CTX txb_ctx;
  const int txw_u = tx_size_wide_unit[tx_size];
  const int txh_u = tx_size_high_unit[tx_size];
  if (UNLIKELY(((txw_u | txh_u) & 16) != 0)) {
    get_txb_ctx_general(bsize, tx_size, 0, xd->plane[0].above_entropy_context,
                        xd->plane[0].left_entropy_context, &txb_ctx);
  } else {
    const uint64_t va =
        load_entropy_ctx_u64(xd->plane[0].above_entropy_context, txw_u);
    const uint64_t vl =
        load_entropy_ctx_u64(xd->plane[0].left_entropy_context, txh_u);
    const uint64_t pos = ((va >> 4) & 0x0101010101010101ULL) +
                         ((vl >> 4) & 0x0101010101010101ULL);
    const uint64_t neg = ((va >> 3) & 0x0101010101010101ULL) +
                         ((vl >> 3) & 0x0101010101010101ULL);
    const uint64_t diff = (pos + 0x0202020202020202ULL) - neg;
    const uint64_t d32 = (diff + (diff >> 32)) & 0xffffffffULL;
    const int dc_sign = (int)(((d32 * 0x01010101ULL) >> 24) & 0xff) - 16;
    txb_ctx.dc_sign_ctx = (dc_sign < 0) + ((dc_sign > 0) << 1);
    if (bsize == txsize_to_bsize[tx_size]) {
      txb_ctx.txb_skip_ctx = 0;
    } else {
      const int top_cat =
          (va != 0) + (((va >> 2) & 0x0101010101010101ULL) != 0);
      const int left_cat =
          (vl != 0) + (((vl >> 2) & 0x0101010101010101ULL) != 0);
      txb_ctx.txb_skip_ctx = kTxbCtxSkipCat[top_cat][left_cat];
    }
  }
  const int zero_blk_rate = x->coeff_costs.coeff_costs[txs_ctx][PLANE_TYPE_Y]
                                .txb_skip_cost[txb_ctx.txb_skip_ctx][1];
  rd_stats->rate = zero_blk_rate *
                   (block_size_wide[bsize] >> tx_size_wide_log2[tx_size]) *
                   (block_size_high[bsize] >> tx_size_high_log2[tx_size]);
}

static inline void save_mb_rd_info(int n4, uint32_t hash,
                                   const MACROBLOCK *const x,
                                   const RD_STATS *const rd_stats,
                                   MB_RD_RECORD *mb_rd_record) {
  int index;
  if (mb_rd_record->num < RD_RECORD_BUFFER_LEN) {
    index =
        (mb_rd_record->index_start + mb_rd_record->num) % RD_RECORD_BUFFER_LEN;
    ++mb_rd_record->num;
  } else {
    index = mb_rd_record->index_start;
    mb_rd_record->index_start =
        (mb_rd_record->index_start + 1) % RD_RECORD_BUFFER_LEN;
  }
  MB_RD_INFO *const mb_rd_info = &mb_rd_record->mb_rd_info[index];
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  mb_rd_info->hash_value = hash;
  mb_rd_info->tx_size = mbmi->tx_size;
  av1_copy(mb_rd_info->inter_tx_size, mbmi->inter_tx_size);
  av1_copy_tx_type_map(mb_rd_info->tx_type_map, xd->tx_type_map, n4);
  mb_rd_info->rd_stats = *rd_stats;
}

// Store the RD Cost of transform no-split.
static inline void push_inter_block_tx_no_split_rd(
    MACROBLOCK *x, const MB_MODE_INFO *mbmi, int64_t tmp_rd, int blk_idx,
    int prune_inter_tx_split_rd_eval_lvl) {
  assert(blk_idx < MAX_TX_BLOCKS_IN_MAX_SB);
  if (!prune_inter_tx_split_rd_eval_lvl) return;

  if (blk_idx == -1 || tmp_rd == INT64_MAX) return;

  // Do not store for skip and intraBC modes
  if (mbmi->skip_mode != 0 || is_intrabc_block(mbmi)) return;

  assert(prune_inter_tx_split_rd_eval_lvl <= 2);
  const int num_top_cand =
      num_inter_tx_no_split_cand[prune_inter_tx_split_rd_eval_lvl - 1];
  assert(num_top_cand <= TOP_INTER_TX_NO_SPLIT_COUNT);

  // Insert the RD Cost in sorted order
  int64_t *const top_rd = x->top_inter_tx_no_split_rd[blk_idx];
  for (int i = 0; i < num_top_cand; i++) {
    if (tmp_rd < top_rd[i]) {
      const int64_t r0 = top_rd[0];
      const int64_t r1 = top_rd[1];
      const int64_t r2 = top_rd[2];
      if (i <= 2 && num_top_cand >= 4) top_rd[3] = r2;
      if (i <= 1 && num_top_cand >= 3) top_rd[2] = r1;
      if (i == 0 && num_top_cand >= 2) top_rd[1] = r0;
      top_rd[i] = tmp_rd;
      break;
    }
  }
}

// Prune the evaluation of transform split.
static inline bool prune_tx_split_eval_using_no_split_rd(
    const MACROBLOCK *x, const MB_MODE_INFO *mbmi, int64_t tmp_rd, int blk_idx,
    int prune_inter_tx_split_rd_eval_lvl) {
  if (!prune_inter_tx_split_rd_eval_lvl) return false;

  if (blk_idx == -1 || tmp_rd == INT64_MAX) return false;

  // Do not prune for skip and intraBC modes
  if (mbmi->skip_mode != 0 || is_intrabc_block(mbmi)) return false;

  assert(prune_inter_tx_split_rd_eval_lvl <= 2);
  const int num_top_cand =
      num_inter_tx_no_split_cand[prune_inter_tx_split_rd_eval_lvl - 1];
  assert(num_top_cand <= TOP_INTER_TX_NO_SPLIT_COUNT);

  // Do not prune if there is no valid top RD Cost for comparison
  if (x->top_inter_tx_no_split_rd[blk_idx][num_top_cand - 1] == INT64_MAX)
    return false;

  if (tmp_rd > x->top_inter_tx_no_split_rd[blk_idx][num_top_cand - 1])
    return true;

  return false;
}

static int get_search_init_depth(int mi_width, int mi_height, int is_inter,
                                 const SPEED_FEATURES *sf,
                                 int tx_size_search_method) {
  if (tx_size_search_method == USE_LARGESTALL) return MAX_VARTX_DEPTH;

  if (sf->tx_sf.tx_size_search_lgr_block) {
    if (mi_width > mi_size_wide[BLOCK_64X64] ||
        mi_height > mi_size_high[BLOCK_64X64])
      return MAX_VARTX_DEPTH;
  }

  if (is_inter) {
    return (mi_height != mi_width)
               ? sf->tx_sf.inter_tx_size_search_init_depth_rect
               : sf->tx_sf.inter_tx_size_search_init_depth_sqr;
  } else {
    return (mi_height != mi_width)
               ? sf->tx_sf.intra_tx_size_search_init_depth_rect
               : sf->tx_sf.intra_tx_size_search_init_depth_sqr;
  }
}

static inline void select_tx_block(
    const AV1_COMP *cpi, MACROBLOCK *x, int blk_row, int blk_col, int block,
    TX_SIZE tx_size, int depth, BLOCK_SIZE plane_bsize, ENTROPY_CONTEXT *ta,
    ENTROPY_CONTEXT *tl, TXFM_CONTEXT *tx_above, TXFM_CONTEXT *tx_left,
    RD_STATS *rd_stats, int64_t prev_level_rd, int64_t ref_best_rd,
    int *is_cost_valid, FAST_TX_SEARCH_MODE ftxs_mode, int blk_idx);

// NOTE: CONFIG_COLLECT_RD_STATS has 3 possible values
// 0: Do not collect any RD stats
// 1: Collect RD stats for transform units
// 2: Collect RD stats for partition units
#if CONFIG_COLLECT_RD_STATS

static inline void get_energy_distribution_fine(
    const AV1_COMP *cpi, BLOCK_SIZE bsize, const uint8_t *src, int src_stride,
    const uint8_t *dst, int dst_stride, int need_4th, double *hordist,
    double *verdist) {
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  unsigned int esq[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  if (bsize < BLOCK_16X16 || (bsize >= BLOCK_4X16 && bsize <= BLOCK_32X8)) {
    // Special cases: calculate 'esq' values manually, as we don't have 'vf'
    // functions for the 16 (very small) sub-blocks of this block.
    const int w_shift = (bw == 4) ? 0 : (bw == 8) ? 1 : (bw == 16) ? 2 : 3;
    const int h_shift = (bh == 4) ? 0 : (bh == 8) ? 1 : (bh == 16) ? 2 : 3;
    assert(bw <= 32);
    assert(bh <= 32);
    assert(((bw - 1) >> w_shift) + (((bh - 1) >> h_shift) << 2) == 15);
    if (cpi->common.seq_params->use_highbitdepth) {
      const uint16_t *src16 = CONVERT_TO_SHORTPTR(src);
      const uint16_t *dst16 = CONVERT_TO_SHORTPTR(dst);
      for (int i = 0; i < bh; ++i)
        for (int j = 0; j < bw; ++j) {
          const int index = (j >> w_shift) + ((i >> h_shift) << 2);
          esq[index] +=
              (src16[j + i * src_stride] - dst16[j + i * dst_stride]) *
              (src16[j + i * src_stride] - dst16[j + i * dst_stride]);
        }
    } else {
      for (int i = 0; i < bh; ++i)
        for (int j = 0; j < bw; ++j) {
          const int index = (j >> w_shift) + ((i >> h_shift) << 2);
          esq[index] += (src[j + i * src_stride] - dst[j + i * dst_stride]) *
                        (src[j + i * src_stride] - dst[j + i * dst_stride]);
        }
    }
  } else {  // Calculate 'esq' values using 'vf' functions on the 16 sub-blocks.
    const int f_index =
        (bsize < BLOCK_SIZES) ? bsize - BLOCK_16X16 : bsize - BLOCK_8X16;
    assert(f_index >= 0 && f_index < BLOCK_SIZES_ALL);
    const BLOCK_SIZE subsize = (BLOCK_SIZE)f_index;
    assert(block_size_wide[bsize] == 4 * block_size_wide[subsize]);
    assert(block_size_high[bsize] == 4 * block_size_high[subsize]);
    cpi->ppi->fn_ptr[subsize].vf(src, src_stride, dst, dst_stride, &esq[0]);
    cpi->ppi->fn_ptr[subsize].vf(src + bw / 4, src_stride, dst + bw / 4,
                                 dst_stride, &esq[1]);
    cpi->ppi->fn_ptr[subsize].vf(src + bw / 2, src_stride, dst + bw / 2,
                                 dst_stride, &esq[2]);
    cpi->ppi->fn_ptr[subsize].vf(src + 3 * bw / 4, src_stride, dst + 3 * bw / 4,
                                 dst_stride, &esq[3]);
    src += bh / 4 * src_stride;
    dst += bh / 4 * dst_stride;

    cpi->ppi->fn_ptr[subsize].vf(src, src_stride, dst, dst_stride, &esq[4]);
    cpi->ppi->fn_ptr[subsize].vf(src + bw / 4, src_stride, dst + bw / 4,
                                 dst_stride, &esq[5]);
    cpi->ppi->fn_ptr[subsize].vf(src + bw / 2, src_stride, dst + bw / 2,
                                 dst_stride, &esq[6]);
    cpi->ppi->fn_ptr[subsize].vf(src + 3 * bw / 4, src_stride, dst + 3 * bw / 4,
                                 dst_stride, &esq[7]);
    src += bh / 4 * src_stride;
    dst += bh / 4 * dst_stride;

    cpi->ppi->fn_ptr[subsize].vf(src, src_stride, dst, dst_stride, &esq[8]);
    cpi->ppi->fn_ptr[subsize].vf(src + bw / 4, src_stride, dst + bw / 4,
                                 dst_stride, &esq[9]);
    cpi->ppi->fn_ptr[subsize].vf(src + bw / 2, src_stride, dst + bw / 2,
                                 dst_stride, &esq[10]);
    cpi->ppi->fn_ptr[subsize].vf(src + 3 * bw / 4, src_stride, dst + 3 * bw / 4,
                                 dst_stride, &esq[11]);
    src += bh / 4 * src_stride;
    dst += bh / 4 * dst_stride;

    cpi->ppi->fn_ptr[subsize].vf(src, src_stride, dst, dst_stride, &esq[12]);
    cpi->ppi->fn_ptr[subsize].vf(src + bw / 4, src_stride, dst + bw / 4,
                                 dst_stride, &esq[13]);
    cpi->ppi->fn_ptr[subsize].vf(src + bw / 2, src_stride, dst + bw / 2,
                                 dst_stride, &esq[14]);
    cpi->ppi->fn_ptr[subsize].vf(src + 3 * bw / 4, src_stride, dst + 3 * bw / 4,
                                 dst_stride, &esq[15]);
  }

  double total = (double)esq[0] + esq[1] + esq[2] + esq[3] + esq[4] + esq[5] +
                 esq[6] + esq[7] + esq[8] + esq[9] + esq[10] + esq[11] +
                 esq[12] + esq[13] + esq[14] + esq[15];
  if (total > 0) {
    const double e_recip = 1.0 / total;
    hordist[0] = ((double)esq[0] + esq[4] + esq[8] + esq[12]) * e_recip;
    hordist[1] = ((double)esq[1] + esq[5] + esq[9] + esq[13]) * e_recip;
    hordist[2] = ((double)esq[2] + esq[6] + esq[10] + esq[14]) * e_recip;
    if (need_4th) {
      hordist[3] = ((double)esq[3] + esq[7] + esq[11] + esq[15]) * e_recip;
    }
    verdist[0] = ((double)esq[0] + esq[1] + esq[2] + esq[3]) * e_recip;
    verdist[1] = ((double)esq[4] + esq[5] + esq[6] + esq[7]) * e_recip;
    verdist[2] = ((double)esq[8] + esq[9] + esq[10] + esq[11]) * e_recip;
    if (need_4th) {
      verdist[3] = ((double)esq[12] + esq[13] + esq[14] + esq[15]) * e_recip;
    }
  } else {
    hordist[0] = verdist[0] = 0.25;
    hordist[1] = verdist[1] = 0.25;
    hordist[2] = verdist[2] = 0.25;
    if (need_4th) {
      hordist[3] = verdist[3] = 0.25;
    }
  }
}

static double get_sse_norm(const int16_t *diff, int stride, int w, int h) {
  double sum = 0.0;
  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      const int err = diff[j * stride + i];
      sum += err * err;
    }
  }
  assert(w > 0 && h > 0);
  return sum / (w * h);
}

static double get_sad_norm(const int16_t *diff, int stride, int w, int h) {
  double sum = 0.0;
  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      sum += abs(diff[j * stride + i]);
    }
  }
  assert(w > 0 && h > 0);
  return sum / (w * h);
}

static inline void get_2x2_normalized_sses_and_sads(
    const AV1_COMP *const cpi, BLOCK_SIZE tx_bsize, const uint8_t *const src,
    int src_stride, const uint8_t *const dst, int dst_stride,
    const int16_t *const src_diff, int diff_stride, double *const sse_norm_arr,
    double *const sad_norm_arr) {
  const BLOCK_SIZE tx_bsize_half =
      get_partition_subsize(tx_bsize, PARTITION_SPLIT);
  if (tx_bsize_half == BLOCK_INVALID) {  // manually calculate stats
    const int half_width = block_size_wide[tx_bsize] / 2;
    const int half_height = block_size_high[tx_bsize] / 2;
    for (int row = 0; row < 2; ++row) {
      for (int col = 0; col < 2; ++col) {
        const int16_t *const this_src_diff =
            src_diff + row * half_height * diff_stride + col * half_width;
        if (sse_norm_arr) {
          sse_norm_arr[row * 2 + col] =
              get_sse_norm(this_src_diff, diff_stride, half_width, half_height);
        }
        if (sad_norm_arr) {
          sad_norm_arr[row * 2 + col] =
              get_sad_norm(this_src_diff, diff_stride, half_width, half_height);
        }
      }
    }
  } else {  // use function pointers to calculate stats
    const int half_width = block_size_wide[tx_bsize_half];
    const int half_height = block_size_high[tx_bsize_half];
    const int num_samples_half = half_width * half_height;
    for (int row = 0; row < 2; ++row) {
      for (int col = 0; col < 2; ++col) {
        const uint8_t *const this_src =
            src + row * half_height * src_stride + col * half_width;
        const uint8_t *const this_dst =
            dst + row * half_height * dst_stride + col * half_width;

        if (sse_norm_arr) {
          unsigned int this_sse;
          cpi->ppi->fn_ptr[tx_bsize_half].vf(this_src, src_stride, this_dst,
                                             dst_stride, &this_sse);
          sse_norm_arr[row * 2 + col] = (double)this_sse / num_samples_half;
        }

        if (sad_norm_arr) {
          const unsigned int this_sad = cpi->ppi->fn_ptr[tx_bsize_half].sdf(
              this_src, src_stride, this_dst, dst_stride);
          sad_norm_arr[row * 2 + col] = (double)this_sad / num_samples_half;
        }
      }
    }
  }
}

#if CONFIG_COLLECT_RD_STATS == 1
static double get_mean(const int16_t *diff, int stride, int w, int h) {
  double sum = 0.0;
  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      sum += diff[j * stride + i];
    }
  }
  assert(w > 0 && h > 0);
  return sum / (w * h);
}
static inline void PrintTransformUnitStats(
    const AV1_COMP *const cpi, MACROBLOCK *x, const RD_STATS *const rd_stats,
    int blk_row, int blk_col, BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
    TX_TYPE tx_type, int64_t rd) {
  if (rd_stats->rate == INT_MAX || rd_stats->dist == INT64_MAX) return;

  // Generate small sample to restrict output size.
  static unsigned int seed = 21743;
  if (lcg_rand16(&seed) % 256 > 0) return;

  const char output_file[] = "tu_stats.txt";
  FILE *fout = fopen(output_file, "a");
  if (!fout) return;

  const BLOCK_SIZE tx_bsize = txsize_to_bsize[tx_size];
  const MACROBLOCKD *const xd = &x->e_mbd;
  const int plane = 0;
  struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const int txw = tx_size_wide[tx_size];
  const int txh = tx_size_high[tx_size];
  const int dequant_shift = (is_cur_buf_hbd(xd)) ? xd->bd - 5 : 3;
  const int q_step = p->dequant_QTX[1] >> dequant_shift;
  const int num_samples = txw * txh;

  const double rate_norm = (double)rd_stats->rate / num_samples;
  const double dist_norm = (double)rd_stats->dist / num_samples;

  fprintf(fout, "%g %g", rate_norm, dist_norm);

  const int src_stride = p->src.stride;
  const uint8_t *const src =
      &p->src.buf[(blk_row * src_stride + blk_col) << MI_SIZE_LOG2];
  const int dst_stride = pd->dst.stride;
  const uint8_t *const dst =
      &pd->dst.buf[(blk_row * dst_stride + blk_col) << MI_SIZE_LOG2];
  unsigned int sse;
  cpi->ppi->fn_ptr[tx_bsize].vf(src, src_stride, dst, dst_stride, &sse);
  const double sse_norm = (double)sse / num_samples;

  const unsigned int sad =
      cpi->ppi->fn_ptr[tx_bsize].sdf(src, src_stride, dst, dst_stride);
  const double sad_norm = (double)sad / num_samples;

  fprintf(fout, " %g %g", sse_norm, sad_norm);

  const int diff_stride = block_size_wide[plane_bsize];
  const int16_t *const src_diff =
      &p->src_diff[(blk_row * diff_stride + blk_col) << MI_SIZE_LOG2];

  double sse_norm_arr[4], sad_norm_arr[4];
  get_2x2_normalized_sses_and_sads(cpi, tx_bsize, src, src_stride, dst,
                                   dst_stride, src_diff, diff_stride,
                                   sse_norm_arr, sad_norm_arr);
  for (int i = 0; i < 4; ++i) {
    fprintf(fout, " %g", sse_norm_arr[i]);
  }
  for (int i = 0; i < 4; ++i) {
    fprintf(fout, " %g", sad_norm_arr[i]);
  }

  const TX_TYPE_1D tx_type_1d_row = htx_tab[tx_type];
  const TX_TYPE_1D tx_type_1d_col = vtx_tab[tx_type];

  fprintf(fout, " %d %d %d %d %d", q_step, tx_size_wide[tx_size],
          tx_size_high[tx_size], tx_type_1d_row, tx_type_1d_col);

  int model_rate;
  int64_t model_dist;
  model_rd_sse_fn[MODELRD_CURVFIT](cpi, x, tx_bsize, plane, sse, num_samples,
                                   &model_rate, &model_dist);
  const double model_rate_norm = (double)model_rate / num_samples;
  const double model_dist_norm = (double)model_dist / num_samples;
  fprintf(fout, " %g %g", model_rate_norm, model_dist_norm);

  const double mean = get_mean(src_diff, diff_stride, txw, txh);
  float hor_corr, vert_corr;
  av1_get_horver_correlation_full(src_diff, diff_stride, txw, txh, &hor_corr,
                                  &vert_corr);
  fprintf(fout, " %g %g %g", mean, hor_corr, vert_corr);

  double hdist[4] = { 0 }, vdist[4] = { 0 };
  get_energy_distribution_fine(cpi, tx_bsize, src, src_stride, dst, dst_stride,
                               1, hdist, vdist);
  fprintf(fout, " %g %g %g %g %g %g %g %g", hdist[0], hdist[1], hdist[2],
          hdist[3], vdist[0], vdist[1], vdist[2], vdist[3]);

  fprintf(fout, " %d %" PRId64, x->rdmult, rd);

  fprintf(fout, "\n");
  fclose(fout);
}
#endif  // CONFIG_COLLECT_RD_STATS == 1

#if CONFIG_COLLECT_RD_STATS >= 2
static int64_t get_sse(const AV1_COMP *cpi, const MACROBLOCK *x) {
  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const MACROBLOCKD *xd = &x->e_mbd;
  const MB_MODE_INFO *mbmi = xd->mi[0];
  int64_t total_sse = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    const struct macroblock_plane *const p = &x->plane[plane];
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    const BLOCK_SIZE bs =
        get_plane_block_size(mbmi->bsize, pd->subsampling_x, pd->subsampling_y);
    unsigned int sse;

    if (plane) continue;

    cpi->ppi->fn_ptr[bs].vf(p->src.buf, p->src.stride, pd->dst.buf,
                            pd->dst.stride, &sse);
    total_sse += sse;
  }
  total_sse <<= 4;
  return total_sse;
}

static int get_est_rate_dist(const TileDataEnc *tile_data, BLOCK_SIZE bsize,
                             int64_t sse, int *est_residue_cost,
                             int64_t *est_dist) {
  const InterModeRdModel *md = &tile_data->inter_mode_rd_models[bsize];
  if (md->ready) {
    if (sse < md->dist_mean) {
      *est_residue_cost = 0;
      *est_dist = sse;
    } else {
      *est_dist = (int64_t)round(md->dist_mean);
      const double est_ld = md->a * sse + md->b;
      // Clamp estimated rate cost by INT_MAX / 2.
      // TODO(angiebird@google.com): find better solution than clamping.
      if (fabs(est_ld) < 1e-2) {
        *est_residue_cost = INT_MAX / 2;
      } else {
        double est_residue_cost_dbl = ((sse - md->dist_mean) / est_ld);
        if (est_residue_cost_dbl < 0) {
          *est_residue_cost = 0;
        } else {
          *est_residue_cost =
              (int)AOMMIN((int64_t)round(est_residue_cost_dbl), INT_MAX / 2);
        }
      }
      if (*est_residue_cost <= 0) {
        *est_residue_cost = 0;
        *est_dist = sse;
      }
    }
    return 1;
  }
  return 0;
}

static double get_highbd_diff_mean(const uint8_t *src8, int src_stride,
                                   const uint8_t *dst8, int dst_stride, int w,
                                   int h) {
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  double sum = 0.0;
  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      const int diff = src[j * src_stride + i] - dst[j * dst_stride + i];
      sum += diff;
    }
  }
  assert(w > 0 && h > 0);
  return sum / (w * h);
}

static double get_diff_mean(const uint8_t *src, int src_stride,
                            const uint8_t *dst, int dst_stride, int w, int h) {
  double sum = 0.0;
  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      const int diff = src[j * src_stride + i] - dst[j * dst_stride + i];
      sum += diff;
    }
  }
  assert(w > 0 && h > 0);
  return sum / (w * h);
}

static inline void PrintPredictionUnitStats(const AV1_COMP *const cpi,
                                            const TileDataEnc *tile_data,
                                            MACROBLOCK *x,
                                            const RD_STATS *const rd_stats,
                                            BLOCK_SIZE plane_bsize) {
  if (rd_stats->rate == INT_MAX || rd_stats->dist == INT64_MAX) return;

  if (cpi->sf.inter_sf.inter_mode_rd_model_estimation == 1 &&
      (tile_data == NULL ||
       !tile_data->inter_mode_rd_models[plane_bsize].ready))
    return;
  (void)tile_data;
  // Generate small sample to restrict output size.
  static unsigned int seed = 95014;

  if ((lcg_rand16(&seed) % (1 << (14 - num_pels_log2_lookup[plane_bsize]))) !=
      1)
    return;

  const char output_file[] = "pu_stats.txt";
  FILE *fout = fopen(output_file, "a");
  if (!fout) return;

  MACROBLOCKD *const xd = &x->e_mbd;
  const int plane = 0;
  struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  const int diff_stride = block_size_wide[plane_bsize];
  int bw, bh;
  get_txb_dimensions(xd, plane, plane_bsize, 0, 0, plane_bsize, NULL, NULL, &bw,
                     &bh);
  const int num_samples = bw * bh;
  const int dequant_shift = (is_cur_buf_hbd(xd)) ? xd->bd - 5 : 3;
  const int q_step = p->dequant_QTX[1] >> dequant_shift;
  const int shift = (xd->bd - 8);

  const double rate_norm = (double)rd_stats->rate / num_samples;
  const double dist_norm = (double)rd_stats->dist / num_samples;
  const double rdcost_norm =
      (double)RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist) / num_samples;

  fprintf(fout, "%g %g %g", rate_norm, dist_norm, rdcost_norm);

  const int src_stride = p->src.stride;
  const uint8_t *const src = p->src.buf;
  const int dst_stride = pd->dst.stride;
  const uint8_t *const dst = pd->dst.buf;
  const int16_t *const src_diff = p->src_diff;

  int64_t sse = calculate_sse(xd, p, pd, bw, bh);
  const double sse_norm = (double)sse / num_samples;

  const unsigned int sad =
      cpi->ppi->fn_ptr[plane_bsize].sdf(src, src_stride, dst, dst_stride);
  const double sad_norm =
      (double)sad / (1 << num_pels_log2_lookup[plane_bsize]);

  fprintf(fout, " %g %g", sse_norm, sad_norm);

  double sse_norm_arr[4], sad_norm_arr[4];
  get_2x2_normalized_sses_and_sads(cpi, plane_bsize, src, src_stride, dst,
                                   dst_stride, src_diff, diff_stride,
                                   sse_norm_arr, sad_norm_arr);
  if (shift) {
    for (int k = 0; k < 4; ++k) sse_norm_arr[k] /= (1 << (2 * shift));
    for (int k = 0; k < 4; ++k) sad_norm_arr[k] /= (1 << shift);
  }
  for (int i = 0; i < 4; ++i) {
    fprintf(fout, " %g", sse_norm_arr[i]);
  }
  for (int i = 0; i < 4; ++i) {
    fprintf(fout, " %g", sad_norm_arr[i]);
  }

  fprintf(fout, " %d %d %d %d", q_step, x->rdmult, bw, bh);

  int model_rate;
  int64_t model_dist;
  model_rd_sse_fn[MODELRD_CURVFIT](cpi, x, plane_bsize, plane, sse, num_samples,
                                   &model_rate, &model_dist);
  const double model_rdcost_norm =
      (double)RDCOST(x->rdmult, model_rate, model_dist) / num_samples;
  const double model_rate_norm = (double)model_rate / num_samples;
  const double model_dist_norm = (double)model_dist / num_samples;
  fprintf(fout, " %g %g %g", model_rate_norm, model_dist_norm,
          model_rdcost_norm);

  double mean;
  if (is_cur_buf_hbd(xd)) {
    mean = get_highbd_diff_mean(p->src.buf, p->src.stride, pd->dst.buf,
                                pd->dst.stride, bw, bh);
  } else {
    mean = get_diff_mean(p->src.buf, p->src.stride, pd->dst.buf, pd->dst.stride,
                         bw, bh);
  }
  mean /= (1 << shift);
  float hor_corr, vert_corr;
  av1_get_horver_correlation_full(src_diff, diff_stride, bw, bh, &hor_corr,
                                  &vert_corr);
  fprintf(fout, " %g %g %g", mean, hor_corr, vert_corr);

  double hdist[4] = { 0 }, vdist[4] = { 0 };
  get_energy_distribution_fine(cpi, plane_bsize, src, src_stride, dst,
                               dst_stride, 1, hdist, vdist);
  fprintf(fout, " %g %g %g %g %g %g %g %g", hdist[0], hdist[1], hdist[2],
          hdist[3], vdist[0], vdist[1], vdist[2], vdist[3]);

  if (cpi->sf.inter_sf.inter_mode_rd_model_estimation == 1) {
    assert(tile_data->inter_mode_rd_models[plane_bsize].ready);
    const int64_t overall_sse = get_sse(cpi, x);
    int est_residue_cost = 0;
    int64_t est_dist = 0;
    get_est_rate_dist(tile_data, plane_bsize, overall_sse, &est_residue_cost,
                      &est_dist);
    const double est_residue_cost_norm = (double)est_residue_cost / num_samples;
    const double est_dist_norm = (double)est_dist / num_samples;
    const double est_rdcost_norm =
        (double)RDCOST(x->rdmult, est_residue_cost, est_dist) / num_samples;
    fprintf(fout, " %g %g %g", est_residue_cost_norm, est_dist_norm,
            est_rdcost_norm);
  }

  fprintf(fout, "\n");
  fclose(fout);
}
#endif  // CONFIG_COLLECT_RD_STATS >= 2
#endif  // CONFIG_COLLECT_RD_STATS

#if CONFIG_AV1_HIGHBITDEPTH
// Compute the pixel domain distortion from src and dst on all visible 4x4s in
// the transform block.
static unsigned pixel_dist(const AV1_COMP *const cpi, const MACROBLOCK *x,
                           int plane, const uint8_t *src, const int src_stride,
                           const uint8_t *dst, const int dst_stride,
                           int blk_row, int blk_col,
                           const BLOCK_SIZE plane_bsize,
                           const BLOCK_SIZE tx_bsize) {
  int txb_rows, txb_cols, visible_rows, visible_cols;
  txb_cols = block_size_wide[tx_bsize];
  txb_rows = block_size_high[tx_bsize];

  get_visible_dimensions(x, plane, plane_bsize, blk_col, blk_row, txb_cols,
                         txb_rows, /*clip_dims=*/true, &visible_cols,
                         &visible_rows);
  assert(visible_rows > 0);
  assert(visible_cols > 0);

  unsigned sse = pixel_dist_visible_only(cpi, x, src, src_stride, dst,
                                         dst_stride, tx_bsize, txb_rows,
                                         txb_cols, visible_rows, visible_cols);

  return sse;
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

// Packs transform block coordinates into a single 64-bit register.
// byte7 holds:
//   - dist_block_px_domain: (is_hbd << 7) | (lossless << 6) | (tx_type & 0x3f)
//   - get_tx_mask (plane == 0): ftxs_mode (FTXS_DCT_AND_1D_DCT_ONLY)
//   - get_tx_mask (plane > 0):  uv_tx_type
static AOM_FORCE_INLINE uint64_t pack_tx_blk_args(int block, int blk_row,
                                                  int blk_col, int plane,
                                                  BLOCK_SIZE plane_bsize,
                                                  TX_SIZE tx_size, int byte7) {
  return (uint64_t)(uint16_t)block | ((uint64_t)(uint8_t)blk_row << 16) |
         ((uint64_t)(uint8_t)blk_col << 24) | ((uint64_t)(uint8_t)plane << 32) |
         ((uint64_t)(uint8_t)plane_bsize << 40) |
         ((uint64_t)(uint8_t)tx_size << 48) | ((uint64_t)(uint8_t)byte7 << 56);
}

static inline int64_t pixel_dist_lbd_maybe_border(
    const MACROBLOCK *x, int plane, const uint8_t *src, int src_stride,
    const uint8_t *dst, int dst_stride, int bsw, int bsh, int blk_row,
    int blk_col, BLOCK_SIZE plane_bsize) {
  if (LIKELY((x->pix_to_bottom_edge | x->pix_to_right_edge) >= 0)) {
    return 16 * (int64_t)(unsigned)aom_sse(src, src_stride, dst, dst_stride,
                                           bsw, bsh);
  }
  int visible_cols, visible_rows;
  get_visible_dimensions(x, plane, plane_bsize, blk_col, blk_row, bsw, bsh,
                         /*clip_dims=*/true, &visible_cols, &visible_rows);
  if (visible_rows == 0 || visible_cols == 0) return 0;
  const unsigned sse = (((visible_rows | visible_cols) & 3) == 0)
                           ? (unsigned)aom_sse(src, src_stride, dst, dst_stride,
                                               visible_cols, visible_rows)
                           : aom_sse_odd_size(src, src_stride, dst, dst_stride,
                                              visible_cols, visible_rows);
  return 16 * (int64_t)sse;
}

#if TX_SEARCH_SSE2
static AOM_FORCE_INLINE int calc_lowbd_idct_dc(int dc_qcoeff, TX_SIZE tx_size) {
  int dc = clamp(dc_qcoeff, INT16_MIN, INT16_MAX);
  if ((unsigned)(tx_size - TX_4X8) <= (unsigned)(TX_64X32 - TX_4X8)) {
    dc = (dc * 23168 + 16384) >> 15;
  }
  dc = (dc * 23168 + 16384) >> 15;
  const int neg_s0 = (int)((0x2A955542A4ULL >> ((unsigned)tx_size * 2)) & 3u);
  dc = (dc + ((1 << neg_s0) >> 1)) >> neg_s0;
  dc = (dc * 23168 + 16384) >> 15;
  return (dc + 8) >> 4;
}

static inline int64_t sse_dc_add_8bit_sse2(const uint8_t *src, int src_stride,
                                           const uint8_t *dst, int dst_stride,
                                           int bsw, int bsh, int dc) {
  const __m128i v_pos = _mm_set1_epi8((char)(uint8_t)clamp(dc, 0, 255));
  const __m128i v_neg = _mm_set1_epi8((char)(uint8_t)clamp(-dc, 0, 255));
  const __m128i zero = _mm_setzero_si128();
  __m128i v_acc = zero;
  if (bsw == 4) {
    for (int row = 0; row < bsh; row += 4) {
      uint32_t s0, s1, s2, s3, d0, d1, d2, d3;
      memcpy(&s0, src, 4);
      memcpy(&s1, src + src_stride, 4);
      memcpy(&s2, src + 2 * src_stride, 4);
      memcpy(&s3, src + 3 * src_stride, 4);
      memcpy(&d0, dst, 4);
      memcpy(&d1, dst + dst_stride, 4);
      memcpy(&d2, dst + 2 * dst_stride, 4);
      memcpy(&d3, dst + 3 * dst_stride, 4);
      const __m128i vs = _mm_setr_epi32((int)s0, (int)s1, (int)s2, (int)s3);
      const __m128i vd = _mm_setr_epi32((int)d0, (int)d1, (int)d2, (int)d3);
      const __m128i vr = _mm_subs_epu8(_mm_adds_epu8(vd, v_pos), v_neg);
      const __m128i va =
          _mm_or_si128(_mm_subs_epu8(vs, vr), _mm_subs_epu8(vr, vs));
      const __m128i vlo = _mm_unpacklo_epi8(va, zero);
      const __m128i vhi = _mm_unpackhi_epi8(va, zero);
      v_acc = _mm_add_epi32(v_acc, _mm_add_epi32(_mm_madd_epi16(vlo, vlo),
                                                 _mm_madd_epi16(vhi, vhi)));
      src += 4 * src_stride;
      dst += 4 * dst_stride;
    }
  } else if (bsw == 8) {
    for (int row = 0; row < bsh; row += 2) {
      const __m128i vs = _mm_unpacklo_epi64(
          _mm_loadl_epi64((const __m128i *)src),
          _mm_loadl_epi64((const __m128i *)(src + src_stride)));
      const __m128i vd = _mm_unpacklo_epi64(
          _mm_loadl_epi64((const __m128i *)dst),
          _mm_loadl_epi64((const __m128i *)(dst + dst_stride)));
      const __m128i vr = _mm_subs_epu8(_mm_adds_epu8(vd, v_pos), v_neg);
      const __m128i va =
          _mm_or_si128(_mm_subs_epu8(vs, vr), _mm_subs_epu8(vr, vs));
      const __m128i vlo = _mm_unpacklo_epi8(va, zero);
      const __m128i vhi = _mm_unpackhi_epi8(va, zero);
      v_acc = _mm_add_epi32(v_acc, _mm_add_epi32(_mm_madd_epi16(vlo, vlo),
                                                 _mm_madd_epi16(vhi, vhi)));
      src += 2 * src_stride;
      dst += 2 * dst_stride;
    }
  } else {
    for (int row = 0; row < bsh; ++row) {
      for (int col = 0; col < bsw; col += 16) {
        const __m128i vs = _mm_loadu_si128((const __m128i *)(src + col));
        const __m128i vd = _mm_loadu_si128((const __m128i *)(dst + col));
        const __m128i vr = _mm_subs_epu8(_mm_adds_epu8(vd, v_pos), v_neg);
        const __m128i va =
            _mm_or_si128(_mm_subs_epu8(vs, vr), _mm_subs_epu8(vr, vs));
        const __m128i vlo = _mm_unpacklo_epi8(va, zero);
        const __m128i vhi = _mm_unpackhi_epi8(va, zero);
        v_acc = _mm_add_epi32(v_acc, _mm_add_epi32(_mm_madd_epi16(vlo, vlo),
                                                   _mm_madd_epi16(vhi, vhi)));
      }
      src += src_stride;
      dst += dst_stride;
    }
  }
  v_acc = _mm_add_epi32(v_acc, _mm_srli_si128(v_acc, 8));
  v_acc = _mm_add_epi32(v_acc, _mm_srli_si128(v_acc, 4));
  return 16 * (int64_t)(uint32_t)_mm_cvtsi128_si32(v_acc);
}
#endif  // TX_SEARCH_SSE2

static inline void get_txb_ctx_fast(const BLOCK_SIZE plane_bsize,
                                    const TX_SIZE tx_size, const int plane,
                                    const ENTROPY_CONTEXT *const a,
                                    const ENTROPY_CONTEXT *const l,
                                    TXB_CTX *const txb_ctx) {
  const int txw_unit = tx_size_wide_unit[tx_size];
  const int txh_unit = tx_size_high_unit[tx_size];
  if (UNLIKELY(((txw_unit | txh_unit) & 16) != 0)) {
    get_txb_ctx_general(plane_bsize, tx_size, plane, a, l, txb_ctx);
    return;
  }
  const uint64_t va = load_entropy_ctx_u64(a, txw_unit);
  const uint64_t vl = load_entropy_ctx_u64(l, txh_unit);
  const uint64_t pos =
      ((va >> 4) & 0x0101010101010101ULL) + ((vl >> 4) & 0x0101010101010101ULL);
  const uint64_t neg =
      ((va >> 3) & 0x0101010101010101ULL) + ((vl >> 3) & 0x0101010101010101ULL);
  const uint64_t diff = (pos + 0x0202020202020202ULL) - neg;
  const uint64_t d32 = (diff + (diff >> 32)) & 0xffffffffULL;
  const int dc_sign = (int)(((d32 * 0x01010101ULL) >> 24) & 0xff) - 16;
  txb_ctx->dc_sign_ctx = (dc_sign < 0) + ((dc_sign > 0) << 1);

  const BLOCK_SIZE tx_bsize = txsize_to_bsize[tx_size];
  if (plane == 0) {
    if (plane_bsize == tx_bsize) {
      txb_ctx->txb_skip_ctx = 0;
    } else {
      const int top_cat =
          (va != 0) + (((va >> 2) & 0x0101010101010101ULL) != 0);
      const int left_cat =
          (vl != 0) + (((vl >> 2) & 0x0101010101010101ULL) != 0);
      txb_ctx->txb_skip_ctx = kTxbCtxSkipCat[top_cat][left_cat];
    }
  } else {
    const int ctx_base = (va != 0) + (vl != 0);
    const int ctx_offset =
        (num_pels_log2_lookup[plane_bsize] > num_pels_log2_lookup[tx_bsize])
            ? 10
            : 7;
    txb_ctx->txb_skip_ctx = ctx_base + ctx_offset;
  }
}

// Returns the txb_skip_ctx field computed by get_txb_ctx_fast(), skipping the
// dc_sign_ctx computation. Transform sizes with a 64-sample side are not
// supported.
static AOM_FORCE_INLINE int get_txb_skip_ctx_only(
    const BLOCK_SIZE plane_bsize, const BLOCK_SIZE tx_bsize,
    const TX_SIZE tx_size, const int plane, const ENTROPY_CONTEXT *const a,
    const ENTROPY_CONTEXT *const l) {
  assert(((tx_size_wide_unit[tx_size] | tx_size_high_unit[tx_size]) & 16) == 0);
  if (plane == 0) {
    if (plane_bsize == tx_bsize) return 0;
    const uint64_t va = load_entropy_ctx_u64(a, tx_size_wide_unit[tx_size]);
    const uint64_t vl = load_entropy_ctx_u64(l, tx_size_high_unit[tx_size]);
    const int top_cat = (va != 0) + (((va >> 2) & 0x0101010101010101ULL) != 0);
    const int left_cat = (vl != 0) + (((vl >> 2) & 0x0101010101010101ULL) != 0);
    return kTxbCtxSkipCat[top_cat][left_cat];
  }
  const uint64_t va = load_entropy_ctx_u64(a, tx_size_wide_unit[tx_size]);
  const uint64_t vl = load_entropy_ctx_u64(l, tx_size_high_unit[tx_size]);
  const int ctx_base = (va != 0) + (vl != 0);
  const int ctx_offset =
      (num_pels_log2_lookup[plane_bsize] > num_pels_log2_lookup[tx_bsize]) ? 10
                                                                           : 7;
  return ctx_base + ctx_offset;
}

#if CONFIG_AV1_HIGHBITDEPTH
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int64_t dist_block_px_domain_hbd(const AV1_COMP *cpi, MACROBLOCK *x,
                                        uint64_t blk_pack) {
  const int block = (uint16_t)blk_pack;
  const int blk_row = (uint8_t)(blk_pack >> 16);
  const int blk_col = (uint8_t)(blk_pack >> 24);
  const int plane = (uint8_t)(blk_pack >> 32);
  const BLOCK_SIZE plane_bsize = (BLOCK_SIZE)(uint8_t)(blk_pack >> 40);
  const TX_SIZE tx_size = (TX_SIZE)(uint8_t)(blk_pack >> 48);
  const TX_TYPE tx_type = (TX_TYPE)((blk_pack >> 56) & 0x3f);
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblock_plane *const p = &x->plane[plane];
  const uint16_t eob = p->eobs[block];
  const BLOCK_SIZE tx_bsize = txsize_to_bsize[tx_size];
  const int bsw = block_size_wide[tx_bsize];
  const int bsh = block_size_high[tx_bsize];
  const int src_stride = p->src.stride;
  const int dst_stride = xd->plane[plane].dst.stride;
  const int src_idx = (blk_row * src_stride + blk_col) << MI_SIZE_LOG2;
  const int dst_idx = (blk_row * dst_stride + blk_col) << MI_SIZE_LOG2;
  const uint8_t *src = &p->src.buf[src_idx];
  const uint8_t *dst = &xd->plane[plane].dst.buf[dst_idx];
  const tran_low_t *dqcoeff = p->dqcoeff + BLOCK_OFFSET(block);

  DECLARE_ALIGNED(16, uint16_t, recon16[MAX_TX_SQUARE]);
  uint8_t *recon = CONVERT_TO_BYTEPTR(recon16);
  aom_highbd_convolve_copy(CONVERT_TO_SHORTPTR(dst), dst_stride,
                           CONVERT_TO_SHORTPTR(recon), MAX_TX_SIZE, bsw, bsh);
  av1_inverse_transform_block(xd, dqcoeff, plane, tx_type, tx_size, recon,
                              MAX_TX_SIZE, eob,
                              cpi->common.features.reduced_tx_set_used);
  return 16 * pixel_dist(cpi, x, plane, src, src_stride, recon, MAX_TX_SIZE,
                         blk_row, blk_col, plane_bsize, tx_bsize);
}
#endif

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int64_t dist_block_px_domain_64(const AV1_COMP *cpi, MACROBLOCK *x,
                                       uint64_t blk_pack) {
  const int block = (uint16_t)blk_pack;
  const int blk_row = (uint8_t)(blk_pack >> 16);
  const int blk_col = (uint8_t)(blk_pack >> 24);
  const int plane = (uint8_t)(blk_pack >> 32);
  const BLOCK_SIZE plane_bsize = (BLOCK_SIZE)(uint8_t)(blk_pack >> 40);
  const TX_SIZE tx_size = (TX_SIZE)(uint8_t)(blk_pack >> 48);
  const TX_TYPE tx_type = (TX_TYPE)((blk_pack >> 56) & 0x3f);
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblock_plane *const p = &x->plane[plane];
  const BLOCK_SIZE tx_bsize = txsize_to_bsize[tx_size];
  const int bsw = block_size_wide[tx_bsize];
  const int bsh = block_size_high[tx_bsize];
  const int src_stride = p->src.stride;
  const int dst_stride = xd->plane[plane].dst.stride;
  const uint8_t *src =
      &p->src.buf[(blk_row * src_stride + blk_col) << MI_SIZE_LOG2];
  const uint8_t *dst =
      &xd->plane[plane]
           .dst.buf[(blk_row * dst_stride + blk_col) << MI_SIZE_LOG2];
  DECLARE_ALIGNED(32, uint8_t, recon[MAX_TX_SQUARE]);
  for (int row = 0; row < bsh; ++row) {
    memcpy(recon + row * bsw, dst + row * dst_stride, (size_t)bsw);
  }
  av1_inverse_transform_block(xd, p->dqcoeff + BLOCK_OFFSET(block), plane,
                              tx_type, tx_size, recon, bsw, p->eobs[block],
                              cpi->common.features.reduced_tx_set_used);
  return pixel_dist_lbd_maybe_border(x, plane, src, src_stride, recon, bsw, bsw,
                                     bsh, blk_row, blk_col, plane_bsize);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int64_t dist_block_px_domain(const AV1_COMP *cpi, MACROBLOCK *x,
                                    uint64_t blk_pack) {
#if CONFIG_AV1_HIGHBITDEPTH
  if (UNLIKELY((int64_t)blk_pack < 0)) {
    return dist_block_px_domain_hbd(cpi, x, blk_pack);
  }
#endif
  const int block = (uint16_t)blk_pack;
  const int blk_row = (uint8_t)(blk_pack >> 16);
  const int blk_col = (uint8_t)(blk_pack >> 24);
  const int plane = (uint8_t)(blk_pack >> 32);
  const BLOCK_SIZE plane_bsize = (BLOCK_SIZE)(uint8_t)(blk_pack >> 40);
  const TX_SIZE tx_size = (TX_SIZE)(uint8_t)(blk_pack >> 48);
  const uint32_t byte7 = (uint8_t)(blk_pack >> 56);
  const TX_TYPE tx_type = (TX_TYPE)(byte7 & 0x3fu);
  const int lossless = (int)((byte7 >> 6) & 1u);
  const struct macroblock_plane *const p = &x->plane[plane];
  const uint16_t eob = p->eobs[block];
  const int bsw = tx_size_wide[tx_size];
  const int bsh = tx_size_high[tx_size];
  const int src_stride = p->src.stride;
  const int dst_stride = x->e_mbd.plane[plane].dst.stride;
  const uint8_t *src =
      &p->src.buf[(blk_row * src_stride + blk_col) << MI_SIZE_LOG2];
  const uint8_t *dst =
      &x->e_mbd.plane[plane]
           .dst.buf[(blk_row * dst_stride + blk_col) << MI_SIZE_LOG2];
  const tran_low_t *dqcoeff = p->dqcoeff + BLOCK_OFFSET(block);

#if TX_SEARCH_SSE2
  if (eob == 1 && tx_type == DCT_DCT &&
      LIKELY((x->pix_to_bottom_edge | x->pix_to_right_edge) >= 0 &&
             !lossless)) {
    const int dc = calc_lowbd_idct_dc(dqcoeff[0], tx_size);
    return sse_dc_add_8bit_sse2(src, src_stride, dst, dst_stride, bsw, bsh, dc);
  }
#endif

  if (UNLIKELY(((bsw | bsh) & 64) != 0)) {
    return dist_block_px_domain_64(cpi, x, blk_pack);
  }

  DECLARE_ALIGNED(32, uint8_t, recon[32 * 32]);
  if (bsw == 4) {
    for (int r = 0; r < bsh; r += 4) {
      memcpy(recon + r * 4, dst + r * dst_stride, 4);
      memcpy(recon + (r + 1) * 4, dst + (r + 1) * dst_stride, 4);
      memcpy(recon + (r + 2) * 4, dst + (r + 2) * dst_stride, 4);
      memcpy(recon + (r + 3) * 4, dst + (r + 3) * dst_stride, 4);
    }
  } else if (bsw == 8) {
    for (int r = 0; r < bsh; r += 4) {
      memcpy(recon + r * 8, dst + r * dst_stride, 8);
      memcpy(recon + (r + 1) * 8, dst + (r + 1) * dst_stride, 8);
      memcpy(recon + (r + 2) * 8, dst + (r + 2) * dst_stride, 8);
      memcpy(recon + (r + 3) * 8, dst + (r + 3) * dst_stride, 8);
    }
  } else if (bsw == 16) {
    for (int r = 0; r < bsh; r += 4) {
      memcpy(recon + r * 16, dst + r * dst_stride, 16);
      memcpy(recon + (r + 1) * 16, dst + (r + 1) * dst_stride, 16);
      memcpy(recon + (r + 2) * 16, dst + (r + 2) * dst_stride, 16);
      memcpy(recon + (r + 3) * 16, dst + (r + 3) * dst_stride, 16);
    }
  } else {
    for (int r = 0; r < bsh; r += 4) {
      memcpy(recon + r * 32, dst + r * dst_stride, 32);
      memcpy(recon + (r + 1) * 32, dst + (r + 1) * dst_stride, 32);
      memcpy(recon + (r + 2) * 32, dst + (r + 2) * dst_stride, 32);
      memcpy(recon + (r + 3) * 32, dst + (r + 3) * dst_stride, 32);
    }
  }

  TxfmParam txfm_param;
  txfm_param.tx_type = tx_type;
  txfm_param.tx_size = tx_size;
  txfm_param.eob = eob;
  txfm_param.lossless = lossless;
  txfm_param.bd = 8;
  txfm_param.is_hbd = 0;
  (void)cpi;
  if (LIKELY(eob != 0)) av1_inv_txfm_add(dqcoeff, recon, bsw, &txfm_param);

  return pixel_dist_lbd_maybe_border(x, plane, src, src_stride, recon, bsw, bsw,
                                     bsh, blk_row, blk_col, plane_bsize);
}

#if !CONFIG_COLLECT_RD_STATS && !COLLECT_TX_SIZE_DATA
// Adds the 8-bit non-lossless inverse transform of dqcoeff to dst.
static AOM_FORCE_INLINE void inv_txfm_add_lbd_nonlossless(
    const tran_low_t *dqcoeff, uint8_t *dst, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, uint16_t eob) {
#if HAVE_AVX2
  // av1_inv_txfm_add_avx2() forwards non-lossless blocks to
  // av1_lowbd_inv_txfm2d_add_avx2(); call it directly when it is selected.
#if CONFIG_RUNTIME_CPU_DETECT
  const int has_avx2_inv_txfm = (av1_inv_txfm_add == av1_inv_txfm_add_avx2);
#else
  const int has_avx2_inv_txfm = 1;
#endif
  if (LIKELY(has_avx2_inv_txfm)) {
    av1_lowbd_inv_txfm2d_add_avx2(dqcoeff, dst, stride, tx_type, tx_size, eob);
    return;
  }
#endif
  TxfmParam param;
  param.tx_type = tx_type;
  param.tx_size = tx_size;
  param.eob = eob;
  param.lossless = 0;
  param.bd = 8;
  param.is_hbd = 0;
  param.tx_set_type = EXT_TX_SET_ALL16;
  av1_inv_txfm_add(dqcoeff, dst, stride, &param);
}

static AOM_FORCE_INLINE void fwd_txfm_lbd_nonlossless(const int16_t *src_diff,
                                                      tran_low_t *coeff,
                                                      int diff_stride,
                                                      TX_SIZE tx_size,
                                                      TX_TYPE tx_type) {
  TxfmParam param;
  param.tx_type = tx_type;
  param.tx_size = tx_size;
  param.lossless = 0;
  param.bd = 8;
  param.is_hbd = 0;
  param.tx_set_type = EXT_TX_SET_ALL16;
  av1_lowbd_fwd_txfm(src_diff, coeff, diff_stride, &param);
}

// Pixel domain distortion of an 8-bit, non-lossless, fully visible transform
// block of at most 32x32. Equivalent to dist_block_px_domain() for such
// blocks. tx_info packs tx_size (bits 0-7), tx_type (bits 8-15) and eob
// (bits 16-31).
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int64_t dist_block_px_lbd_fast(const uint8_t *src, int src_stride,
                                      const uint8_t *dst, int dst_stride,
                                      const tran_low_t *dqcoeff,
                                      uint32_t tx_info) {
  const TX_SIZE tx_size = (TX_SIZE)(uint8_t)tx_info;
  const TX_TYPE tx_type = (TX_TYPE)(uint8_t)(tx_info >> 8);
  const uint16_t eob = (uint16_t)(tx_info >> 16);
  const int bsw = tx_size_wide[tx_size];
  const int bsh = tx_size_high[tx_size];
  assert(((bsw | bsh) & 64) == 0);
  assert(eob != 0);
#if TX_SEARCH_SSE2
  if (eob == 1 && tx_type == DCT_DCT) {
    const int dc = calc_lowbd_idct_dc(dqcoeff[0], tx_size);
    return sse_dc_add_8bit_sse2(src, src_stride, dst, dst_stride, bsw, bsh, dc);
  }
#endif
  DECLARE_ALIGNED(32, uint8_t, recon[32 * 32]);
#if TX_SEARCH_SSE2
  if (bsw == 8) {
    for (int r = 0; r < bsh; r += 4) {
      const __m128i r01 = _mm_unpacklo_epi64(
          _mm_loadl_epi64((const __m128i *)(dst + r * dst_stride)),
          _mm_loadl_epi64((const __m128i *)(dst + (r + 1) * dst_stride)));
      const __m128i r23 = _mm_unpacklo_epi64(
          _mm_loadl_epi64((const __m128i *)(dst + (r + 2) * dst_stride)),
          _mm_loadl_epi64((const __m128i *)(dst + (r + 3) * dst_stride)));
      _mm_store_si128((__m128i *)(recon + r * 8), r01);
      _mm_store_si128((__m128i *)(recon + r * 8 + 16), r23);
    }
  } else if (bsw == 4) {
    for (int r = 0; r < bsh; r += 4) {
      int d0, d1, d2, d3;
      memcpy(&d0, dst + r * dst_stride, 4);
      memcpy(&d1, dst + (r + 1) * dst_stride, 4);
      memcpy(&d2, dst + (r + 2) * dst_stride, 4);
      memcpy(&d3, dst + (r + 3) * dst_stride, 4);
      _mm_store_si128((__m128i *)(recon + r * 4),
                      _mm_setr_epi32(d0, d1, d2, d3));
    }
  } else if (bsw == 16) {
    for (int r = 0; r < bsh; ++r) {
      _mm_store_si128((__m128i *)(recon + r * 16),
                      _mm_loadu_si128((const __m128i *)(dst + r * dst_stride)));
    }
  } else {
    for (int r = 0; r < bsh; ++r) {
      const uint8_t *const d = dst + r * dst_stride;
      _mm_store_si128((__m128i *)(recon + r * 32),
                      _mm_loadu_si128((const __m128i *)d));
      _mm_store_si128((__m128i *)(recon + r * 32 + 16),
                      _mm_loadu_si128((const __m128i *)(d + 16)));
    }
  }
#else
  for (int r = 0; r < bsh; ++r) {
    memcpy(recon + r * bsw, dst + r * dst_stride, (size_t)bsw);
  }
#endif
  inv_txfm_add_lbd_nonlossless(dqcoeff, recon, bsw, tx_type, tx_size, eob);
  return 16 * (int64_t)(unsigned)aom_sse(src, src_stride, recon, bsw, bsw, bsh);
}
#endif  // !CONFIG_COLLECT_RD_STATS && !COLLECT_TX_SIZE_DATA

// pruning thresholds for prune_txk_type and prune_txk_type_separ
static const int prune_factors[5] = { 200, 200, 120, 80, 40 };  // scale 1000
static const int mul_factors[5] = { 80, 80, 70, 50, 30 };       // scale 100

// R-D costs are sorted in ascending order.
static inline void sort_rd(int64_t rds[], int txk[], int len) {
  for (int i = 1; i < len; ++i) {
    const int64_t temprd = rds[i];
    const int tempi = txk[i];
    int j = i;
    while (j > 0 && rds[j - 1] > temprd) {
      rds[j] = rds[j - 1];
      txk[j] = txk[j - 1];
      --j;
    }
    rds[j] = temprd;
    txk[j] = tempi;
  }
}

#if HAVE_AVX2
int64_t av1_block_error_dist_avx2(const tran_low_t *coeff,
                                  const tran_low_t *dqcoeff,
                                  intptr_t block_size);

int64_t av1_block_error_qm_dist_avx2(const tran_low_t *coeff,
                                     const tran_low_t *dqcoeff,
                                     intptr_t block_size,
                                     const qm_val_t *qmatrix_scan);
#endif

int64_t av1_block_error_qm_c(const tran_low_t *coeff, const tran_low_t *dqcoeff,
                             intptr_t block_size, const qm_val_t *qmatrix,
                             const int16_t *scan, int64_t *ssz, int bd) {
  int i;
  int64_t error = 0, sqcoeff = 0;
  int shift = 2 * (bd - 8);
  int rounding = (1 << shift) >> 1;

  for (i = 0; i < block_size; i++) {
    int64_t weight = qmatrix[scan[i]];
    int64_t dd = coeff[i] - dqcoeff[i];
    dd *= weight;
    int64_t cc = coeff[i];
    cc *= weight;
    error += (dd * dd + (1 << (2 * AOM_QM_BITS - 1))) >> (2 * AOM_QM_BITS);
    sqcoeff += (cc * cc + (1 << (2 * AOM_QM_BITS - 1))) >> (2 * AOM_QM_BITS);
  }

  error = (error + rounding) >> shift;
  sqcoeff = (sqcoeff + rounding) >> shift;

  if (ssz) *ssz = sqcoeff;
  return error;
}

static inline int64_t block_error_qm_fast(
    const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size,
    const qm_val_t *qmatrix, const int16_t *scan, int64_t *ssz, int bd) {
#if HAVE_AVX2
#if CONFIG_RUNTIME_CPU_DETECT
  const int has_avx2_blk_err = (av1_block_error == av1_block_error_avx2);
#else
  const int has_avx2_blk_err = 1;
#endif
  if (ssz == NULL && has_avx2_blk_err) {
    const qm_val_t *const qmatrix_scan = av1_get_qmatrix_scan(qmatrix);
    int64_t error =
        av1_block_error_qm_dist_avx2(coeff, dqcoeff, block_size, qmatrix_scan);
    if (bd > 8) {
      const int shift = 2 * (bd - 8);
      const int rounding = (1 << shift) >> 1;
      error = (error + rounding) >> shift;
    }
    return error;
  }
#endif
  int64_t dummy_ssz = 0;
  return av1_block_error_qm(coeff, dqcoeff, block_size, qmatrix, scan,
                            ssz ? ssz : &dummy_ssz, bd);
}

static inline int64_t block_error_lbd(const tran_low_t *coeff,
                                      const tran_low_t *dqcoeff,
                                      intptr_t block_size, int64_t *ssz,
                                      bool need_sse) {
#if HAVE_AVX2
#if CONFIG_RUNTIME_CPU_DETECT
  const int has_avx2_blk_err = (av1_block_error == av1_block_error_avx2);
#else
  const int has_avx2_blk_err = 1;
#endif
  if (!need_sse && has_avx2_blk_err) {
    return av1_block_error_dist_avx2(coeff, dqcoeff, block_size);
  }
#else
  (void)need_sse;
#endif
  return av1_block_error(coeff, dqcoeff, block_size, ssz);
}

static inline void dist_block_tx_domain(MACROBLOCK *x, int plane, int block,
                                        TX_SIZE tx_size, TX_TYPE tx_type,
                                        const qm_val_t *qmatrix,
                                        int64_t *out_dist, int64_t *out_sse) {
  const struct macroblock_plane *const p = &x->plane[plane];
  const int buffer_length = av1_get_max_eob(tx_size);
  int64_t this_sse;
  int64_t *const sse_ptr = out_sse ? &this_sse : NULL;
  int shift = (MAX_TX_SCALE - av1_get_tx_scale(tx_size)) * 2;
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *const coeff = p->coeff + block_offset;
  tran_low_t *const dqcoeff = p->dqcoeff + block_offset;
#if CONFIG_AV1_HIGHBITDEPTH
  MACROBLOCKD *const xd = &x->e_mbd;
  if (is_cur_buf_hbd(xd)) {
    if (qmatrix == NULL || !x->txfm_search_params.use_qm_dist_metric) {
      *out_dist = av1_highbd_block_error(coeff, dqcoeff, buffer_length,
                                         &this_sse, xd->bd);
    } else {
      const int16_t *const scan = get_scan(tx_size, tx_type)->scan;
      *out_dist = block_error_qm_fast(coeff, dqcoeff, buffer_length, qmatrix,
                                      scan, sse_ptr, xd->bd);
    }
  } else {
#endif
    if (qmatrix == NULL || !x->txfm_search_params.use_qm_dist_metric) {
      *out_dist = block_error_lbd(coeff, dqcoeff, buffer_length, &this_sse,
                                  sse_ptr != NULL);
    } else {
      const int16_t *const scan = get_scan(tx_size, tx_type)->scan;
      *out_dist = block_error_qm_fast(coeff, dqcoeff, buffer_length, qmatrix,
                                      scan, sse_ptr, 8);
    }
#if CONFIG_AV1_HIGHBITDEPTH
  }
#endif

  *out_dist = RIGHT_SIGNED_SHIFT(*out_dist, shift);
  if (out_sse) *out_sse = RIGHT_SIGNED_SHIFT(this_sse, shift);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static uint16_t prune_txk_type_separ(
    const AV1_COMP *cpi, MACROBLOCK *x, int plane, int block, TX_SIZE tx_size,
    int blk_row, int blk_col, BLOCK_SIZE plane_bsize, int *txk_map,
    int16_t allowed_tx_mask, int prune_factor, const TXB_CTX *const txb_ctx,
    int reduced_tx_set_used, int64_t ref_best_rd, int num_sel) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;

  int idx;

  int64_t rds_v[4];
  int64_t rds_h[4];
  int idx_v[4] = { 0, 1, 2, 3 };
  int idx_h[4] = { 0, 1, 2, 3 };
  int skip_v[4] = { 0 };
  int skip_h[4] = { 0 };
  static const int idx_map[16] = {
    DCT_DCT,      DCT_ADST,      DCT_FLIPADST,      V_DCT,
    ADST_DCT,     ADST_ADST,     ADST_FLIPADST,     V_ADST,
    FLIPADST_DCT, FLIPADST_ADST, FLIPADST_FLIPADST, V_FLIPADST,
    H_DCT,        H_ADST,        H_FLIPADST,        IDTX
  };

  static const int sel_pattern_v[16] = { 0, 0, 1, 1, 0, 2, 1, 2,
                                         2, 0, 3, 1, 3, 2, 3, 3 };
  static const int sel_pattern_h[16] = { 0, 1, 0, 1, 2, 0, 2, 1,
                                         2, 3, 0, 3, 1, 3, 2, 3 };

  QUANT_PARAM quant_param;
  TxfmParam txfm_param;
  av1_setup_xform(cm, x, tx_size, DCT_DCT, &txfm_param);
  av1_setup_quant(tx_size, 1, AV1_XFORM_QUANT_B, cpi->oxcf.q_cfg.quant_b_adapt,
                  &quant_param);
  int tx_type;
  // to ensure we can try ones even outside of ext_tx_set of current block
  // this function should only be called for size < 16
  assert(txsize_sqr_up_map[tx_size] <= TX_16X16);
  txfm_param.tx_set_type = EXT_TX_SET_ALL16;

  int rate_cost = 0;
  int64_t dist = 0;
  // evaluate horizontal with vertical DCT
  for (idx = 0; idx < 4; ++idx) {
    tx_type = idx_map[idx];
    txfm_param.tx_type = tx_type;

    av1_setup_qmatrix(&cm->quant_params, xd, plane, tx_size, tx_type,
                      &quant_param);

    av1_xform_quant(x, plane, block, blk_row, blk_col, plane_bsize, &txfm_param,
                    &quant_param);

    dist_block_tx_domain(x, plane, block, tx_size, tx_type, quant_param.qmatrix,
                         &dist, NULL);

    rate_cost = av1_cost_coeffs_txb_laplacian(x, plane, block, tx_size, tx_type,
                                              txb_ctx, reduced_tx_set_used, 0);

    rds_h[idx] = RDCOST(x->rdmult, rate_cost, dist);

    if ((rds_h[idx] - (rds_h[idx] >> 2)) > ref_best_rd) {
      skip_h[idx] = 1;
    }
  }
  sort_rd(rds_h, idx_h, 4);
  for (idx = 1; idx < 4; idx++) {
    if (rds_h[idx] > rds_h[0] * 1.2) skip_h[idx_h[idx]] = 1;
  }

  if (skip_h[idx_h[0]]) return (uint16_t)0xFFFF;

  // evaluate vertical with the best horizontal chosen
  rds_v[0] = rds_h[0];
  int start_v = 1, end_v = 4;
  const int *idx_map_v = idx_map + idx_h[0];

  for (idx = start_v; idx < end_v; ++idx) {
    tx_type = idx_map_v[idx_v[idx] * 4];
    txfm_param.tx_type = tx_type;

    av1_setup_qmatrix(&cm->quant_params, xd, plane, tx_size, tx_type,
                      &quant_param);

    av1_xform_quant(x, plane, block, blk_row, blk_col, plane_bsize, &txfm_param,
                    &quant_param);

    dist_block_tx_domain(x, plane, block, tx_size, tx_type, quant_param.qmatrix,
                         &dist, NULL);

    rate_cost = av1_cost_coeffs_txb_laplacian(x, plane, block, tx_size, tx_type,
                                              txb_ctx, reduced_tx_set_used, 0);

    rds_v[idx] = RDCOST(x->rdmult, rate_cost, dist);

    if ((rds_v[idx] - (rds_v[idx] >> 2)) > ref_best_rd) {
      skip_v[idx] = 1;
    }
  }
  sort_rd(rds_v, idx_v, 4);
  for (idx = 1; idx < 4; idx++) {
    if (rds_v[idx] > rds_v[0] * 1.2) skip_v[idx_v[idx]] = 1;
  }

  // combine rd_h and rd_v to prune tx candidates
  int i_v, i_h;
  int64_t rds[16];
  int num_cand = 0, last = TX_TYPES - 1;

  for (int i = 0; i < 16; i++) {
    i_v = sel_pattern_v[i];
    i_h = sel_pattern_h[i];
    tx_type = idx_map[idx_v[i_v] * 4 + idx_h[i_h]];
    if (!(allowed_tx_mask & (1 << tx_type)) || skip_h[idx_h[i_h]] ||
        skip_v[idx_v[i_v]]) {
      txk_map[last] = tx_type;
      last--;
    } else {
      txk_map[num_cand] = tx_type;
      rds[num_cand] = rds_v[i_v] + rds_h[i_h];
      if (rds[num_cand] == 0) rds[num_cand] = 1;
      num_cand++;
    }
  }
  sort_rd(rds, txk_map, num_cand);

  uint16_t prune = (uint16_t)(~(1 << txk_map[0]));
  num_sel = AOMMIN(num_sel, num_cand);

  for (int i = 1; i < num_sel; i++) {
    int64_t factor = 1800 * (rds[i] - rds[0]) / (rds[0]);
    if (factor < (int64_t)prune_factor)
      prune &= ~(1 << txk_map[i]);
    else
      break;
  }
  return prune;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static uint16_t prune_txk_type(const AV1_COMP *cpi, MACROBLOCK *x, int plane,
                               int block, TX_SIZE tx_size, int blk_row,
                               int blk_col, BLOCK_SIZE plane_bsize,
                               int *txk_map, uint16_t allowed_tx_mask,
                               int prune_factor, const TXB_CTX *const txb_ctx,
                               int reduced_tx_set_used) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  int tx_type;

  int64_t rds[TX_TYPES];

  int num_cand = 0;
  int last = TX_TYPES - 1;

  TxfmParam txfm_param;
  QUANT_PARAM quant_param;
  av1_setup_xform(cm, x, tx_size, DCT_DCT, &txfm_param);
  av1_setup_quant(tx_size, 1, AV1_XFORM_QUANT_B, cpi->oxcf.q_cfg.quant_b_adapt,
                  &quant_param);

  for (int idx = 0; idx < TX_TYPES; idx++) {
    tx_type = idx;
    int rate_cost = 0;
    int64_t dist = 0;
    if (!(allowed_tx_mask & (1 << tx_type))) {
      txk_map[last] = tx_type;
      last--;
      continue;
    }
    txfm_param.tx_type = tx_type;

    av1_setup_qmatrix(&cm->quant_params, xd, plane, tx_size, tx_type,
                      &quant_param);

    // do txfm and quantization
    av1_xform_quant(x, plane, block, blk_row, blk_col, plane_bsize, &txfm_param,
                    &quant_param);
    // estimate rate cost
    rate_cost = av1_cost_coeffs_txb_laplacian(x, plane, block, tx_size, tx_type,
                                              txb_ctx, reduced_tx_set_used, 0);
    // tx domain dist
    dist_block_tx_domain(x, plane, block, tx_size, tx_type, quant_param.qmatrix,
                         &dist, NULL);

    txk_map[num_cand] = tx_type;
    rds[num_cand] = RDCOST(x->rdmult, rate_cost, dist);
    if (rds[num_cand] == 0) rds[num_cand] = 1;
    num_cand++;
  }

  if (num_cand == 0) return (uint16_t)0xFFFF;

  sort_rd(rds, txk_map, num_cand);
  uint16_t prune = (uint16_t)(~(1 << txk_map[0]));

  // 0 < prune_factor <= 1000 controls aggressiveness
  int64_t factor = 0;
  for (int idx = 1; idx < num_cand; idx++) {
    factor = 1000 * (rds[idx] - rds[0]) / rds[0];
    if (factor < (int64_t)prune_factor)
      prune &= ~(1 << txk_map[idx]);
    else
      break;
  }
  return prune;
}

// These thresholds were calibrated to provide a certain number of TX types
// pruned by the model on average, i.e. selecting a threshold with index i
// will lead to pruning i+1 TX types on average
static const float *prune_2D_adaptive_thresholds[] = {
  // TX_4X4
  (float[]){ 0.00549f, 0.01306f, 0.02039f, 0.02747f, 0.03406f, 0.04065f,
             0.04724f, 0.05383f, 0.06067f, 0.06799f, 0.07605f, 0.08533f,
             0.09778f, 0.11780f },
  // TX_8X8
  (float[]){ 0.00037f, 0.00183f, 0.00525f, 0.01038f, 0.01697f, 0.02502f,
             0.03381f, 0.04333f, 0.05286f, 0.06287f, 0.07434f, 0.08850f,
             0.10803f, 0.14124f },
  // TX_16X16
  (float[]){ 0.01404f, 0.02000f, 0.04211f, 0.05164f, 0.05798f, 0.06335f,
             0.06897f, 0.07629f, 0.08875f, 0.11169f },
  // TX_32X32
  NULL,
  // TX_64X64
  NULL,
  // TX_4X8
  (float[]){ 0.00183f, 0.00745f, 0.01428f, 0.02185f, 0.02966f, 0.03723f,
             0.04456f, 0.05188f, 0.05920f, 0.06702f, 0.07605f, 0.08704f,
             0.10168f, 0.12585f },
  // TX_8X4
  (float[]){ 0.00085f, 0.00476f, 0.01135f, 0.01892f, 0.02698f, 0.03528f,
             0.04358f, 0.05164f, 0.05994f, 0.06848f, 0.07849f, 0.09021f,
             0.10583f, 0.13123f },
  // TX_8X16
  (float[]){ 0.00037f, 0.00232f, 0.00671f, 0.01257f, 0.01965f, 0.02722f,
             0.03552f, 0.04382f, 0.05237f, 0.06189f, 0.07336f, 0.08728f,
             0.10730f, 0.14221f },
  // TX_16X8
  (float[]){ 0.00061f, 0.00330f, 0.00818f, 0.01453f, 0.02185f, 0.02966f,
             0.03772f, 0.04578f, 0.05383f, 0.06262f, 0.07288f, 0.08582f,
             0.10339f, 0.13464f },
  // TX_16X32
  NULL,
  // TX_32X16
  NULL,
  // TX_32X64
  NULL,
  // TX_64X32
  NULL,
  // TX_4X16
  (float[]){ 0.00232f, 0.00671f, 0.01257f, 0.01941f, 0.02673f, 0.03430f,
             0.04211f, 0.04968f, 0.05750f, 0.06580f, 0.07507f, 0.08655f,
             0.10242f, 0.12878f },
  // TX_16X4
  (float[]){ 0.00110f, 0.00525f, 0.01208f, 0.01990f, 0.02795f, 0.03601f,
             0.04358f, 0.05115f, 0.05896f, 0.06702f, 0.07629f, 0.08752f,
             0.10217f, 0.12610f },
  // TX_8X32
  NULL,
  // TX_32X8
  NULL,
  // TX_16X64
  NULL,
  // TX_64X16
  NULL,
};

static inline float get_adaptive_thresholds(
    TX_SIZE tx_size, TxSetType tx_set_type,
    TX_TYPE_PRUNE_MODE prune_2d_txfm_mode) {
  static const int prune_aggr_table[5][2] = {
    { 4, 1 }, { 6, 3 }, { 9, 6 }, { 9, 6 }, { 12, 9 }
  };
  int pruning_aggressiveness = 0;
  if (tx_set_type == EXT_TX_SET_ALL16)
    pruning_aggressiveness =
        prune_aggr_table[prune_2d_txfm_mode - TX_TYPE_PRUNE_1][0];
  else if (tx_set_type == EXT_TX_SET_DTT9_IDTX_1DDCT)
    pruning_aggressiveness =
        prune_aggr_table[prune_2d_txfm_mode - TX_TYPE_PRUNE_1][1];

  return prune_2D_adaptive_thresholds[tx_size][pruning_aggressiveness];
}

static inline void get_energy_distribution_finer(const int16_t *diff,
                                                 int stride, int bw, int bh,
                                                 float *hordist,
                                                 float *verdist) {
  // First compute downscaled block energy values (esq); downscale factors
  // are defined by w_shift and h_shift.
  unsigned int esq[256];
  const int w_shift = bw <= 8 ? 0 : 1;
  const int h_shift = bh <= 8 ? 0 : 1;
  const int esq_w = bw >> w_shift;
  const int esq_h = bh >> h_shift;
  assert(esq_w >= 4 && esq_h >= 4);
  int i, j;
  uint64_t total = 0;
  if (h_shift) {
    if (w_shift) {
      for (i = 0; i < esq_h; i++) {
        unsigned int *cur_esq_row = esq + i * esq_w;
        const int16_t *diff0 = diff + (2 * i) * stride;
        const int16_t *diff1 = diff0 + stride;
        for (j = 0; j < bw; j += 2) {
          const unsigned int v =
              (unsigned int)(diff0[j] * diff0[j] +
                             diff0[j + 1] * diff0[j + 1]) +
              (unsigned int)(diff1[j] * diff1[j] + diff1[j + 1] * diff1[j + 1]);
          cur_esq_row[j >> 1] = v;
          total += v;
        }
      }
    } else {
      for (i = 0; i < esq_h; i++) {
        unsigned int *cur_esq_row = esq + i * esq_w;
        const int16_t *diff0 = diff + (2 * i) * stride;
        const int16_t *diff1 = diff0 + stride;
        for (j = 0; j < bw; j++) {
          const unsigned int v = (unsigned int)(diff0[j] * diff0[j]) +
                                 (unsigned int)(diff1[j] * diff1[j]);
          cur_esq_row[j] = v;
          total += v;
        }
      }
    }
  } else {
    if (w_shift) {
      for (i = 0; i < bh; i++) {
        unsigned int *cur_esq_row = esq + i * esq_w;
        const int16_t *cur_diff_row = diff + i * stride;
        for (j = 0; j < bw; j += 2) {
          const unsigned int v =
              (unsigned int)(cur_diff_row[j] * cur_diff_row[j] +
                             cur_diff_row[j + 1] * cur_diff_row[j + 1]);
          cur_esq_row[j >> 1] = v;
          total += v;
        }
      }
    } else {
      for (i = 0; i < bh; i++) {
        unsigned int *cur_esq_row = esq + i * esq_w;
        const int16_t *cur_diff_row = diff + i * stride;
        for (j = 0; j < bw; j++) {
          const unsigned int v =
              (unsigned int)(cur_diff_row[j] * cur_diff_row[j]);
          cur_esq_row[j] = v;
          total += v;
        }
      }
    }
  }

  // Output hordist and verdist arrays are normalized 1D projections of esq
  if (total == 0) {
    float hor_val = 1.0f / esq_w;
    for (j = 0; j < esq_w - 1; j++) hordist[j] = hor_val;
    float ver_val = 1.0f / esq_h;
    for (i = 0; i < esq_h - 1; i++) verdist[i] = ver_val;
    return;
  }

  const float e_recip = 1.0f / (float)total;
  const unsigned int *cur_esq_row = esq;
  float ver_sum = 0.0f;
  for (j = 0; j < esq_w - 1; j++) {
    const float v = (float)cur_esq_row[j];
    hordist[j] = v;
    ver_sum += v;
  }
  verdist[0] = (ver_sum + (float)cur_esq_row[j]) * e_recip;
  for (i = 1; i < esq_h - 1; i++) {
    cur_esq_row = esq + i * esq_w;
    ver_sum = 0.0f;
    for (j = 0; j < esq_w - 1; j++) {
      const float v = (float)cur_esq_row[j];
      hordist[j] += v;
      ver_sum += v;
    }
    verdist[i] = (ver_sum + (float)cur_esq_row[j]) * e_recip;
  }
  cur_esq_row = esq + i * esq_w;
  for (j = 0; j < esq_w - 1; j++) {
    hordist[j] = (hordist[j] + (float)cur_esq_row[j]) * e_recip;
  }
}

static inline bool check_bit_mask(uint16_t mask, int val) {
  return mask & (1 << val);
}

static inline void set_bit_mask(uint16_t *mask, int val) {
  *mask |= (1 << val);
}

static inline void unset_bit_mask(uint16_t *mask, int val) {
  *mask &= ~(1 << val);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void prune_tx_2D(MACROBLOCK *x, BLOCK_SIZE bsize, TX_SIZE tx_size,
                        int blk_row, int blk_col, TxSetType tx_set_type,
                        TX_TYPE_PRUNE_MODE prune_2d_txfm_mode, int *txk_map,
                        uint16_t *allowed_tx_mask, int *is_txk_map_populated) {
  // This table is used because the search order is different from the enum
  // order.
  static const int tx_type_table_2D[16] = {
    DCT_DCT,      DCT_ADST,      DCT_FLIPADST,      V_DCT,
    ADST_DCT,     ADST_ADST,     ADST_FLIPADST,     V_ADST,
    FLIPADST_DCT, FLIPADST_ADST, FLIPADST_FLIPADST, V_FLIPADST,
    H_DCT,        H_ADST,        H_FLIPADST,        IDTX
  };
  if (tx_set_type != EXT_TX_SET_ALL16 &&
      tx_set_type != EXT_TX_SET_DTT9_IDTX_1DDCT)
    return;
#if CONFIG_NN_V2
  NN_CONFIG_V2 *nn_config_hor = av1_tx_type_nnconfig_map_hor[tx_size];
  NN_CONFIG_V2 *nn_config_ver = av1_tx_type_nnconfig_map_ver[tx_size];
#else
  const NN_CONFIG *nn_config_hor = av1_tx_type_nnconfig_map_hor[tx_size];
  const NN_CONFIG *nn_config_ver = av1_tx_type_nnconfig_map_ver[tx_size];
#endif
  if (!nn_config_hor || !nn_config_ver) return;  // Model not established yet.

  float hfeatures[16], vfeatures[16];
  float hscores[4], vscores[4];
  float scores_2D_raw[16];
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];
  const int hfeatures_num = bw <= 8 ? bw : bw / 2;
  const int vfeatures_num = bh <= 8 ? bh : bh / 2;
  assert(hfeatures_num <= 16);
  assert(vfeatures_num <= 16);

  const struct macroblock_plane *const p = &x->plane[0];
  const int diff_stride = block_size_wide[bsize];
  const int16_t *diff = p->src_diff + 4 * blk_row * diff_stride + 4 * blk_col;
  get_energy_distribution_finer(diff, diff_stride, bw, bh, hfeatures,
                                vfeatures);

  av1_get_horver_correlation_full(diff, diff_stride, bw, bh,
                                  &hfeatures[hfeatures_num - 1],
                                  &vfeatures[vfeatures_num - 1]);

#if CONFIG_NN_V2
  av1_nn_predict_v2(hfeatures, nn_config_hor, 0, hscores);
  av1_nn_predict_v2(vfeatures, nn_config_ver, 0, vscores);
#else
  av1_nn_predict(hfeatures, nn_config_hor, 1, hscores);
  av1_nn_predict(vfeatures, nn_config_ver, 1, vscores);
#endif

  for (int i = 0; i < 4; i++) {
    float *cur_scores_2D = scores_2D_raw + i * 4;
    cur_scores_2D[0] = vscores[i] * hscores[0];
    cur_scores_2D[1] = vscores[i] * hscores[1];
    cur_scores_2D[2] = vscores[i] * hscores[2];
    cur_scores_2D[3] = vscores[i] * hscores[3];
  }

  assert(TX_TYPES == 16);
  // This version of the function only works when there are at most 16 classes.
  // So we will need to change the optimization or use av1_nn_softmax instead if
  // this ever gets changed.
  av1_nn_fast_softmax_16(scores_2D_raw, scores_2D_raw);

  const float score_thresh =
      get_adaptive_thresholds(tx_size, tx_set_type, prune_2d_txfm_mode);

  // Always keep the TX type with the highest score, prune all others with
  // score below score_thresh.
  int max_score_i = 0;
  float max_score = 0.0f;
  uint16_t allow_bitmask = 0;
  float sum_score = 0.0;
  // Calculate sum of allowed tx type score and Populate allow bit mask based
  // on score_thresh and allowed_tx_mask
  int allow_count = 0;
  int tx_type_allowed[16] = { TX_TYPE_INVALID, TX_TYPE_INVALID, TX_TYPE_INVALID,
                              TX_TYPE_INVALID, TX_TYPE_INVALID, TX_TYPE_INVALID,
                              TX_TYPE_INVALID, TX_TYPE_INVALID, TX_TYPE_INVALID,
                              TX_TYPE_INVALID, TX_TYPE_INVALID, TX_TYPE_INVALID,
                              TX_TYPE_INVALID, TX_TYPE_INVALID, TX_TYPE_INVALID,
                              TX_TYPE_INVALID };
  float scores_2D[16] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  };
  for (int tx_idx = 0; tx_idx < TX_TYPES; tx_idx++) {
    const int allow_tx_type =
        check_bit_mask(*allowed_tx_mask, tx_type_table_2D[tx_idx]);
    if (!allow_tx_type) {
      continue;
    }
    if (scores_2D_raw[tx_idx] > max_score) {
      max_score = scores_2D_raw[tx_idx];
      max_score_i = tx_idx;
    }
    if (scores_2D_raw[tx_idx] >= score_thresh) {
      // Set allow mask based on score_thresh
      set_bit_mask(&allow_bitmask, tx_type_table_2D[tx_idx]);

      // Accumulate score of allowed tx type
      sum_score += scores_2D_raw[tx_idx];

      scores_2D[allow_count] = scores_2D_raw[tx_idx];
      tx_type_allowed[allow_count] = tx_type_table_2D[tx_idx];
      allow_count += 1;
    }
  }
  if (!check_bit_mask(allow_bitmask, tx_type_table_2D[max_score_i])) {
    // If even the tx_type with max score is pruned, this means that no other
    // tx_type is feasible. When this happens, we force enable max_score_i and
    // end the search.
    set_bit_mask(&allow_bitmask, tx_type_table_2D[max_score_i]);
    memcpy(txk_map, tx_type_table_2D, sizeof(tx_type_table_2D));
    *is_txk_map_populated = 1;
    *allowed_tx_mask = allow_bitmask;
    return;
  }

  // Sort tx type probability of all types
  if (allow_count <= 8) {
    av1_sort_fi32_8(scores_2D, tx_type_allowed);
  } else {
    av1_sort_fi32_16(scores_2D, tx_type_allowed);
  }

  // Enable more pruning based on tx type probability and number of allowed tx
  // types
  if (prune_2d_txfm_mode >= TX_TYPE_PRUNE_4) {
    float temp_score = 0.0;
    float score_ratio = 0.0;
    int tx_idx, tx_count = 0;
    const float inv_sum_score = 100 / sum_score;
    // Get allowed tx types based on sorted probability score and tx count
    for (tx_idx = 0; tx_idx < allow_count; tx_idx++) {
      // Skip the tx type which has more than 30% of cumulative
      // probability and allowed tx type count is more than 2
      if (score_ratio > 30.0 && tx_count >= 2) break;

      assert(check_bit_mask(allow_bitmask, tx_type_allowed[tx_idx]));
      // Calculate cumulative probability
      temp_score += scores_2D[tx_idx];

      // Calculate percentage of cumulative probability of allowed tx type
      score_ratio = temp_score * inv_sum_score;
      tx_count++;
    }
    // Set remaining tx types as pruned
    for (; tx_idx < allow_count; tx_idx++)
      unset_bit_mask(&allow_bitmask, tx_type_allowed[tx_idx]);
  }

  memcpy(txk_map, tx_type_allowed, sizeof(tx_type_table_2D));
  *is_txk_map_populated = 1;
  *allowed_tx_mask = allow_bitmask;
}

static float get_dev(float mean, double x2_sum, int num) {
  const float e_x2 = (float)(x2_sum / num);
  const float diff = e_x2 - mean * mean;
  const float dev = (diff > 0) ? sqrtf(diff) : 0;
  return dev;
}

#if TX_SEARCH_SSE2
// Same as aom_get_blk_sse_sum(data, stride, 4, 4, x_sum, x2_sum), exact for
// all int16_t inputs.
static AOM_FORCE_INLINE void sse_sum_4x4_sse2(const int16_t *data, int stride,
                                              int *x_sum, int64_t *x2_sum) {
  const __m128i r01 =
      _mm_unpacklo_epi64(_mm_loadl_epi64((const __m128i *)data),
                         _mm_loadl_epi64((const __m128i *)(data + stride)));
  const __m128i r23 =
      _mm_unpacklo_epi64(_mm_loadl_epi64((const __m128i *)(data + 2 * stride)),
                         _mm_loadl_epi64((const __m128i *)(data + 3 * stride)));
  const __m128i ones = _mm_set1_epi16(1);
  __m128i s1 =
      _mm_add_epi32(_mm_madd_epi16(r01, ones), _mm_madd_epi16(r23, ones));
  s1 = _mm_add_epi32(s1, _mm_srli_si128(s1, 8));
  s1 = _mm_add_epi32(s1, _mm_srli_si128(s1, 4));
  *x_sum = _mm_cvtsi128_si32(s1);
  // Each madd lane is a sum of two squares, at most 2^31, so it is exact when
  // read as uint32. Zero-extend to 64 bits before accumulating.
  const __m128i zero = _mm_setzero_si128();
  const __m128i q01 = _mm_madd_epi16(r01, r01);
  const __m128i q23 = _mm_madd_epi16(r23, r23);
  __m128i s2 = _mm_add_epi64(_mm_add_epi64(_mm_unpacklo_epi32(q01, zero),
                                           _mm_unpackhi_epi32(q01, zero)),
                             _mm_add_epi64(_mm_unpacklo_epi32(q23, zero),
                                           _mm_unpackhi_epi32(q23, zero)));
  s2 = _mm_add_epi64(s2, _mm_srli_si128(s2, 8));
  const uint32_t lo = (uint32_t)_mm_cvtsi128_si32(s2);
  const uint32_t hi = (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(s2, 4));
  *x2_sum = (int64_t)(((uint64_t)hi << 32) | lo);
}
#endif  // TX_SEARCH_SSE2

// Writes the features required by the ML model to predict tx split based on
// mean and standard deviation values of the block and sub-blocks.
// Returns the number of elements written to the output array which is at most
// 12 currently. Hence 'features' buffer should be able to accommodate at least
// 12 elements.
static inline int get_mean_dev_features(const int16_t *data, int stride, int bw,
                                        int bh, float *features) {
  const int16_t *const data_ptr = &data[0];
  const int subh = (bh >= bw) ? (bh >> 1) : bh;
  const int subw = (bw >= bh) ? (bw >> 1) : bw;
  const int num = bw * bh;
  const int sub_num = subw * subh;
  int feature_idx = 2;
  int total_x_sum = 0;
  int64_t total_x2_sum = 0;
  int num_sub_blks = 0;
  double mean2_sum = 0.0f;
  float dev_sum = 0.0f;

  for (int row = 0; row < bh; row += subh) {
    for (int col = 0; col < bw; col += subw) {
      int x_sum;
      int64_t x2_sum;
#if TX_SEARCH_SSE2
      if (subw == 4 && subh == 4) {
        sse_sum_4x4_sse2(data_ptr + row * stride + col, stride, &x_sum,
                         &x2_sum);
      } else
#endif
      {
        aom_get_blk_sse_sum(data_ptr + row * stride + col, stride, subw, subh,
                            &x_sum, &x2_sum);
      }
      total_x_sum += x_sum;
      total_x2_sum += x2_sum;

      const float mean = (float)x_sum / sub_num;
      const float dev = get_dev(mean, (double)x2_sum, sub_num);
      features[feature_idx++] = mean;
      features[feature_idx++] = dev;
      mean2_sum += (double)(mean * mean);
      dev_sum += dev;
      num_sub_blks++;
    }
  }

  const float lvl0_mean = (float)total_x_sum / num;
  features[0] = lvl0_mean;
  features[1] = get_dev(lvl0_mean, (double)total_x2_sum, num);

  // Deviation of means.
  features[feature_idx++] = get_dev(lvl0_mean, mean2_sum, num_sub_blks);
  // Mean of deviations.
  features[feature_idx++] = dev_sum / num_sub_blks;

  return feature_idx;
}

static int ml_predict_tx_split(MACROBLOCK *x, BLOCK_SIZE bsize, int blk_row,
                               int blk_col, TX_SIZE tx_size) {
  const NN_CONFIG *nn_config = av1_tx_split_nnconfig_map[tx_size];
  if (!nn_config) return -1;

  const int diff_stride = block_size_wide[bsize];
  const int16_t *diff =
      x->plane[0].src_diff + 4 * blk_row * diff_stride + 4 * blk_col;
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];

  float features[64] = { 0.0f };
  get_mean_dev_features(diff, diff_stride, bw, bh, features);

  float score = 0.0f;
  av1_nn_predict(features, nn_config, 1, &score);

  int int_score = (int)(score * 10000);
  return clamp(int_score, -80000, 80000);
}

static inline const int *get_tx_type_probs_ppi(const AV1_PRIMARY *ppi,
                                               FRAME_UPDATE_TYPE update_type,
                                               TX_SIZE tx_size) {
#if CONFIG_FPMT_TEST
  if (ppi->fpmt_unit_test_cfg == PARALLEL_SIMULATION_ENCODE) {
    return (const int *)
        ppi->temp_frame_probs.tx_type_probs[update_type][tx_size];
  }
#endif
  return ppi->frame_probs.tx_type_probs[update_type][tx_size];
}

typedef struct {
  uint64_t txk_order;
  int num_candidates;
} TxMaskOrder;

// Returns the intra prediction direction used to restrict the transform set.
static inline PREDICTION_MODE get_tx_search_intra_dir(
    const MB_MODE_INFO *mbmi) {
  return mbmi->filter_intra_mode_info.use_filter_intra
             ? fimode_to_intradir[mbmi->filter_intra_mode_info
                                      .filter_intra_mode]
             : mbmi->mode;
}

// Returns the mask of transform types in tx_set_type, restricted by the
// use_reduced_intra_txset speed feature. Shared by get_tx_mask() and
// get_default_intra_tx_type().
static inline uint16_t get_ext_tx_used_flag(const AV1_COMP *cpi,
                                            TxSetType tx_set_type,
                                            PREDICTION_MODE intra_dir) {
  const int use_reduced_intra_txset =
      cpi->sf.tx_sf.tx_type_search.use_reduced_intra_txset;
  uint16_t ext_tx_used_flag =
      use_reduced_intra_txset != 0 && tx_set_type == EXT_TX_SET_DTT4_IDTX_1DDCT
          ? av1_reduced_intra_tx_used_flag[intra_dir]
          : av1_ext_tx_used_flag[tx_set_type];
  if (use_reduced_intra_txset == 2)
    ext_tx_used_flag &= av1_derived_intra_tx_used_flag[intra_dir];
  return ext_tx_used_flag;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static uint32_t get_tx_mask(const AV1_COMP *cpi, MACROBLOCK *x,
                            uint64_t blk_pack, const TXB_CTX *const txb_ctx,
                            int64_t ref_best_rd, TxMaskOrder *order_out) {
  const int plane = (uint8_t)(blk_pack >> 32);
  const TX_SIZE tx_size = (TX_SIZE)(uint8_t)(blk_pack >> 48);
  const int byte7 = (uint8_t)(blk_pack >> 56);
  const TX_TYPE uv_tx_type = (TX_TYPE)byte7;
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const TxfmSearchParams *txfm_params = &x->txfm_search_params;
  const int is_inter = is_inter_block(mbmi);
  const int fast_tx_search = byte7 & FTXS_DCT_AND_1D_DCT_ONLY;
  // if txk_allowed = TX_TYPES, >1 tx types are allowed, else, if txk_allowed <
  // TX_TYPES, only that specific tx type is allowed.
  TX_TYPE txk_allowed = TX_TYPES;
  int txk_map[TX_TYPES];
  int is_txk_map_populated = 0;

  if (UNLIKELY(plane)) {
    // tx_type of PLANE_TYPE_UV should be the same as PLANE_TYPE_Y
    txk_allowed = uv_tx_type;
  } else if ((!is_inter && txfm_params->use_default_intra_tx_type) ||
             (UNLIKELY(is_inter) &&
              txfm_params->default_inter_tx_type_prob_thresh == 0)) {
    txk_allowed =
        get_default_tx_type(0, xd, tx_size, cpi->use_screen_content_tools);
  } else if (UNLIKELY(is_inter &&
                      txfm_params->default_inter_tx_type_prob_thresh !=
                          INT_MAX)) {
    const AV1_PRIMARY *const ppi = cpi->ppi;
    const FRAME_UPDATE_TYPE update_type =
        get_frame_update_type(&ppi->gf_group, cpi->gf_frame_index);
    const int *const tx_type_probs =
        get_tx_type_probs_ppi(ppi, update_type, tx_size);
    if (tx_type_probs[DEFAULT_INTER_TX_TYPE] >
        txfm_params->default_inter_tx_type_prob_thresh) {
      txk_allowed = DEFAULT_INTER_TX_TYPE;
    } else {
      int force_tx_type = 0;
      int max_prob = 0;
      const int tx_type_prob_threshold =
          txfm_params->default_inter_tx_type_prob_thresh +
          PROB_THRESH_OFFSET_TX_TYPE;
      for (int i = 1; i < TX_TYPES; i++) {  // find maximum probability.
        if (tx_type_probs[i] > max_prob) {
          max_prob = tx_type_probs[i];
          force_tx_type = i;
        }
      }
      if (max_prob > tx_type_prob_threshold)  // force tx type with max prob.
        txk_allowed = force_tx_type;
      else if (x->rd_model == LOW_TXFM_RD) {
        txk_allowed = DCT_DCT;
      }
    }
  } else if (UNLIKELY(x->rd_model == LOW_TXFM_RD)) {
    txk_allowed = DCT_DCT;
  }

  if (txk_allowed == DCT_DCT) {
    return (1u << DCT_DCT) | ((uint32_t)DCT_DCT << 16);
  }

  const TxSetType tx_set_type = av1_get_ext_tx_set_type(
      tx_size, is_inter, cm->features.reduced_tx_set_used);
  const PREDICTION_MODE intra_dir = get_tx_search_intra_dir(mbmi);
  uint16_t ext_tx_used_flag = get_ext_tx_used_flag(cpi, tx_set_type, intra_dir);

  if (xd->lossless[mbmi->segment_id] || txsize_sqr_up_map[tx_size] > TX_32X32 ||
      ext_tx_used_flag == 0x0001 ||
      (is_inter && cpi->oxcf.txfm_cfg.use_inter_dct_only) ||
      (!is_inter && cpi->oxcf.txfm_cfg.use_intra_dct_only)) {
    txk_allowed = DCT_DCT;
  }

  if (cpi->oxcf.txfm_cfg.enable_flip_idtx == 0)
    ext_tx_used_flag &= DCT_ADST_TX_MASK;

  uint16_t allowed_tx_mask = 0;  // 1: allow; 0: skip.
  if (txk_allowed < TX_TYPES) {
    allowed_tx_mask = 1 << txk_allowed;
    allowed_tx_mask &= ext_tx_used_flag;
  } else if (UNLIKELY(fast_tx_search)) {
    allowed_tx_mask = 0x0c01;  // V_DCT, H_DCT, DCT_DCT
    allowed_tx_mask &= ext_tx_used_flag;
  } else if (UNLIKELY(!is_inter &&
                      txfm_params->use_derived_intra_tx_type_set)) {
    allowed_tx_mask = av1_derived_intra_tx_used_flag[intra_dir];
    allowed_tx_mask &= ext_tx_used_flag;
  } else {
    assert(plane == 0);
    allowed_tx_mask = ext_tx_used_flag;
    int num_allowed = 0;
    int i;

    if (cpi->sf.tx_sf.tx_type_search.prune_tx_type_using_stats) {
      const AV1_PRIMARY *const ppi = cpi->ppi;
      const FRAME_UPDATE_TYPE update_type =
          get_frame_update_type(&ppi->gf_group, cpi->gf_frame_index);
      const int *const tx_type_probs =
          get_tx_type_probs_ppi(ppi, update_type, tx_size);
      static const int thresh_arr[2][7] = { { 10, 15, 15, 10, 15, 15, 15 },
                                            { 10, 17, 17, 10, 17, 17, 17 } };
      const int thresh =
          thresh_arr[cpi->sf.tx_sf.tx_type_search.prune_tx_type_using_stats - 1]
                    [update_type];
#if TX_SEARCH_SSE2
      const __m128i p0 = _mm_loadu_si128((const __m128i *)(tx_type_probs + 0));
      const __m128i p1 = _mm_loadu_si128((const __m128i *)(tx_type_probs + 4));
      const __m128i p2 = _mm_loadu_si128((const __m128i *)(tx_type_probs + 8));
      const __m128i p3 = _mm_loadu_si128((const __m128i *)(tx_type_probs + 12));
      const __m128i v_thresh = _mm_set1_epi16((int16_t)thresh);
      const __m128i cmp01 = _mm_cmplt_epi16(_mm_packs_epi32(p0, p1), v_thresh);
      const __m128i cmp23 = _mm_cmplt_epi16(_mm_packs_epi32(p2, p3), v_thresh);
      const uint16_t prune =
          (uint16_t)_mm_movemask_epi8(_mm_packs_epi16(cmp01, cmp23));
      const uint16_t unpruned = allowed_tx_mask & (uint16_t)(~prune);
      if (LIKELY(unpruned != 0)) {
        allowed_tx_mask = unpruned;
      } else {
        int max_prob = -1;
        int max_idx = 0;
        for (i = 0; i < TX_TYPES; i++) {
          if (tx_type_probs[i] > max_prob && (allowed_tx_mask & (1 << i))) {
            max_prob = tx_type_probs[i];
            max_idx = i;
          }
        }
        allowed_tx_mask = (uint16_t)(1 << max_idx);
      }
#else
      uint16_t prune = 0;
      int max_prob = -1;
      int max_idx = 0;
      for (i = 0; i < TX_TYPES; i++) {
        if (tx_type_probs[i] > max_prob && (allowed_tx_mask & (1 << i))) {
          max_prob = tx_type_probs[i];
          max_idx = i;
        }
        if (tx_type_probs[i] < thresh) prune |= (1 << i);
      }
      if ((prune >> max_idx) & 0x01) prune &= ~(1 << max_idx);
      allowed_tx_mask &= (~prune);
#endif
    }

    if (UNLIKELY(cpi->sf.tx_sf.tx_type_search.prune_tx_type_est_rd)) {
#if defined(__GNUC__) || defined(__clang__)
      num_allowed = __builtin_popcount(allowed_tx_mask);
#else
      for (i = 0; i < TX_TYPES; i++) {
        if (allowed_tx_mask & (1 << i)) num_allowed++;
      }
#endif
      assert(num_allowed > 0);
      if (num_allowed > 2) {
        const int block = (uint16_t)blk_pack;
        const int blk_row = (uint8_t)(blk_pack >> 16);
        const int blk_col = (uint8_t)(blk_pack >> 24);
        const BLOCK_SIZE plane_bsize = (BLOCK_SIZE)(uint8_t)(blk_pack >> 40);
        int pf = prune_factors[txfm_params->prune_2d_txfm_mode];
        int mf = mul_factors[txfm_params->prune_2d_txfm_mode];
        if (num_allowed <= 7) {
          const uint16_t prune =
              prune_txk_type(cpi, x, plane, block, tx_size, blk_row, blk_col,
                             plane_bsize, txk_map, allowed_tx_mask, pf, txb_ctx,
                             cm->features.reduced_tx_set_used);
          allowed_tx_mask &= (~prune);
        } else {
          const int num_sel = (num_allowed * mf + 50) / 100;
          const uint16_t prune = prune_txk_type_separ(
              cpi, x, plane, block, tx_size, blk_row, blk_col, plane_bsize,
              txk_map, allowed_tx_mask, pf, txb_ctx,
              cm->features.reduced_tx_set_used, ref_best_rd, num_sel);

          allowed_tx_mask &= (~prune);
        }
        is_txk_map_populated = 1;
      }
    } else if (UNLIKELY(txfm_params->prune_2d_txfm_mode >= TX_TYPE_PRUNE_1 &&
                        is_inter)) {
#if defined(__GNUC__) || defined(__clang__)
      num_allowed = __builtin_popcount(allowed_tx_mask);
#else
      for (i = 0; i < TX_TYPES; i++) {
        if (allowed_tx_mask & (1 << i)) num_allowed++;
      }
#endif
      assert(num_allowed > 0);
      int allowed_tx_count =
          (txfm_params->prune_2d_txfm_mode >= TX_TYPE_PRUNE_4) ? 1 : 5;
      // !fast_tx_search && txk_end != txk_start && plane == 0
      if (num_allowed > allowed_tx_count) {
        const int blk_row = (uint8_t)(blk_pack >> 16);
        const int blk_col = (uint8_t)(blk_pack >> 24);
        const BLOCK_SIZE plane_bsize = (BLOCK_SIZE)(uint8_t)(blk_pack >> 40);
        prune_tx_2D(x, plane_bsize, tx_size, blk_row, blk_col, tx_set_type,
                    txfm_params->prune_2d_txfm_mode, txk_map, &allowed_tx_mask,
                    &is_txk_map_populated);
      }
    }
  }

  // Need to have at least one transform type allowed.
  if (UNLIKELY(allowed_tx_mask == 0)) {
    txk_allowed = (plane ? uv_tx_type : DCT_DCT);
    allowed_tx_mask = (1 << txk_allowed);
  } else if (UNLIKELY(is_txk_map_populated)) {
    uint64_t order = 0;
    int shift = 0;
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 1
#endif
    for (int idx = 0; idx < TX_TYPES; ++idx) {
      const int tx_type = txk_map[idx];
      if (tx_type != TX_TYPE_INVALID &&
          check_bit_mask(allowed_tx_mask, tx_type)) {
        order |= (uint64_t)tx_type << shift;
        shift += 4;
      }
    }
    order_out->txk_order = order;
    order_out->num_candidates = shift >> 2;
  }

  assert(IMPLIES(txk_allowed < TX_TYPES, allowed_tx_mask == 1 << txk_allowed));
  return (uint32_t)allowed_tx_mask | ((uint32_t)txk_allowed << 16);
}

#if CONFIG_RD_DEBUG
static inline void update_txb_coeff_cost(RD_STATS *rd_stats, int plane,
                                         int txb_coeff_cost) {
  rd_stats->txb_coeff_cost[plane] += txb_coeff_cost;
}
#endif

static inline int cost_coeffs(MACROBLOCK *x, int plane, int block,
                              TX_SIZE tx_size, const TX_TYPE tx_type,
                              const TXB_CTX *const txb_ctx,
                              int reduced_tx_set_used) {
#if TXCOEFF_COST_TIMER
  struct aom_usec_timer timer;
  aom_usec_timer_start(&timer);
#endif
  const int cost = av1_cost_coeffs_txb(x, plane, block, tx_size, tx_type,
                                       txb_ctx, reduced_tx_set_used);
#if TXCOEFF_COST_TIMER
  AV1_COMMON *tmp_cm = (AV1_COMMON *)&cpi->common;
  aom_usec_timer_mark(&timer);
  const int64_t elapsed_time = aom_usec_timer_elapsed(&timer);
  tmp_cm->txcoeff_cost_timer += elapsed_time;
  ++tmp_cm->txcoeff_cost_count;
#endif
  return cost;
}

// Sets the RD stats, eob and entropy context of a transform block predicted
// to be skipped (all coefficients zero). skip_ctx is the txb_skip_ctx of the
// block and skip_dist its distortion (the block SSE scaled by 16). Shared by
// predict_dc_only_block() and the search_tx_type() fast path.
static inline void set_dc_only_skip_rd_stats(MACROBLOCK *x, int plane,
                                             int block, TX_SIZE tx_size,
                                             int skip_ctx, int64_t skip_dist,
                                             RD_STATS *best_rd_stats) {
#if CONFIG_RD_DEBUG
  av1_invalid_rd_stats(best_rd_stats);
#else
  best_rd_stats->zero_rate = 0;
#endif
  best_rd_stats->skip_txfm = 1;
  x->plane[plane].eobs[block] = 0;
  best_rd_stats->dist = skip_dist;
  best_rd_stats->sse = skip_dist;
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  const int zero_blk_rate =
      x->coeff_costs.coeff_costs[txs_ctx][get_plane_type(plane)]
          .txb_skip_cost[skip_ctx][1];
  best_rd_stats->rate = zero_blk_rate;
  best_rd_stats->rdcost = RDCOST(x->rdmult, zero_blk_rate, skip_dist);
  x->plane[plane].txb_entropy_ctx[block] = 0;
}

// Predict DC only blocks if the residual variance is below a qstep based
// threshold. For such blocks, transform type search is bypassed.
// Returns 1 if the block is predicted as skip_txfm, 0 otherwise.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int predict_dc_only_block(MACROBLOCK *x, int plane,
                                 BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                                 int block, int blk_row, int blk_col, int qstep,
                                 int is_hbd, RD_STATS *best_rd_stats,
                                 int64_t *block_sse, unsigned int *block_mse_q8,
                                 int64_t *per_px_mean, int *dc_only_blk) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const BLOCK_SIZE tx_bsize = txsize_to_bsize[tx_size];
  int visible_rows, visible_cols;
  const int txb_cols = block_size_wide[tx_bsize];
  const int txb_rows = block_size_high[tx_bsize];
  const int is_border = get_visible_dimensions(
      x, plane, plane_bsize, blk_col, blk_row, txb_cols, txb_rows,
      /*clip_dims=*/true, &visible_cols, &visible_rows);

  const uint64_t var_threshold = ((uint64_t)(qstep * qstep) * 9) / 5;
  int64_t abs_mean = 0;
  int sum = 0;
  if (LIKELY(visible_cols > 0 && visible_rows > 0)) {
    const int diff_stride = block_size_wide[plane_bsize];
    const int16_t *diff = x->plane[plane].src_diff +
                          ((blk_row * diff_stride + blk_col) << MI_SIZE_LOG2);
    const uint64_t sse =
        aom_sum_sse_2d_i16(diff, diff_stride, visible_cols, visible_rows, &sum);
    *block_sse = sse;
    uint64_t block_var;
    if (LIKELY(!is_border)) {
      const int log2_n = num_pels_log2_lookup[tx_bsize];
      *block_mse_q8 = (unsigned int)((sse << 8) >> log2_n);
      block_var = sse - (((uint64_t)((int64_t)sum * sum)) >> log2_n);
      if (UNLIKELY(is_hbd))
        block_var = ROUND_POWER_OF_TWO(block_var, (xd->bd - 8) * 2);
      if (LIKELY(block_var >= var_threshold)) return 0;
      abs_mean = (int64_t)(abs(sum) >> log2_n) << 7;
    } else {
      double norm_factor = 1.0 / (visible_cols * visible_rows);
      *block_mse_q8 = (unsigned int)(norm_factor * (256 * sse));
      block_var = (uint64_t)(sse - (uint64_t)(norm_factor * sum * sum));
      if (UNLIKELY(is_hbd))
        block_var = ROUND_POWER_OF_TWO(block_var, (xd->bd - 8) * 2);
      if (LIKELY(block_var >= var_threshold)) return 0;
      abs_mean = (int64_t)(norm_factor * abs(sum)) << 7;
    }
  } else {
    *block_sse = 0;
    *block_mse_q8 = 0;
    if (LIKELY(0 >= var_threshold)) return 0;
  }

  const int dc_qstep = x->plane[plane].dequant_QTX[0] >> 3;
  const unsigned int predict_dc_level = x->txfm_search_params.predict_dc_level;
  assert(predict_dc_level != 0);

  // Prediction of skip block if residual mean and variance are less
  // than qstep based threshold
  if ((abs_mean * dc_coeff_scale[tx_size]) < (dc_qstep << 12)) {
    // If the normalized mean of residual block is less than the dc qstep and
    // the  normalized block variance is less than ac qstep, then the block is
    // assumed to be a skip block and its rdcost is updated accordingly.
    if (UNLIKELY(is_hbd))
      *block_sse = ROUND_POWER_OF_TWO((*block_sse), (xd->bd - 8) * 2);

    TXB_CTX txb_ctx_tmp;
    get_txb_ctx_fast(plane_bsize, tx_size, plane,
                     xd->plane[plane].above_entropy_context,
                     xd->plane[plane].left_entropy_context, &txb_ctx_tmp);
    set_dc_only_skip_rd_stats(x, plane, block, tx_size,
                              txb_ctx_tmp.txb_skip_ctx, (*block_sse) << 4,
                              best_rd_stats);
    return 1;
  } else if (predict_dc_level > 1) {
    // Predict DC only blocks based on residual variance.
    // For chroma plane, this prediction is disabled for intra blocks.
    if ((plane == 0) || (plane > 0 && is_inter_block(mbmi))) {
      *dc_only_blk = 1;
      *per_px_mean = (sum > 0) ? abs_mean : -abs_mean;
    }
  }
  return 0;
}

static inline void dist_block_tx_domain_fast(
    MACROBLOCK *x, const tran_low_t *coeff, const tran_low_t *dqcoeff,
    int buffer_length, int shift, int is_hbd, TX_SIZE tx_size, TX_TYPE tx_type,
    const qm_val_t *qmatrix, const qm_val_t *qmatrix_scan, int64_t *out_dist,
    int64_t *out_sse) {
  (void)qmatrix_scan;
  int64_t this_sse;
  int64_t *const sse_ptr = out_sse ? &this_sse : NULL;
#if CONFIG_AV1_HIGHBITDEPTH
  if (UNLIKELY(is_hbd)) {
    MACROBLOCKD *const xd = &x->e_mbd;
    if (qmatrix == NULL || !x->txfm_search_params.use_qm_dist_metric) {
      *out_dist = av1_highbd_block_error(coeff, dqcoeff, buffer_length,
                                         &this_sse, xd->bd);
    } else {
      const int16_t *const scan = get_scan(tx_size, tx_type)->scan;
      *out_dist = block_error_qm_fast(coeff, dqcoeff, buffer_length, qmatrix,
                                      scan, sse_ptr, xd->bd);
    }
  } else
#else
  (void)is_hbd;
#endif
  {
    if (LIKELY(qmatrix == NULL || !x->txfm_search_params.use_qm_dist_metric)) {
      *out_dist = block_error_lbd(coeff, dqcoeff, buffer_length, &this_sse,
                                  sse_ptr != NULL);
    } else {
      const int16_t *const scan = get_scan(tx_size, tx_type)->scan;
      *out_dist = block_error_qm_fast(coeff, dqcoeff, buffer_length, qmatrix,
                                      scan, sse_ptr, 8);
    }
  }

  *out_dist = RIGHT_SIGNED_SHIFT(*out_dist, shift);
  if (out_sse) *out_sse = RIGHT_SIGNED_SHIFT(this_sse, shift);
}

static AOM_FORCE_INLINE void quantize_lbd_fast(
    const AV1_COMP *cpi, const tran_low_t *coeff_ptr, intptr_t n_coeffs,
    const MACROBLOCK_PLANE *p, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr,
    uint16_t *eob_ptr, const SCAN_ORDER *sc, const qm_val_t *qm_ptr,
    const qm_val_t *iqm_ptr, int log_scale, int this_skip_trellis,
    TX_SIZE tx_size) {
#if USE_B_QUANT_NO_TRELLIS
  if (this_skip_trellis) {
    const int quant_b_adapt = cpi->oxcf.q_cfg.quant_b_adapt;
    if (LIKELY(!quant_b_adapt) && qm_ptr != NULL && iqm_ptr != NULL) {
      av1_quantize_b_qm(coeff_ptr, n_coeffs, p->zbin_QTX, p->round_QTX,
                        p->quant_QTX, p->quant_shift_QTX, qcoeff_ptr,
                        dqcoeff_ptr, p->dequant_QTX, eob_ptr, sc->scan,
                        sc->iscan, qm_ptr, iqm_ptr, log_scale);
    } else {
      QUANT_PARAM qparam;
      qparam.log_scale = log_scale;
      qparam.tx_size = tx_size;
      qparam.qmatrix = qm_ptr;
      qparam.iqmatrix = iqm_ptr;
      qparam.use_quant_b_adapt = quant_b_adapt;
      qparam.use_optimize_b = 0;
      qparam.xform_quant_idx = AV1_XFORM_QUANT_B;
      av1_quantize_b_facade(coeff_ptr, n_coeffs, p, qcoeff_ptr, dqcoeff_ptr,
                            eob_ptr, sc, &qparam);
    }
    return;
  }
#else
  (void)cpi;
  (void)this_skip_trellis;
  (void)tx_size;
#endif
  if (qm_ptr != NULL && iqm_ptr != NULL) {
    av1_quantize_fp_qm(coeff_ptr, n_coeffs, p->round_fp_QTX, p->quant_fp_QTX,
                       qcoeff_ptr, dqcoeff_ptr, p->dequant_QTX, eob_ptr,
                       sc->scan, sc->iscan, qm_ptr, iqm_ptr, log_scale);
  } else if (LIKELY(log_scale == 0)) {
    av1_quantize_fp(coeff_ptr, n_coeffs, p->zbin_QTX, p->round_fp_QTX,
                    p->quant_fp_QTX, p->quant_shift_QTX, qcoeff_ptr,
                    dqcoeff_ptr, p->dequant_QTX, eob_ptr, sc->scan, sc->iscan);
  } else if (log_scale == 1) {
    av1_quantize_fp_32x32(coeff_ptr, n_coeffs, p->zbin_QTX, p->round_fp_QTX,
                          p->quant_fp_QTX, p->quant_shift_QTX, qcoeff_ptr,
                          dqcoeff_ptr, p->dequant_QTX, eob_ptr, sc->scan,
                          sc->iscan);
  } else {
    av1_quantize_fp_64x64(coeff_ptr, n_coeffs, p->zbin_QTX, p->round_fp_QTX,
                          p->quant_fp_QTX, p->quant_shift_QTX, qcoeff_ptr,
                          dqcoeff_ptr, p->dequant_QTX, eob_ptr, sc->scan,
                          sc->iscan);
  }
}

// Returns the only transform type searched for intra luma blocks when
// use_default_intra_tx_type is set: get_default_tx_type() if the transform set
// and speed features allow it, DCT_DCT otherwise. For non-lossless blocks with
// txw and txh <= 32 and use_intra_dct_only off, this matches get_tx_mask().
// Shared by search_tx_type_general() and the search_tx_type() fast path.
static inline TX_TYPE get_default_intra_tx_type(const AV1_COMP *cpi,
                                                const MACROBLOCKD *xd,
                                                TX_SIZE tx_size,
                                                int reduced_tx_set_used) {
  TX_TYPE txk_allowed =
      get_default_tx_type(0, xd, tx_size, cpi->use_screen_content_tools);
  if (txk_allowed != DCT_DCT) {
    const TxSetType tx_set_type =
        av1_get_ext_tx_set_type(tx_size, 0, reduced_tx_set_used);
    uint16_t ext_flag = get_ext_tx_used_flag(
        cpi, tx_set_type, get_tx_search_intra_dir(xd->mi[0]));
    if (cpi->oxcf.txfm_cfg.enable_flip_idtx == 0) ext_flag &= DCT_ADST_TX_MASK;
    if (!((ext_flag >> txk_allowed) & 1u)) txk_allowed = DCT_DCT;
  }
  return txk_allowed;
}

// Appends the transform types set in mask (non-zero) to *txk_order, which must
// be 0 on entry, 4 bits per type in increasing order. Returns the number of
// transform types.
static AOM_FORCE_INLINE int tx_mask_to_txk_order(uint32_t mask,
                                                 uint64_t *txk_order) {
  assert(mask != 0);
  int shift = 0;
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 1
#endif
  do {
    *txk_order |= (uint64_t)
#if defined(__GNUC__) || defined(__clang__)
                      __builtin_ctz(mask)
#else
                      (aom_clzll(mask & -(int64_t)mask) ^ 63)
#endif
                  << shift;
    shift += 4;
    mask &= mask - 1;
  } while (mask);
  return shift >> 2;
}

// Runs trellis optimization on a quantized block whose eob before
// optimization is pre_opt_eob (> 0). Returns the rate of the optimized block.
// Shared by all search_tx_type*() variants.
static AOM_FORCE_INLINE int optimize_txb_rate(const AV1_COMP *cpi,
                                              MACROBLOCK *x, int plane,
                                              int block, TX_SIZE tx_size,
                                              TX_TYPE tx_type,
                                              const TXB_CTX *const txb_ctx,
                                              int pre_opt_eob, int sharpness) {
  int rate_cost;
  x->plane[plane].txb_entropy_ctx[block] = 0;
  if (pre_opt_eob > 1) {
    av1_optimize_txb_multi(cpi, x, plane, block, tx_size, tx_type, txb_ctx,
                           &rate_cost, sharpness);
  } else {
    av1_optimize_txb(cpi, x, plane, block, tx_size, tx_type, txb_ctx,
                     &rate_cost, sharpness);
  }
  return rate_cost;
}

// Search for the best transform type for a given transform block.
// This function can be used for both inter and intra, both luma and chroma.
// It handles all cases; search_tx_type() routes the common 8-bit cases to
// faster specialized paths.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void search_tx_type_general(const AV1_COMP *cpi, MACROBLOCK *x,
                                   uint64_t blk_pack,
                                   const TXB_CTX *const txb_ctx,
                                   int64_t ref_best_rd,
                                   RD_STATS *best_rd_stats) {
  const int block = (uint16_t)blk_pack;
  const int blk_row = (uint8_t)(blk_pack >> 16);
  const int blk_col = (uint8_t)(blk_pack >> 24);
  const int plane = (uint8_t)(blk_pack >> 32);
  const BLOCK_SIZE plane_bsize = (BLOCK_SIZE)(uint8_t)(blk_pack >> 40);
  const TX_SIZE tx_size = (TX_SIZE)(uint8_t)(blk_pack >> 48);
  const uint64_t base_px_pack = blk_pack & 0x00FFFFFFFFFFFFFFULL;
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const TxfmSearchParams *txfm_params = &x->txfm_search_params;
  int64_t best_rd = INT64_MAX;
  uint16_t best_eob = 0;
  TX_TYPE best_tx_type = DCT_DCT;
  int rate_cost = 0;
  struct macroblock_plane *const p = &x->plane[plane];
  tran_low_t *orig_dqcoeff = p->dqcoeff;
  tran_low_t *best_dqcoeff = x->dqcoeff_buf;

  const int seg_id = mbmi->segment_id;
  int skip_trellis =
      !is_trellis_used(cpi->optimize_seg_arr[seg_id], DRY_RUN_NORMAL);

  uint8_t best_txb_ctx = 0;
  // txk_allowed = TX_TYPES: >1 tx types are allowed
  // txk_allowed < TX_TYPES: only that specific tx type is allowed.
  TX_TYPE txk_allowed = TX_TYPES;
  uint64_t txk_order = 0;
  int num_tx_candidates = 0;
  const int bd = xd->bd;
  const int is_hbd = is_cur_buf_hbd(xd);
  const int qstep = p->dequant_QTX[1] >> (bd - 5);

  const uint8_t txw = tx_size_wide[tx_size];
  const uint8_t txh = tx_size_high[tx_size];
  int64_t block_sse;
  unsigned int block_mse_q8;
  int dc_only_blk = 0;
  const bool predict_dc_block =
      txfm_params->predict_dc_level >= 1 && ((txw | txh) & 64) == 0;
  int64_t per_px_mean = INT64_MAX;

  int is_border_block = 0;
  if (UNLIKELY(cpi->do_border_pad)) {
    is_border_block =
        get_visible_dimensions(x, plane, plane_bsize, blk_col, blk_row, txw,
                               txh, /*clip_dims=*/true, NULL, NULL);
    if (is_border_block)
      av1_subtract_txb(x, plane, plane_bsize, blk_col, blk_row, tx_size,
                       best_tx_type, cpi->do_border_pad);
  }

  if (LIKELY(predict_dc_block)) {
    if (UNLIKELY(predict_dc_only_block(x, plane, plane_bsize, tx_size, block,
                                       blk_row, blk_col, qstep, is_hbd,
                                       best_rd_stats, &block_sse, &block_mse_q8,
                                       &per_px_mean, &dc_only_blk))) {
      if (plane == 0)
        xd->tx_type_map[blk_row * xd->tx_type_map_stride + blk_col] = DCT_DCT;
      return;
    }
  } else {
    block_sse = av1_pixel_diff_dist(x, plane, blk_row, blk_col, plane_bsize,
                                    txsize_to_bsize[tx_size], &block_mse_q8);
    assert(block_mse_q8 != UINT_MAX);
  }

  const int lossless = xd->lossless[seg_id];
  const int is_inter = is_inter_block(mbmi);
  const int reduced_tx_set_used = cm->features.reduced_tx_set_used;
  const TX_TYPE uv_tx_type =
      plane ? av1_get_tx_type(xd, PLANE_TYPE_UV, blk_row, blk_col, tx_size,
                              reduced_tx_set_used)
            : DCT_DCT;

  // Bit mask to indicate which transform types are allowed in the RD search.
  uint16_t tx_mask;

  // Use DCT_DCT transform for DC only block.
  if (UNLIKELY(dc_only_blk || cpi->sf.rt_sf.dct_only_palette_nonrd == 1)) {
    tx_mask = 1 << DCT_DCT;
    txk_order = DCT_DCT;
    num_tx_candidates = 1;
  } else if (plane) {
    if (LIKELY(uv_tx_type == DCT_DCT)) {
      txk_allowed = DCT_DCT;
      tx_mask = 1 << DCT_DCT;
      txk_order = DCT_DCT;
      num_tx_candidates = 1;
    } else {
      TxMaskOrder mask_order = { 0, 0 };
      const uint32_t mres = get_tx_mask(
          cpi, x, base_px_pack | ((uint64_t)(uint8_t)uv_tx_type << 56), txb_ctx,
          ref_best_rd, &mask_order);
      tx_mask = (uint16_t)mres;
      txk_allowed = (TX_TYPE)(mres >> 16);
      txk_order = mask_order.txk_order;
      num_tx_candidates = mask_order.num_candidates;
    }
  } else if (UNLIKELY(lossless || txsize_sqr_up_map[tx_size] > TX_32X32 ||
                      (is_inter ? cpi->oxcf.txfm_cfg.use_inter_dct_only
                                : cpi->oxcf.txfm_cfg.use_intra_dct_only))) {
    txk_allowed = DCT_DCT;
    tx_mask = 1 << DCT_DCT;
    txk_order = DCT_DCT;
    num_tx_candidates = 1;
  } else if (!is_inter && txfm_params->use_default_intra_tx_type) {
    txk_allowed =
        get_default_intra_tx_type(cpi, xd, tx_size, reduced_tx_set_used);
    tx_mask = (uint16_t)(1u << txk_allowed);
    txk_order = (uint64_t)txk_allowed;
    num_tx_candidates = 1;
  } else {
    TxMaskOrder mask_order = { 0, 0 };
    const uint32_t mres =
        get_tx_mask(cpi, x, blk_pack, txb_ctx, ref_best_rd, &mask_order);
    tx_mask = (uint16_t)mres;
    txk_allowed = (TX_TYPE)(mres >> 16);
    txk_order = mask_order.txk_order;
    num_tx_candidates = mask_order.num_candidates;
  }
  const uint16_t allowed_tx_mask = tx_mask;

  if (UNLIKELY(is_hbd)) {
    block_sse = ROUND_POWER_OF_TWO(block_sse, (bd - 8) * 2);
    block_mse_q8 = ROUND_POWER_OF_TWO(block_mse_q8, (bd - 8) * 2);
  }
  block_sse *= 16;
  const int perform_block_coeff_opt =
      ((uint64_t)block_mse_q8 <=
       (uint64_t)txfm_params->coeff_opt_thresholds[0] * qstep * qstep);
  skip_trellis |= !perform_block_coeff_opt;

  int use_transform_domain_distortion =
      (txfm_params->use_transform_domain_distortion > 0) &&
      (block_mse_q8 >= txfm_params->tx_domain_dist_threshold) &&
      (txsize_sqr_up_map[tx_size] != TX_64X64) && !dc_only_blk;
  int calc_pixel_domain_distortion_final =
      txfm_params->use_transform_domain_distortion == 1 &&
      use_transform_domain_distortion && x->rd_model != LOW_TXFM_RD;
  if (calc_pixel_domain_distortion_final &&
      (txk_allowed < TX_TYPES || allowed_tx_mask == 0x0001))
    calc_pixel_domain_distortion_final = use_transform_domain_distortion = 0;

  const int diff_stride = block_size_wide[plane_bsize];
  const int16_t *const src_diff =
      &p->src_diff[(blk_row * diff_stride + blk_col) << MI_SIZE_LOG2];
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *const coeff_ptr = p->coeff + block_offset;
  tran_low_t *const qcoeff_ptr = p->qcoeff + block_offset;
  uint16_t *const eob_ptr = &p->eobs[block];
  uint8_t *const txb_entropy_ctx_ptr = &p->txb_entropy_ctx[block];
  const int n_coeffs = av1_get_max_eob(tx_size);
  const int log_scale = av1_get_tx_scale(tx_size);
  const int satd_shift = MAX_TX_SCALE - log_scale;
  const int tx_domain_shift = satd_shift * 2;
  const unsigned int coeff_opt_satd_threshold =
      txfm_params->coeff_opt_thresholds[1];
  const bool check_satd_skip_trellis =
      !skip_trellis && (coeff_opt_satd_threshold != UINT_MAX);
  const uint64_t satd_thresh =
      (uint64_t)coeff_opt_satd_threshold * qstep * sqrt_tx_pixels_2d[tx_size];
  const uint64_t px_flags_pack =
      base_px_pack |
      ((uint64_t)(((uint32_t)is_hbd << 7) | ((uint32_t)lossless << 6)) << 56);

  TxfmParam txfm_param;
  txfm_param.tx_type = DCT_DCT;
  txfm_param.tx_size = tx_size;
  txfm_param.lossless = lossless;
  txfm_param.bd = bd;
  txfm_param.is_hbd = is_hbd;
  txfm_param.tx_set_type = EXT_TX_SET_ALL16;

  const bool use_qmatrix =
      cm->quant_params.using_qmatrix && !lossless && !check_satd_skip_trellis;
  const qm_val_t *const seg_qmatrix =
      use_qmatrix ? xd->plane[plane].seg_qmatrix[seg_id][tx_size] : NULL;
  const qm_val_t *const seg_iqmatrix =
      use_qmatrix ? xd->plane[plane].seg_iqmatrix[seg_id][tx_size] : NULL;
  const qm_val_t *const seg_qmatrix_scan =
      (use_qmatrix && txfm_params->use_qm_dist_metric && seg_qmatrix != NULL)
          ? av1_get_qmatrix_scan(seg_qmatrix)
          : NULL;

  if (num_tx_candidates == 0) {
    if ((allowed_tx_mask & (allowed_tx_mask - 1)) == 0) {
#if defined(__GNUC__) || defined(__clang__)
      txk_order = (uint64_t)__builtin_ctz(allowed_tx_mask);
#else
      txk_order = (uint64_t)get_msb(allowed_tx_mask);
#endif
      num_tx_candidates = 1;
    } else {
      num_tx_candidates = tx_mask_to_txk_order(allowed_tx_mask, &txk_order);
    }
  }

  int best_rate = INT_MAX;
  int64_t best_dist = INT64_MAX;
  int64_t best_sse = INT64_MAX;
  const int64_t high_energy_thresh = (int64_t)128 * 128 * tx_size_2d[tx_size];
  const int is_high_energy = (block_sse >= high_energy_thresh);
  const bool need_recon_intra =
      !is_inter &&
      (blk_row + tx_size_high_unit[tx_size] < mi_size_high[plane_bsize] ||
       blk_col + tx_size_wide_unit[tx_size] < mi_size_wide[plane_bsize]);
#if !CONFIG_COLLECT_RD_STATS && !COLLECT_TX_SIZE_DATA
  const bool defer_single_px_dist = need_recon_intra &&
                                    num_tx_candidates == 1 &&
                                    !use_transform_domain_distortion &&
                                    tx_size != TX_64X64 && !is_high_energy;
  if (defer_single_px_dist) calc_pixel_domain_distortion_final = 1;
#else
  const bool defer_single_px_dist = false;
#endif
  const bool need_tx_sse =
      !calc_pixel_domain_distortion_final && (plane != 0 || is_inter);
  const int rdmult = x->rdmult;
  const uint8_t seg_skip_block = x->seg_skip_block;

  // Iterate through all transform type candidates.
  for (int rem_candidates = num_tx_candidates; rem_candidates > 0;
       --rem_candidates, txk_order >>= 4) {
    const TX_TYPE tx_type = (TX_TYPE)(txk_order & 0xF);
    txfm_param.tx_type = tx_type;
    const bool cand_use_qm = use_qmatrix && (tx_type < IDTX);
    const qm_val_t *const qm_ptr = cand_use_qm ? seg_qmatrix : NULL;
    const qm_val_t *const iqm_ptr = cand_use_qm ? seg_iqmatrix : NULL;

    if (UNLIKELY(is_border_block))
      av1_subtract_txb(x, plane, plane_bsize, blk_col, blk_row, tx_size,
                       tx_type, cpi->do_border_pad);

    if (LIKELY(!dc_only_blk))
      av1_fwd_txfm(src_diff, coeff_ptr, diff_stride, &txfm_param);
    else
      av1_xform_dc_only(x, plane, block, &txfm_param, per_px_mean);

    int this_skip_trellis = skip_trellis;
    if (check_satd_skip_trellis) {
      int satd = UNLIKELY(dc_only_blk) ? abs(coeff_ptr[0])
                                       : aom_satd(coeff_ptr, n_coeffs);
      satd = RIGHT_SIGNED_SHIFT(satd, satd_shift);
      satd >>= (xd->bd - 8);
      this_skip_trellis = (uint64_t)satd > satd_thresh;
    }

    const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
    tran_low_t *const dqcoeff_ptr = p->dqcoeff + block_offset;
    if (LIKELY(!seg_skip_block)) {
#if CONFIG_AV1_HIGHBITDEPTH
      if (LIKELY(!is_hbd)) {
        quantize_lbd_fast(cpi, coeff_ptr, n_coeffs, p, qcoeff_ptr, dqcoeff_ptr,
                          eob_ptr, scan_order, qm_ptr, iqm_ptr, log_scale,
                          this_skip_trellis, tx_size);
      } else {
        QUANT_PARAM quant_param;
        av1_setup_quant(tx_size, !this_skip_trellis,
                        this_skip_trellis
                            ? (USE_B_QUANT_NO_TRELLIS ? AV1_XFORM_QUANT_B
                                                      : AV1_XFORM_QUANT_FP)
                            : AV1_XFORM_QUANT_FP,
                        cpi->oxcf.q_cfg.quant_b_adapt, &quant_param);
        quant_param.qmatrix = qm_ptr;
        quant_param.iqmatrix = iqm_ptr;
        if (!this_skip_trellis || !USE_B_QUANT_NO_TRELLIS) {
          av1_highbd_quantize_fp_facade(coeff_ptr, n_coeffs, p, qcoeff_ptr,
                                        dqcoeff_ptr, eob_ptr, scan_order,
                                        &quant_param);
        } else {
          av1_highbd_quantize_b_facade(coeff_ptr, n_coeffs, p, qcoeff_ptr,
                                       dqcoeff_ptr, eob_ptr, scan_order,
                                       &quant_param);
        }
      }
#else
      quantize_lbd_fast(cpi, coeff_ptr, n_coeffs, p, qcoeff_ptr, dqcoeff_ptr,
                        eob_ptr, scan_order, qm_ptr, iqm_ptr, log_scale,
                        this_skip_trellis, tx_size);
#endif
    } else {
      av1_quantize_skip(n_coeffs, qcoeff_ptr, dqcoeff_ptr, eob_ptr);
    }

    // Calculate rate cost of quantized coefficients.
    const int pre_opt_eob = *eob_ptr;
    if (UNLIKELY(pre_opt_eob == 0)) {
      rate_cost = av1_cost_skip_txb(&x->coeff_costs, txb_ctx, plane, tx_size);
      *txb_entropy_ctx_ptr = 0;
    } else if (!this_skip_trellis) {
      if (LIKELY(!lossless)) {
        rate_cost =
            optimize_txb_rate(cpi, x, plane, block, tx_size, tx_type, txb_ctx,
                              pre_opt_eob, cpi->oxcf.algo_cfg.sharpness);
      } else {
        rate_cost = av1_cost_skip_txb(&x->coeff_costs, txb_ctx, plane, tx_size);
        *txb_entropy_ctx_ptr =
            av1_get_txb_entropy_context(qcoeff_ptr, scan_order, pre_opt_eob);
      }
    } else {
      rate_cost = cost_coeffs(x, plane, block, tx_size, tx_type, txb_ctx,
                              reduced_tx_set_used);
    }

    // If rd cost based on coeff rate alone is already more than best_rd,
    // terminate early.
    if (RDCOST(rdmult, rate_cost, 0) > best_rd) continue;

    int64_t this_dist = INT64_MAX;
    int64_t this_sse = block_sse;

    // Calculate distortion.
    const uint16_t this_eob = *eob_ptr;
    if (UNLIKELY(this_eob == 0)) {
      // When eob is 0, pixel domain distortion is more efficient and accurate.
      this_dist = this_sse = block_sse;
    } else if (defer_single_px_dist) {
      this_dist = 0;
      this_sse = block_sse;
    } else if (UNLIKELY(dc_only_blk)) {
      this_sse = block_sse;
      const TX_TYPE px_tx = plane ? uv_tx_type : tx_type;
      this_dist = dist_block_px_domain(
          cpi, x, px_flags_pack | ((uint64_t)(uint8_t)px_tx << 56));
    } else if (use_transform_domain_distortion) {
      dist_block_tx_domain_fast(x, coeff_ptr, dqcoeff_ptr, n_coeffs,
                                tx_domain_shift, is_hbd, tx_size, tx_type,
                                qm_ptr, qm_ptr ? seg_qmatrix_scan : NULL,
                                &this_dist, need_tx_sse ? &this_sse : NULL);
    } else {
      int64_t sse_diff = INT64_MAX;
      if (UNLIKELY(tx_size == TX_64X64 || is_high_energy)) {
        dist_block_tx_domain_fast(x, coeff_ptr, dqcoeff_ptr, n_coeffs,
                                  tx_domain_shift, is_hbd, tx_size, tx_type,
                                  qm_ptr, qm_ptr ? seg_qmatrix_scan : NULL,
                                  &this_dist, &this_sse);
        sse_diff = block_sse - this_sse;
      }
      if (LIKELY(tx_size != TX_64X64 || !is_high_energy ||
                 (sse_diff * 2) < this_sse)) {
        const int64_t tx_domain_dist = this_dist;
        const TX_TYPE px_tx = plane ? uv_tx_type : tx_type;
        this_dist = dist_block_px_domain(
            cpi, x, px_flags_pack | ((uint64_t)(uint8_t)px_tx << 56));
        if (UNLIKELY(is_high_energy && this_dist < tx_domain_dist))
          this_dist = tx_domain_dist;
      } else {
        assert(sse_diff < INT64_MAX);
        this_dist += sse_diff;
      }
      this_sse = block_sse;
    }

    const int64_t rd = RDCOST(rdmult, rate_cost, this_dist);

    if (rd < best_rd) {
      best_rd = rd;
      best_rate = rate_cost;
      best_dist = this_dist;
      best_sse = this_sse;
      best_tx_type = tx_type;
      best_eob = this_eob;
      best_txb_ctx =
          UNLIKELY(this_skip_trellis && best_eob > 0)
              ? av1_get_txb_entropy_context(qcoeff_ptr, scan_order, best_eob)
              : *txb_entropy_ctx_ptr;
      // Swap dqcoeff buffers
      tran_low_t *const tmp_dqcoeff = best_dqcoeff;
      best_dqcoeff = p->dqcoeff;
      p->dqcoeff = tmp_dqcoeff;
      if (rem_candidates > 1) {
        const int adaptive_txb_search_level =
            cpi->sf.tx_sf.adaptive_txb_search_level;
        if (adaptive_txb_search_level &&
            (best_rd - (best_rd >> adaptive_txb_search_level)) > ref_best_rd) {
          break;
        }
        if (!best_eob && cpi->sf.tx_sf.tx_type_search.skip_tx_search) break;
      }
    }

#if CONFIG_COLLECT_RD_STATS == 1
    if (plane == 0) {
      RD_STATS this_rd_stats;
      av1_invalid_rd_stats(&this_rd_stats);
      this_rd_stats.rate = rate_cost;
      this_rd_stats.dist = this_dist;
      this_rd_stats.sse = this_sse;
      PrintTransformUnitStats(cpi, x, &this_rd_stats, blk_row, blk_col,
                              plane_bsize, tx_size, tx_type, rd);
    }
#endif  // CONFIG_COLLECT_RD_STATS == 1

#if COLLECT_TX_SIZE_DATA
    // Generate small sample to restrict output size.
    static unsigned int seed = 21743;
    if (lcg_rand16(&seed) % 200 == 0) {
      FILE *fp = NULL;

      if (within_border) {
        fp = fopen(av1_tx_size_data_output_file, "a");
      }

      if (fp) {
        // Transform info and RD
        const int txb_w = tx_size_wide[tx_size];
        const int txb_h = tx_size_high[tx_size];

        // Residue signal.
        const int diff_stride = block_size_wide[plane_bsize];
        struct macroblock_plane *const p = &x->plane[plane];
        const int16_t *src_diff =
            &p->src_diff[(blk_row * diff_stride + blk_col) * 4];

        for (int r = 0; r < txb_h; ++r) {
          for (int c = 0; c < txb_w; ++c) {
            fprintf(fp, "%d,", src_diff[c]);
          }
          src_diff += diff_stride;
        }

        fprintf(fp, "%d,%d,%d,%" PRId64, txb_w, txb_h, tx_type, rd);
        fprintf(fp, "\n");
        fclose(fp);
      }
    }
#endif  // COLLECT_TX_SIZE_DATA
  }

  assert(best_rd != INT64_MAX);

#if CONFIG_RD_DEBUG
  av1_invalid_rd_stats(best_rd_stats);
#endif
  best_rd_stats->rate = best_rate;
  best_rd_stats->dist = best_dist;
  best_rd_stats->rdcost = INT64_MAX;
  best_rd_stats->sse = best_sse;
  best_rd_stats->skip_txfm = best_eob == 0;
  best_rd_stats->zero_rate = 0;
  if (plane == 0) update_txk_array(xd, blk_row, blk_col, tx_size, best_tx_type);
  *txb_entropy_ctx_ptr = best_txb_ctx;
  *eob_ptr = best_eob;

  // Point dqcoeff to the quantized coefficients corresponding to the best
  // transform type, then we can skip transform and quantization, e.g. in the
  // final pixel domain distortion calculation and recon_intra().
  p->dqcoeff = best_dqcoeff;

  if (UNLIKELY(is_border_block))
    av1_subtract_txb(x, plane, plane_bsize, blk_col, blk_row, tx_size,
                     best_tx_type, cpi->do_border_pad);

  // Intra mode needs decoded pixels such that the next transform block
  // can use them for prediction. If recon_intra reconstructs dst in-place,
  // reuse dst directly for calc_pixel_domain_distortion_final without a
  // redundant inverse transform and buffer copy.
  const TX_TYPE final_tx_type = plane ? uv_tx_type : best_tx_type;
  if (need_recon_intra && best_eob) {
    struct macroblockd_plane *const pd = &xd->plane[plane];
    const int dst_stride = pd->dst.stride;
    uint8_t *dst =
        &pd->dst.buf[(blk_row * dst_stride + blk_col) << MI_SIZE_LOG2];
    txfm_param.tx_type = final_tx_type;
    txfm_param.eob = best_eob;
#if !defined(NDEBUG)
    txfm_param.tx_set_type =
        av1_get_ext_tx_set_type(tx_size, 0, reduced_tx_set_used);
    assert(av1_ext_tx_used[txfm_param.tx_set_type][txfm_param.tx_type]);
#endif
#if CONFIG_AV1_HIGHBITDEPTH
    if (LIKELY(!is_hbd)) {
      av1_inv_txfm_add(best_dqcoeff + block_offset, dst, dst_stride,
                       &txfm_param);
      if (calc_pixel_domain_distortion_final) {
        const int src_stride = p->src.stride;
        const uint8_t *src =
            &p->src.buf[(blk_row * src_stride + blk_col) << MI_SIZE_LOG2];
        best_rd_stats->dist = pixel_dist_lbd_maybe_border(
            x, plane, src, src_stride, dst, dst_stride, txw, txh, blk_row,
            blk_col, plane_bsize);
        best_rd_stats->sse = block_sse;
      }
    } else {
      av1_highbd_inv_txfm_add(best_dqcoeff + block_offset, dst, dst_stride,
                              &txfm_param);
      if (calc_pixel_domain_distortion_final) {
        const int src_stride = p->src.stride;
        const uint8_t *src =
            &p->src.buf[(blk_row * src_stride + blk_col) << MI_SIZE_LOG2];
        best_rd_stats->dist =
            16 * pixel_dist(cpi, x, plane, src, src_stride, dst, dst_stride,
                            blk_row, blk_col, plane_bsize,
                            txsize_to_bsize[tx_size]);
        best_rd_stats->sse = block_sse;
      }
    }
#else
    av1_inv_txfm_add(best_dqcoeff + block_offset, dst, dst_stride, &txfm_param);
    if (calc_pixel_domain_distortion_final) {
      const int src_stride = p->src.stride;
      const uint8_t *src =
          &p->src.buf[(blk_row * src_stride + blk_col) << MI_SIZE_LOG2];
      best_rd_stats->dist = pixel_dist_lbd_maybe_border(
          x, plane, src, src_stride, dst, dst_stride, txw, txh, blk_row,
          blk_col, plane_bsize);
      best_rd_stats->sse = block_sse;
    }
#endif
  } else if (calc_pixel_domain_distortion_final && best_eob) {
    best_rd_stats->dist = dist_block_px_domain(
        cpi, x, px_flags_pack | ((uint64_t)(uint8_t)final_tx_type << 56));
    best_rd_stats->sse = block_sse;
  }
  p->dqcoeff = orig_dqcoeff;
}

#if !CONFIG_COLLECT_RD_STATS && !COLLECT_TX_SIZE_DATA
// Multi-candidate transform type search for 8-bit, fully visible, non-lossless
// luma blocks with txw and txh <= 32. Produces the same results as
// search_tx_type_general() for such blocks.
// multi_pack is laid out like blk_pack, except that bits 32-39 hold the number
// of candidates (plane is always 0) and bits 56-58 hold the skip_trellis,
// use_transform_domain_distortion and calc_pixel_domain_distortion_final
// flags.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void search_tx_type_multi_lbd(const AV1_COMP *cpi, MACROBLOCK *x,
                                     uint64_t multi_pack,
                                     const TXB_CTX *const txb_ctx,
                                     int64_t ref_best_rd,
                                     RD_STATS *best_rd_stats,
                                     uint64_t txk_order, int64_t block_sse) {
  const int block = (uint16_t)multi_pack;
  const int blk_row = (uint8_t)(multi_pack >> 16);
  const int blk_col = (uint8_t)(multi_pack >> 24);
  int rem_candidates = (uint8_t)(multi_pack >> 32);
  const BLOCK_SIZE plane_bsize = (BLOCK_SIZE)(uint8_t)(multi_pack >> 40);
  const TX_SIZE tx_size = (TX_SIZE)(uint8_t)(multi_pack >> 48);
  const uint32_t flags = (uint8_t)(multi_pack >> 56);
  const int skip_trellis = (int)(flags & 1u);
  const bool use_transform_domain_distortion = ((flags >> 1) & 1u) != 0;
  const bool calc_pixel_domain_distortion_final = ((flags >> 2) & 1u) != 0;
  assert(rem_candidates >= 2);

  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const int seg_id = mbmi->segment_id;
  const int is_inter = is_inter_block(mbmi);
  const int reduced_tx_set_used = cm->features.reduced_tx_set_used;
  const TxfmSearchParams *const txfm_params = &x->txfm_search_params;
  struct macroblock_plane *const p = &x->plane[0];
  tran_low_t *const orig_dqcoeff = p->dqcoeff;
  tran_low_t *best_dqcoeff = x->dqcoeff_buf;

  const int diff_stride = block_size_wide[plane_bsize];
  const int16_t *const src_diff =
      &p->src_diff[(blk_row * diff_stride + blk_col) << MI_SIZE_LOG2];
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *const coeff_ptr = p->coeff + block_offset;
  tran_low_t *const qcoeff_ptr = p->qcoeff + block_offset;
  uint16_t *const eob_ptr = &p->eobs[block];
  uint8_t *const txb_entropy_ctx_ptr = &p->txb_entropy_ctx[block];
  const int n_coeffs = av1_get_max_eob(tx_size);
  const int log_scale = av1_get_tx_scale(tx_size);
  assert(log_scale <= MAX_TX_SCALE);
  const int satd_shift = MAX_TX_SCALE - log_scale;
  const int tx_domain_shift = satd_shift * 2;
  const unsigned int coeff_opt_satd_threshold =
      txfm_params->coeff_opt_thresholds[1];
  const bool check_satd_skip_trellis =
      !skip_trellis && (coeff_opt_satd_threshold != UINT_MAX);
  const uint64_t satd_thresh = (uint64_t)coeff_opt_satd_threshold *
                               (p->dequant_QTX[1] >> 3) *
                               sqrt_tx_pixels_2d[tx_size];

  const bool use_qmatrix =
      cm->quant_params.using_qmatrix && !check_satd_skip_trellis;
  const qm_val_t *const seg_qmatrix =
      use_qmatrix ? xd->plane[0].seg_qmatrix[seg_id][tx_size] : NULL;
  const qm_val_t *const seg_iqmatrix =
      use_qmatrix ? xd->plane[0].seg_iqmatrix[seg_id][tx_size] : NULL;

  const int is_high_energy =
      (block_sse >= (int64_t)128 * 128 * tx_size_2d[tx_size]);
  const bool need_tx_sse = !calc_pixel_domain_distortion_final && is_inter;
  const int rdmult = x->rdmult;
  const int sharpness = cpi->oxcf.algo_cfg.sharpness;
  const int adaptive_txb_search_level = cpi->sf.tx_sf.adaptive_txb_search_level;
  const int skip_tx_search = cpi->sf.tx_sf.tx_type_search.skip_tx_search;
  const int src_stride = p->src.stride;
  const uint8_t *const src =
      &p->src.buf[(blk_row * src_stride + blk_col) << MI_SIZE_LOG2];
  const int dst_stride = xd->plane[0].dst.stride;
  uint8_t *const dst =
      &xd->plane[0].dst.buf[(blk_row * dst_stride + blk_col) << MI_SIZE_LOG2];

  int64_t best_rd = INT64_MAX;
  int best_rate = INT_MAX;
  int64_t best_dist = INT64_MAX;
  int64_t best_sse = INT64_MAX;
  TX_TYPE best_tx_type = DCT_DCT;
  uint16_t best_eob = 0;
  uint8_t best_txb_ctx = 0;

  for (; rem_candidates > 0; --rem_candidates, txk_order >>= 4) {
    const TX_TYPE tx_type = (TX_TYPE)(txk_order & 0xF);
    const bool cand_use_qm = use_qmatrix && (tx_type < IDTX);
    const qm_val_t *const qm_ptr = cand_use_qm ? seg_qmatrix : NULL;
    const qm_val_t *const iqm_ptr = cand_use_qm ? seg_iqmatrix : NULL;

    fwd_txfm_lbd_nonlossless(src_diff, coeff_ptr, diff_stride, tx_size,
                             tx_type);

    int this_skip_trellis = skip_trellis;
    if (check_satd_skip_trellis) {
      const int satd = aom_satd(coeff_ptr, n_coeffs) >> satd_shift;
      this_skip_trellis = (uint64_t)satd > satd_thresh;
    }

    const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
    tran_low_t *const dqcoeff_ptr = p->dqcoeff + block_offset;
    quantize_lbd_fast(cpi, coeff_ptr, n_coeffs, p, qcoeff_ptr, dqcoeff_ptr,
                      eob_ptr, scan_order, qm_ptr, iqm_ptr, log_scale,
                      this_skip_trellis, tx_size);

    int rate_cost;
    const int pre_opt_eob = *eob_ptr;
    if (UNLIKELY(pre_opt_eob == 0)) {
      rate_cost = av1_cost_skip_txb(&x->coeff_costs, txb_ctx, 0, tx_size);
      *txb_entropy_ctx_ptr = 0;
    } else if (!this_skip_trellis) {
      rate_cost = optimize_txb_rate(cpi, x, 0, block, tx_size, tx_type, txb_ctx,
                                    pre_opt_eob, sharpness);
    } else {
      rate_cost = cost_coeffs(x, 0, block, tx_size, tx_type, txb_ctx,
                              reduced_tx_set_used);
    }

    // If rd cost based on coeff rate alone is already more than best_rd,
    // terminate early.
    if (RDCOST(rdmult, rate_cost, 0) > best_rd) continue;

    int64_t this_dist;
    int64_t this_sse = block_sse;
    const uint16_t this_eob = *eob_ptr;
    if (UNLIKELY(this_eob == 0)) {
      this_dist = block_sse;
    } else if (use_transform_domain_distortion) {
      dist_block_tx_domain_fast(x, coeff_ptr, dqcoeff_ptr, n_coeffs,
                                tx_domain_shift, /*is_hbd=*/0, tx_size, tx_type,
                                qm_ptr, NULL, &this_dist,
                                need_tx_sse ? &this_sse : NULL);
    } else {
      int64_t tx_domain_dist = 0;
      if (UNLIKELY(is_high_energy)) {
        int64_t tx_domain_sse;
        dist_block_tx_domain_fast(
            x, coeff_ptr, dqcoeff_ptr, n_coeffs, tx_domain_shift, /*is_hbd=*/0,
            tx_size, tx_type, qm_ptr, NULL, &tx_domain_dist, &tx_domain_sse);
      }
      this_dist =
          dist_block_px_lbd_fast(src, src_stride, dst, dst_stride, dqcoeff_ptr,
                                 (uint32_t)tx_size | ((uint32_t)tx_type << 8) |
                                     ((uint32_t)this_eob << 16));
      if (UNLIKELY(is_high_energy && this_dist < tx_domain_dist))
        this_dist = tx_domain_dist;
    }

    const int64_t rd = RDCOST(rdmult, rate_cost, this_dist);
    if (rd < best_rd) {
      best_rd = rd;
      best_rate = rate_cost;
      best_dist = this_dist;
      best_sse = this_sse;
      best_tx_type = tx_type;
      best_eob = this_eob;
      best_txb_ctx =
          UNLIKELY(this_skip_trellis && best_eob > 0)
              ? av1_get_txb_entropy_context(qcoeff_ptr, scan_order, best_eob)
              : *txb_entropy_ctx_ptr;
      // Swap dqcoeff buffers
      tran_low_t *const tmp_dqcoeff = best_dqcoeff;
      best_dqcoeff = p->dqcoeff;
      p->dqcoeff = tmp_dqcoeff;
      if (rem_candidates > 1) {
        if (adaptive_txb_search_level &&
            (best_rd - (best_rd >> adaptive_txb_search_level)) > ref_best_rd) {
          break;
        }
        if (!best_eob && skip_tx_search) break;
      }
    }
  }

  assert(best_rd != INT64_MAX);
#if CONFIG_RD_DEBUG
  av1_invalid_rd_stats(best_rd_stats);
#endif
  best_rd_stats->rate = best_rate;
  best_rd_stats->dist = best_dist;
  best_rd_stats->rdcost = INT64_MAX;
  best_rd_stats->sse = best_sse;
  best_rd_stats->skip_txfm = best_eob == 0;
  best_rd_stats->zero_rate = 0;
  update_txk_array(xd, blk_row, blk_col, tx_size, best_tx_type);
  *txb_entropy_ctx_ptr = best_txb_ctx;
  *eob_ptr = best_eob;

  // Intra mode needs decoded pixels such that the next transform block can use
  // them for prediction.
  const bool need_recon_intra =
      !is_inter &&
      (blk_row + tx_size_high_unit[tx_size] < mi_size_high[plane_bsize] ||
       blk_col + tx_size_wide_unit[tx_size] < mi_size_wide[plane_bsize]);
  if (best_eob) {
    if (need_recon_intra) {
      inv_txfm_add_lbd_nonlossless(best_dqcoeff + block_offset, dst, dst_stride,
                                   best_tx_type, tx_size, best_eob);
      if (calc_pixel_domain_distortion_final) {
        best_rd_stats->dist =
            16 * (int64_t)(unsigned)aom_sse(src, src_stride, dst, dst_stride,
                                            tx_size_wide[tx_size],
                                            tx_size_high[tx_size]);
        best_rd_stats->sse = block_sse;
      }
    } else if (calc_pixel_domain_distortion_final) {
      best_rd_stats->dist = dist_block_px_lbd_fast(
          src, src_stride, dst, dst_stride, best_dqcoeff + block_offset,
          (uint32_t)tx_size | ((uint32_t)best_tx_type << 8) |
              ((uint32_t)best_eob << 16));
      best_rd_stats->sse = block_sse;
    }
  }
  p->dqcoeff = orig_dqcoeff;
}
#endif  // !CONFIG_COLLECT_RD_STATS && !COLLECT_TX_SIZE_DATA

// Search for the best transform type for a given transform block.
// This function can be used for both inter and intra, both luma and chroma.
//
// Fast path. Blocks that meet all of the following preconditions are handled
// here and in search_tx_type_multi_lbd(); all other blocks go to
// search_tx_type_general():
//   - 8-bit input and buffers (xd->bd == 8, !is_cur_buf_hbd(xd));
//   - the transform block is fully visible (no frame edge clipping) and
//     cpi->do_border_pad is 0;
//   - txw and txh <= 32 (no 64-point transforms);
//   - not lossless and not a segment skip block;
//   - predict_dc_level <= 1 (so DC-only transforms are never used);
//   - x->rd_model != LOW_TXFM_RD and dct_only_palette_nonrd != 1;
//   - !CONFIG_COLLECT_RD_STATS && !COLLECT_TX_SIZE_DATA.
// The fast path must produce exactly the same results as
// search_tx_type_general(). Any change to the general path, or to the
// functions listed below, must be mirrored here:
//   - DC-only skip prediction: the !is_border, !is_hbd case of
//     predict_dc_only_block() (set_dc_only_skip_rd_stats() is shared);
//     predict_dc_level == 0 mirrors av1_pixel_diff_dist();
//   - tx type candidates: the tx_mask selection in search_tx_type_general()
//     and get_tx_mask(), including the chroma and intra 32x32 DCT_DCT
//     shortcuts (get_default_intra_tx_type() and tx_mask_to_txk_order() are
//     shared);
//   - skip_trellis, use_transform_domain_distortion and
//     calc_pixel_domain_distortion_final selection;
//   - the per-candidate transform, quantization, rate (optimize_txb_rate() is
//     shared) and distortion steps of the search_tx_type_general() candidate
//     loop: search_tx_type_multi_lbd() for several candidates, the code below
//     for one (including the defer_single_px_dist case);
//   - the final reconstruction and pixel domain distortion.
static void search_tx_type(const AV1_COMP *cpi, MACROBLOCK *x,
                           uint64_t blk_pack, const TXB_CTX *const txb_ctx,
                           int64_t ref_best_rd, RD_STATS *best_rd_stats) {
#if CONFIG_COLLECT_RD_STATS || COLLECT_TX_SIZE_DATA
  search_tx_type_general(cpi, x, blk_pack, txb_ctx, ref_best_rd, best_rd_stats);
#else
  MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const int seg_id = mbmi->segment_id;
  const TX_SIZE tx_size = (TX_SIZE)(uint8_t)(blk_pack >> 48);
  const uint8_t txw = tx_size_wide[tx_size];
  const uint8_t txh = tx_size_high[tx_size];
  const TxfmSearchParams *const txfm_params = &x->txfm_search_params;

  if (UNLIKELY(xd->bd != 8 || is_cur_buf_hbd(xd) ||
               (x->pix_to_bottom_edge | x->pix_to_right_edge) < 0 ||
               ((txw | txh) & 64) != 0 || xd->lossless[seg_id] ||
               cpi->do_border_pad || x->seg_skip_block ||
               txfm_params->predict_dc_level > 1 ||
               x->rd_model == LOW_TXFM_RD ||
               cpi->sf.rt_sf.dct_only_palette_nonrd == 1)) {
    search_tx_type_general(cpi, x, blk_pack, txb_ctx, ref_best_rd,
                           best_rd_stats);
    return;
  }

  const int plane = (uint8_t)(blk_pack >> 32);
  struct macroblock_plane *const p = &x->plane[plane];
  const int qstep = p->dequant_QTX[1] >> 3;
  const BLOCK_SIZE plane_bsize = (BLOCK_SIZE)(uint8_t)(blk_pack >> 40);
  const int blk_row = (uint8_t)(blk_pack >> 16);
  const int blk_col = (uint8_t)(blk_pack >> 24);
  const int block = (uint16_t)blk_pack;
  const int diff_stride = block_size_wide[plane_bsize];
  const int16_t *const src_diff =
      &p->src_diff[(blk_row * diff_stride + blk_col) << MI_SIZE_LOG2];
  const BLOCK_SIZE tx_bsize = txsize_to_bsize[tx_size];
  const int log2_n = num_pels_log2_lookup[tx_bsize];
  uint64_t raw_sse;
  unsigned int block_mse_q8;

  if (LIKELY(txfm_params->predict_dc_level != 0)) {
    // Same as predict_dc_only_block() for fully visible blocks with
    // predict_dc_level == 1.
    int sum = 0;
    raw_sse = aom_sum_sse_2d_i16(src_diff, diff_stride, txw, txh, &sum);
    block_mse_q8 = (unsigned int)((raw_sse << 8) >> log2_n);
    const uint64_t block_var =
        raw_sse - (((uint64_t)((int64_t)sum * sum)) >> log2_n);
    const uint64_t var_threshold = ((uint64_t)(qstep * qstep) * 9) / 5;
    if (UNLIKELY(block_var < var_threshold)) {
      const int64_t abs_mean = (int64_t)(abs(sum) >> log2_n) << 7;
      const int dc_qstep = p->dequant_QTX[0] >> 3;
      if ((abs_mean * dc_coeff_scale[tx_size]) < (dc_qstep << 12)) {
        const int skip_ctx =
            get_txb_skip_ctx_only(plane_bsize, tx_bsize, tx_size, plane,
                                  xd->plane[plane].above_entropy_context,
                                  xd->plane[plane].left_entropy_context);
        set_dc_only_skip_rd_stats(x, plane, block, tx_size, skip_ctx,
                                  (int64_t)(raw_sse << 4), best_rd_stats);
        if (plane == 0)
          xd->tx_type_map[blk_row * xd->tx_type_map_stride + blk_col] = DCT_DCT;
        return;
      }
    }
  } else {
    // Same as av1_pixel_diff_dist() for fully visible blocks.
    raw_sse = aom_sum_squares_2d_i16(src_diff, diff_stride, txw, txh);
    block_mse_q8 = (unsigned int)((raw_sse << 8) >> log2_n);
  }

  const int64_t block_sse = (int64_t)(raw_sse << 4);
  const AV1_COMMON *const cm = &cpi->common;
  const int is_inter = is_inter_block(mbmi);
  const int reduced_tx_set_used = cm->features.reduced_tx_set_used;
  TX_TYPE uv_tx_type = DCT_DCT;
  TX_TYPE txk_allowed = TX_TYPES;
  uint16_t allowed_tx_mask;
  uint64_t txk_order = 0;
  int num_tx_candidates = 0;

  // The shortcuts below match the corresponding get_tx_mask() results: intra
  // 32x32-class transforms only allow DCT_DCT, and for chroma the only other
  // allowed type is uv_tx_type.
  if (plane != 0) {
    uv_tx_type = av1_get_tx_type(xd, PLANE_TYPE_UV, blk_row, blk_col, tx_size,
                                 reduced_tx_set_used);
    if (LIKELY(uv_tx_type == DCT_DCT ||
               (!is_inter && txsize_sqr_up_map[tx_size] == TX_32X32))) {
      txk_allowed = DCT_DCT;
      allowed_tx_mask = 1u << DCT_DCT;
      txk_order = DCT_DCT;
      num_tx_candidates = 1;
    } else if (LIKELY(!(is_inter ? cpi->oxcf.txfm_cfg.use_inter_dct_only
                                 : cpi->oxcf.txfm_cfg.use_intra_dct_only) &&
                      cpi->sf.tx_sf.tx_type_search.use_reduced_intra_txset !=
                          2)) {
      txk_allowed = uv_tx_type;
      allowed_tx_mask = (uint16_t)(1u << uv_tx_type);
      txk_order = (uint64_t)uv_tx_type;
      num_tx_candidates = 1;
    } else {
      TxMaskOrder mask_order = { 0, 0 };
      const uint32_t mres =
          get_tx_mask(cpi, x,
                      (blk_pack & 0x00FFFFFFFFFFFFFFULL) |
                          ((uint64_t)(uint8_t)uv_tx_type << 56),
                      txb_ctx, ref_best_rd, &mask_order);
      allowed_tx_mask = (uint16_t)mres;
      txk_allowed = (TX_TYPE)(mres >> 16);
      txk_order = mask_order.txk_order;
      num_tx_candidates = mask_order.num_candidates;
    }
  } else if ((!is_inter && txsize_sqr_up_map[tx_size] == TX_32X32) ||
             UNLIKELY(is_inter ? cpi->oxcf.txfm_cfg.use_inter_dct_only
                               : cpi->oxcf.txfm_cfg.use_intra_dct_only)) {
    txk_allowed = DCT_DCT;
    allowed_tx_mask = 1u << DCT_DCT;
    txk_order = DCT_DCT;
    num_tx_candidates = 1;
  } else if (!is_inter && txfm_params->use_default_intra_tx_type) {
    txk_allowed =
        get_default_intra_tx_type(cpi, xd, tx_size, reduced_tx_set_used);
    allowed_tx_mask = (uint16_t)(1u << txk_allowed);
    txk_order = (uint64_t)txk_allowed;
    num_tx_candidates = 1;
  } else {
    TxMaskOrder mask_order = { 0, 0 };
    const uint32_t mres =
        get_tx_mask(cpi, x, blk_pack, txb_ctx, ref_best_rd, &mask_order);
    allowed_tx_mask = (uint16_t)mres;
    txk_allowed = (TX_TYPE)(mres >> 16);
    txk_order = mask_order.txk_order;
    num_tx_candidates = mask_order.num_candidates;
  }

  if (num_tx_candidates == 0) {
    assert(allowed_tx_mask != 0);
    if ((allowed_tx_mask & (allowed_tx_mask - 1)) == 0) {
#if defined(__GNUC__) || defined(__clang__)
      txk_order = (uint64_t)__builtin_ctz(allowed_tx_mask);
#else
      txk_order = (uint64_t)get_msb(allowed_tx_mask);
#endif
      num_tx_candidates = 1;
    } else {
      // Common case: DCT_DCT, ADST_DCT, DCT_ADST, ADST_ADST plus at most one
      // other type.
      const uint32_t hi_mask = allowed_tx_mask & ~0xFu;
      if (LIKELY((allowed_tx_mask & 0xFu) == 0xFu &&
                 (hi_mask == 0 || (hi_mask & (hi_mask - 1)) == 0))) {
        txk_order = 0x3210;
        num_tx_candidates = 4;
        if (hi_mask != 0) {
          txk_order |= (uint64_t)get_msb(hi_mask) << 16;
          num_tx_candidates = 5;
        }
      } else {
        num_tx_candidates = tx_mask_to_txk_order(allowed_tx_mask, &txk_order);
      }
    }
  }

  const int skip_trellis =
      !is_trellis_used(cpi->optimize_seg_arr[seg_id], DRY_RUN_NORMAL) ||
      ((uint64_t)block_mse_q8 >
       (uint64_t)txfm_params->coeff_opt_thresholds[0] * qstep * qstep);

  const int td_mode = txfm_params->use_transform_domain_distortion;
  int use_tx_dist =
      (td_mode > 0) && (block_mse_q8 >= txfm_params->tx_domain_dist_threshold);
  int calc_px_final = use_tx_dist && (td_mode == 1);
  if (calc_px_final && (txk_allowed < TX_TYPES || allowed_tx_mask == 0x0001)) {
    calc_px_final = 0;
    use_tx_dist = 0;
  }

  if (num_tx_candidates > 1) {
    assert(plane == 0);
    const uint64_t multi_pack =
        (blk_pack & 0x00FFFF00FFFFFFFFULL) |
        ((uint64_t)(uint8_t)num_tx_candidates << 32) |
        ((uint64_t)(((uint32_t)skip_trellis) | ((uint32_t)use_tx_dist << 1) |
                    ((uint32_t)calc_px_final << 2))
         << 56);
    search_tx_type_multi_lbd(cpi, x, multi_pack, txb_ctx, ref_best_rd,
                             best_rd_stats, txk_order, block_sse);
    return;
  }

  // Single candidate: no candidate loop and no dqcoeff buffer swap.
  const TX_TYPE tx_type = (TX_TYPE)(txk_order & 0xF);
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *const coeff_ptr = p->coeff + block_offset;
  tran_low_t *const qcoeff_ptr = p->qcoeff + block_offset;
  tran_low_t *const dqcoeff_ptr = p->dqcoeff + block_offset;
  uint16_t *const eob_ptr = &p->eobs[block];
  uint8_t *const txb_entropy_ctx_ptr = &p->txb_entropy_ctx[block];
  const int n_coeffs = av1_get_max_eob(tx_size);
  const int log_scale = av1_get_tx_scale(tx_size);
  assert(log_scale <= MAX_TX_SCALE);
  const int satd_shift = MAX_TX_SCALE - log_scale;
  const unsigned int coeff_opt_satd_threshold =
      txfm_params->coeff_opt_thresholds[1];
  const bool check_satd_skip_trellis =
      !skip_trellis && (coeff_opt_satd_threshold != UINT_MAX);

  fwd_txfm_lbd_nonlossless(src_diff, coeff_ptr, diff_stride, tx_size, tx_type);

  int this_skip_trellis = skip_trellis;
  if (check_satd_skip_trellis) {
    const int satd = aom_satd(coeff_ptr, n_coeffs) >> satd_shift;
    this_skip_trellis = (uint64_t)satd > (uint64_t)coeff_opt_satd_threshold *
                                             qstep * sqrt_tx_pixels_2d[tx_size];
  }

  const bool cand_use_qm = cm->quant_params.using_qmatrix &&
                           !check_satd_skip_trellis && (tx_type < IDTX);
  const qm_val_t *const qm_ptr =
      cand_use_qm ? xd->plane[plane].seg_qmatrix[seg_id][tx_size] : NULL;
  const qm_val_t *const iqm_ptr =
      cand_use_qm ? xd->plane[plane].seg_iqmatrix[seg_id][tx_size] : NULL;
  const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
  quantize_lbd_fast(cpi, coeff_ptr, n_coeffs, p, qcoeff_ptr, dqcoeff_ptr,
                    eob_ptr, scan_order, qm_ptr, iqm_ptr, log_scale,
                    this_skip_trellis, tx_size);

  int rate_cost;
  const int pre_opt_eob = *eob_ptr;
  if (UNLIKELY(pre_opt_eob == 0)) {
    rate_cost = av1_cost_skip_txb(&x->coeff_costs, txb_ctx, plane, tx_size);
    *txb_entropy_ctx_ptr = 0;
  } else if (!this_skip_trellis) {
    rate_cost =
        optimize_txb_rate(cpi, x, plane, block, tx_size, tx_type, txb_ctx,
                          pre_opt_eob, cpi->oxcf.algo_cfg.sharpness);
  } else {
    rate_cost = cost_coeffs(x, plane, block, tx_size, tx_type, txb_ctx,
                            reduced_tx_set_used);
    *txb_entropy_ctx_ptr =
        av1_get_txb_entropy_context(qcoeff_ptr, scan_order, pre_opt_eob);
  }

  const uint16_t this_eob = *eob_ptr;
  if (plane == 0) update_txk_array(xd, blk_row, blk_col, tx_size, tx_type);

  int64_t best_dist;
  int64_t best_sse = block_sse;
  if (LIKELY(this_eob != 0)) {
    const TX_TYPE final_tx_type = plane ? uv_tx_type : tx_type;
    const bool need_recon_intra =
        !is_inter &&
        (blk_row + tx_size_high_unit[tx_size] < mi_size_high[plane_bsize] ||
         blk_col + tx_size_wide_unit[tx_size] < mi_size_wide[plane_bsize]);
    const bool use_tx_dist_only = use_tx_dist && !calc_px_final;
    const bool is_high_energy =
        (block_sse >= (int64_t)128 * 128 * tx_size_2d[tx_size]);
    const int src_stride = p->src.stride;
    const uint8_t *const src =
        &p->src.buf[(blk_row * src_stride + blk_col) << MI_SIZE_LOG2];
    const int dst_stride = xd->plane[plane].dst.stride;
    uint8_t *const dst =
        &xd->plane[plane]
             .dst.buf[(blk_row * dst_stride + blk_col) << MI_SIZE_LOG2];
    const uint32_t px_tx_info = (uint32_t)tx_size |
                                ((uint32_t)final_tx_type << 8) |
                                ((uint32_t)this_eob << 16);
    const int tx_domain_shift = satd_shift * 2;

    if (use_tx_dist_only) {
      dist_block_tx_domain_fast(x, coeff_ptr, dqcoeff_ptr, n_coeffs,
                                tx_domain_shift, /*is_hbd=*/0, tx_size, tx_type,
                                qm_ptr, NULL, &best_dist,
                                (plane != 0 || is_inter) ? &best_sse : NULL);
      if (need_recon_intra) {
        inv_txfm_add_lbd_nonlossless(dqcoeff_ptr, dst, dst_stride,
                                     final_tx_type, tx_size, this_eob);
      }
    } else {
      // Pixel domain distortion. With calc_px_final, the final pixel domain
      // distortion replaces the transform domain one. Otherwise, high energy
      // blocks use the larger of the two.
      int64_t tx_domain_dist = 0;
      const bool use_max_dist = is_high_energy && !calc_px_final;
      if (UNLIKELY(use_max_dist)) {
        int64_t tx_domain_sse;
        dist_block_tx_domain_fast(
            x, coeff_ptr, dqcoeff_ptr, n_coeffs, tx_domain_shift, /*is_hbd=*/0,
            tx_size, tx_type, qm_ptr, NULL, &tx_domain_dist, &tx_domain_sse);
      }
      if (need_recon_intra) {
        inv_txfm_add_lbd_nonlossless(dqcoeff_ptr, dst, dst_stride,
                                     final_tx_type, tx_size, this_eob);
        best_dist = 16 * (int64_t)(unsigned)aom_sse(src, src_stride, dst,
                                                    dst_stride, txw, txh);
      } else {
        best_dist = dist_block_px_lbd_fast(src, src_stride, dst, dst_stride,
                                           dqcoeff_ptr, px_tx_info);
      }
      if (UNLIKELY(use_max_dist && best_dist < tx_domain_dist))
        best_dist = tx_domain_dist;
    }
  } else {
    best_dist = block_sse;
  }

#if CONFIG_RD_DEBUG
  av1_invalid_rd_stats(best_rd_stats);
#endif
  best_rd_stats->rate = rate_cost;
  best_rd_stats->dist = best_dist;
  best_rd_stats->rdcost = INT64_MAX;
  best_rd_stats->sse = best_sse;
  best_rd_stats->skip_txfm = this_eob == 0;
  best_rd_stats->zero_rate = 0;
#endif  // CONFIG_COLLECT_RD_STATS || COLLECT_TX_SIZE_DATA
}

// Pick transform type for a luma transform block of tx_size. Note this function
// is used only for inter-predicted blocks.
static inline void tx_type_rd(const AV1_COMP *cpi, MACROBLOCK *x,
                              TX_SIZE tx_size, int blk_row, int blk_col,
                              int block, int plane_bsize, TXB_CTX *txb_ctx,
                              RD_STATS *rd_stats, FAST_TX_SEARCH_MODE ftxs_mode,
                              int64_t ref_rdcost) {
  assert(is_inter_block(x->e_mbd.mi[0]));
  RD_STATS this_rd_stats;
  search_tx_type(cpi, x,
                 pack_tx_blk_args(block, blk_row, blk_col, 0,
                                  (BLOCK_SIZE)plane_bsize, tx_size, ftxs_mode),
                 txb_ctx, ref_rdcost, &this_rd_stats);

  av1_merge_rd_stats(rd_stats, &this_rd_stats);
}

static inline void try_tx_block_no_split(
    const AV1_COMP *cpi, MACROBLOCK *x, int blk_row, int blk_col, int block,
    TX_SIZE tx_size, int depth, BLOCK_SIZE plane_bsize,
    const ENTROPY_CONTEXT *ta, const ENTROPY_CONTEXT *tl,
    int txfm_partition_ctx, RD_STATS *rd_stats, int64_t ref_best_rd,
    FAST_TX_SEARCH_MODE ftxs_mode, TxCandidateInfo *no_split) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  struct macroblock_plane *const p = &x->plane[0];
  const ENTROPY_CONTEXT *const pta = ta + blk_col;
  const ENTROPY_CONTEXT *const ptl = tl + blk_row;
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  TXB_CTX txb_ctx;
  get_txb_ctx_fast(plane_bsize, tx_size, 0, pta, ptl, &txb_ctx);
  const int zero_blk_rate = x->coeff_costs.coeff_costs[txs_ctx][PLANE_TYPE_Y]
                                .txb_skip_cost[txb_ctx.txb_skip_ctx][1];
  rd_stats->zero_rate = zero_blk_rate;
  const int index = av1_get_txb_size_index(plane_bsize, blk_row, blk_col);
  mbmi->inter_tx_size[index] = tx_size;
  tx_type_rd(cpi, x, tx_size, blk_row, blk_col, block, plane_bsize, &txb_ctx,
             rd_stats, ftxs_mode, ref_best_rd);
  assert(rd_stats->rate < INT_MAX);

  const int pick_skip_txfm =
      !xd->lossless[mbmi->segment_id] &&
      (rd_stats->skip_txfm == 1 ||
       RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist) >=
           RDCOST(x->rdmult, zero_blk_rate, rd_stats->sse));
  if (pick_skip_txfm) {
#if CONFIG_RD_DEBUG
    update_txb_coeff_cost(rd_stats, 0, zero_blk_rate - rd_stats->rate);
#endif  // CONFIG_RD_DEBUG
    rd_stats->rate = zero_blk_rate;
    rd_stats->dist = rd_stats->sse;
    p->eobs[block] = 0;
    update_txk_array(xd, blk_row, blk_col, tx_size, DCT_DCT);
  }
  rd_stats->skip_txfm = pick_skip_txfm;

  if (tx_size > TX_4X4 && depth < MAX_VARTX_DEPTH)
    rd_stats->rate += x->mode_costs.txfm_partition_cost[txfm_partition_ctx][0];

  no_split->rd = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
  no_split->txb_entropy_ctx = p->txb_entropy_ctx[block];
  no_split->tx_type =
      xd->tx_type_map[blk_row * xd->tx_type_map_stride + blk_col];
}

static inline void try_tx_block_split(
    const AV1_COMP *cpi, MACROBLOCK *x, int blk_row, int blk_col, int block,
    TX_SIZE tx_size, int depth, BLOCK_SIZE plane_bsize, ENTROPY_CONTEXT *ta,
    ENTROPY_CONTEXT *tl, TXFM_CONTEXT *tx_above, TXFM_CONTEXT *tx_left,
    int txfm_partition_ctx, int64_t no_split_rd, int64_t ref_best_rd,
    FAST_TX_SEARCH_MODE ftxs_mode, RD_STATS *split_rd_stats) {
  assert(tx_size < TX_SIZES_ALL);
  MACROBLOCKD *const xd = &x->e_mbd;
  const int max_blocks_high = max_block_high(xd, plane_bsize, 0);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, 0);
  const int txb_width = tx_size_wide_unit[tx_size];
  const int txb_height = tx_size_high_unit[tx_size];
  // Transform size after splitting current block.
  const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
  const int sub_txb_width = tx_size_wide_unit[sub_txs];
  const int sub_txb_height = tx_size_high_unit[sub_txs];
  const int sub_step = sub_txb_width * sub_txb_height;
  const int nblks = (txb_height / sub_txb_height) * (txb_width / sub_txb_width);
  assert(nblks > 0);
  av1_init_rd_stats(split_rd_stats);
  split_rd_stats->rate =
      x->mode_costs.txfm_partition_cost[txfm_partition_ctx][1];

  for (int r = 0, blk_idx = 0; r < txb_height; r += sub_txb_height) {
    const int offsetr = blk_row + r;
    if (offsetr >= max_blocks_high) break;
    for (int c = 0; c < txb_width; c += sub_txb_width, ++blk_idx) {
      assert(blk_idx < 4);
      (void)blk_idx;
      const int offsetc = blk_col + c;
      if (offsetc >= max_blocks_wide) continue;

      RD_STATS this_rd_stats;
      int this_cost_valid = 1;
      select_tx_block(cpi, x, offsetr, offsetc, block, sub_txs, depth + 1,
                      plane_bsize, ta, tl, tx_above, tx_left, &this_rd_stats,
                      no_split_rd / nblks, ref_best_rd - split_rd_stats->rdcost,
                      &this_cost_valid, ftxs_mode, -1);
      if (!this_cost_valid) {
        split_rd_stats->rdcost = INT64_MAX;
        return;
      }
      av1_merge_rd_stats(split_rd_stats, &this_rd_stats);
      split_rd_stats->rdcost =
          RDCOST(x->rdmult, split_rd_stats->rate, split_rd_stats->dist);
      if (split_rd_stats->rdcost > ref_best_rd) {
        split_rd_stats->rdcost = INT64_MAX;
        return;
      }
      block += sub_step;
    }
  }
}

static float get_var(float mean, double x2_sum, int num) {
  const float e_x2 = (float)(x2_sum / num);
  const float diff = e_x2 - mean * mean;
  return diff;
}

static inline void get_blk_var_dev(const int16_t *data, int stride, int bw,
                                   int bh, float *dev_of_mean,
                                   float *var_of_vars) {
  const int16_t *const data_ptr = &data[0];
  const int subh = (bh >= bw) ? (bh >> 1) : bh;
  const int subw = (bw >= bh) ? (bw >> 1) : bw;
  const int num = bw * bh;
  const int sub_num = subw * subh;
  int total_x_sum = 0;
  int64_t total_x2_sum = 0;
  int blk_idx = 0;
  float var_sum = 0.0f;
  float mean_sum = 0.0f;
  double var2_sum = 0.0f;
  double mean2_sum = 0.0f;

  for (int row = 0; row < bh; row += subh) {
    for (int col = 0; col < bw; col += subw) {
      int x_sum;
      int64_t x2_sum;
#if TX_SEARCH_SSE2
      if (subw == 4 && subh == 4) {
        sse_sum_4x4_sse2(data_ptr + row * stride + col, stride, &x_sum,
                         &x2_sum);
      } else
#endif
      {
        aom_get_blk_sse_sum(data_ptr + row * stride + col, stride, subw, subh,
                            &x_sum, &x2_sum);
      }
      total_x_sum += x_sum;
      total_x2_sum += x2_sum;

      const float mean = (float)x_sum / sub_num;
      const float var = get_var(mean, (double)x2_sum, sub_num);
      mean_sum += mean;
      mean2_sum += (double)(mean * mean);
      var_sum += var;
      var2_sum += var * var;
      blk_idx++;
    }
  }

  const float lvl0_mean = (float)total_x_sum / num;
  const float block_var = get_var(lvl0_mean, (double)total_x2_sum, num);
  mean_sum += lvl0_mean;
  mean2_sum += (double)(lvl0_mean * lvl0_mean);
  var_sum += block_var;
  var2_sum += block_var * block_var;
  const float av_mean = mean_sum / 5;

  if (blk_idx > 1) {
    // Deviation of means.
    *dev_of_mean = get_dev(av_mean, mean2_sum, (blk_idx + 1));
    // Variance of variances.
    const float mean_var = var_sum / (blk_idx + 1);
    *var_of_vars = get_var(mean_var, var2_sum, (blk_idx + 1));
  }
}

static void prune_tx_split_no_split(MACROBLOCK *x, BLOCK_SIZE bsize,
                                    int blk_row, int blk_col, TX_SIZE tx_size,
                                    int *try_no_split, int *try_split,
                                    int pruning_level) {
  const int diff_stride = block_size_wide[bsize];
  const int16_t *diff =
      x->plane[0].src_diff + 4 * blk_row * diff_stride + 4 * blk_col;
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];
  float dev_of_means = 0.0f;
  float var_of_vars = 0.0f;

  // This function calculates the deviation of means, and the variance of pixel
  // variances of the block as well as it's sub-blocks.
  get_blk_var_dev(diff, diff_stride, bw, bh, &dev_of_means, &var_of_vars);
  const int dc_q = x->plane[0].dequant_QTX[0] >> 3;
  const int ac_q = x->plane[0].dequant_QTX[1] >> 3;
  const int no_split_thresh_scales[4] = { 0, 24, 8, 8 };
  const int no_split_thresh_scale = no_split_thresh_scales[pruning_level];
  const int split_thresh_scales[4] = { 0, 24, 10, 8 };
  const int split_thresh_scale = split_thresh_scales[pruning_level];

  if ((dev_of_means <= dc_q) &&
      (split_thresh_scale * var_of_vars <= ac_q * ac_q)) {
    *try_split = 0;
  }
  if ((dev_of_means > no_split_thresh_scale * dc_q) &&
      (var_of_vars > no_split_thresh_scale * ac_q * ac_q)) {
    *try_no_split = 0;
  }
}

// Search for the best transform partition(recursive)/type for a given
// inter-predicted luma block. The obtained transform selection will be saved
// in xd->mi[0], the corresponding RD stats will be saved in rd_stats.
static inline void select_tx_block(
    const AV1_COMP *cpi, MACROBLOCK *x, int blk_row, int blk_col, int block,
    TX_SIZE tx_size, int depth, BLOCK_SIZE plane_bsize, ENTROPY_CONTEXT *ta,
    ENTROPY_CONTEXT *tl, TXFM_CONTEXT *tx_above, TXFM_CONTEXT *tx_left,
    RD_STATS *rd_stats, int64_t prev_level_rd, int64_t ref_best_rd,
    int *is_cost_valid, FAST_TX_SEARCH_MODE ftxs_mode, int blk_idx) {
  assert(tx_size < TX_SIZES_ALL);
  av1_init_rd_stats(rd_stats);
  if (ref_best_rd < 0) {
    *is_cost_valid = 0;
    return;
  }

  MACROBLOCKD *const xd = &x->e_mbd;
  assert(blk_row < max_block_high(xd, plane_bsize, 0) &&
         blk_col < max_block_wide(xd, plane_bsize, 0));
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int ctx = txfm_partition_context(tx_above + blk_col, tx_left + blk_row,
                                         mbmi->bsize, tx_size);
  struct macroblock_plane *const p = &x->plane[0];

  int try_no_split = (cpi->oxcf.txfm_cfg.enable_tx64 ||
                      txsize_sqr_up_map[tx_size] != TX_64X64) &&
                     (cpi->oxcf.txfm_cfg.enable_rect_tx ||
                      tx_size_wide[tx_size] == tx_size_high[tx_size]);
  int try_split = tx_size > TX_4X4 && depth < MAX_VARTX_DEPTH;
  TxCandidateInfo no_split = { INT64_MAX, 0, TX_TYPES };

  // Prune tx_split and no-split based on sub-block properties.
  if (tx_size != TX_4X4 && try_split == 1 && try_no_split == 1 &&
      cpi->sf.tx_sf.prune_tx_size_level > 0) {
    prune_tx_split_no_split(x, plane_bsize, blk_row, blk_col, tx_size,
                            &try_no_split, &try_split,
                            cpi->sf.tx_sf.prune_tx_size_level);
  }

  if (cpi->sf.rt_sf.skip_tx_no_split_var_based_partition) {
    if (x->try_merge_partition && try_split && p->eobs[block]) try_no_split = 0;
  }

  // Try using current block as a single transform block without split.
  if (try_no_split) {
    try_tx_block_no_split(cpi, x, blk_row, blk_col, block, tx_size, depth,
                          plane_bsize, ta, tl, ctx, rd_stats, ref_best_rd,
                          ftxs_mode, &no_split);

    push_inter_block_tx_no_split_rd(
        x, mbmi, no_split.rd, blk_idx,
        cpi->sf.tx_sf.prune_inter_tx_split_rd_eval_lvl);

    // Speed features for early termination.
    const int search_level = cpi->sf.tx_sf.adaptive_txb_search_level;
    if (search_level) {
      if ((no_split.rd - (no_split.rd >> (1 + search_level))) > ref_best_rd) {
        *is_cost_valid = 0;
        return;
      }
      if (no_split.rd - (no_split.rd >> (2 + search_level)) > prev_level_rd) {
        try_split = 0;
      }
    }
    if (cpi->sf.tx_sf.txb_split_cap) {
      if (p->eobs[block] == 0) try_split = 0;
    }
    if (prune_tx_split_eval_using_no_split_rd(
            x, mbmi, no_split.rd, blk_idx,
            cpi->sf.tx_sf.prune_inter_tx_split_rd_eval_lvl)) {
      try_split = 0;
    }
  }

  // ML based speed feature to skip searching for split transform blocks.
  if (x->e_mbd.bd == 8 && try_split &&
      !(ref_best_rd == INT64_MAX && no_split.rd == INT64_MAX)) {
    const int threshold = cpi->sf.tx_sf.tx_type_search.ml_tx_split_thresh;
    if (threshold >= 0) {
      const int split_score =
          ml_predict_tx_split(x, plane_bsize, blk_row, blk_col, tx_size);
      if (split_score < -threshold) try_split = 0;
    }
  }

  RD_STATS split_rd_stats;
  split_rd_stats.rdcost = INT64_MAX;
  // Try splitting current block into smaller transform blocks.
  if (try_split) {
    try_tx_block_split(cpi, x, blk_row, blk_col, block, tx_size, depth,
                       plane_bsize, ta, tl, tx_above, tx_left, ctx, no_split.rd,
                       AOMMIN(no_split.rd, ref_best_rd), ftxs_mode,
                       &split_rd_stats);
  }

  if (no_split.rd < split_rd_stats.rdcost) {
    ENTROPY_CONTEXT *pta = ta + blk_col;
    ENTROPY_CONTEXT *ptl = tl + blk_row;
    p->txb_entropy_ctx[block] = no_split.txb_entropy_ctx;
    av1_set_txb_context(x, 0, block, tx_size, pta, ptl);
    txfm_partition_update(tx_above + blk_col, tx_left + blk_row, tx_size,
                          tx_size);
    for (int idy = 0; idy < tx_size_high_unit[tx_size]; ++idy) {
      for (int idx = 0; idx < tx_size_wide_unit[tx_size]; ++idx) {
        const int index =
            av1_get_txb_size_index(plane_bsize, blk_row + idy, blk_col + idx);
        mbmi->inter_tx_size[index] = tx_size;
      }
    }
    mbmi->tx_size = tx_size;
    update_txk_array(xd, blk_row, blk_col, tx_size, no_split.tx_type);
  } else {
    *rd_stats = split_rd_stats;
    if (split_rd_stats.rdcost == INT64_MAX) *is_cost_valid = 0;
  }
}

static inline void choose_largest_tx_size(const AV1_COMP *const cpi,
                                          MACROBLOCK *x, RD_STATS *rd_stats,
                                          int64_t ref_best_rd, BLOCK_SIZE bs) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const TxfmSearchParams *txfm_params = &x->txfm_search_params;
  mbmi->tx_size = tx_size_from_tx_mode(bs, txfm_params->tx_mode_search_type);

  // If tx64 is not enabled, we need to go down to the next available size
  if (!cpi->oxcf.txfm_cfg.enable_tx64 && cpi->oxcf.txfm_cfg.enable_rect_tx) {
    static const TX_SIZE tx_size_max_32[TX_SIZES_ALL] = {
      TX_4X4,    // 4x4 transform
      TX_8X8,    // 8x8 transform
      TX_16X16,  // 16x16 transform
      TX_32X32,  // 32x32 transform
      TX_32X32,  // 64x64 transform
      TX_4X8,    // 4x8 transform
      TX_8X4,    // 8x4 transform
      TX_8X16,   // 8x16 transform
      TX_16X8,   // 16x8 transform
      TX_16X32,  // 16x32 transform
      TX_32X16,  // 32x16 transform
      TX_32X32,  // 32x64 transform
      TX_32X32,  // 64x32 transform
      TX_4X16,   // 4x16 transform
      TX_16X4,   // 16x4 transform
      TX_8X32,   // 8x32 transform
      TX_32X8,   // 32x8 transform
      TX_16X32,  // 16x64 transform
      TX_32X16,  // 64x16 transform
    };
    mbmi->tx_size = tx_size_max_32[mbmi->tx_size];
  } else if (cpi->oxcf.txfm_cfg.enable_tx64 &&
             !cpi->oxcf.txfm_cfg.enable_rect_tx) {
    static const TX_SIZE tx_size_max_square[TX_SIZES_ALL] = {
      TX_4X4,    // 4x4 transform
      TX_8X8,    // 8x8 transform
      TX_16X16,  // 16x16 transform
      TX_32X32,  // 32x32 transform
      TX_64X64,  // 64x64 transform
      TX_4X4,    // 4x8 transform
      TX_4X4,    // 8x4 transform
      TX_8X8,    // 8x16 transform
      TX_8X8,    // 16x8 transform
      TX_16X16,  // 16x32 transform
      TX_16X16,  // 32x16 transform
      TX_32X32,  // 32x64 transform
      TX_32X32,  // 64x32 transform
      TX_4X4,    // 4x16 transform
      TX_4X4,    // 16x4 transform
      TX_8X8,    // 8x32 transform
      TX_8X8,    // 32x8 transform
      TX_16X16,  // 16x64 transform
      TX_16X16,  // 64x16 transform
    };
    mbmi->tx_size = tx_size_max_square[mbmi->tx_size];
  } else if (!cpi->oxcf.txfm_cfg.enable_tx64 &&
             !cpi->oxcf.txfm_cfg.enable_rect_tx) {
    static const TX_SIZE tx_size_max_32_square[TX_SIZES_ALL] = {
      TX_4X4,    // 4x4 transform
      TX_8X8,    // 8x8 transform
      TX_16X16,  // 16x16 transform
      TX_32X32,  // 32x32 transform
      TX_32X32,  // 64x64 transform
      TX_4X4,    // 4x8 transform
      TX_4X4,    // 8x4 transform
      TX_8X8,    // 8x16 transform
      TX_8X8,    // 16x8 transform
      TX_16X16,  // 16x32 transform
      TX_16X16,  // 32x16 transform
      TX_32X32,  // 32x64 transform
      TX_32X32,  // 64x32 transform
      TX_4X4,    // 4x16 transform
      TX_4X4,    // 16x4 transform
      TX_8X8,    // 8x32 transform
      TX_8X8,    // 32x8 transform
      TX_16X16,  // 16x64 transform
      TX_16X16,  // 64x16 transform
    };

    mbmi->tx_size = tx_size_max_32_square[mbmi->tx_size];
  }

  const int skip_ctx = av1_get_skip_txfm_context(xd);
  const int no_skip_txfm_rate = x->mode_costs.skip_txfm_cost[skip_ctx][0];
  const int skip_txfm_rate = x->mode_costs.skip_txfm_cost[skip_ctx][1];
  // Skip RDcost is used only for Inter blocks
  const int64_t skip_txfm_rd =
      is_inter_block(mbmi) ? RDCOST(x->rdmult, skip_txfm_rate, 0) : INT64_MAX;
  const int64_t no_skip_txfm_rd = RDCOST(x->rdmult, no_skip_txfm_rate, 0);
  av1_txfm_rd_in_plane(x, cpi, rd_stats, ref_best_rd,
                       AOMMIN(no_skip_txfm_rd, skip_txfm_rd), AOM_PLANE_Y, bs,
                       mbmi->tx_size, FTXS_NONE);
}

static inline void choose_smallest_tx_size(const AV1_COMP *const cpi,
                                           MACROBLOCK *x, RD_STATS *rd_stats,
                                           int64_t ref_best_rd, BLOCK_SIZE bs) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];

  mbmi->tx_size = TX_4X4;
  // TODO(any) : Pass this_rd based on skip/non-skip cost
  av1_txfm_rd_in_plane(x, cpi, rd_stats, ref_best_rd, 0, 0, bs, mbmi->tx_size,
                       FTXS_NONE);
}

#if !CONFIG_REALTIME_ONLY
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void ml_predict_intra_tx_depth_prune(MACROBLOCK *x, int blk_row,
                                            int blk_col, BLOCK_SIZE bsize,
                                            TX_SIZE tx_size) {
  // Currently NN model based pruning is supported only when largest transform
  // size is 8x8 and matches the current block size in 8-bit non-lossless mode.
  if (tx_size != TX_8X8 || bsize != BLOCK_8X8) return;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  if (xd->bd != 8 || xd->lossless[mbmi->segment_id]) return;

  // Neural network model is a sequential neural net and was trained using SGD
  // optimizer. The model can be further improved in terms of speed/quality by
  // considering the following experiments:
  // 1) Generate ML model by training with balanced data for different learning
  // rates and optimizers.
  // 2) Experiment with ML model by adding features related to the statistics of
  // top and left pixels to capture the accuracy of reconstructed neighbouring
  // pixels for 4x4 blocks numbered 1, 2, 3 in 8x8 block, source variance of 4x4
  // sub-blocks, etc.
  // 3) Generate ML models for transform blocks other than 8x8.
  const NN_CONFIG *const nn_config = &av1_intra_tx_split_nnconfig_8x8;
  const float *const intra_tx_prune_thresh = av1_intra_tx_prune_nn_thresh_8x8;

  float features[NUM_INTRA_TX_SPLIT_FEATURES] = { 0.0f };
  const int16_t *diff = x->plane[0].src_diff + 32 * blk_row + 4 * blk_col;

  int feature_idx = get_mean_dev_features(diff, 8, 8, 8, features);

  features[feature_idx++] = log1pf((float)x->source_variance);

  const int dc_q = av1_dc_quant_QTX(x->qindex, 0, xd->bd) >> (xd->bd - 8);
  const float log_dc_q_square = log1pf((float)(dc_q * dc_q) / 256.0f);
  features[feature_idx++] = log_dc_q_square;
  assert(feature_idx == NUM_INTRA_TX_SPLIT_FEATURES);
  for (int i = 0; i < NUM_INTRA_TX_SPLIT_FEATURES; i++) {
    features[i] = (features[i] - av1_intra_tx_split_8x8_mean[i]) /
                  av1_intra_tx_split_8x8_std[i];
  }

  float score;
  av1_nn_predict(features, nn_config, 1, &score);

  TxfmSearchParams *const txfm_params = &x->txfm_search_params;
  if (score <= intra_tx_prune_thresh[0])
    txfm_params->nn_prune_depths_for_intra_tx = TX_PRUNE_SPLIT;
  else if (score > intra_tx_prune_thresh[1])
    txfm_params->nn_prune_depths_for_intra_tx = TX_PRUNE_LARGEST;
}
#endif  // !CONFIG_REALTIME_ONLY

/*!\brief Transform type search for luma macroblock with fixed transform size.
 *
 * \ingroup transform_search
 * Search for the best transform type and return the transform coefficients RD
 * cost of current luma macroblock with the given uniform transform size.
 *
 * \param[in]    x              Pointer to structure holding the data for the
                                current encoding macroblock
 * \param[in]    cpi            Top-level encoder structure
 * \param[in]    rd_stats       Pointer to struct to keep track of the RD stats
 * \param[in]    ref_best_rd    Best RD cost seen for this block so far
 * \param[in]    bs             Size of the current macroblock
 * \param[in]    tx_size        The given transform size
 * \param[in]    ftxs_mode      Transform search mode specifying desired speed
                                and quality tradeoff
 * \return       An int64_t value that is the best RD cost found.
 */
static int64_t uniform_txfm_yrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                                RD_STATS *rd_stats, int64_t ref_best_rd,
                                BLOCK_SIZE bs, TX_SIZE tx_size,
                                FAST_TX_SEARCH_MODE ftxs_mode) {
  assert(IMPLIES(is_rect_tx(tx_size), is_rect_tx_allowed_bsize(bs)));
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const TxfmSearchParams *txfm_params = &x->txfm_search_params;
  const ModeCosts *mode_costs = &x->mode_costs;
  const int is_inter = is_inter_block(mbmi);
  const int tx_select = txfm_params->tx_mode_search_type == TX_MODE_SELECT &&
                        block_signals_txsize(mbmi->bsize);
  int tx_size_rate = 0;
  if (tx_select) {
    const int ctx = txfm_partition_context(
        xd->above_txfm_context, xd->left_txfm_context, mbmi->bsize, tx_size);
    tx_size_rate = is_inter ? mode_costs->txfm_partition_cost[ctx][0]
                            : tx_size_cost(x, bs, tx_size);
  }
  const int skip_ctx = av1_get_skip_txfm_context(xd);
  const int no_skip_txfm_rate = mode_costs->skip_txfm_cost[skip_ctx][0];
  const int skip_txfm_rate = mode_costs->skip_txfm_cost[skip_ctx][1];
  const int64_t skip_txfm_rd =
      is_inter ? RDCOST(x->rdmult, skip_txfm_rate, 0) : INT64_MAX;
  const int64_t no_this_rd =
      RDCOST(x->rdmult, no_skip_txfm_rate + tx_size_rate, 0);

  mbmi->tx_size = tx_size;
  av1_txfm_rd_in_plane(x, cpi, rd_stats, ref_best_rd,
                       AOMMIN(no_this_rd, skip_txfm_rd), AOM_PLANE_Y, bs,
                       tx_size, ftxs_mode);
  if (rd_stats->rate == INT_MAX) return INT64_MAX;

  int64_t rd;
  // rdstats->rate should include all the rate except skip/non-skip cost as the
  // same is accounted in the caller functions after rd evaluation of all
  // planes. However the decisions should be done after considering the
  // skip/non-skip header cost
  if (rd_stats->skip_txfm && is_inter) {
    rd = RDCOST(x->rdmult, skip_txfm_rate, rd_stats->sse);
  } else {
    // Intra blocks are always signalled as non-skip
    rd = RDCOST(x->rdmult, rd_stats->rate + no_skip_txfm_rate + tx_size_rate,
                rd_stats->dist);
    rd_stats->rate += tx_size_rate;
  }
  // Check if forcing the block to skip transform leads to smaller RD cost.
  if (is_inter && !rd_stats->skip_txfm && !xd->lossless[mbmi->segment_id]) {
#if CONFIG_AV1_HIGHBITDEPTH
    if (xd->bd > 8 && cpi->oxcf.algo_cfg.sharpness == 3) {
      int64_t no_skip_dist, skip_dist;
      av1_get_tx_skip_dist(cpi, x, bs, rd_stats->dist, rd_stats->sse,
                           &no_skip_dist, &skip_dist);
      const int64_t temp_skip_txfm_rd =
          RDCOST(x->rdmult, skip_txfm_rate, skip_dist);
      const int64_t temp_no_skip_txfm_rd =
          RDCOST(x->rdmult, rd_stats->rate + no_skip_txfm_rate + tx_size_rate,
                 no_skip_dist);
      if (temp_skip_txfm_rd <= temp_no_skip_txfm_rd) {
        rd = temp_skip_txfm_rd;
        rd_stats->rate = 0;
        rd_stats->dist = rd_stats->sse;
        rd_stats->skip_txfm = 1;
      }
    } else
#endif
    {
      const int64_t temp_skip_txfm_rd =
          RDCOST(x->rdmult, skip_txfm_rate, rd_stats->sse);
      if (temp_skip_txfm_rd <= rd) {
        rd = temp_skip_txfm_rd;
        rd_stats->rate = 0;
        rd_stats->dist = rd_stats->sse;
        rd_stats->skip_txfm = 1;
      }
    }
  }

  return rd;
}

// Search for the best uniform transform size and type for current coding block.
// best_txk_type_map must hold at least num_blks entries.
static AOM_FORCE_INLINE void choose_tx_size_type_from_rd_impl(
    const AV1_COMP *const cpi, MACROBLOCK *x, RD_STATS *rd_stats,
    int64_t ref_best_rd, BLOCK_SIZE bs, int num_blks,
    uint8_t *best_txk_type_map) {
  av1_invalid_rd_stats(rd_stats);

  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  TxfmSearchParams *const txfm_params = &x->txfm_search_params;
  const TX_SIZE max_rect_tx_size = max_txsize_rect_lookup[bs];
  const int tx_select = txfm_params->tx_mode_search_type == TX_MODE_SELECT;
  int start_tx;
  // The split depth can be at most MAX_TX_DEPTH, so the init_depth controls
  // how many times of splitting is allowed during the RD search.
  int init_depth;

  if (tx_select) {
    start_tx = max_rect_tx_size;
    init_depth = get_search_init_depth(mi_size_wide[bs], mi_size_high[bs],
                                       is_inter_block(mbmi), &cpi->sf,
                                       txfm_params->tx_size_search_method);
    if (init_depth == MAX_TX_DEPTH && !cpi->oxcf.txfm_cfg.enable_tx64 &&
        txsize_sqr_up_map[start_tx] == TX_64X64) {
      start_tx = sub_tx_size_map[start_tx];
    }
  } else {
    const TX_SIZE chosen_tx_size =
        tx_size_from_tx_mode(bs, txfm_params->tx_mode_search_type);
    start_tx = chosen_tx_size;
    init_depth = MAX_TX_DEPTH;
  }

  bool best_in_xd = false;
  TX_SIZE best_tx_size = max_rect_tx_size;
  int64_t best_rd = INT64_MAX;
  x->rd_model = FULL_TXFM_RD;
  int64_t rd[MAX_TX_DEPTH + 1] = { INT64_MAX, INT64_MAX, INT64_MAX };
  for (int tx_size = start_tx, depth = init_depth; depth <= MAX_TX_DEPTH;
       depth++, tx_size = sub_tx_size_map[tx_size]) {
    if ((!cpi->oxcf.txfm_cfg.enable_tx64 &&
         txsize_sqr_up_map[tx_size] == TX_64X64) ||
        (!cpi->oxcf.txfm_cfg.enable_rect_tx &&
         tx_size_wide[tx_size] != tx_size_high[tx_size])) {
      continue;
    }

#if !CONFIG_REALTIME_ONLY
    if (txfm_params->nn_prune_depths_for_intra_tx == TX_PRUNE_SPLIT) break;

    // Set the flag to enable the evaluation of NN classifier to prune transform
    // depths. As the features are based on intra residual information of
    // largest transform, the evaluation of NN model is enabled only for this
    // case.
    txfm_params->enable_nn_prune_intra_tx_depths =
        (cpi->sf.tx_sf.prune_intra_tx_depths_using_nn && tx_size == start_tx);
#endif

    if (best_in_xd) {
      av1_copy_tx_type_map(best_txk_type_map, xd->tx_type_map, num_blks);
      best_in_xd = false;
    }

    RD_STATS this_rd_stats;
    // When the speed feature use_rd_based_breakout_for_intra_tx_search is
    // enabled, use the known minimum best_rd for early termination.
    const int64_t rd_thresh =
        cpi->sf.tx_sf.use_rd_based_breakout_for_intra_tx_search
            ? AOMMIN(ref_best_rd, best_rd)
            : ref_best_rd;
    rd[depth] = uniform_txfm_yrd(cpi, x, &this_rd_stats, rd_thresh, bs, tx_size,
                                 FTXS_NONE);
    if (rd[depth] < best_rd) {
      best_in_xd = true;
      best_tx_size = tx_size;
      best_rd = rd[depth];
      *rd_stats = this_rd_stats;
    }
    if (tx_size == TX_4X4) break;
    // If we are searching three depths, prune the smallest size depending
    // on rd results for the first two depths for low contrast blocks.
    if (depth > init_depth && depth != MAX_TX_DEPTH &&
        x->source_variance < 256) {
      if (rd[depth - 1] != INT64_MAX && rd[depth] > rd[depth - 1]) break;
    }
  }

  if (rd_stats->rate != INT_MAX) {
    mbmi->tx_size = best_tx_size;
    if (!best_in_xd) {
      av1_copy_tx_type_map(xd->tx_type_map, best_txk_type_map, num_blks);
    }
  }

#if !CONFIG_REALTIME_ONLY
  // Reset the flags to avoid any unintentional evaluation of NN model and
  // consumption of prune depths.
  txfm_params->enable_nn_prune_intra_tx_depths = false;
  txfm_params->nn_prune_depths_for_intra_tx = TX_PRUNE_NONE;
#endif
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void choose_tx_size_type_from_rd_large(const AV1_COMP *const cpi,
                                              MACROBLOCK *x, RD_STATS *rd_stats,
                                              int64_t ref_best_rd,
                                              BLOCK_SIZE bs, int num_blks) {
  uint8_t best_txk_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  choose_tx_size_type_from_rd_impl(cpi, x, rd_stats, ref_best_rd, bs, num_blks,
                                   best_txk_type_map);
}

// Blocks with at most 64 4x4 units use a small transform type map buffer.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void choose_tx_size_type_from_rd(const AV1_COMP *const cpi,
                                        MACROBLOCK *x, RD_STATS *rd_stats,
                                        int64_t ref_best_rd, BLOCK_SIZE bs) {
  const int num_blks = bsize_to_num_blk(bs);
  if (UNLIKELY(num_blks > 64)) {
    choose_tx_size_type_from_rd_large(cpi, x, rd_stats, ref_best_rd, bs,
                                      num_blks);
    return;
  }
  uint8_t best_txk_type_map[64];
  choose_tx_size_type_from_rd_impl(cpi, x, rd_stats, ref_best_rd, bs, num_blks,
                                   best_txk_type_map);
}

// Search for the best transform type for the given transform block in the
// given plane/channel, and calculate the corresponding RD cost.
static inline void block_rd_txfm_multi(
    const AV1_COMP *cpi, const AV1_COMMON *cm, MACROBLOCK *x, MACROBLOCKD *xd,
    int is_inter, int rdmult, bool store_cfl, uint64_t pack_hi, int plane,
    int block, int blk_row, int blk_col, BLOCK_SIZE plane_bsize,
    TX_SIZE tx_size, ENTROPY_CONTEXT *t_above, ENTROPY_CONTEXT *t_left,
    RD_STATS *rd_stats, int64_t *current_rd, int64_t best_rd, int *exit_early) {
  if (!is_inter) {
    av1_predict_intra_block_facade(cm, xd, plane, blk_col, blk_row, tx_size);
    av1_subtract_txb(x, plane, plane_bsize, blk_col, blk_row, tx_size, DCT_DCT,
                     false);
#if !CONFIG_REALTIME_ONLY
    const TxfmSearchParams *const txfm_params = &x->txfm_search_params;
    if (txfm_params->enable_nn_prune_intra_tx_depths) {
      ml_predict_intra_tx_depth_prune(x, blk_row, blk_col, plane_bsize,
                                      tx_size);
      if (txfm_params->nn_prune_depths_for_intra_tx == TX_PRUNE_LARGEST) {
        *exit_early = 1;
        return;
      }
    }
#endif
  }

  ENTROPY_CONTEXT *const pta = t_above + blk_col;
  ENTROPY_CONTEXT *const ptl = t_left + blk_row;
  TXB_CTX txb_ctx;
  get_txb_ctx_fast(plane_bsize, tx_size, plane, pta, ptl, &txb_ctx);

  RD_STATS this_rd_stats;
  const uint64_t blk_pack = pack_hi | (uint16_t)block |
                            ((uint32_t)(uint8_t)blk_row << 16) |
                            ((uint32_t)(uint8_t)blk_col << 24);
  search_tx_type(cpi, x, blk_pack, &txb_ctx, best_rd - *current_rd,
                 &this_rd_stats);

#if !CONFIG_REALTIME_ONLY
  if (UNLIKELY(store_cfl)) {
    assert(!is_inter || plane_bsize < BLOCK_8X8);
    cfl_store_tx(xd, blk_row, blk_col, tx_size, plane_bsize);
  }
#else
  (void)store_cfl;
#endif

#if CONFIG_RD_DEBUG
  update_txb_coeff_cost(&this_rd_stats, plane, this_rd_stats.rate);
#endif  // CONFIG_RD_DEBUG

  int64_t rd;
  if (is_inter) {
    const int64_t no_skip_txfm_rd =
        RDCOST(rdmult, this_rd_stats.rate, this_rd_stats.dist);
    const int64_t skip_txfm_rd = RDCOST(rdmult, 0, this_rd_stats.sse);
    rd = AOMMIN(no_skip_txfm_rd, skip_txfm_rd);
    this_rd_stats.skip_txfm &= !x->plane[plane].eobs[block];
  } else {
    rd = RDCOST(rdmult, this_rd_stats.rate, this_rd_stats.dist);
    this_rd_stats.skip_txfm = 0;
  }

  av1_merge_rd_stats(rd_stats, &this_rd_stats);

  *current_rd += rd;
  if (*current_rd > best_rd) {
    *exit_early = 1;
    return;
  }
  av1_set_txb_context(x, plane, block, tx_size, pta, ptl);
}

int64_t av1_estimate_txfm_yrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                              RD_STATS *rd_stats, int64_t ref_best_rd,
                              BLOCK_SIZE bs, TX_SIZE tx_size) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const TxfmSearchParams *txfm_params = &x->txfm_search_params;
  const ModeCosts *mode_costs = &x->mode_costs;
  const int is_inter = is_inter_block(mbmi);
  const int tx_select = txfm_params->tx_mode_search_type == TX_MODE_SELECT &&
                        block_signals_txsize(mbmi->bsize);
  int tx_size_rate = 0;
  if (tx_select) {
    const int ctx = txfm_partition_context(
        xd->above_txfm_context, xd->left_txfm_context, mbmi->bsize, tx_size);
    tx_size_rate = mode_costs->txfm_partition_cost[ctx][0];
  }
  const int skip_ctx = av1_get_skip_txfm_context(xd);
  const int no_skip_txfm_rate = mode_costs->skip_txfm_cost[skip_ctx][0];
  const int skip_txfm_rate = mode_costs->skip_txfm_cost[skip_ctx][1];
  const int64_t skip_txfm_rd = RDCOST(x->rdmult, skip_txfm_rate, 0);
  const int64_t no_this_rd =
      RDCOST(x->rdmult, no_skip_txfm_rate + tx_size_rate, 0);
  mbmi->tx_size = tx_size;

  const uint8_t txw_unit = tx_size_wide_unit[tx_size];
  const uint8_t txh_unit = tx_size_high_unit[tx_size];
  const int step = txw_unit * txh_unit;
  const int max_blocks_wide = max_block_wide(xd, bs, 0);
  const int max_blocks_high = max_block_high(xd, bs, 0);

  struct rdcost_block_args args;
  args.x = x;
  args.cpi = cpi;
  args.best_rd = ref_best_rd;
  args.current_rd = AOMMIN(no_this_rd, skip_txfm_rd);
  args.exit_early = 0;
  args.incomplete_exit = 0;
  args.rd_stats = rd_stats;
  av1_init_rd_stats(rd_stats);
  av1_get_entropy_contexts(bs, &xd->plane[0], args.t_above, args.t_left);
  int i = 0;
  for (int blk_row = 0; blk_row < max_blocks_high && !args.incomplete_exit;
       blk_row += txh_unit) {
    for (int blk_col = 0; blk_col < max_blocks_wide; blk_col += txw_unit) {
      RD_STATS this_rd_stats;
      av1_init_rd_stats(&this_rd_stats);

      if (args.exit_early) {
        args.incomplete_exit = 1;
        break;
      }

      ENTROPY_CONTEXT *a = args.t_above + blk_col;
      ENTROPY_CONTEXT *l = args.t_left + blk_row;
      TXB_CTX txb_ctx;
      get_txb_ctx(bs, tx_size, 0, a, l, &txb_ctx);

      TxfmParam txfm_param;
      QUANT_PARAM quant_param;
      av1_setup_xform(&cpi->common, x, tx_size, DCT_DCT, &txfm_param);
      av1_setup_quant(tx_size, 0, AV1_XFORM_QUANT_B, 0, &quant_param);
      av1_subtract_txb(x, PLANE_TYPE_Y, bs, blk_col, blk_row, tx_size, DCT_DCT,
                       cpi->do_border_pad);

      av1_xform(x, 0, i, blk_row, blk_col, bs, &txfm_param);
      av1_quant(x, 0, i, &txfm_param, &quant_param);

      this_rd_stats.rate =
          cost_coeffs(x, 0, i, tx_size, txfm_param.tx_type, &txb_ctx, 0);

      dist_block_tx_domain(x, 0, i, tx_size, txfm_param.tx_type,
                           quant_param.qmatrix, &this_rd_stats.dist,
                           &this_rd_stats.sse);

      const int64_t no_skip_txfm_rd =
          RDCOST(x->rdmult, this_rd_stats.rate, this_rd_stats.dist);
      const int64_t skip_rd = RDCOST(x->rdmult, 0, this_rd_stats.sse);

      this_rd_stats.skip_txfm &= !x->plane[0].eobs[i];

      av1_merge_rd_stats(rd_stats, &this_rd_stats);
      args.current_rd += AOMMIN(no_skip_txfm_rd, skip_rd);

      if (args.current_rd > ref_best_rd) {
        args.exit_early = 1;
        break;
      }

      av1_set_txb_context(x, 0, i, tx_size, a, l);
      i += step;
    }
  }

  if (args.incomplete_exit) {
    av1_invalid_rd_stats(rd_stats);
    return INT64_MAX;
  }

  int64_t rd;
  // rdstats->rate should include all the rate except skip/non-skip cost as the
  // same is accounted in the caller functions after rd evaluation of all
  // planes. However the decisions should be done after considering the
  // skip/non-skip header cost
  if (rd_stats->skip_txfm && is_inter) {
    rd = RDCOST(x->rdmult, skip_txfm_rate, rd_stats->sse);
  } else {
    // Intra blocks are always signalled as non-skip
    rd = RDCOST(x->rdmult, rd_stats->rate + no_skip_txfm_rate + tx_size_rate,
                rd_stats->dist);
    rd_stats->rate += tx_size_rate;
  }
  // Check if forcing the block to skip transform leads to smaller RD cost.
  if (is_inter && !rd_stats->skip_txfm && !xd->lossless[mbmi->segment_id]) {
    int64_t temp_skip_txfm_rd =
        RDCOST(x->rdmult, skip_txfm_rate, rd_stats->sse);
    if (temp_skip_txfm_rd <= rd) {
      rd = temp_skip_txfm_rd;
      rd_stats->rate = 0;
      rd_stats->dist = rd_stats->sse;
      rd_stats->skip_txfm = 1;
    }
  }

  return rd;
}

// Search for the best transform type for a luma inter-predicted block, given
// the transform block partitions.
// This function is used only when some speed features are enabled.
static inline void tx_block_yrd(const AV1_COMP *cpi, MACROBLOCK *x, int blk_row,
                                int blk_col, int block, TX_SIZE tx_size,
                                BLOCK_SIZE plane_bsize, int depth,
                                ENTROPY_CONTEXT *above_ctx,
                                ENTROPY_CONTEXT *left_ctx,
                                TXFM_CONTEXT *tx_above, TXFM_CONTEXT *tx_left,
                                int64_t ref_best_rd, RD_STATS *rd_stats,
                                FAST_TX_SEARCH_MODE ftxs_mode) {
  assert(tx_size < TX_SIZES_ALL);
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(is_inter_block(mbmi));
  const int max_blocks_high = max_block_high(xd, plane_bsize, 0);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, 0);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  const TX_SIZE plane_tx_size = mbmi->inter_tx_size[av1_get_txb_size_index(
      plane_bsize, blk_row, blk_col)];
  const int ctx = txfm_partition_context(tx_above + blk_col, tx_left + blk_row,
                                         mbmi->bsize, tx_size);

  av1_init_rd_stats(rd_stats);
  if (tx_size == plane_tx_size) {
    ENTROPY_CONTEXT *ta = above_ctx + blk_col;
    ENTROPY_CONTEXT *tl = left_ctx + blk_row;
    const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
    TXB_CTX txb_ctx;
    get_txb_ctx_fast(plane_bsize, tx_size, 0, ta, tl, &txb_ctx);

    const int zero_blk_rate =
        x->coeff_costs.coeff_costs[txs_ctx][get_plane_type(0)]
            .txb_skip_cost[txb_ctx.txb_skip_ctx][1];
    rd_stats->zero_rate = zero_blk_rate;
    tx_type_rd(cpi, x, tx_size, blk_row, blk_col, block, plane_bsize, &txb_ctx,
               rd_stats, ftxs_mode, ref_best_rd);
    if (RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist) >=
            RDCOST(x->rdmult, zero_blk_rate, rd_stats->sse) ||
        rd_stats->skip_txfm == 1) {
      rd_stats->rate = zero_blk_rate;
      rd_stats->dist = rd_stats->sse;
      rd_stats->skip_txfm = 1;
      x->plane[0].eobs[block] = 0;
      x->plane[0].txb_entropy_ctx[block] = 0;
      update_txk_array(xd, blk_row, blk_col, tx_size, DCT_DCT);
    } else {
      rd_stats->skip_txfm = 0;
    }
    if (tx_size > TX_4X4 && depth < MAX_VARTX_DEPTH)
      rd_stats->rate += x->mode_costs.txfm_partition_cost[ctx][0];
    av1_set_txb_context(x, 0, block, tx_size, ta, tl);
    txfm_partition_update(tx_above + blk_col, tx_left + blk_row, tx_size,
                          tx_size);
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int txb_width = tx_size_wide_unit[sub_txs];
    const int txb_height = tx_size_high_unit[sub_txs];
    const int step = txb_height * txb_width;
    const int row_end =
        AOMMIN(tx_size_high_unit[tx_size], max_blocks_high - blk_row);
    const int col_end =
        AOMMIN(tx_size_wide_unit[tx_size], max_blocks_wide - blk_col);
    RD_STATS pn_rd_stats;
    int64_t this_rd = 0;
    assert(txb_width > 0 && txb_height > 0);

    for (int row = 0; row < row_end; row += txb_height) {
      const int offsetr = blk_row + row;
      for (int col = 0; col < col_end; col += txb_width) {
        const int offsetc = blk_col + col;

        av1_init_rd_stats(&pn_rd_stats);
        tx_block_yrd(cpi, x, offsetr, offsetc, block, sub_txs, plane_bsize,
                     depth + 1, above_ctx, left_ctx, tx_above, tx_left,
                     ref_best_rd - this_rd, &pn_rd_stats, ftxs_mode);
        if (pn_rd_stats.rate == INT_MAX) {
          av1_invalid_rd_stats(rd_stats);
          return;
        }
        av1_merge_rd_stats(rd_stats, &pn_rd_stats);
        this_rd += RDCOST(x->rdmult, pn_rd_stats.rate, pn_rd_stats.dist);
        block += step;
      }
    }

    if (tx_size > TX_4X4 && depth < MAX_VARTX_DEPTH)
      rd_stats->rate += x->mode_costs.txfm_partition_cost[ctx][1];
  }
}

// search for tx type with tx sizes already decided for a inter-predicted luma
// partition block. It's used only when some speed features are enabled.
// Return value 0: early termination triggered, no valid rd cost available;
//              1: rd cost values are valid.
static int inter_block_yrd(const AV1_COMP *cpi, MACROBLOCK *x,
                           RD_STATS *rd_stats, BLOCK_SIZE bsize,
                           int64_t ref_best_rd, FAST_TX_SEARCH_MODE ftxs_mode) {
  if (ref_best_rd < 0) {
    av1_invalid_rd_stats(rd_stats);
    return 0;
  }

  av1_init_rd_stats(rd_stats);

  MACROBLOCKD *const xd = &x->e_mbd;
  const TxfmSearchParams *txfm_params = &x->txfm_search_params;
  const struct macroblockd_plane *const pd = &xd->plane[0];
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
  const TX_SIZE max_tx_size = get_vartx_max_txsize(xd, bsize, 0);
  const int bh = tx_size_high_unit[max_tx_size];
  const int bw = tx_size_wide_unit[max_tx_size];
  const int step = bw * bh;
  const int init_depth = get_search_init_depth(
      mi_width, mi_height, 1, &cpi->sf, txfm_params->tx_size_search_method);
  ENTROPY_CONTEXT ctxa[MAX_MIB_SIZE];
  ENTROPY_CONTEXT ctxl[MAX_MIB_SIZE];
  TXFM_CONTEXT tx_above[MAX_MIB_SIZE];
  TXFM_CONTEXT tx_left[MAX_MIB_SIZE];
  av1_get_entropy_contexts(bsize, pd, ctxa, ctxl);
  memcpy(tx_above, xd->above_txfm_context, sizeof(TXFM_CONTEXT) * mi_width);
  memcpy(tx_left, xd->left_txfm_context, sizeof(TXFM_CONTEXT) * mi_height);

  int64_t this_rd = 0;
  for (int idy = 0, block = 0; idy < mi_height; idy += bh) {
    for (int idx = 0; idx < mi_width; idx += bw) {
      RD_STATS pn_rd_stats;
      av1_init_rd_stats(&pn_rd_stats);
      tx_block_yrd(cpi, x, idy, idx, block, max_tx_size, bsize, init_depth,
                   ctxa, ctxl, tx_above, tx_left, ref_best_rd - this_rd,
                   &pn_rd_stats, ftxs_mode);
      if (pn_rd_stats.rate == INT_MAX) {
        av1_invalid_rd_stats(rd_stats);
        return 0;
      }
      av1_merge_rd_stats(rd_stats, &pn_rd_stats);
      this_rd +=
          AOMMIN(RDCOST(x->rdmult, pn_rd_stats.rate, pn_rd_stats.dist),
                 RDCOST(x->rdmult, pn_rd_stats.zero_rate, pn_rd_stats.sse));
      block += step;
    }
  }

  const int skip_ctx = av1_get_skip_txfm_context(xd);
  const int no_skip_txfm_rate = x->mode_costs.skip_txfm_cost[skip_ctx][0];
  const int skip_txfm_rate = x->mode_costs.skip_txfm_cost[skip_ctx][1];
  const int64_t skip_txfm_rd = RDCOST(x->rdmult, skip_txfm_rate, rd_stats->sse);
  this_rd =
      RDCOST(x->rdmult, rd_stats->rate + no_skip_txfm_rate, rd_stats->dist);
  if (skip_txfm_rd < this_rd) {
    this_rd = skip_txfm_rd;
    rd_stats->rate = 0;
    rd_stats->dist = rd_stats->sse;
    rd_stats->skip_txfm = 1;
  }

  const int is_cost_valid = this_rd > ref_best_rd;
  if (!is_cost_valid) {
    // reset cost value
    av1_invalid_rd_stats(rd_stats);
  }
  return is_cost_valid;
}

// Search for the best transform size and type for current inter-predicted
// luma block with recursive transform block partitioning. The obtained
// transform selection will be saved in xd->mi[0], the corresponding RD stats
// will be saved in rd_stats. The returned value is the corresponding RD cost.
static int64_t select_tx_size_and_type(const AV1_COMP *cpi, MACROBLOCK *x,
                                       RD_STATS *rd_stats, BLOCK_SIZE bsize,
                                       int64_t ref_best_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const TxfmSearchParams *txfm_params = &x->txfm_search_params;
  assert(is_inter_block(xd->mi[0]));
  assert(bsize < BLOCK_SIZES_ALL);
  const int fast_tx_search = txfm_params->tx_size_search_method > USE_FULL_RD;
  int64_t rd_thresh = ref_best_rd;
  if (rd_thresh == 0) {
    av1_invalid_rd_stats(rd_stats);
    return INT64_MAX;
  }
  if (fast_tx_search && rd_thresh < INT64_MAX) {
    if (INT64_MAX - rd_thresh > (rd_thresh >> 3)) rd_thresh += (rd_thresh >> 3);
  }
  assert(rd_thresh > 0);
  const FAST_TX_SEARCH_MODE ftxs_mode =
      fast_tx_search ? FTXS_DCT_AND_1D_DCT_ONLY : FTXS_NONE;
  const struct macroblockd_plane *const pd = &xd->plane[0];
  assert(bsize < BLOCK_SIZES_ALL);
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
  ENTROPY_CONTEXT ctxa[MAX_MIB_SIZE];
  ENTROPY_CONTEXT ctxl[MAX_MIB_SIZE];
  TXFM_CONTEXT tx_above[MAX_MIB_SIZE];
  TXFM_CONTEXT tx_left[MAX_MIB_SIZE];
  av1_get_entropy_contexts(bsize, pd, ctxa, ctxl);
  memcpy(tx_above, xd->above_txfm_context, sizeof(TXFM_CONTEXT) * mi_width);
  memcpy(tx_left, xd->left_txfm_context, sizeof(TXFM_CONTEXT) * mi_height);
  const int init_depth = get_search_init_depth(
      mi_width, mi_height, 1, &cpi->sf, txfm_params->tx_size_search_method);
  const TX_SIZE max_tx_size = max_txsize_rect_lookup[bsize];
  const int bh = tx_size_high_unit[max_tx_size];
  const int bw = tx_size_wide_unit[max_tx_size];
  const int step = bw * bh;
  const int skip_ctx = av1_get_skip_txfm_context(xd);
  const int no_skip_txfm_cost = x->mode_costs.skip_txfm_cost[skip_ctx][0];
  const int skip_txfm_cost = x->mode_costs.skip_txfm_cost[skip_ctx][1];
  int64_t skip_txfm_rd = RDCOST(x->rdmult, skip_txfm_cost, 0);
  int64_t no_skip_txfm_rd = RDCOST(x->rdmult, no_skip_txfm_cost, 0);
  int block = 0;
  int blk_idx = 0;

  av1_init_rd_stats(rd_stats);
  for (int idy = 0; idy < max_block_high(xd, bsize, 0); idy += bh) {
    for (int idx = 0; idx < max_block_wide(xd, bsize, 0); idx += bw) {
      const int64_t best_rd_sofar =
          (rd_thresh == INT64_MAX)
              ? INT64_MAX
              : (rd_thresh - (AOMMIN(skip_txfm_rd, no_skip_txfm_rd)));
      int is_cost_valid = 1;
      RD_STATS pn_rd_stats;
      // Search for the best transform block size and type for the sub-block.
      select_tx_block(cpi, x, idy, idx, block, max_tx_size, init_depth, bsize,
                      ctxa, ctxl, tx_above, tx_left, &pn_rd_stats, INT64_MAX,
                      best_rd_sofar, &is_cost_valid, ftxs_mode, blk_idx);
      blk_idx++;
      if (!is_cost_valid || pn_rd_stats.rate == INT_MAX) {
        av1_invalid_rd_stats(rd_stats);
        return INT64_MAX;
      }
      av1_merge_rd_stats(rd_stats, &pn_rd_stats);
      skip_txfm_rd = RDCOST(x->rdmult, skip_txfm_cost, rd_stats->sse);
      no_skip_txfm_rd =
          RDCOST(x->rdmult, rd_stats->rate + no_skip_txfm_cost, rd_stats->dist);
      block += step;
    }
  }

  if (rd_stats->rate == INT_MAX) return INT64_MAX;

  rd_stats->skip_txfm = (skip_txfm_rd <= no_skip_txfm_rd);

  // If fast_tx_search is true, only DCT and 1D DCT were tested in
  // select_inter_block_yrd() above. Do a better search for tx type with
  // tx sizes already decided.
  if (fast_tx_search && cpi->sf.tx_sf.refine_fast_tx_search_results) {
    if (!inter_block_yrd(cpi, x, rd_stats, bsize, ref_best_rd, FTXS_NONE))
      return INT64_MAX;
  }

  int64_t final_rd;
  if (rd_stats->skip_txfm) {
    final_rd = RDCOST(x->rdmult, skip_txfm_cost, rd_stats->sse);
  } else {
    final_rd =
        RDCOST(x->rdmult, rd_stats->rate + no_skip_txfm_cost, rd_stats->dist);
    if (!xd->lossless[xd->mi[0]->segment_id]) {
      final_rd =
          AOMMIN(final_rd, RDCOST(x->rdmult, skip_txfm_cost, rd_stats->sse));
    }
  }

  return final_rd;
}

// Return 1 to terminate transform search early. The decision is made based on
// the comparison with the reference RD cost and the model-estimated RD cost.
static inline int model_based_tx_search_prune(const AV1_COMP *cpi,
                                              MACROBLOCK *x, BLOCK_SIZE bsize,
                                              int64_t ref_best_rd) {
  const int level = cpi->sf.tx_sf.model_based_prune_tx_search_level;
  assert(level >= 0 && level <= 2);
  int model_rate;
  int64_t model_dist;
  uint8_t model_skip;
  MACROBLOCKD *const xd = &x->e_mbd;
  model_rd_sb_fn[MODELRD_TYPE_TX_SEARCH_PRUNE](
      cpi, bsize, x, xd, 0, 0, &model_rate, &model_dist, &model_skip, NULL,
      NULL, NULL, NULL);
  if (model_skip) return 0;
  const int64_t model_rd = RDCOST(x->rdmult, model_rate, model_dist);
  // TODO(debargha, urvang): Improve the model and make the check below
  // tighter.
  static const int prune_factor_by8[] = { 3, 5 };
  const int factor = prune_factor_by8[level - 1];
  return ((model_rd * factor) >> 3) > ref_best_rd;
}

void av1_pick_recursive_tx_size_type_yrd(const AV1_COMP *cpi, MACROBLOCK *x,
                                         RD_STATS *rd_stats, BLOCK_SIZE bsize,
                                         int64_t ref_best_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const TxfmSearchParams *txfm_params = &x->txfm_search_params;
  assert(is_inter_block(xd->mi[0]));

  av1_invalid_rd_stats(rd_stats);

  // If modeled RD cost is a lot worse than the best so far, terminate early.
  if (cpi->sf.tx_sf.model_based_prune_tx_search_level &&
      ref_best_rd != INT64_MAX) {
    if (model_based_tx_search_prune(cpi, x, bsize, ref_best_rd)) return;
  }

  // Hashing based speed feature. If the hash of the prediction residue block is
  // found in the hash table, use previous search results and terminate early.
  uint32_t hash = 0;
  MB_RD_RECORD *mb_rd_record = NULL;
  const int mi_row = x->e_mbd.mi_row;
  const int mi_col = x->e_mbd.mi_col;
  const int within_border =
      mi_row >= xd->tile.mi_row_start &&
      (mi_row + mi_size_high[bsize] < xd->tile.mi_row_end) &&
      mi_col >= xd->tile.mi_col_start &&
      (mi_col + mi_size_wide[bsize] < xd->tile.mi_col_end);
  const int is_mb_rd_hash_enabled =
      (within_border && cpi->sf.rd_sf.use_mb_rd_hash);
  const int n4 = bsize_to_num_blk(bsize);
  if (is_mb_rd_hash_enabled) {
    hash = get_block_residue_hash(x, bsize);
    mb_rd_record = x->txfm_search_info.mb_rd_record;
    const int match_index = find_mb_rd_info(mb_rd_record, ref_best_rd, hash);
    if (match_index != -1) {
      MB_RD_INFO *mb_rd_info = &mb_rd_record->mb_rd_info[match_index];
      fetch_mb_rd_info(n4, mb_rd_info, rd_stats, x);
      return;
    }
  }

  // If we predict that skip is the optimal RD decision - set the respective
  // context and terminate early.
  int64_t dist;
  if (txfm_params->skip_txfm_level &&
      predict_skip_txfm(cpi, x, bsize, &dist,
                        cpi->common.features.reduced_tx_set_used)) {
    set_skip_txfm(x, rd_stats, bsize, dist);
    // Save the RD search results into mb_rd_record.
    if (is_mb_rd_hash_enabled)
      save_mb_rd_info(n4, hash, x, rd_stats, mb_rd_record);
    return;
  }
#if CONFIG_SPEED_STATS
  ++x->txfm_search_info.tx_search_count;
#endif  // CONFIG_SPEED_STATS

  const int64_t rd =
      select_tx_size_and_type(cpi, x, rd_stats, bsize, ref_best_rd);

  if (rd == INT64_MAX) {
    // We should always find at least one candidate unless ref_best_rd is less
    // than INT64_MAX (in which case, all the calls to select_tx_size_fix_type
    // might have failed to find something better)
    assert(ref_best_rd != INT64_MAX);
    av1_invalid_rd_stats(rd_stats);
    return;
  }

  // Save the RD search results into mb_rd_record.
  if (is_mb_rd_hash_enabled) {
    assert(mb_rd_record != NULL);
    save_mb_rd_info(n4, hash, x, rd_stats, mb_rd_record);
  }
}

void av1_pick_uniform_tx_size_type_yrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                                       RD_STATS *rd_stats, BLOCK_SIZE bs,
                                       int64_t ref_best_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const TxfmSearchParams *tx_params = &x->txfm_search_params;
  assert(bs == mbmi->bsize);
  const int is_inter = is_inter_block(mbmi);

  av1_init_rd_stats(rd_stats);

  uint32_t hash = 0;
  MB_RD_RECORD *mb_rd_record = NULL;
  // Only set (and only used) when mb_rd_record is set.
  int num_blks = 0;
  if (is_inter) {
    // Hashing based speed feature for inter blocks. If the hash of the residue
    // block is found in the table, use previously saved search results and
    // terminate early.
    if (cpi->sf.rd_sf.use_mb_rd_hash) {
      const int mi_row = xd->mi_row;
      const int mi_col = xd->mi_col;
      const int within_border =
          mi_row >= xd->tile.mi_row_start &&
          (mi_row + mi_size_high[bs] < xd->tile.mi_row_end) &&
          mi_col >= xd->tile.mi_col_start &&
          (mi_col + mi_size_wide[bs] < xd->tile.mi_col_end);
      if (within_border) {
        num_blks = bsize_to_num_blk(bs);
        hash = get_block_residue_hash(x, bs);
        mb_rd_record = x->txfm_search_info.mb_rd_record;
        const int match_index =
            find_mb_rd_info(mb_rd_record, ref_best_rd, hash);
        if (match_index != -1) {
          MB_RD_INFO *mb_rd_info = &mb_rd_record->mb_rd_info[match_index];
          fetch_mb_rd_info(num_blks, mb_rd_info, rd_stats, x);
          return;
        }
      }
    }

    // If we predict that skip is the optimal RD decision - set the respective
    // context and terminate early.
    int64_t dist;
    if (tx_params->skip_txfm_level && !xd->lossless[mbmi->segment_id] &&
        predict_skip_txfm(cpi, x, bs, &dist,
                          cpi->common.features.reduced_tx_set_used)) {
      // Populate rdstats as per skip decision
      set_skip_txfm(x, rd_stats, bs, dist);
      // Save the RD search results into mb_rd_record.
      if (mb_rd_record) {
        save_mb_rd_info(num_blks, hash, x, rd_stats, mb_rd_record);
      }
      return;
    }
  }

  if (UNLIKELY(xd->lossless[mbmi->segment_id])) {
    // Lossless mode can only pick the smallest (4x4) transform size.
    choose_smallest_tx_size(cpi, x, rd_stats, ref_best_rd, bs);
  } else if (tx_params->tx_size_search_method == USE_LARGESTALL) {
    choose_largest_tx_size(cpi, x, rd_stats, ref_best_rd, bs);
  } else {
    choose_tx_size_type_from_rd(cpi, x, rd_stats, ref_best_rd, bs);
  }

  // Save the RD search results into mb_rd_record for possible reuse in future.
  if (mb_rd_record) {
    save_mb_rd_info(num_blks, hash, x, rd_stats, mb_rd_record);
  }
}

int av1_txfm_uvrd(const AV1_COMP *const cpi, MACROBLOCK *x, RD_STATS *rd_stats,
                  BLOCK_SIZE bsize, int64_t ref_best_rd) {
  av1_init_rd_stats(rd_stats);
  if (ref_best_rd < 0) return 0;
  if (!x->e_mbd.is_chroma_ref) return 1;

  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  struct macroblockd_plane *const pd = &xd->plane[AOM_PLANE_U];
  const int is_inter = is_inter_block(mbmi);
  int64_t this_rd = 0, skip_txfm_rd = 0;
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(bsize, pd->subsampling_x, pd->subsampling_y);

  if (is_inter) {
    for (int plane = 1; plane < MAX_MB_PLANE; ++plane)
      av1_subtract_plane(x, plane_bsize, plane, cpi->do_border_pad);
  }

  const TX_SIZE uv_tx_size = av1_get_tx_size(AOM_PLANE_U, xd);
  int is_cost_valid = 1;
  for (int plane = 1; plane < MAX_MB_PLANE; ++plane) {
    RD_STATS this_rd_stats;
    int64_t chroma_ref_best_rd = ref_best_rd;
    // For inter blocks, refined ref_best_rd is used for early exit
    // For intra blocks, even though current rd crosses ref_best_rd, early
    // exit is not recommended as current rd is used for gating subsequent
    // modes as well (say, for angular modes)
    // TODO(any): Extend the early exit mechanism for intra modes as well
    if (cpi->sf.inter_sf.perform_best_rd_based_gating_for_chroma && is_inter &&
        chroma_ref_best_rd != INT64_MAX)
      chroma_ref_best_rd = ref_best_rd - AOMMIN(this_rd, skip_txfm_rd);
    av1_txfm_rd_in_plane(x, cpi, &this_rd_stats, chroma_ref_best_rd, 0, plane,
                         plane_bsize, uv_tx_size, FTXS_NONE);
    if (this_rd_stats.rate == INT_MAX) {
      is_cost_valid = 0;
      break;
    }
    av1_merge_rd_stats(rd_stats, &this_rd_stats);
    this_rd = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
    skip_txfm_rd = RDCOST(x->rdmult, 0, rd_stats->sse);
    if (AOMMIN(this_rd, skip_txfm_rd) > ref_best_rd) {
      is_cost_valid = 0;
      break;
    }
  }

  if (!is_cost_valid) {
    // reset cost value
    av1_invalid_rd_stats(rd_stats);
  }

  return is_cost_valid;
}

// Transform search for a plane block that is split into multiple transform
// blocks. pack_hi holds the plane, plane_bsize, tx_size and ftxs_mode fields
// of pack_tx_blk_args().
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void txfm_rd_in_plane_multi(MACROBLOCK *x, const AV1_COMP *cpi,
                                   RD_STATS *rd_stats, int64_t ref_best_rd,
                                   int64_t current_rd, uint64_t pack_hi) {
  const int plane = (uint8_t)(pack_hi >> 32);
  const BLOCK_SIZE plane_bsize = (BLOCK_SIZE)(uint8_t)(pack_hi >> 40);
  const TX_SIZE plane_tx_size = (TX_SIZE)(uint8_t)(pack_hi >> 48);
  MACROBLOCKD *const xd = &x->e_mbd;
  const int is_inter = is_inter_block(xd->mi[0]);
  const AV1_COMMON *const cm = &cpi->common;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  ENTROPY_CONTEXT t_above[MAX_MIB_SIZE];
  ENTROPY_CONTEXT t_left[MAX_MIB_SIZE];
  av1_init_rd_stats(rd_stats);
  av1_get_entropy_contexts(plane_bsize, pd, t_above, t_left);
  const uint8_t txw_unit = tx_size_wide_unit[plane_tx_size];
  const uint8_t txh_unit = tx_size_high_unit[plane_tx_size];
  const int step = txw_unit * txh_unit;
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const BLOCK_SIZE max_unit_bsize =
      get_plane_block_size(BLOCK_64X64, pd->subsampling_x, pd->subsampling_y);
  const int mu_blocks_wide =
      AOMMIN(mi_size_wide[max_unit_bsize], max_blocks_wide);
  const int mu_blocks_high =
      AOMMIN(mi_size_high[max_unit_bsize], max_blocks_high);
  const int rdmult = x->rdmult;
  const bool store_cfl = (plane == AOM_PLANE_Y && xd->cfl.store_y != 0);
  int exit_early = 0;
  int incomplete_exit = 0;
  int i = 0;

  for (int row_base = 0; row_base < max_blocks_high && !incomplete_exit;
       row_base += mu_blocks_high) {
    const int unit_height = AOMMIN(mu_blocks_high + row_base, max_blocks_high);
    for (int col_base = 0; col_base < max_blocks_wide && !incomplete_exit;
         col_base += mu_blocks_wide) {
      const int unit_width = AOMMIN(mu_blocks_wide + col_base, max_blocks_wide);
      for (int blk_row = row_base; blk_row < unit_height && !incomplete_exit;
           blk_row += txh_unit) {
        for (int blk_col = col_base; blk_col < unit_width;
             blk_col += txw_unit) {
          if (exit_early) {
            incomplete_exit = 1;
            break;
          }
          block_rd_txfm_multi(cpi, cm, x, xd, is_inter, rdmult, store_cfl,
                              pack_hi, plane, i, blk_row, blk_col, plane_bsize,
                              plane_tx_size, t_above, t_left, rd_stats,
                              &current_rd, ref_best_rd, &exit_early);
          i += step;
        }
      }
    }
  }

  if (is_inter ? incomplete_exit : exit_early) {
    av1_invalid_rd_stats(rd_stats);
  }
}

void av1_txfm_rd_in_plane(MACROBLOCK *x, const AV1_COMP *cpi,
                          RD_STATS *rd_stats, int64_t ref_best_rd,
                          int64_t current_rd, int plane, BLOCK_SIZE plane_bsize,
                          TX_SIZE tx_size, FAST_TX_SEARCH_MODE ftxs_mode) {
  assert(IMPLIES(plane == 0, x->e_mbd.mi[0]->tx_size == tx_size));

  if (UNLIKELY(!cpi->oxcf.txfm_cfg.enable_tx64 &&
               txsize_sqr_up_map[tx_size] == TX_64X64)) {
    av1_invalid_rd_stats(rd_stats);
    return;
  }

  if (current_rd > ref_best_rd) {
    av1_invalid_rd_stats(rd_stats);
    return;
  }

  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const TX_SIZE plane_tx_size =
      UNLIKELY(xd->lossless[mbmi->segment_id]) ? TX_4X4 : tx_size;
  const BLOCK_SIZE tx_bsize = txsize_to_bsize[plane_tx_size];

  if (LIKELY(plane_bsize == tx_bsize)) {
    const int is_inter = is_inter_block(mbmi);
    if (!is_inter) {
      av1_predict_intra_block_facade(&cpi->common, xd, plane, 0, 0,
                                     plane_tx_size);
      av1_subtract_txb(x, plane, plane_bsize, 0, 0, plane_tx_size, DCT_DCT,
                       false);
#if !CONFIG_REALTIME_ONLY
      const TxfmSearchParams *const txfm_params = &x->txfm_search_params;
      if (txfm_params->enable_nn_prune_intra_tx_depths) {
        ml_predict_intra_tx_depth_prune(x, 0, 0, plane_bsize, plane_tx_size);
        if (txfm_params->nn_prune_depths_for_intra_tx == TX_PRUNE_LARGEST) {
          av1_invalid_rd_stats(rd_stats);
          return;
        }
      }
#endif
    }
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    TXB_CTX txb_ctx;
    get_txb_ctx_fast(plane_bsize, plane_tx_size, plane,
                     pd->above_entropy_context, pd->left_entropy_context,
                     &txb_ctx);
    const uint64_t pack_hi =
        pack_tx_blk_args(0, 0, 0, plane, plane_bsize, plane_tx_size, ftxs_mode);
    search_tx_type(cpi, x, pack_hi, &txb_ctx, ref_best_rd - current_rd,
                   rd_stats);
#if !CONFIG_REALTIME_ONLY
    if (UNLIKELY(plane == AOM_PLANE_Y && xd->cfl.store_y)) {
      assert(!is_inter || plane_bsize < BLOCK_8X8);
      cfl_store_tx(xd, 0, 0, plane_tx_size, plane_bsize);
    }
#endif
#if CONFIG_RD_DEBUG
    update_txb_coeff_cost(rd_stats, plane, rd_stats->rate);
#endif
    if (is_inter) {
      rd_stats->skip_txfm &= !x->plane[plane].eobs[0];
    } else {
      const int64_t rd = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
      if (current_rd + rd > ref_best_rd) {
        av1_invalid_rd_stats(rd_stats);
        return;
      }
      rd_stats->skip_txfm = 0;
    }
    rd_stats->rdcost = 0;
    return;
  }

  txfm_rd_in_plane_multi(
      x, cpi, rd_stats, ref_best_rd, current_rd,
      pack_tx_blk_args(0, 0, 0, plane, plane_bsize, plane_tx_size, ftxs_mode));
}

int av1_txfm_search(const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
                    RD_STATS *rd_stats, RD_STATS *rd_stats_y,
                    RD_STATS *rd_stats_uv, int mode_rate, int64_t ref_best_rd) {
  MACROBLOCKD *const xd = &x->e_mbd;
  TxfmSearchParams *txfm_params = &x->txfm_search_params;
  const int skip_ctx = av1_get_skip_txfm_context(xd);
  const int skip_txfm_cost[2] = { x->mode_costs.skip_txfm_cost[skip_ctx][0],
                                  x->mode_costs.skip_txfm_cost[skip_ctx][1] };
  const int64_t min_header_rate =
      mode_rate + AOMMIN(skip_txfm_cost[0], skip_txfm_cost[1]);
  // Account for minimum skip and non_skip rd.
  // Eventually either one of them will be added to mode_rate
  const int64_t min_header_rd_possible = RDCOST(x->rdmult, min_header_rate, 0);
  if (min_header_rd_possible > ref_best_rd) {
    av1_invalid_rd_stats(rd_stats_y);
    return 0;
  }

  const AV1_COMMON *cm = &cpi->common;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int64_t mode_rd = RDCOST(x->rdmult, mode_rate, 0);
  const int64_t rd_thresh =
      ref_best_rd == INT64_MAX ? INT64_MAX : ref_best_rd - mode_rd;
  av1_init_rd_stats(rd_stats);
  av1_init_rd_stats(rd_stats_y);
  rd_stats->rate = mode_rate;

  // cost and distortion
  av1_subtract_plane(x, bsize, PLANE_TYPE_Y, cpi->do_border_pad);
  if (txfm_params->tx_mode_search_type == TX_MODE_SELECT &&
      !xd->lossless[mbmi->segment_id]) {
    av1_pick_recursive_tx_size_type_yrd(cpi, x, rd_stats_y, bsize, rd_thresh);
#if CONFIG_COLLECT_RD_STATS == 2
    PrintPredictionUnitStats(cpi, tile_data, x, rd_stats_y, bsize);
#endif  // CONFIG_COLLECT_RD_STATS == 2
  } else {
    av1_pick_uniform_tx_size_type_yrd(cpi, x, rd_stats_y, bsize, rd_thresh);
    memset(mbmi->inter_tx_size, mbmi->tx_size, sizeof(mbmi->inter_tx_size));
  }

  if (rd_stats_y->rate == INT_MAX) return 0;

  av1_merge_rd_stats(rd_stats, rd_stats_y);

  const int64_t non_skip_txfm_rdcosty =
      RDCOST(x->rdmult, rd_stats->rate + skip_txfm_cost[0], rd_stats->dist);
  const int64_t skip_txfm_rdcosty =
      RDCOST(x->rdmult, mode_rate + skip_txfm_cost[1], rd_stats->sse);
  const int64_t min_rdcosty = AOMMIN(non_skip_txfm_rdcosty, skip_txfm_rdcosty);
  if (min_rdcosty > ref_best_rd) return 0;

  av1_init_rd_stats(rd_stats_uv);
  const int num_planes = av1_num_planes(cm);
  if (num_planes > 1) {
    int64_t ref_best_chroma_rd = ref_best_rd;
    // Calculate best rd cost possible for chroma
    if (cpi->sf.inter_sf.perform_best_rd_based_gating_for_chroma &&
        (ref_best_chroma_rd != INT64_MAX)) {
      ref_best_chroma_rd = (ref_best_chroma_rd -
                            AOMMIN(non_skip_txfm_rdcosty, skip_txfm_rdcosty));
    }
    const int is_cost_valid_uv =
        av1_txfm_uvrd(cpi, x, rd_stats_uv, bsize, ref_best_chroma_rd);
    if (!is_cost_valid_uv) return 0;

    if (cpi->sf.hl_sf.weighted_chroma_distortion) {
      // Apply weighted distortion/SSE accumulation while merging uv rd stats to
      // y rd stats.
      av1_merge_rd_stats_weighted(rd_stats, rd_stats_uv);
    } else {
      av1_merge_rd_stats(rd_stats, rd_stats_uv);
    }
  }

  int choose_skip_txfm = rd_stats->skip_txfm;
  if (!choose_skip_txfm && !xd->lossless[mbmi->segment_id]) {
    int64_t no_skip_dist = rd_stats->dist;
    int64_t skip_dist = rd_stats->sse;
#if CONFIG_AV1_HIGHBITDEPTH
    if (xd->bd > 8 && cpi->oxcf.algo_cfg.sharpness == 3) {
      av1_get_tx_skip_dist(cpi, x, bsize, rd_stats->dist, rd_stats->sse,
                           &no_skip_dist, &skip_dist);
    }
#endif
    const int64_t rdcost_no_skip_txfm = RDCOST(
        x->rdmult, rd_stats_y->rate + rd_stats_uv->rate + skip_txfm_cost[0],
        no_skip_dist);
    const int64_t rdcost_skip_txfm =
        RDCOST(x->rdmult, skip_txfm_cost[1], skip_dist);
    if (rdcost_no_skip_txfm >= rdcost_skip_txfm) choose_skip_txfm = 1;
  }
  if (choose_skip_txfm) {
    const int naturally_skip = rd_stats->skip_txfm;
    rd_stats_y->rate = 0;
    rd_stats_uv->rate = 0;
    rd_stats->rate = mode_rate + skip_txfm_cost[1];
    rd_stats->dist = rd_stats->sse;
    rd_stats_y->dist = rd_stats_y->sse;
    rd_stats_uv->dist = rd_stats_uv->sse;
    mbmi->skip_txfm = 1;
    rd_stats->skip_txfm = 1;
    if (naturally_skip) {
      const int64_t tmprd = RDCOST(x->rdmult, rd_stats->rate, rd_stats->dist);
      if (tmprd > ref_best_rd) return 0;
    }
  } else {
    rd_stats->rate += skip_txfm_cost[0];
    mbmi->skip_txfm = 0;
    rd_stats->skip_txfm = 0;
  }

  return 1;
}
