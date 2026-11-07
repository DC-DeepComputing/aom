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

#include <stdbool.h>
#include <stddef.h>

#include "config/aom_config.h"

#if HAVE_SSE2 && (defined(__SSE2__) || defined(_M_X64) || \
                  (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#include <emmintrin.h>
#define INTRA_MODE_SEARCH_SSE2 1
#else
#define INTRA_MODE_SEARCH_SSE2 0
#endif

#include "av1/common/av1_common_int.h"
#include "av1/common/cfl.h"
#include "av1/common/reconintra.h"

#include "av1/encoder/intra_mode_search.h"
#include "av1/encoder/intra_mode_search_utils.h"
#include "av1/encoder/palette.h"
#include "av1/encoder/speed_features.h"
#include "av1/encoder/tx_search.h"

// Even though there are 7 delta angles, this macro is set to 9 to facilitate
// the rd threshold check to prune -3 and 3 delta angles.
#define SIZE_OF_ANGLE_DELTA_RD_COST_ARRAY (2 * MAX_ANGLE_DELTA + 3)

/*!\cond */
static const UV_PREDICTION_MODE uv_rd_search_mode_order[UV_INTRA_MODES] = {
  UV_DC_PRED,     UV_CFL_PRED,   UV_H_PRED,        UV_V_PRED,
  UV_SMOOTH_PRED, UV_PAETH_PRED, UV_SMOOTH_V_PRED, UV_SMOOTH_H_PRED,
  UV_D135_PRED,   UV_D203_PRED,  UV_D157_PRED,     UV_D67_PRED,
  UV_D113_PRED,   UV_D45_PRED,
};

// The bitmask corresponds to the filter intra modes as defined in enums.h
// FILTER_INTRA_MODE enumeration type. Setting a bit to 0 in the mask means to
// disable the evaluation of corresponding filter intra mode. The table
// av1_derived_filter_intra_mode_used_flag is used when speed feature
// prune_filter_intra_level is 1. The evaluated filter intra modes are union
// of the following:
// 1) FILTER_DC_PRED
// 2) mode that corresponds to best mode so far of DC_PRED, V_PRED, H_PRED,
// D157_PRED and PAETH_PRED. (Eg: FILTER_V_PRED if best mode so far is V_PRED).
static const uint8_t av1_derived_filter_intra_mode_used_flag[INTRA_MODES] = {
  0x01,  // DC_PRED:           0000 0001
  0x03,  // V_PRED:            0000 0011
  0x05,  // H_PRED:            0000 0101
  0x01,  // D45_PRED:          0000 0001
  0x01,  // D135_PRED:         0000 0001
  0x01,  // D113_PRED:         0000 0001
  0x09,  // D157_PRED:         0000 1001
  0x01,  // D203_PRED:         0000 0001
  0x01,  // D67_PRED:          0000 0001
  0x01,  // SMOOTH_PRED:       0000 0001
  0x01,  // SMOOTH_V_PRED:     0000 0001
  0x01,  // SMOOTH_H_PRED:     0000 0001
  0x11   // PAETH_PRED:        0001 0001
};

// The bitmask corresponds to the chroma intra modes as defined in enums.h
// UV_PREDICTION_MODE enumeration type. Setting a bit to 0 in the mask means to
// disable the evaluation of corresponding chroma intra mode. The table
// av1_derived_chroma_intra_mode_used_flag is used when speed feature
// prune_chroma_modes_using_luma_winner is enabled. The evaluated chroma
// intra modes are union of the following:
// 1) UV_DC_PRED
// 2) UV_SMOOTH_PRED
// 3) UV_CFL_PRED
// 4) mode that corresponds to luma intra mode winner (Eg : UV_V_PRED if luma
// intra mode winner is V_PRED).
static const uint16_t av1_derived_chroma_intra_mode_used_flag[INTRA_MODES] = {
  0x2201,  // DC_PRED:           0010 0010 0000 0001
  0x2203,  // V_PRED:            0010 0010 0000 0011
  0x2205,  // H_PRED:            0010 0010 0000 0101
  0x2209,  // D45_PRED:          0010 0010 0000 1001
  0x2211,  // D135_PRED:         0010 0010 0001 0001
  0x2221,  // D113_PRED:         0010 0010 0010 0001
  0x2241,  // D157_PRED:         0010 0010 0100 0001
  0x2281,  // D203_PRED:         0010 0010 1000 0001
  0x2301,  // D67_PRED:          0010 0011 0000 0001
  0x2201,  // SMOOTH_PRED:       0010 0010 0000 0001
  0x2601,  // SMOOTH_V_PRED:     0010 0110 0000 0001
  0x2a01,  // SMOOTH_H_PRED:     0010 1010 0000 0001
  0x3201   // PAETH_PRED:        0011 0010 0000 0001
};

DECLARE_ALIGNED(16, static const uint8_t, all_zeros[MAX_SB_SIZE]) = { 0 };
DECLARE_ALIGNED(16, static const uint16_t,
                highbd_all_zeros[MAX_SB_SIZE]) = { 0 };

int av1_calc_normalized_variance(aom_variance_fn_t vf, const uint8_t *const buf,
                                 const int stride, const int is_hbd) {
  unsigned int sse;

  if (is_hbd)
    return vf(buf, stride, CONVERT_TO_BYTEPTR(highbd_all_zeros), 0, &sse);
  else
    return vf(buf, stride, all_zeros, 0, &sse);
}

// Computes average of log(1 + variance) across 4x4 sub-blocks for source and
// reconstructed blocks.
static void compute_avg_log_variance(const AV1_COMP *const cpi, MACROBLOCK *x,
                                     const BLOCK_SIZE bs,
                                     double *avg_log_src_variance,
                                     double *avg_log_recon_variance) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const BLOCK_SIZE sb_size = cpi->common.seq_params->sb_size;
  const int mi_row_in_sb = x->e_mbd.mi_row & (mi_size_high[sb_size] - 1);
  const int mi_col_in_sb = x->e_mbd.mi_col & (mi_size_wide[sb_size] - 1);
  const int right_overflow =
      (xd->mb_to_right_edge < 0) ? ((-xd->mb_to_right_edge) >> 3) : 0;
  const int bottom_overflow =
      (xd->mb_to_bottom_edge < 0) ? ((-xd->mb_to_bottom_edge) >> 3) : 0;
  const int bw = (MI_SIZE * mi_size_wide[bs] - right_overflow);
  const int bh = (MI_SIZE * mi_size_high[bs] - bottom_overflow);
  const int is_hbd = is_cur_buf_hbd(xd);

  aom_variance_fn_t vf = cpi->ppi->fn_ptr[BLOCK_4X4].vf;
  for (int i = 0; i < bh; i += MI_SIZE) {
    const int r = mi_row_in_sb + (i >> MI_SIZE_LOG2);
    for (int j = 0; j < bw; j += MI_SIZE) {
      const int c = mi_col_in_sb + (j >> MI_SIZE_LOG2);
      const int mi_offset = r * mi_size_wide[sb_size] + c;
      Block4x4VarInfo *block_4x4_var_info =
          &x->src_var_info_of_4x4_sub_blocks[mi_offset];
      int src_var = block_4x4_var_info->var;
      double log_src_var = block_4x4_var_info->log_var;
      // Compute average of log(1 + variance) for the source block from 4x4
      // sub-block variance values. Calculate and store 4x4 sub-block variance
      // and log(1 + variance), if the values present in
      // src_var_of_4x4_sub_blocks are invalid. Reuse the same if it is readily
      // available with valid values.
      if (src_var < 0) {
        src_var = av1_calc_normalized_variance(
            vf, x->plane[0].src.buf + i * x->plane[0].src.stride + j,
            x->plane[0].src.stride, is_hbd);
        block_4x4_var_info->var = src_var;
        log_src_var = log1p(src_var / 16.0);
        block_4x4_var_info->log_var = log_src_var;
      } else {
        // When source variance is already calculated and available for
        // retrieval, check if log(1 + variance) is also available. If it is
        // available, then retrieve from buffer. Else, calculate the same and
        // store to the buffer.
        if (log_src_var < 0) {
          log_src_var = log1p(src_var / 16.0);
          block_4x4_var_info->log_var = log_src_var;
        }
      }
      *avg_log_src_variance += log_src_var;

      const int recon_var = av1_calc_normalized_variance(
          vf, xd->plane[0].dst.buf + i * xd->plane[0].dst.stride + j,
          xd->plane[0].dst.stride, is_hbd);
      *avg_log_recon_variance += log1p(recon_var / 16.0);
    }
  }

  const int blocks = (bw * bh) / 16;
  *avg_log_src_variance /= (double)blocks;
  *avg_log_recon_variance /= (double)blocks;
}

// Returns a factor to be applied to the RD value based on how well the
// reconstructed block variance matches the source variance.
static double intra_rd_variance_factor(const AV1_COMP *cpi, MACROBLOCK *x,
                                       BLOCK_SIZE bs) {
  double threshold = INTRA_RD_VAR_THRESH(cpi->oxcf.speed);
  // For non-positive threshold values, the comparison of source and
  // reconstructed variances with threshold evaluates to false
  // (src_var < threshold/rec_var < threshold) as these metrics are greater than
  // than 0. Hence further calculations are skipped.
  if (threshold <= 0) return 1.0;

  double variance_rd_factor = 1.0;
  double avg_log_src_variance = 0.0;
  double avg_log_recon_variance = 0.0;
  double var_diff = 0.0;

  compute_avg_log_variance(cpi, x, bs, &avg_log_src_variance,
                           &avg_log_recon_variance);

  // Dont allow 0 to prevent / 0 below.
  avg_log_src_variance += 0.000001;
  avg_log_recon_variance += 0.000001;

  if (avg_log_src_variance >= avg_log_recon_variance) {
    var_diff = (avg_log_src_variance - avg_log_recon_variance);
    if ((var_diff > 0.5) && (avg_log_recon_variance < threshold)) {
      variance_rd_factor = 1.0 + ((var_diff * 2) / avg_log_src_variance);
    }
  } else {
    var_diff = (avg_log_recon_variance - avg_log_src_variance);
    if ((var_diff > 0.5) && (avg_log_src_variance < threshold)) {
      variance_rd_factor = 1.0 + (var_diff / (2 * avg_log_src_variance));
    }
  }

  // Limit adjustment;
  variance_rd_factor = AOMMIN(3.0, variance_rd_factor);

  return variance_rd_factor;
}
/*!\endcond */

/*!\brief Search for the best filter_intra mode when coding intra frame.
 *
 * \ingroup intra_mode_search
 * \callergraph
 * This function loops through all filter_intra modes to find the best one.
 *
 * \return Returns 1 if a new filter_intra mode is selected; 0 otherwise.
 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int rd_pick_filter_intra_sby(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    int *rate, int *rate_tokenonly,
                                    int64_t *distortion, uint8_t *skippable,
                                    BLOCK_SIZE bsize, int mode_cost,
                                    PREDICTION_MODE best_mode_so_far,
                                    int64_t *best_rd, int64_t *best_model_rd,
                                    PICK_MODE_CONTEXT *ctx) {
  // Skip the evaluation of filter intra modes.
  if (cpi->sf.intra_sf.prune_filter_intra_level == 2) return 0;

  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  int filter_intra_selected_flag = 0;
  FILTER_INTRA_MODE mode;
  TX_SIZE best_tx_size = TX_8X8;
  FILTER_INTRA_MODE_INFO filter_intra_mode_info;
  av1_zero(filter_intra_mode_info);
  mbmi->filter_intra_mode_info.use_filter_intra = 1;
  mbmi->mode = DC_PRED;
  mbmi->palette_mode_info.palette_size[0] = 0;

  // Skip the evaluation of filter-intra if cached MB_MODE_INFO does not have
  // filter-intra as winner.
  if (x->use_mb_mode_cache &&
      !x->mb_mode_cache->filter_intra_mode_info.use_filter_intra)
    return 0;

  const ModeCosts *const mode_costs = &x->mode_costs;
  int fi_base_cost = mode_cost;
  if (av1_allow_palette(cpi->common.features.allow_screen_content_tools,
                        mbmi->bsize)) {
    fi_base_cost +=
        mode_costs->palette_y_mode_cost[av1_get_palette_bsize_ctx(bsize)]
                                       [av1_get_palette_mode_ctx(xd)][0];
  }
  const int fi_allowed =
      av1_filter_intra_allowed_bsize(&cpi->common, mbmi->bsize);
  if (fi_allowed) {
    fi_base_cost += mode_costs->filter_intra_cost[mbmi->bsize][1];
  }
  if (av1_allow_intrabc(&cpi->common)) {
    fi_base_cost += mode_costs->intrabc_cost[mbmi->use_intrabc];
  }
  const int use_var_factor =
      (cpi->oxcf.mode == ALLINTRA) && (cpi->oxcf.speed < 4);

  for (mode = 0; mode < FILTER_INTRA_MODES; ++mode) {
    int64_t this_rd;
    RD_STATS tokenonly_rd_stats;
    mbmi->filter_intra_mode_info.filter_intra_mode = mode;

    if ((cpi->sf.intra_sf.prune_filter_intra_level == 1) &&
        !(av1_derived_filter_intra_mode_used_flag[best_mode_so_far] &
          (1 << mode)))
      continue;

    // Skip the evaluation of modes that do not match with the winner mode in
    // x->mb_mode_cache.
    if (x->use_mb_mode_cache &&
        mode != x->mb_mode_cache->filter_intra_mode_info.filter_intra_mode)
      continue;

    if (model_intra_yrd_and_prune(cpi, x, bsize, best_model_rd)) {
      continue;
    }
    av1_pick_uniform_tx_size_type_yrd(cpi, x, &tokenonly_rd_stats, bsize,
                                      *best_rd);
    if (tokenonly_rd_stats.rate == INT_MAX) continue;
    const int this_rate =
        tokenonly_rd_stats.rate + fi_base_cost +
        (fi_allowed ? mode_costs->filter_intra_mode_cost[mode] : 0);
    this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);

    // Visual quality adjustment based on recon vs source variance.
    if (use_var_factor && (this_rd != INT64_MAX)) {
      this_rd = (int64_t)(this_rd * intra_rd_variance_factor(cpi, x, bsize));
    }

    // Collect mode stats for multiwinner mode processing
    const int txfm_search_done = 1;
    store_winner_mode_stats(
        cpi, x, mbmi, NULL, NULL, NULL, 0, NULL, bsize, this_rd,
        cpi->sf.winner_mode_sf.multi_winner_mode_type, txfm_search_done);
    if (this_rd < *best_rd) {
      *best_rd = this_rd;
      best_tx_size = mbmi->tx_size;
      filter_intra_mode_info = mbmi->filter_intra_mode_info;
      av1_copy_tx_type_map(ctx->tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
      *rate = this_rate;
      *rate_tokenonly = tokenonly_rd_stats.rate;
      *distortion = tokenonly_rd_stats.dist;
      *skippable = tokenonly_rd_stats.skip_txfm;
      filter_intra_selected_flag = 1;
    }
  }

  if (filter_intra_selected_flag) {
    mbmi->mode = DC_PRED;
    mbmi->tx_size = best_tx_size;
    mbmi->filter_intra_mode_info = filter_intra_mode_info;
    return 1;
  } else {
    return 0;
  }
}

#if INTRA_MODE_SEARCH_SSE2
static inline int count_bits_32(uint32_t x) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_popcount(x);
#else
  x = x - ((x >> 1) & 0x55555555U);
  x = (x & 0x33333333U) + ((x >> 2) & 0x33333333U);
  return (int)((((x + (x >> 4)) & 0x0F0F0F0FU) * 0x01010101U) >> 24);
#endif
}
#endif

void av1_count_colors(const uint8_t *src, int stride, int rows, int cols,
                      int *val_count, int *num_colors) {
  const int max_pix_val = 1 << 8;
  memset(val_count, 0, max_pix_val * sizeof(val_count[0]));
  for (int r = 0; r < rows; ++r) {
    int c = 0;
    for (; c <= cols - 8; c += 8) {
      uint64_t w;
      memcpy(&w, src + c, sizeof(w));
      const uint8_t b0 = src[c];
      if (w == (uint64_t)b0 * 0x0101010101010101ULL) {
        val_count[b0] += 8;
      } else {
        ++val_count[b0];
        ++val_count[src[c + 1]];
        ++val_count[src[c + 2]];
        ++val_count[src[c + 3]];
        ++val_count[src[c + 4]];
        ++val_count[src[c + 5]];
        ++val_count[src[c + 6]];
        ++val_count[src[c + 7]];
      }
    }
    for (; c < cols; ++c) ++val_count[src[c]];
    src += stride;
  }
#if INTRA_MODE_SEARCH_SSE2
  const __m128i zero = _mm_setzero_si128();
  int zero_cnt = 0;
  for (int i = 0; i < max_pix_val; i += 16) {
    const __m128i v0 = _mm_loadu_si128((const __m128i *)(val_count + i + 0));
    const __m128i v1 = _mm_loadu_si128((const __m128i *)(val_count + i + 4));
    const __m128i v2 = _mm_loadu_si128((const __m128i *)(val_count + i + 8));
    const __m128i v3 = _mm_loadu_si128((const __m128i *)(val_count + i + 12));
    const __m128i p01 = _mm_packs_epi32(v0, v1);
    const __m128i p23 = _mm_packs_epi32(v2, v3);
    const __m128i eq0 = _mm_cmpeq_epi8(_mm_packs_epi16(p01, p23), zero);
    zero_cnt += count_bits_32((uint32_t)_mm_movemask_epi8(eq0));
  }
  *num_colors = max_pix_val - zero_cnt;
#else
  int n = 0;
  for (int i = 0; i < max_pix_val; ++i) {
    if (val_count[i]) ++n;
  }
  *num_colors = n;
#endif
}

void av1_count_colors_highbd(const uint8_t *src8, int stride, int rows,
                             int cols, int bit_depth, int *val_count,
                             int *bin_val_count, int *num_color_bins,
                             int *num_colors) {
  assert(bit_depth <= 12);
  const int max_bin_val = 1 << 8;
  const int max_pix_val = 1 << bit_depth;
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  memset(bin_val_count, 0, max_bin_val * sizeof(val_count[0]));
  if (val_count != NULL)
    memset(val_count, 0, max_pix_val * sizeof(val_count[0]));
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      /*
       * Down-convert the pixels to 8-bit domain before counting.
       * This provides consistency of behavior for palette search
       * between lbd and hbd encodes. This down-converted pixels
       * are only used for calculating the threshold (n).
       */
      const int this_val = ((src[r * stride + c]) >> (bit_depth - 8));
      assert(this_val < max_bin_val);
      if (this_val >= max_bin_val) continue;
      ++bin_val_count[this_val];
      if (val_count != NULL) ++val_count[(src[r * stride + c])];
    }
  }
  int n = 0;
  // Count the colors based on 8-bit domain used to gate the palette path
  for (int i = 0; i < max_bin_val; ++i) {
    if (bin_val_count[i]) ++n;
  }
  *num_color_bins = n;

  // Count the actual hbd colors used to create top_colors
  n = 0;
  if (val_count != NULL) {
    for (int i = 0; i < max_pix_val; ++i) {
      if (val_count[i]) ++n;
    }
    *num_colors = n;
  }
}

static inline int count_bits_64(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_popcountll(x);
#else
  x = x - ((x >> 1) & 0x5555555555555555ULL);
  x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
  return (
      int)((((x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL) * 0x0101010101010101ULL) >>
           56);
#endif
}

bool av1_count_colors_with_threshold(const uint8_t *src, int stride, int rows,
                                     int cols, int num_colors_threshold,
                                     int *num_colors) {
  uint64_t seen0[4] = { 0 };
  uint64_t seen1[4] = { 0 };
  uint64_t seen2[4] = { 0 };
  uint64_t seen3[4] = { 0 };
  int max_possible = 0;
  const int cols4 = cols & ~3;
  uint64_t prev_w = ~(uint64_t)0;

  do {
    const uint8_t *p = src;
    const uint8_t *const end4 = src + cols4;
    while (p < end4) {
      uint32_t w;
      memcpy(&w, p, sizeof(w));
      if (w != prev_w) {
        prev_w = w;
        const uint32_t v0 = (uint8_t)w;
        const uint32_t v1 = (uint8_t)(w >> 8);
        const uint32_t v2 = (uint8_t)(w >> 16);
        const uint32_t v3 = w >> 24;
        seen0[v0 >> 6] |= 1ULL << (v0 & 63);
        seen1[v1 >> 6] |= 1ULL << (v1 & 63);
        seen2[v2 >> 6] |= 1ULL << (v2 & 63);
        seen3[v3 >> 6] |= 1ULL << (v3 & 63);
      }
      p += 4;
    }
    if (cols & 2) {
      const uint32_t v0 = p[0];
      const uint32_t v1 = p[1];
      seen2[v0 >> 6] |= 1ULL << (v0 & 63);
      seen3[v1 >> 6] |= 1ULL << (v1 & 63);
    }
    if (UNLIKELY(cols & 1)) {
      const uint32_t v0 = p[cols & 2];
      seen0[v0 >> 6] |= 1ULL << (v0 & 63);
    }
    src += stride;
    max_possible += cols;
    if (max_possible > num_colors_threshold || rows == 1) {
      const int count =
          count_bits_64((seen0[0] | seen1[0]) | (seen2[0] | seen3[0])) +
          count_bits_64((seen0[1] | seen1[1]) | (seen2[1] | seen3[1])) +
          count_bits_64((seen0[2] | seen1[2]) | (seen2[2] | seen3[2])) +
          count_bits_64((seen0[3] | seen1[3]) | (seen2[3] | seen3[3]));
      if (count > num_colors_threshold) {
        *num_colors = count;
        return false;
      }
      max_possible = count;
    }
  } while (--rows > 0);
  *num_colors = max_possible;
  return true;
}

#define HOG_BINS 32
static inline bool generate_hog_from_cache_fast(const MACROBLOCK *x, int rows,
                                                int cols, BLOCK_SIZE sb_size,
                                                int plane, float *hist) {
  assert(sizeof(PixelLevelGradientInfo) == 4 &&
         offsetof(PixelLevelGradientInfo, abs_dx_abs_dy_sum) == 0 &&
         offsetof(PixelLevelGradientInfo, hist_bin_idx) == 2);
  const int ss_x = x->e_mbd.plane[plane].subsampling_x;
  const int ss_y = x->e_mbd.plane[plane].subsampling_y;
  const int sb_width = block_size_wide[sb_size] >> ss_x;
  const int mi_row_in_sb = x->e_mbd.mi_row & (mi_size_high[sb_size] - 1);
  const int mi_col_in_sb = x->e_mbd.mi_col & (mi_size_wide[sb_size] - 1);
  const int block_offset_in_grad_cache =
      sb_width * (mi_row_in_sb << (MI_SIZE_LOG2 - ss_y)) +
      (mi_col_in_sb << (MI_SIZE_LOG2 - ss_x));
  const PixelLevelGradientInfo *const grad_info_blk =
      x->pixel_gradient_info + plane * MAX_SB_SQUARE +
      block_offset_in_grad_cache;
  const int n_cols = (cols - 2) & ~1;
  if (UNLIKELY(n_cols <= 0)) return false;

  DECLARE_ALIGNED(16, int32_t, int_hist[4][HOG_BINS + 4]);
  const int use_4banks = (n_cols >= 6 && rows >= 8);
  const int num_banks = use_4banks ? 4 : 2;
#if INTRA_MODE_SEARCH_SSE2
  const __m128i zero = _mm_setzero_si128();
  for (int i = 0; i < num_banks; ++i) {
    _mm_store_si128((__m128i *)(int_hist[i] + 0), zero);
    _mm_store_si128((__m128i *)(int_hist[i] + 4), zero);
    _mm_store_si128((__m128i *)(int_hist[i] + 8), zero);
    _mm_store_si128((__m128i *)(int_hist[i] + 12), zero);
    _mm_store_si128((__m128i *)(int_hist[i] + 16), zero);
    _mm_store_si128((__m128i *)(int_hist[i] + 20), zero);
    _mm_store_si128((__m128i *)(int_hist[i] + 24), zero);
    _mm_store_si128((__m128i *)(int_hist[i] + 28), zero);
    int_hist[i][HOG_BINS] = 0;
  }
#else
  memset(int_hist, 0, (size_t)num_banks * sizeof(int_hist[0]));
#endif

  if (use_4banks) {
    const int n_cols4 = n_cols & ~3;
    for (int r = 1; r < rows - 1; ++r) {
      const PixelLevelGradientInfo *p = grad_info_blk + r * sb_width + 1;
      const PixelLevelGradientInfo *const p_end4 = p + n_cols4;
      do {
        int_hist[0][(uint8_t)p[0].hist_bin_idx] += p[0].abs_dx_abs_dy_sum;
        int_hist[1][(uint8_t)p[1].hist_bin_idx] += p[1].abs_dx_abs_dy_sum;
        int_hist[2][(uint8_t)p[2].hist_bin_idx] += p[2].abs_dx_abs_dy_sum;
        int_hist[3][(uint8_t)p[3].hist_bin_idx] += p[3].abs_dx_abs_dy_sum;
        p += 4;
      } while (p != p_end4);
      if (n_cols & 2) {
        int_hist[2][(uint8_t)p[0].hist_bin_idx] += p[0].abs_dx_abs_dy_sum;
        int_hist[3][(uint8_t)p[1].hist_bin_idx] += p[1].abs_dx_abs_dy_sum;
      }
    }
  } else {
    for (int r = 1; r < rows - 1; ++r) {
      const PixelLevelGradientInfo *p = grad_info_blk + r * sb_width + 1;
      const PixelLevelGradientInfo *const p_end = p + n_cols;
      do {
        int_hist[0][(uint8_t)p[0].hist_bin_idx] += p[0].abs_dx_abs_dy_sum;
        int_hist[1][(uint8_t)p[1].hist_bin_idx] += p[1].abs_dx_abs_dy_sum;
        p += 2;
      } while (p != p_end);
    }
  }

#if INTRA_MODE_SEARCH_SSE2
  __m128i v0 =
      _mm_add_epi32(_mm_load_si128((const __m128i *)(int_hist[0] + 0)),
                    _mm_load_si128((const __m128i *)(int_hist[1] + 0)));
  __m128i v1 =
      _mm_add_epi32(_mm_load_si128((const __m128i *)(int_hist[0] + 4)),
                    _mm_load_si128((const __m128i *)(int_hist[1] + 4)));
  __m128i v2 =
      _mm_add_epi32(_mm_load_si128((const __m128i *)(int_hist[0] + 8)),
                    _mm_load_si128((const __m128i *)(int_hist[1] + 8)));
  __m128i v3 =
      _mm_add_epi32(_mm_load_si128((const __m128i *)(int_hist[0] + 12)),
                    _mm_load_si128((const __m128i *)(int_hist[1] + 12)));
  __m128i v4 =
      _mm_add_epi32(_mm_load_si128((const __m128i *)(int_hist[0] + 16)),
                    _mm_load_si128((const __m128i *)(int_hist[1] + 16)));
  __m128i v5 =
      _mm_add_epi32(_mm_load_si128((const __m128i *)(int_hist[0] + 20)),
                    _mm_load_si128((const __m128i *)(int_hist[1] + 20)));
  __m128i v6 =
      _mm_add_epi32(_mm_load_si128((const __m128i *)(int_hist[0] + 24)),
                    _mm_load_si128((const __m128i *)(int_hist[1] + 24)));
  __m128i v7 =
      _mm_add_epi32(_mm_load_si128((const __m128i *)(int_hist[0] + 28)),
                    _mm_load_si128((const __m128i *)(int_hist[1] + 28)));
  int32_t dx_zero = int_hist[0][HOG_BINS] + int_hist[1][HOG_BINS];
  if (use_4banks) {
    v0 = _mm_add_epi32(
        v0, _mm_add_epi32(_mm_load_si128((const __m128i *)(int_hist[2] + 0)),
                          _mm_load_si128((const __m128i *)(int_hist[3] + 0))));
    v1 = _mm_add_epi32(
        v1, _mm_add_epi32(_mm_load_si128((const __m128i *)(int_hist[2] + 4)),
                          _mm_load_si128((const __m128i *)(int_hist[3] + 4))));
    v2 = _mm_add_epi32(
        v2, _mm_add_epi32(_mm_load_si128((const __m128i *)(int_hist[2] + 8)),
                          _mm_load_si128((const __m128i *)(int_hist[3] + 8))));
    v3 = _mm_add_epi32(
        v3, _mm_add_epi32(_mm_load_si128((const __m128i *)(int_hist[2] + 12)),
                          _mm_load_si128((const __m128i *)(int_hist[3] + 12))));
    v4 = _mm_add_epi32(
        v4, _mm_add_epi32(_mm_load_si128((const __m128i *)(int_hist[2] + 16)),
                          _mm_load_si128((const __m128i *)(int_hist[3] + 16))));
    v5 = _mm_add_epi32(
        v5, _mm_add_epi32(_mm_load_si128((const __m128i *)(int_hist[2] + 20)),
                          _mm_load_si128((const __m128i *)(int_hist[3] + 20))));
    v6 = _mm_add_epi32(
        v6, _mm_add_epi32(_mm_load_si128((const __m128i *)(int_hist[2] + 24)),
                          _mm_load_si128((const __m128i *)(int_hist[3] + 24))));
    v7 = _mm_add_epi32(
        v7, _mm_add_epi32(_mm_load_si128((const __m128i *)(int_hist[2] + 28)),
                          _mm_load_si128((const __m128i *)(int_hist[3] + 28))));
    dx_zero += int_hist[2][HOG_BINS] + int_hist[3][HOG_BINS];
  }
  const __m128i s01 = _mm_add_epi32(v0, v1);
  const __m128i s23 = _mm_add_epi32(v2, v3);
  const __m128i s45 = _mm_add_epi32(v4, v5);
  const __m128i s67 = _mm_add_epi32(v6, v7);
  __m128i sum = _mm_add_epi32(_mm_add_epi32(s01, s23), _mm_add_epi32(s45, s67));
  sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 8));
  sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 4));
  const int32_t int_total = _mm_cvtsi128_si32(sum) + dx_zero;
  if (int_total == 0) return false;
  const __m128i v_half = _mm_cvtsi32_si128(dx_zero >> 1);
  v0 = _mm_add_epi32(v0, v_half);
  v7 = _mm_add_epi32(v7, _mm_slli_si128(v_half, 12));
  const __m128 v_total = _mm_set1_ps(0.1f + (float)int_total);
  _mm_storeu_ps(hist + 0, _mm_div_ps(_mm_cvtepi32_ps(v0), v_total));
  _mm_storeu_ps(hist + 4, _mm_div_ps(_mm_cvtepi32_ps(v1), v_total));
  _mm_storeu_ps(hist + 8, _mm_div_ps(_mm_cvtepi32_ps(v2), v_total));
  _mm_storeu_ps(hist + 12, _mm_div_ps(_mm_cvtepi32_ps(v3), v_total));
  _mm_storeu_ps(hist + 16, _mm_div_ps(_mm_cvtepi32_ps(v4), v_total));
  _mm_storeu_ps(hist + 20, _mm_div_ps(_mm_cvtepi32_ps(v5), v_total));
  _mm_storeu_ps(hist + 24, _mm_div_ps(_mm_cvtepi32_ps(v6), v_total));
  _mm_storeu_ps(hist + 28, _mm_div_ps(_mm_cvtepi32_ps(v7), v_total));
#else
  int32_t dx_zero = int_hist[0][HOG_BINS] + int_hist[1][HOG_BINS];
  if (use_4banks) dx_zero += int_hist[2][HOG_BINS] + int_hist[3][HOG_BINS];
  int32_t int_total = dx_zero;
  for (int b = 0; b < HOG_BINS; ++b) {
    int32_t s = int_hist[0][b] + int_hist[1][b];
    if (use_4banks) s += int_hist[2][b] + int_hist[3][b];
    int_hist[0][b] = s;
    int_total += s;
  }
  if (int_total == 0) return false;
  const float total = 0.1f + (float)int_total;
  const int32_t dx_zero_half = dx_zero >> 1;
  int_hist[0][0] += dx_zero_half;
  int_hist[0][HOG_BINS - 1] += dx_zero_half;
  for (int b = 0; b < HOG_BINS; ++b) {
    hist[b] = (float)int_hist[0][b] / total;
  }
#endif
  return true;
}

static inline uint32_t hog_scores_to_skip_mask(const float *scores, float th) {
#if INTRA_MODE_SEARCH_SSE2
  const __m128 v_th = _mm_set1_ps(th);
  const uint32_t m_lo =
      (uint32_t)_mm_movemask_ps(_mm_cmple_ps(_mm_loadu_ps(scores + 0), v_th));
  const uint32_t m_hi =
      (uint32_t)_mm_movemask_ps(_mm_cmple_ps(_mm_loadu_ps(scores + 4), v_th));
  return m_lo | (m_hi << 4);
#else
  uint32_t mask = 0;
  for (int i = 0; i < DIRECTIONAL_MODES; ++i) {
    if (scores[i] <= th) mask |= 1u << i;
  }
  return mask;
#endif
}

// Returns an 8-bit mask (bit 0 = V_PRED/UV_V_PRED .. bit 7 = D67_PRED/UV_D67)
// indicating which directional modes should be pruned by HoG.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static uint32_t prune_intra_mode_with_hog_fast(const MACROBLOCK *x,
                                               BLOCK_SIZE bsize,
                                               BLOCK_SIZE sb_size, float th,
                                               int is_chroma) {
  static const float kZeroHogScores[DIRECTIONAL_MODES] = {
    231.0f / 512.0f,  356.0f / 512.0f,  -367.0f / 512.0f, -327.0f / 512.0f,
    -307.0f / 512.0f, -231.0f / 512.0f, 29.0f / 512.0f,   -237.0f / 512.0f,
  };
  const int plane = is_chroma ? AOM_PLANE_U : AOM_PLANE_Y;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const int ss_x = pd->subsampling_x;
  const int ss_y = pd->subsampling_y;
  const int bh = block_size_high[bsize];
  const int bw = block_size_wide[bsize];
  const int rows =
      ((xd->mb_to_bottom_edge >= 0) ? bh : (xd->mb_to_bottom_edge >> 3) + bh) >>
      ss_y;
  const int cols =
      ((xd->mb_to_right_edge >= 0) ? bw : (xd->mb_to_right_edge >> 3) + bw) >>
      ss_x;
  if (rows <= 2 || cols <= 2) {
    return hog_scores_to_skip_mask(kZeroHogScores, th);
  }

  DECLARE_ALIGNED(16, float, hist[HOG_BINS]);
  if (LIKELY(x->is_sb_gradient_cached[plane])) {
    if (!generate_hog_from_cache_fast(x, rows, cols, sb_size, plane, hist)) {
      return hog_scores_to_skip_mask(kZeroHogScores, th);
    }
  } else {
    memset(hist, 0, sizeof(hist));
    generate_hog(x->plane[plane].src.buf, x->plane[plane].src.stride, rows,
                 cols, hist, is_cur_buf_hbd(xd));
  }

  if (plane != AOM_PLANE_Y && (ss_x | ss_y)) {
    const float scale = (float)((1 + ss_x) * (1 + ss_y));
#if INTRA_MODE_SEARCH_SSE2
    const __m128 v_scale = _mm_set1_ps(scale);
    for (int b = 0; b < HOG_BINS; b += 4) {
      _mm_store_ps(hist + b, _mm_mul_ps(_mm_load_ps(hist + b), v_scale));
    }
#else
    for (int b = 0; b < HOG_BINS; ++b) {
      hist[b] *= scale;
    }
#endif
  }

  DECLARE_ALIGNED(16, float, scores[DIRECTIONAL_MODES]);
  av1_nn_predict(hist, &av1_intra_hog_model_nnconfig, 1, scores);
  return hog_scores_to_skip_mask(scores, th);
}
#undef HOG_BINS

static inline int get_model_rd_index_for_pruning(
    const MACROBLOCK *const x,
    const INTRA_MODE_SPEED_FEATURES *const intra_sf) {
  const int top_intra_model_count_allowed =
      intra_sf->top_intra_model_count_allowed;
  if (!intra_sf->adapt_top_model_rd_count_using_neighbors)
    return top_intra_model_count_allowed - 1;

  const MACROBLOCKD *const xd = &x->e_mbd;
  const PREDICTION_MODE mode = xd->mi[0]->mode;
  int model_rd_index_for_pruning = top_intra_model_count_allowed - 1;
  int is_left_mode_neq_cur_mode = 0, is_above_mode_neq_cur_mode = 0;
  if (xd->left_available)
    is_left_mode_neq_cur_mode = xd->left_mbmi->mode != mode;
  if (xd->up_available)
    is_above_mode_neq_cur_mode = xd->above_mbmi->mode != mode;
  // The pruning of luma intra modes is made more aggressive at lower quantizers
  // and vice versa. The value for model_rd_index_for_pruning is derived as
  // follows.
  // qidx 0 to 127: Reduce the index of a candidate used for comparison only if
  // the current mode does not match either of the available neighboring modes.
  // qidx 128 to 255: Reduce the index of a candidate used for comparison only
  // if the current mode does not match both the available neighboring modes.
  if (x->qindex <= 127) {
    if (is_left_mode_neq_cur_mode || is_above_mode_neq_cur_mode)
      model_rd_index_for_pruning = AOMMAX(model_rd_index_for_pruning - 1, 0);
  } else {
    if (is_left_mode_neq_cur_mode && is_above_mode_neq_cur_mode)
      model_rd_index_for_pruning = AOMMAX(model_rd_index_for_pruning - 1, 0);
  }
  return model_rd_index_for_pruning;
}

/*! \brief prune luma intra mode based on the model rd.
 * \param[in]    this_model_rd              model rd for current mode.
 * \param[in]    best_model_rd              Best model RD seen for this block so
 *                                          far.
 * \param[in]    top_intra_model_rd         Top intra model RD seen for this
 *                                          block so far.
 * \param[in]    max_model_cnt_allowed      The maximum number of top intra
 *                                          model RD allowed.
 * \param[in]    model_rd_index_for_pruning Index of the candidate used for
 *                                          pruning based on model rd.
 */
static inline int prune_intra_y_mode(int64_t this_model_rd,
                                     int64_t *best_model_rd,
                                     int64_t top_intra_model_rd[],
                                     int max_model_cnt_allowed,
                                     int model_rd_index_for_pruning) {
  assert(max_model_cnt_allowed > 0 &&
         max_model_cnt_allowed <= TOP_INTRA_MODEL_COUNT);
  if (this_model_rd < top_intra_model_rd[max_model_cnt_allowed - 1]) {
    for (int i = 0; i < max_model_cnt_allowed; i++) {
      if (this_model_rd < top_intra_model_rd[i]) {
        const int64_t r0 = top_intra_model_rd[0];
        const int64_t r1 = top_intra_model_rd[1];
        const int64_t r2 = top_intra_model_rd[2];
        if (i <= 2 && max_model_cnt_allowed >= 4) top_intra_model_rd[3] = r2;
        if (i <= 1 && max_model_cnt_allowed >= 3) top_intra_model_rd[2] = r1;
        if (i == 0 && max_model_cnt_allowed >= 2) top_intra_model_rd[1] = r0;
        top_intra_model_rd[i] = this_model_rd;
        break;
      }
    }
  }
  if (this_model_rd > top_intra_model_rd[model_rd_index_for_pruning]) return 1;

  if (this_model_rd != INT64_MAX && *best_model_rd != INT64_MAX &&
      this_model_rd > *best_model_rd + (*best_model_rd >> 1))
    return 1;
  if (this_model_rd < *best_model_rd) *best_model_rd = this_model_rd;
  return 0;
}

// Run RD calculation with given chroma intra prediction angle., and return
// the RD cost. Update the best mode info. if the RD cost is the best so far.
static int64_t pick_intra_angle_routine_sbuv(
    const AV1_COMP *const cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
    int rate_overhead, const int *angle_cost_row, int64_t best_rd_in, int *rate,
    RD_STATS *rd_stats, int *best_angle_delta, int64_t *best_rd) {
  MB_MODE_INFO *mbmi = x->e_mbd.mi[0];
  assert(!is_inter_block(mbmi));
  const int cur_angle_delta = mbmi->angle_delta[PLANE_TYPE_UV];
  RD_STATS tokenonly_rd_stats;

  if (!av1_txfm_uvrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd_in))
    return INT64_MAX;
  const int this_rate = tokenonly_rd_stats.rate + rate_overhead +
                        angle_cost_row[cur_angle_delta + MAX_ANGLE_DELTA];
  const int64_t this_rd = RDCOST(x->rdmult, this_rate, tokenonly_rd_stats.dist);
  if (this_rd < *best_rd) {
    *best_rd = this_rd;
    *best_angle_delta = cur_angle_delta;
    *rate = this_rate;
    rd_stats->rate = tokenonly_rd_stats.rate;
    rd_stats->dist = tokenonly_rd_stats.dist;
    rd_stats->skip_txfm = tokenonly_rd_stats.skip_txfm;
  }
  return this_rd;
}

/*!\brief Search for the best angle delta for chroma prediction
 *
 * \ingroup intra_mode_search
 * \callergraph
 * Given a chroma directional intra prediction mode, this function will try to
 * estimate the best delta_angle.
 *
 * \returns Return if there is a new mode with smaller rdcost than best_rd.
 */
static int rd_pick_intra_angle_sbuv(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    BLOCK_SIZE bsize, int rate_overhead,
                                    int64_t best_rd, int *rate,
                                    RD_STATS *rd_stats) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  const PREDICTION_MODE intra_mode = get_uv_mode(mbmi->uv_mode);
  assert(av1_is_directional_mode(intra_mode) && av1_use_angle_delta(bsize));
  const int *const angle_cost_row =
      x->mode_costs.angle_delta_cost[intra_mode - V_PRED];
  int i, angle_delta, best_angle_delta = 0;
  int64_t this_rd, best_rd_in, rd_cost[2 * (MAX_ANGLE_DELTA + 2)];

  rd_stats->rate = INT_MAX;
  rd_stats->skip_txfm = 0;
  rd_stats->dist = INT64_MAX;
  for (i = 0; i < 2 * (MAX_ANGLE_DELTA + 2); ++i) rd_cost[i] = INT64_MAX;

  for (angle_delta = 0; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    for (i = 0; i < 2; ++i) {
      best_rd_in = (best_rd == INT64_MAX)
                       ? INT64_MAX
                       : (best_rd + (best_rd >> ((angle_delta == 0) ? 3 : 5)));
      mbmi->angle_delta[PLANE_TYPE_UV] = (1 - 2 * i) * angle_delta;
      this_rd = pick_intra_angle_routine_sbuv(
          cpi, x, bsize, rate_overhead, angle_cost_row, best_rd_in, rate,
          rd_stats, &best_angle_delta, &best_rd);
      rd_cost[2 * angle_delta + i] = this_rd;
      if (angle_delta == 0) {
        if (this_rd == INT64_MAX) return 0;
        rd_cost[1] = this_rd;
        break;
      }
    }
  }

  assert(best_rd != INT64_MAX);
  for (angle_delta = 1; angle_delta <= MAX_ANGLE_DELTA; angle_delta += 2) {
    int64_t rd_thresh;
    for (i = 0; i < 2; ++i) {
      int skip_search = 0;
      rd_thresh = best_rd + (best_rd >> 5);
      if (rd_cost[2 * (angle_delta + 1) + i] > rd_thresh &&
          rd_cost[2 * (angle_delta - 1) + i] > rd_thresh)
        skip_search = 1;
      if (!skip_search) {
        mbmi->angle_delta[PLANE_TYPE_UV] = (1 - 2 * i) * angle_delta;
        pick_intra_angle_routine_sbuv(cpi, x, bsize, rate_overhead,
                                      angle_cost_row, best_rd, rate, rd_stats,
                                      &best_angle_delta, &best_rd);
      }
    }
  }

  mbmi->angle_delta[PLANE_TYPE_UV] = best_angle_delta;
  return rd_stats->rate != INT_MAX;
}

#define PLANE_SIGN_TO_JOINT_SIGN(plane, a, b) \
  (plane == CFL_PRED_U ? a * CFL_SIGNS + b - 1 : b * CFL_SIGNS + a - 1)

static void cfl_idx_to_sign_and_alpha(int cfl_idx, CFL_SIGN_TYPE *cfl_sign,
                                      int *cfl_alpha) {
  int cfl_linear_idx = cfl_idx - CFL_INDEX_ZERO;
  if (cfl_linear_idx == 0) {
    *cfl_sign = CFL_SIGN_ZERO;
    *cfl_alpha = 0;
  } else {
    *cfl_sign = cfl_linear_idx > 0 ? CFL_SIGN_POS : CFL_SIGN_NEG;
    *cfl_alpha = abs(cfl_linear_idx) - 1;
  }
}

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((noinline, noclone))
#elif defined(__clang__)
__attribute__((noinline))
#endif
static int64_t cfl_compute_rd_fast(const AV1_COMP *const cpi, MACROBLOCK *x,
                                   int plane, TX_SIZE tx_size,
                                   BLOCK_SIZE plane_bsize, int cfl_idx) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  int cfl_plane = get_cfl_pred_type(plane);
  CFL_SIGN_TYPE cfl_sign;
  int cfl_alpha;
  cfl_idx_to_sign_and_alpha(cfl_idx, &cfl_sign, &cfl_alpha);
  // We conly build CFL for a given plane, the other plane's sign is dummy
  int dummy_sign = CFL_SIGN_NEG;
  const int8_t orig_cfl_alpha_signs = mbmi->cfl_alpha_signs;
  const uint8_t orig_cfl_alpha_idx = mbmi->cfl_alpha_idx;
  mbmi->cfl_alpha_signs =
      PLANE_SIGN_TO_JOINT_SIGN(cfl_plane, cfl_sign, dummy_sign);
  mbmi->cfl_alpha_idx = (cfl_alpha << CFL_ALPHABET_SIZE_LOG2) + cfl_alpha;
  const int64_t cfl_cost =
      intra_model_rd(cm, x, plane, plane_bsize, tx_size, /*use_hadamard=*/0,
                     cpi->do_border_pad);
  mbmi->cfl_alpha_signs = orig_cfl_alpha_signs;
  mbmi->cfl_alpha_idx = orig_cfl_alpha_idx;
  return cfl_cost;
}

static const int cfl_dir_ls[2] = { 1, -1 };

#if (!CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER) && HAVE_SSSE3 && \
    (defined(__SSE2__) || defined(_M_X64) ||                       \
     (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((noinline, noclone))
#elif defined(__clang__)
__attribute__((noinline))
#endif
static int cfl_pick_plane_parameter_lbd(const AV1_COMP *const cpi,
                                        MACROBLOCK *x, int plane,
                                        cfl_predict_lbd_fn predict_lbd,
                                        TxfmParam *txfm_param) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const TX_SIZE tx_size = txfm_param->tx_size;
  struct macroblock_plane *const p = &x->plane[plane];
  uint8_t *const dst = xd->plane[plane].dst.buf;
  const int dst_stride = xd->plane[plane].dst.stride;
  const uint8_t *const src = p->src.buf;
  const int src_stride = p->src.stride;
  int16_t *const src_diff = p->src_diff;
  tran_low_t *const coeff = p->coeff;
  const int16_t *const ac_buf_q3 = xd->cfl.ac_buf_q3;
  const int txbw = tx_size_wide[tx_size];
  const int txbh = tx_size_high[tx_size];
  const int n_coeffs = tx_size_2d[tx_size];

  const int8_t orig_cfl_alpha_signs = mbmi->cfl_alpha_signs;
  const uint8_t orig_cfl_alpha_idx = mbmi->cfl_alpha_idx;
  mbmi->cfl_alpha_signs = (plane == AOM_PLANE_U) ? 0 : 2;
  mbmi->cfl_alpha_idx = 0;
  av1_predict_intra_block_facade(&cpi->common, xd, plane, 0, 0, tx_size);
  mbmi->cfl_alpha_signs = orig_cfl_alpha_signs;
  mbmi->cfl_alpha_idx = orig_cfl_alpha_idx;

  aom_subtract_block(txbh, txbw, src_diff, txbw, src, src_stride, dst,
                     dst_stride);
  av1_lowbd_fwd_txfm(src_diff, coeff, txbw, txfm_param);
  int64_t best_cfl_cost = aom_satd(coeff, n_coeffs);
  int est_best_cfl_idx = CFL_INDEX_ZERO;
  const uint8_t dc_val = ((const uint8_t *)xd->cfl.dc_pred_cache[plane - 1])[0];

  for (int si = 0; si < 2; ++si) {
    const int dir = si ? -1 : 1;
    for (int i = 1; i <= CFL_INDEX_ZERO; ++i) {
      const int alpha_q3 = dir * i;
      dst[0] = dc_val;
      predict_lbd(ac_buf_q3, dst, dst_stride, alpha_q3);
      aom_subtract_block(txbh, txbw, src_diff, txbw, src, src_stride, dst,
                         dst_stride);
      av1_lowbd_fwd_txfm(src_diff, coeff, txbw, txfm_param);
      const int64_t cfl_cost = aom_satd(coeff, n_coeffs);
      if (cfl_cost < best_cfl_cost) {
        best_cfl_cost = cfl_cost;
        est_best_cfl_idx = CFL_INDEX_ZERO + alpha_q3;
      } else {
        break;
      }
    }
  }
  return est_best_cfl_idx;
}
#endif

// If cfl_search_range is CFL_MAGS_SIZE, return zero. Otherwise return the index
// of the best alpha found using intra_model_rd().
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((noinline, noclone))
#elif defined(__clang__)
__attribute__((noinline))
#endif
static int cfl_pick_plane_parameter(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    int plane, TX_SIZE tx_size,
                                    BLOCK_SIZE plane_bsize,
                                    int cfl_search_range) {
  assert(cfl_search_range >= 1 && cfl_search_range <= CFL_MAGS_SIZE);
  assert(x->e_mbd.mi[0]->uv_mode == UV_CFL_PRED);

  if (cfl_search_range == CFL_MAGS_SIZE) return CFL_INDEX_ZERO;

  int est_best_cfl_idx = CFL_INDEX_ZERO;
  int start_cfl_idx = CFL_INDEX_ZERO;
  int64_t best_cfl_cost =
      cfl_compute_rd_fast(cpi, x, plane, tx_size, plane_bsize, start_cfl_idx);
  for (int si = 0; si < 2; ++si) {
    const int dir = cfl_dir_ls[si];
    for (int i = 1; i < CFL_MAGS_SIZE; ++i) {
      int cfl_idx = start_cfl_idx + dir * i;
      if (cfl_idx < 0 || cfl_idx >= CFL_MAGS_SIZE) break;
      int64_t cfl_cost =
          cfl_compute_rd_fast(cpi, x, plane, tx_size, plane_bsize, cfl_idx);
      if (cfl_cost < best_cfl_cost) {
        best_cfl_cost = cfl_cost;
        est_best_cfl_idx = cfl_idx;
      } else {
        break;
      }
    }
  }
  return est_best_cfl_idx;
}

static inline void set_invalid_cfl_parameters(uint8_t *best_cfl_alpha_idx,
                                              int8_t *best_cfl_alpha_signs) {
  *best_cfl_alpha_idx = 0;
  *best_cfl_alpha_signs = 0;
}

static void cfl_eval_plane_rd(const AV1_COMP *const cpi, MACROBLOCK *x,
                              int plane, TX_SIZE tx_size,
                              BLOCK_SIZE plane_bsize, int cfl_idx,
                              RD_STATS *rd_stats) {
  MB_MODE_INFO *const mbmi = x->e_mbd.mi[0];
  const int cfl_plane = get_cfl_pred_type(plane);
  CFL_SIGN_TYPE cfl_sign;
  int cfl_alpha;
  cfl_idx_to_sign_and_alpha(cfl_idx, &cfl_sign, &cfl_alpha);
  const int8_t orig_cfl_alpha_signs = mbmi->cfl_alpha_signs;
  const uint8_t orig_cfl_alpha_idx = mbmi->cfl_alpha_idx;
  mbmi->cfl_alpha_signs =
      PLANE_SIGN_TO_JOINT_SIGN(cfl_plane, cfl_sign, CFL_SIGN_NEG);
  mbmi->cfl_alpha_idx = (cfl_alpha << CFL_ALPHABET_SIZE_LOG2) + cfl_alpha;
  av1_init_rd_stats(rd_stats);
  av1_txfm_rd_in_plane(x, cpi, rd_stats, INT64_MAX, 0, plane, plane_bsize,
                       tx_size, FTXS_NONE);
  mbmi->cfl_alpha_signs = orig_cfl_alpha_signs;
  mbmi->cfl_alpha_idx = orig_cfl_alpha_idx;
}

static void cfl_pick_plane_rd(const AV1_COMP *const cpi, MACROBLOCK *x,
                              int plane, TX_SIZE tx_size, int cfl_search_range,
                              RD_STATS cfl_rd_arr[CFL_MAGS_SIZE],
                              int est_best_cfl_idx, int *idx_lo, int *idx_hi) {
  assert(cfl_search_range >= 1 && cfl_search_range <= CFL_MAGS_SIZE);
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(mbmi->uv_mode == UV_CFL_PRED);
  const MACROBLOCKD_PLANE *pd = &xd->plane[plane];
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(mbmi->bsize, pd->subsampling_x, pd->subsampling_y);

  const int start_cfl_idx = est_best_cfl_idx;
  *idx_lo = AOMMAX(0, start_cfl_idx - (cfl_search_range - 1));
  *idx_hi = AOMMIN(CFL_MAGS_SIZE - 1, start_cfl_idx + (cfl_search_range - 1));
  cfl_eval_plane_rd(cpi, x, plane, tx_size, plane_bsize, start_cfl_idx,
                    &cfl_rd_arr[start_cfl_idx]);

  if (cfl_search_range == 1) return;

  for (int si = 0; si < 2; ++si) {
    const int dir = cfl_dir_ls[si];
    for (int i = 1; i < cfl_search_range; ++i) {
      int cfl_idx = start_cfl_idx + dir * i;
      if (cfl_idx < 0 || cfl_idx >= CFL_MAGS_SIZE) break;
      cfl_eval_plane_rd(cpi, x, plane, tx_size, plane_bsize, cfl_idx,
                        &cfl_rd_arr[cfl_idx]);
    }
  }
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int cfl_rd_pick_alpha_multi(
    MACROBLOCK *const x, const AV1_COMP *const cpi, TX_SIZE tx_size,
    int64_t ref_best_rd, int cfl_search_range, int est_best_cfl_idx_u,
    int est_best_cfl_idx_v, RD_STATS *best_rd_stats,
    uint8_t *best_cfl_alpha_idx, int8_t *best_cfl_alpha_signs) {
  const ModeCosts *mode_costs = &x->mode_costs;
  RD_STATS cfl_rd_arr_u[CFL_MAGS_SIZE];
  RD_STATS cfl_rd_arr_v[CFL_MAGS_SIZE];
  MACROBLOCKD *const xd = &x->e_mbd;
  int u_lo, u_hi, v_lo, v_hi;
  cfl_pick_plane_rd(cpi, x, 1, tx_size, cfl_search_range, cfl_rd_arr_u,
                    est_best_cfl_idx_u, &u_lo, &u_hi);
  cfl_pick_plane_rd(cpi, x, 2, tx_size, cfl_search_range, cfl_rd_arr_v,
                    est_best_cfl_idx_v, &v_lo, &v_hi);

  clear_cfl_dc_pred_cache_flags(&xd->cfl);

  for (int ui = u_lo; ui <= u_hi; ++ui) {
    if (cfl_rd_arr_u[ui].rate == INT_MAX) continue;
    int cfl_alpha_u;
    CFL_SIGN_TYPE cfl_sign_u;
    cfl_idx_to_sign_and_alpha(ui, &cfl_sign_u, &cfl_alpha_u);
    for (int vi = v_lo; vi <= v_hi; ++vi) {
      if (cfl_rd_arr_v[vi].rate == INT_MAX) continue;
      int cfl_alpha_v;
      CFL_SIGN_TYPE cfl_sign_v;
      cfl_idx_to_sign_and_alpha(vi, &cfl_sign_v, &cfl_alpha_v);
      // cfl_sign_u == CFL_SIGN_ZERO && cfl_sign_v == CFL_SIGN_ZERO is not a
      // valid parameter for CFL
      if (cfl_sign_u == CFL_SIGN_ZERO && cfl_sign_v == CFL_SIGN_ZERO) continue;
      int joint_sign = cfl_sign_u * CFL_SIGNS + cfl_sign_v - 1;
      RD_STATS rd_stats = cfl_rd_arr_u[ui];
      av1_merge_rd_stats(&rd_stats, &cfl_rd_arr_v[vi]);
      if (rd_stats.rate != INT_MAX) {
        rd_stats.rate +=
            mode_costs->cfl_cost[joint_sign][CFL_PRED_U][cfl_alpha_u];
        rd_stats.rate +=
            mode_costs->cfl_cost[joint_sign][CFL_PRED_V][cfl_alpha_v];
      }
      av1_rd_cost_update(x->rdmult, &rd_stats);
      if (rd_stats.rdcost < best_rd_stats->rdcost) {
        *best_rd_stats = rd_stats;
        *best_cfl_alpha_idx =
            (cfl_alpha_u << CFL_ALPHABET_SIZE_LOG2) + cfl_alpha_v;
        *best_cfl_alpha_signs = joint_sign;
      }
    }
  }
  if (best_rd_stats->rdcost >= ref_best_rd) {
    av1_invalid_rd_stats(best_rd_stats);
    set_invalid_cfl_parameters(best_cfl_alpha_idx, best_cfl_alpha_signs);
    return 0;
  }
  return 1;
}

/*!\brief Pick the optimal parameters for Chroma to Luma (CFL) component
 *
 * \ingroup intra_mode_search
 * \callergraph
 *
 * This function will use DCT_DCT followed by computing SATD (sum of absolute
 * transformed differences) to estimate the RD score and find the best possible
 * CFL parameter.
 *
 * Then the function will apply a full RD search near the best possible CFL
 * parameter to find the best actual CFL parameter.
 *
 * Side effect:
 * We use ths buffers in x->plane[] and xd->plane[] as throw-away buffers for RD
 * search.
 *
 * \param[in] x                Encoder prediction block structure.
 * \param[in] cpi              Top-level encoder instance structure.
 * \param[in] tx_size          Transform size.
 * \param[in] ref_best_rd      Reference best RD.
 * \param[in] cfl_search_range The search range of full RD search near the
 *                             estimated best CFL parameter.
 *
 * \param[out]   best_rd_stats          RD stats of the best CFL parameter
 * \param[out]   best_cfl_alpha_idx     Best CFL alpha index
 * \param[out]   best_cfl_alpha_signs   Best CFL joint signs
 *
 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int cfl_rd_pick_alpha(MACROBLOCK *const x, const AV1_COMP *const cpi,
                             TX_SIZE tx_size, int64_t ref_best_rd,
                             int cfl_search_range, RD_STATS *best_rd_stats,
                             uint8_t *best_cfl_alpha_idx,
                             int8_t *best_cfl_alpha_signs) {
  assert(cfl_search_range >= 1 && cfl_search_range <= CFL_MAGS_SIZE);
  const ModeCosts *mode_costs = &x->mode_costs;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const BLOCK_SIZE plane_bsize = get_plane_block_size(
      mbmi->bsize, xd->plane[1].subsampling_x, xd->plane[1].subsampling_y);
  int est_best_cfl_idx_u, est_best_cfl_idx_v;

  av1_invalid_rd_stats(best_rd_stats);

  // As the dc pred data is same for different values of alpha, enable the
  // caching of dc pred data. Call clear_cfl_dc_pred_cache_flags() before
  // returning to avoid the unintentional usage of cached dc pred data.
  xd->cfl.use_dc_pred_cache = true;
  // Evaluate alpha parameter of each chroma plane.
#if (!CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER) && HAVE_SSSE3 && \
    (defined(__SSE2__) || defined(_M_X64) ||                       \
     (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
  cfl_predict_lbd_fn predict_lbd = NULL;
  if (LIKELY(cfl_search_range < CFL_MAGS_SIZE && !is_cur_buf_hbd(xd) &&
             !mbmi->palette_mode_info.palette_size[1] &&
             (!cpi->do_border_pad ||
              (x->pix_to_bottom_edge >= 0 && x->pix_to_right_edge >= 0) ||
              (max_block_high(xd, plane_bsize, AOM_PLANE_U) <=
                   tx_size_high_unit[tx_size] &&
               max_block_wide(xd, plane_bsize, AOM_PLANE_U) <=
                   tx_size_wide_unit[tx_size])))) {
    predict_lbd = av1_get_cfl_predict_lbd_fn(tx_size);
  }
  if (LIKELY(predict_lbd != NULL)) {
    TxfmParam txfm_param;
    txfm_param.tx_type = DCT_DCT;
    txfm_param.tx_size = tx_size;
    txfm_param.lossless = 0;
    txfm_param.bd = 8;
    txfm_param.is_hbd = 0;
    txfm_param.tx_set_type = EXT_TX_SET_ALL16;
    est_best_cfl_idx_u =
        cfl_pick_plane_parameter_lbd(cpi, x, 1, predict_lbd, &txfm_param);
    est_best_cfl_idx_v =
        cfl_pick_plane_parameter_lbd(cpi, x, 2, predict_lbd, &txfm_param);
  } else
#endif
  {
    est_best_cfl_idx_u = cfl_pick_plane_parameter(
        cpi, x, 1, tx_size, plane_bsize, cfl_search_range);
    est_best_cfl_idx_v = cfl_pick_plane_parameter(
        cpi, x, 2, tx_size, plane_bsize, cfl_search_range);
  }

  if (LIKELY(cfl_search_range == 1)) {
    // For cfl_search_range=1, further refinement of alpha is not enabled. Hence
    // CfL index=0 for both the chroma planes implies invalid CfL mode.
    if (est_best_cfl_idx_u == CFL_INDEX_ZERO &&
        est_best_cfl_idx_v == CFL_INDEX_ZERO) {
      set_invalid_cfl_parameters(best_cfl_alpha_idx, best_cfl_alpha_signs);
      clear_cfl_dc_pred_cache_flags(&xd->cfl);
      return 0;
    }

    int cfl_alpha_u, cfl_alpha_v;
    CFL_SIGN_TYPE cfl_sign_u, cfl_sign_v;
    cfl_idx_to_sign_and_alpha(est_best_cfl_idx_u, &cfl_sign_u, &cfl_alpha_u);
    cfl_idx_to_sign_and_alpha(est_best_cfl_idx_v, &cfl_sign_v, &cfl_alpha_v);
    const int joint_sign = cfl_sign_u * CFL_SIGNS + cfl_sign_v - 1;
    // Compute alpha and mode signaling rate.
    const int cfl_rate =
        mode_costs->cfl_cost[joint_sign][CFL_PRED_U][cfl_alpha_u] +
        mode_costs->cfl_cost[joint_sign][CFL_PRED_V][cfl_alpha_v];
    const int rate_overhead =
        cfl_rate +
        mode_costs
            ->intra_uv_mode_cost[is_cfl_allowed(xd)][mbmi->mode][UV_CFL_PRED];
    const int64_t rdmult = x->rdmult;
    // Skip the CfL mode evaluation if the RD cost derived using the rate needed
    // to signal the CfL mode and alpha parameter exceeds the ref_best_rd.
    if (RDCOST(rdmult, rate_overhead, 0) > ref_best_rd) {
      set_invalid_cfl_parameters(best_cfl_alpha_idx, best_cfl_alpha_signs);
      clear_cfl_dc_pred_cache_flags(&xd->cfl);
      return 0;
    }
    const int8_t orig_cfl_alpha_signs = mbmi->cfl_alpha_signs;
    const uint8_t orig_cfl_alpha_idx = mbmi->cfl_alpha_idx;
    const uint8_t joint_idx =
        (uint8_t)((cfl_alpha_u << CFL_ALPHABET_SIZE_LOG2) + cfl_alpha_v);
    mbmi->cfl_alpha_signs = (int8_t)joint_sign;
    mbmi->cfl_alpha_idx = joint_idx;
    RD_STATS rd_u, rd_v;
    av1_init_rd_stats(&rd_u);
    av1_txfm_rd_in_plane(x, cpi, &rd_u, INT64_MAX, 0, 1, plane_bsize, tx_size,
                         FTXS_NONE);
    av1_init_rd_stats(&rd_v);
    av1_txfm_rd_in_plane(x, cpi, &rd_v, INT64_MAX, 0, 2, plane_bsize, tx_size,
                         FTXS_NONE);
    clear_cfl_dc_pred_cache_flags(&xd->cfl);
    if (rd_u.rate != INT_MAX && rd_v.rate != INT_MAX) {
      av1_merge_rd_stats(&rd_u, &rd_v);
      if (rd_u.rate != INT_MAX) {
        rd_u.rate += cfl_rate;
      }
      av1_rd_cost_update((int)rdmult, &rd_u);
      if (rd_u.rdcost < ref_best_rd) {
        *best_rd_stats = rd_u;
        *best_cfl_alpha_idx = joint_idx;
        *best_cfl_alpha_signs = (int8_t)joint_sign;
        return 1;
      }
    }
    mbmi->cfl_alpha_signs = orig_cfl_alpha_signs;
    mbmi->cfl_alpha_idx = orig_cfl_alpha_idx;
    set_invalid_cfl_parameters(best_cfl_alpha_idx, best_cfl_alpha_signs);
    return 0;
  }

  return cfl_rd_pick_alpha_multi(x, cpi, tx_size, ref_best_rd, cfl_search_range,
                                 est_best_cfl_idx_u, est_best_cfl_idx_v,
                                 best_rd_stats, best_cfl_alpha_idx,
                                 best_cfl_alpha_signs);
}

static bool should_prune_chroma_smooth_pred_based_on_source_variance(
    const AV1_COMP *cpi, const MACROBLOCK *x, BLOCK_SIZE bsize) {
  if (!cpi->sf.intra_sf.prune_smooth_intra_mode_for_chroma) return false;

  // If the source variance of both chroma planes is less than 20 (empirically
  // derived), prune UV_SMOOTH_PRED.
  for (int i = AOM_PLANE_U; i < av1_num_planes(&cpi->common); i++) {
    const unsigned int variance = av1_get_perpixel_variance_facade(
        cpi, &x->e_mbd, &x->plane[i].src, bsize, i);
    if (variance >= 20) return false;
  }
  return true;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void pick_palette_intra_sbuv_cold(const AV1_COMP *const cpi,
                                         MACROBLOCK *x, int dc_mode_cost,
                                         int64_t *best_rd, int *rate,
                                         int *rate_tokenonly,
                                         int64_t *distortion,
                                         uint8_t *skippable) {
  MB_MODE_INFO *const mbmi = x->e_mbd.mi[0];
  MB_MODE_INFO best_mbmi = *mbmi;
  av1_rd_pick_palette_intra_sbuv(
      cpi, x, dc_mode_cost, x->palette_buffer->best_palette_color_map,
      &best_mbmi, best_rd, rate, rate_tokenonly, distortion, skippable);
  *mbmi = best_mbmi;
}

int64_t av1_rd_pick_intra_sbuv_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                    int *rate, int *rate_tokenonly,
                                    int64_t *distortion, uint8_t *skippable,
                                    BLOCK_SIZE bsize, TX_SIZE max_tx_size) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));

  init_sbuv_mode(mbmi);

  // Return if the current block does not correspond to a chroma block.
  if (!xd->is_chroma_ref) {
    *rate = 0;
    *rate_tokenonly = 0;
    *distortion = 0;
    *skippable = 1;
    return INT64_MAX;
  }

  // Only store reconstructed luma when there's chroma RDO. When there's no
  // chroma RDO, the reconstructed luma will be stored in encode_superblock().
  xd->cfl.store_y = store_cfl_required_rdo(cm, x);
  if (xd->cfl.store_y) {
    // Restore reconstructed luma values.
    // TODO(chiyotsai@google.com): right now we are re-computing the txfm in
    // this function everytime we search through uv modes. There is some
    // potential speed up here if we cache the result to avoid redundant
    // computation.
    av1_encode_intra_block_plane(cpi, x, mbmi->bsize, AOM_PLANE_Y,
                                 DRY_RUN_NORMAL,
                                 cpi->optimize_seg_arr[mbmi->segment_id]);
    xd->cfl.store_y = 0;
  }

  const ModeCosts *const mode_costs = &x->mode_costs;
  const IntraModeCfg *const intra_mode_cfg = &cpi->oxcf.intra_mode_cfg;
  const CFL_ALLOWED_TYPE cfl_allowed = is_cfl_allowed(xd);
  const SPEED_FEATURES *const sf = &cpi->sf;
  assert(mbmi->mode < INTRA_MODES);
  const int *const uv_mode_cost =
      mode_costs->intra_uv_mode_cost[cfl_allowed][mbmi->mode];

  uint32_t allowed_uv_mask =
      sf->intra_sf.intra_uv_mode_mask[txsize_sqr_up_map[max_tx_size]];
  if (sf->intra_sf.prune_chroma_modes_using_luma_winner) {
    allowed_uv_mask &= av1_derived_chroma_intra_mode_used_flag[mbmi->mode];
  }
  if (!intra_mode_cfg->enable_diagonal_intra) {
    allowed_uv_mask &=
        ~((1U << UV_D45_PRED) | (1U << UV_D135_PRED) | (1U << UV_D113_PRED) |
          (1U << UV_D157_PRED) | (1U << UV_D203_PRED) | (1U << UV_D67_PRED));
  }
  if (!intra_mode_cfg->enable_directional_intra) {
    allowed_uv_mask &=
        ~((1U << UV_V_PRED) | (1U << UV_H_PRED) | (1U << UV_D45_PRED) |
          (1U << UV_D135_PRED) | (1U << UV_D113_PRED) | (1U << UV_D157_PRED) |
          (1U << UV_D203_PRED) | (1U << UV_D67_PRED));
  }
  if (!intra_mode_cfg->enable_smooth_intra) {
    allowed_uv_mask &= ~((1U << UV_SMOOTH_PRED) | (1U << UV_SMOOTH_V_PRED) |
                         (1U << UV_SMOOTH_H_PRED));
  }
  if (!intra_mode_cfg->enable_paeth_intra) {
    allowed_uv_mask &= ~(1U << UV_PAETH_PRED);
  }
  if (!cfl_allowed || !intra_mode_cfg->enable_cfl_intra) {
    allowed_uv_mask &= ~(1U << UV_CFL_PRED);
  }
  allowed_uv_mask &= (1U << UV_INTRA_MODES) - 1U;

  const BLOCK_SIZE mbmi_bsize = mbmi->bsize;
  const int use_angle_delta_bsize = av1_use_angle_delta(bsize);
  const int use_angle_delta =
      av1_use_angle_delta(mbmi_bsize) && intra_mode_cfg->enable_angle_delta;
  const int64_t rdmult = x->rdmult;
  int64_t best_rd = INT64_MAX;
  UV_PREDICTION_MODE best_uv_mode = UV_DC_PRED;
  int8_t best_uv_angle_delta = 0;
  uint8_t best_cfl_alpha_idx = mbmi->cfl_alpha_idx;
  int8_t best_cfl_alpha_signs = mbmi->cfl_alpha_signs;
  int dir_mode_skip_mask_ready = 0;

  // Search through all non-palette modes, stopping once no allowed modes
  // remain.
  uint32_t rem_uv_mask = allowed_uv_mask;
  for (int mode_idx = 0; rem_uv_mask != 0; ++mode_idx) {
    const UV_PREDICTION_MODE uv_mode = uv_rd_search_mode_order[mode_idx];
    const uint32_t mode_bit = 1U << uv_mode;
    if (!(rem_uv_mask & mode_bit)) continue;
    rem_uv_mask &= ~mode_bit;

    // Skip the current mode evaluation if the RD cost derived using the mode
    // signaling rate exceeds the best_rd so far.
    const int mode_rate = uv_mode_cost[uv_mode];
    if (RDCOST(rdmult, mode_rate, 0) > best_rd) continue;

    RD_STATS tokenonly_rd_stats;
    const int is_directional_mode =
        (uv_mode >= UV_V_PRED && uv_mode <= UV_D67_PRED);

    mbmi->uv_mode = uv_mode;
    mbmi->angle_delta[PLANE_TYPE_UV] = 0;
    if (uv_mode == UV_CFL_PRED) {
      assert(!is_directional_mode);
      const TX_SIZE uv_tx_size = av1_get_tx_size(AOM_PLANE_U, xd);
      if (!cfl_rd_pick_alpha(x, cpi, uv_tx_size, best_rd,
                             sf->intra_sf.cfl_search_range, &tokenonly_rd_stats,
                             &mbmi->cfl_alpha_idx, &mbmi->cfl_alpha_signs)) {
        continue;
      }
    } else if (is_directional_mode && use_angle_delta) {
      if (sf->intra_sf.chroma_intra_pruning_with_hog &&
          !dir_mode_skip_mask_ready) {
        static const float thresh[2][4] = {
          { -1.2f, 0.0f, 0.0f, 1.2f },    // Interframe
          { -1.2f, -1.2f, -0.6f, 0.4f },  // Intraframe
        };
        const int is_intra_frame = frame_is_intra_only(cm);
        const uint32_t skip_uv =
            prune_intra_mode_with_hog_fast(
                x, bsize, cm->seq_params->sb_size,
                thresh[is_intra_frame]
                      [sf->intra_sf.chroma_intra_pruning_with_hog - 1],
                /*is_chroma=*/1)
            << UV_V_PRED;
        dir_mode_skip_mask_ready = 1;
        rem_uv_mask &= ~skip_uv;
        if (skip_uv & mode_bit) continue;
      }

      // Search through angle delta
      int dummy_rate;
      if (!rd_pick_intra_angle_sbuv(cpi, x, bsize, mode_rate, best_rd,
                                    &dummy_rate, &tokenonly_rd_stats))
        continue;
    } else {
      if (uv_mode == UV_SMOOTH_PRED &&
          should_prune_chroma_smooth_pred_based_on_source_variance(cpi, x,
                                                                   bsize))
        continue;

      // Predict directly if we don't need to search for angle delta.
      if (!av1_txfm_uvrd(cpi, x, &tokenonly_rd_stats, bsize, best_rd)) {
        continue;
      }
    }

    int extra_cost = 0;
    if (uv_mode == UV_DC_PRED) {
      if (av1_allow_palette(cm->features.allow_screen_content_tools,
                            mbmi_bsize)) {
        extra_cost =
            mode_costs->palette_uv_mode_cost[mbmi->palette_mode_info
                                                 .palette_size[0] > 0][0];
      }
    } else if (is_directional_mode && use_angle_delta_bsize) {
      extra_cost =
          x->mode_costs.angle_delta_cost[uv_mode - UV_V_PRED]
                                        [mbmi->angle_delta[PLANE_TYPE_UV] +
                                         MAX_ANGLE_DELTA];
    }
    const int this_rate = tokenonly_rd_stats.rate + mode_rate + extra_cost;
    int64_t this_rd = RDCOST(rdmult, this_rate, tokenonly_rd_stats.dist);

#if CONFIG_AV1_HIGHBITDEPTH
    if (UNLIKELY(xd->bd > 8 && cpi->oxcf.algo_cfg.sharpness == 3)) {
      if (uv_mode == UV_SMOOTH_PRED || uv_mode == UV_SMOOTH_V_PRED ||
          uv_mode == UV_SMOOTH_H_PRED) {
        if (this_rd < INT64_MAX) {
          this_rd += (this_rd >> 2);
        }
      }
    }
#endif

    if (this_rd < best_rd) {
      best_uv_mode = uv_mode;
      best_uv_angle_delta = mbmi->angle_delta[PLANE_TYPE_UV];
      best_cfl_alpha_idx = mbmi->cfl_alpha_idx;
      best_cfl_alpha_signs = mbmi->cfl_alpha_signs;
      best_rd = this_rd;
      *rate = this_rate;
      *rate_tokenonly = tokenonly_rd_stats.rate;
      *distortion = tokenonly_rd_stats.dist;
      *skippable = tokenonly_rd_stats.skip_txfm;
    }
  }

  mbmi->uv_mode = best_uv_mode;
  mbmi->angle_delta[PLANE_TYPE_UV] = best_uv_angle_delta;
  mbmi->cfl_alpha_idx = best_cfl_alpha_idx;
  mbmi->cfl_alpha_signs = best_cfl_alpha_signs;

  // Search palette mode
  if (UNLIKELY(cpi->oxcf.tool_cfg.enable_palette &&
               av1_allow_palette(cm->features.allow_screen_content_tools,
                                 mbmi_bsize))) {
    pick_palette_intra_sbuv_cold(cpi, x, uv_mode_cost[UV_DC_PRED], &best_rd,
                                 rate, rate_tokenonly, distortion, skippable);
  }

  // Make sure we actually chose a mode
  assert(best_rd < INT64_MAX);
  return best_rd;
}

// Searches palette mode for luma channel in inter frame.
int av1_search_palette_mode(IntraModeSearchState *intra_search_state,
                            const AV1_COMP *cpi, MACROBLOCK *x,
                            BLOCK_SIZE bsize, unsigned int ref_frame_cost,
                            PICK_MODE_CONTEXT *ctx, RD_STATS *this_rd_cost,
                            int64_t best_rd) {
  const AV1_COMMON *const cm = &cpi->common;
  MB_MODE_INFO *const mbmi = x->e_mbd.mi[0];
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  int rate2 = 0;
  int64_t distortion2 = 0, best_rd_palette = best_rd, this_rd;
  int skippable = 0;
  uint8_t *const best_palette_color_map =
      x->palette_buffer->best_palette_color_map;
  uint8_t *const color_map = xd->plane[0].color_index_map;
  MB_MODE_INFO best_mbmi_palette = *mbmi;
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  const ModeCosts *mode_costs = &x->mode_costs;
  const int *const intra_mode_cost =
      mode_costs->mbmode_cost[size_group_lookup[bsize]];
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];

  mbmi->mode = DC_PRED;
  mbmi->uv_mode = UV_DC_PRED;
  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;
  av1_zero(pmi->palette_size);

  RD_STATS rd_stats_y;
  av1_invalid_rd_stats(&rd_stats_y);
  av1_rd_pick_palette_intra_sby(
      cpi, x, bsize, intra_mode_cost[DC_PRED], &best_mbmi_palette,
      best_palette_color_map, &best_rd_palette, &rd_stats_y.rate, NULL,
      &rd_stats_y.dist, &rd_stats_y.skip_txfm, NULL, ctx, best_tx_type_map);
  if (rd_stats_y.rate == INT_MAX || pmi->palette_size[0] == 0) {
    this_rd_cost->rdcost = INT64_MAX;
    return skippable;
  }

  av1_copy_tx_type_map(xd->tx_type_map, best_tx_type_map, ctx->num_4x4_blk);
  memcpy(color_map, best_palette_color_map,
         rows * cols * sizeof(best_palette_color_map[0]));

  skippable = rd_stats_y.skip_txfm;
  distortion2 = rd_stats_y.dist;
  rate2 = rd_stats_y.rate + ref_frame_cost;
  if (num_planes > 1) {
    if (intra_search_state->rate_uv_intra == INT_MAX) {
      // We have not found any good uv mode yet, so we need to search for it.
      TX_SIZE uv_tx = av1_get_tx_size(AOM_PLANE_U, xd);
      av1_rd_pick_intra_sbuv_mode(cpi, x, &intra_search_state->rate_uv_intra,
                                  &intra_search_state->rate_uv_tokenonly,
                                  &intra_search_state->dist_uvs,
                                  &intra_search_state->skip_uvs, bsize, uv_tx);
      intra_search_state->mode_uv = mbmi->uv_mode;
      intra_search_state->pmi_uv = *pmi;
      intra_search_state->uv_angle_delta = mbmi->angle_delta[PLANE_TYPE_UV];
    }

    // We have found at least one good uv mode before, so copy and paste it
    // over.
    mbmi->uv_mode = intra_search_state->mode_uv;
    pmi->palette_size[1] = intra_search_state->pmi_uv.palette_size[1];
    if (pmi->palette_size[1] > 0) {
      memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
             intra_search_state->pmi_uv.palette_colors + PALETTE_MAX_SIZE,
             2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
    }
    mbmi->angle_delta[PLANE_TYPE_UV] = intra_search_state->uv_angle_delta;
    skippable = skippable && intra_search_state->skip_uvs;
    distortion2 += intra_search_state->dist_uvs;
    rate2 += intra_search_state->rate_uv_intra;
  }

  if (skippable) {
    rate2 -= rd_stats_y.rate;
    if (num_planes > 1) rate2 -= intra_search_state->rate_uv_tokenonly;
    rate2 += mode_costs->skip_txfm_cost[av1_get_skip_txfm_context(xd)][1];
  } else {
    rate2 += mode_costs->skip_txfm_cost[av1_get_skip_txfm_context(xd)][0];
  }
  this_rd = RDCOST(x->rdmult, rate2, distortion2);
  this_rd_cost->rate = rate2;
  this_rd_cost->dist = distortion2;
  this_rd_cost->rdcost = this_rd;
  return skippable;
}

void av1_search_palette_mode_luma(const AV1_COMP *cpi, MACROBLOCK *x,
                                  BLOCK_SIZE bsize, unsigned int ref_frame_cost,
                                  PICK_MODE_CONTEXT *ctx,
                                  RD_STATS *this_rd_cost, int64_t best_rd) {
  MB_MODE_INFO *const mbmi = x->e_mbd.mi[0];
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  MACROBLOCKD *const xd = &x->e_mbd;
  int64_t best_rd_palette = best_rd, this_rd;
  uint8_t *const best_palette_color_map =
      x->palette_buffer->best_palette_color_map;
  uint8_t *const color_map = xd->plane[0].color_index_map;
  MB_MODE_INFO best_mbmi_palette = *mbmi;
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  const ModeCosts *mode_costs = &x->mode_costs;
  const int *const intra_mode_cost =
      mode_costs->mbmode_cost[size_group_lookup[bsize]];
  const int rows = block_size_high[bsize];
  const int cols = block_size_wide[bsize];

  mbmi->mode = DC_PRED;
  mbmi->uv_mode = UV_DC_PRED;
  mbmi->ref_frame[0] = INTRA_FRAME;
  mbmi->ref_frame[1] = NONE_FRAME;
  av1_zero(pmi->palette_size);

  RD_STATS rd_stats_y;
  av1_invalid_rd_stats(&rd_stats_y);
  av1_rd_pick_palette_intra_sby(
      cpi, x, bsize, intra_mode_cost[DC_PRED], &best_mbmi_palette,
      best_palette_color_map, &best_rd_palette, &rd_stats_y.rate, NULL,
      &rd_stats_y.dist, &rd_stats_y.skip_txfm, NULL, ctx, best_tx_type_map);
  if (rd_stats_y.rate == INT_MAX || pmi->palette_size[0] == 0) {
    this_rd_cost->rdcost = INT64_MAX;
    return;
  }

  av1_copy_tx_type_map(xd->tx_type_map, best_tx_type_map, ctx->num_4x4_blk);
  memcpy(color_map, best_palette_color_map,
         rows * cols * sizeof(best_palette_color_map[0]));

  rd_stats_y.rate += ref_frame_cost;

  if (rd_stats_y.skip_txfm) {
    rd_stats_y.rate =
        ref_frame_cost +
        mode_costs->skip_txfm_cost[av1_get_skip_txfm_context(xd)][1];
  } else {
    rd_stats_y.rate +=
        mode_costs->skip_txfm_cost[av1_get_skip_txfm_context(xd)][0];
  }
  this_rd = RDCOST(x->rdmult, rd_stats_y.rate, rd_stats_y.dist);
  this_rd_cost->rate = rd_stats_y.rate;
  this_rd_cost->dist = rd_stats_y.dist;
  this_rd_cost->rdcost = this_rd;
  this_rd_cost->skip_txfm = rd_stats_y.skip_txfm;
}

/*!\brief Get the intra prediction by searching through tx_type and tx_size.
 *
 * \ingroup intra_mode_search
 * \callergraph
 * Currently this function is only used in the intra frame code path for
 * winner-mode processing.
 *
 * \return Returns whether the current mode is an improvement over best_rd.
 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int intra_block_yrd(const AV1_COMP *const cpi, MACROBLOCK *x,
                           BLOCK_SIZE bsize, const int *bmode_costs,
                           int64_t *best_rd, int *rate, int *rate_tokenonly,
                           int64_t *distortion, uint8_t *skippable,
                           MB_MODE_INFO *best_mbmi, PICK_MODE_CONTEXT *ctx) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  RD_STATS rd_stats;
  // In order to improve txfm search, avoid rd based breakouts during winner
  // mode evaluation. Hence passing ref_best_rd as INT64_MAX by default when the
  // speed feature use_rd_based_breakout_for_intra_tx_search is disabled.
  int64_t ref_best_rd = cpi->sf.tx_sf.use_rd_based_breakout_for_intra_tx_search
                            ? *best_rd
                            : INT64_MAX;
  av1_pick_uniform_tx_size_type_yrd(cpi, x, &rd_stats, bsize, ref_best_rd);
  if (rd_stats.rate == INT_MAX) return 0;
  const int this_rate =
      rd_stats.rate +
      intra_mode_info_cost_y(cpi, x, mbmi, bsize, bmode_costs[mbmi->mode], 0);
  const int64_t this_rd = RDCOST(x->rdmult, this_rate, rd_stats.dist);
  if (this_rd < *best_rd) {
    int this_rate_tokenonly = rd_stats.rate;
    if (!xd->lossless[mbmi->segment_id] && block_signals_txsize(mbmi->bsize)) {
      this_rate_tokenonly -= tx_size_cost(x, bsize, mbmi->tx_size);
    }
    if (best_mbmi != NULL) *best_mbmi = *mbmi;
    *best_rd = this_rd;
    *rate = this_rate;
    *rate_tokenonly = this_rate_tokenonly;
    *distortion = rd_stats.dist;
    *skippable = rd_stats.skip_txfm;
    av1_copy_tx_type_map(ctx->tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
    return 1;
  }
  return 0;
}

/*!\brief Search for the best filter_intra mode when coding inter frame.
 *
 * \ingroup intra_mode_search
 * \callergraph
 * This function loops through all filter_intra modes to find the best one.
 *
 * \remark Returns nothing, but updates the mbmi and rd_stats.
 */
static inline void handle_filter_intra_mode(const AV1_COMP *cpi, MACROBLOCK *x,
                                            BLOCK_SIZE bsize,
                                            const PICK_MODE_CONTEXT *ctx,
                                            RD_STATS *rd_stats_y, int mode_cost,
                                            int64_t best_rd,
                                            int64_t best_rd_so_far) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(mbmi->mode == DC_PRED &&
         av1_filter_intra_allowed_bsize(&cpi->common, bsize));

  RD_STATS rd_stats_y_fi;
  int filter_intra_selected_flag = 0;
  TX_SIZE best_tx_size = mbmi->tx_size;
  FILTER_INTRA_MODE best_fi_mode = FILTER_DC_PRED;
  uint8_t best_tx_type_map[MAX_MIB_SIZE * MAX_MIB_SIZE];
  av1_copy_tx_type_map(best_tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
  mbmi->filter_intra_mode_info.use_filter_intra = 1;
  for (FILTER_INTRA_MODE fi_mode = FILTER_DC_PRED; fi_mode < FILTER_INTRA_MODES;
       ++fi_mode) {
    mbmi->filter_intra_mode_info.filter_intra_mode = fi_mode;
    av1_pick_uniform_tx_size_type_yrd(cpi, x, &rd_stats_y_fi, bsize, best_rd);
    if (rd_stats_y_fi.rate == INT_MAX) continue;
    const int this_rate_tmp =
        rd_stats_y_fi.rate +
        intra_mode_info_cost_y(cpi, x, mbmi, bsize, mode_cost, 0);
    const int64_t this_rd_tmp =
        RDCOST(x->rdmult, this_rate_tmp, rd_stats_y_fi.dist);

    if (this_rd_tmp != INT64_MAX && this_rd_tmp / 2 > best_rd) {
      break;
    }
    if (this_rd_tmp < best_rd_so_far) {
      best_tx_size = mbmi->tx_size;
      av1_copy_tx_type_map(best_tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
      best_fi_mode = fi_mode;
      *rd_stats_y = rd_stats_y_fi;
      filter_intra_selected_flag = 1;
      best_rd_so_far = this_rd_tmp;
    }
  }

  mbmi->tx_size = best_tx_size;
  av1_copy_tx_type_map(xd->tx_type_map, best_tx_type_map, ctx->num_4x4_blk);

  if (filter_intra_selected_flag) {
    mbmi->filter_intra_mode_info.use_filter_intra = 1;
    mbmi->filter_intra_mode_info.filter_intra_mode = best_fi_mode;
  } else {
    mbmi->filter_intra_mode_info.use_filter_intra = 0;
  }
}

// Evaluate a given luma intra-mode in inter frames.
int av1_handle_intra_y_mode(IntraModeSearchState *intra_search_state,
                            const AV1_COMP *cpi, MACROBLOCK *x,
                            BLOCK_SIZE bsize, unsigned int ref_frame_cost,
                            const PICK_MODE_CONTEXT *ctx, RD_STATS *rd_stats_y,
                            int64_t best_rd, int *mode_cost_y, int64_t *rd_y,
                            int64_t *best_model_rd,
                            int64_t top_intra_model_rd[]) {
  const AV1_COMMON *cm = &cpi->common;
  const INTRA_MODE_SPEED_FEATURES *const intra_sf = &cpi->sf.intra_sf;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(mbmi->ref_frame[0] == INTRA_FRAME);
  const PREDICTION_MODE mode = mbmi->mode;
  const ModeCosts *mode_costs = &x->mode_costs;
  const int mode_cost =
      mode_costs->mbmode_cost[size_group_lookup[bsize]][mode] + ref_frame_cost;
  const int skip_ctx = av1_get_skip_txfm_context(xd);
  int known_rate = mode_cost + mode_costs->skip_txfm_cost[skip_ctx][0];
  const int64_t known_rd = RDCOST(x->rdmult, known_rate, 0);
  if (known_rd > best_rd) {
    intra_search_state->skip_intra_modes = 1;
    return 0;
  }

  const int is_directional_mode = av1_is_directional_mode(mode);
  if (is_directional_mode && av1_use_angle_delta(bsize) &&
      cpi->oxcf.intra_mode_cfg.enable_angle_delta) {
    if (intra_sf->intra_pruning_with_hog &&
        !intra_search_state->dir_mode_skip_mask_ready) {
      static const float thresh[4] = { -1.2f, 0.0f, 0.0f, 1.2f };
      const uint32_t skip_bits = prune_intra_mode_with_hog_fast(
          x, bsize, cm->seq_params->sb_size,
          thresh[intra_sf->intra_pruning_with_hog - 1], /*is_chroma=*/0);
      for (int m = 0; m < DIRECTIONAL_MODES; ++m) {
        intra_search_state->directional_mode_skip_mask[V_PRED + m] =
            (uint8_t)((skip_bits >> m) & 1u);
      }
      intra_search_state->dir_mode_skip_mask_ready = 1;
    }
    if (intra_search_state->directional_mode_skip_mask[mode]) return 0;
  }
  const TX_SIZE tx_size = AOMMIN(TX_32X32, max_txsize_lookup[bsize]);
  const int64_t this_model_rd =
      intra_model_rd(&cpi->common, x, 0, bsize, tx_size, /*use_hadamard=*/1,
                     cpi->do_border_pad);

  const int model_rd_index_for_pruning =
      get_model_rd_index_for_pruning(x, intra_sf);

  if (prune_intra_y_mode(this_model_rd, best_model_rd, top_intra_model_rd,
                         intra_sf->top_intra_model_count_allowed,
                         model_rd_index_for_pruning))
    return 0;
  av1_init_rd_stats(rd_stats_y);
  av1_pick_uniform_tx_size_type_yrd(cpi, x, rd_stats_y, bsize, best_rd);

  // Pick filter intra modes.
  if (mode == DC_PRED && av1_filter_intra_allowed_bsize(cm, bsize)) {
    int try_filter_intra = 1;
    int64_t best_rd_so_far = INT64_MAX;
    if (rd_stats_y->rate != INT_MAX) {
      // best_rd_so_far is the rdcost of DC_PRED without using filter_intra.
      // Later, in filter intra search, best_rd_so_far is used for comparison.
      mbmi->filter_intra_mode_info.use_filter_intra = 0;
      const int tmp_rate =
          rd_stats_y->rate +
          intra_mode_info_cost_y(cpi, x, mbmi, bsize, mode_cost, 0);
      best_rd_so_far = RDCOST(x->rdmult, tmp_rate, rd_stats_y->dist);
      try_filter_intra = (best_rd_so_far / 2) <= best_rd;
    } else if (intra_sf->skip_filter_intra_in_inter_frames >= 1) {
      // As rd cost of luma intra dc mode is more than best_rd (i.e.,
      // rd_stats_y->rate = INT_MAX), skip the evaluation of filter intra modes.
      try_filter_intra = 0;
    }

    if (try_filter_intra) {
      handle_filter_intra_mode(cpi, x, bsize, ctx, rd_stats_y, mode_cost,
                               best_rd, best_rd_so_far);
    }
  }

  if (rd_stats_y->rate == INT_MAX) return 0;

  *mode_cost_y = intra_mode_info_cost_y(cpi, x, mbmi, bsize, mode_cost, 0);
  const int rate_y = rd_stats_y->skip_txfm
                         ? mode_costs->skip_txfm_cost[skip_ctx][1]
                         : rd_stats_y->rate;
  *rd_y = RDCOST(x->rdmult, rate_y + *mode_cost_y, rd_stats_y->dist);
  if (best_rd < (INT64_MAX / 2) && *rd_y > (best_rd + (best_rd >> 2))) {
    intra_search_state->skip_intra_modes = 1;
    return 0;
  }

  return 1;
}

int av1_search_intra_uv_modes_in_interframe(
    IntraModeSearchState *intra_search_state, const AV1_COMP *cpi,
    MACROBLOCK *x, BLOCK_SIZE bsize, RD_STATS *rd_stats,
    const RD_STATS *rd_stats_y, RD_STATS *rd_stats_uv, int64_t best_rd) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(mbmi->ref_frame[0] == INTRA_FRAME);

  // TODO(chiyotsai@google.com): Consolidate the chroma search code here with
  // the one in av1_search_palette_mode.
  PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const int try_palette =
      cpi->oxcf.tool_cfg.enable_palette &&
      av1_allow_palette(cm->features.allow_screen_content_tools, mbmi->bsize);
  assert(intra_search_state->rate_uv_intra == INT_MAX);
  // If no good uv-predictor had been found, search for it.
  const TX_SIZE uv_tx = av1_get_tx_size(AOM_PLANE_U, xd);
  av1_rd_pick_intra_sbuv_mode(cpi, x, &intra_search_state->rate_uv_intra,
                              &intra_search_state->rate_uv_tokenonly,
                              &intra_search_state->dist_uvs,
                              &intra_search_state->skip_uvs, bsize, uv_tx);
  intra_search_state->mode_uv = mbmi->uv_mode;
  if (try_palette) intra_search_state->pmi_uv = *pmi;
  intra_search_state->uv_angle_delta = mbmi->angle_delta[PLANE_TYPE_UV];

  const int uv_rate = intra_search_state->rate_uv_tokenonly;
  const int64_t uv_dist = intra_search_state->dist_uvs;
  const int64_t uv_rd = RDCOST(x->rdmult, uv_rate, uv_dist);
  if (uv_rd > best_rd) {
    // If there is no good intra uv-mode available, we can skip all intra
    // modes.
    intra_search_state->skip_intra_modes = 1;
    return 0;
  }

  // If we are here, then the encoder has found at least one good intra uv
  // predictor, so we can directly copy its statistics over.
  // TODO(any): the stats here is not right if the best uv mode is CFL but the
  // best y mode is palette.
  rd_stats_uv->rate = intra_search_state->rate_uv_tokenonly;
  rd_stats_uv->dist = intra_search_state->dist_uvs;
  rd_stats_uv->skip_txfm = intra_search_state->skip_uvs;
  rd_stats->skip_txfm = rd_stats_y->skip_txfm && rd_stats_uv->skip_txfm;
  mbmi->uv_mode = intra_search_state->mode_uv;
  if (try_palette) {
    pmi->palette_size[1] = intra_search_state->pmi_uv.palette_size[1];
    memcpy(pmi->palette_colors + PALETTE_MAX_SIZE,
           intra_search_state->pmi_uv.palette_colors + PALETTE_MAX_SIZE,
           2 * PALETTE_MAX_SIZE * sizeof(pmi->palette_colors[0]));
  }
  mbmi->angle_delta[PLANE_TYPE_UV] = intra_search_state->uv_angle_delta;

  return 1;
}

static inline int ctz_32(uint32_t x) {
  assert(x != 0);
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_ctz(x);
#else
  int n = 0;
  if ((x & 0x0000FFFFu) == 0) {
    n += 16;
    x >>= 16;
  }
  if ((x & 0x000000FFu) == 0) {
    n += 8;
    x >>= 8;
  }
  if ((x & 0x0000000Fu) == 0) {
    n += 4;
    x >>= 4;
  }
  if ((x & 0x00000003u) == 0) {
    n += 2;
    x >>= 2;
  }
  if ((x & 0x00000001u) == 0) {
    n += 1;
  }
  return n;
#endif
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int64_t finish_intra_sby_slow_path(
    const AV1_COMP *const cpi, MACROBLOCK *x, int *rate, int *rate_tokenonly,
    int64_t *distortion, uint8_t *skippable, BLOCK_SIZE bsize,
    const int *bmode_costs, PICK_MODE_CONTEXT *ctx, int64_t best_rd,
    int64_t best_model_rd, int beat_best_rd, PREDICTION_MODE best_mode,
    int8_t best_angle_delta_y, TX_SIZE best_tx_size, int try_palette) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  MB_MODE_INFO best_mbmi = *mbmi;
  best_mbmi.mode = best_mode;
  best_mbmi.angle_delta[PLANE_TYPE_Y] = best_angle_delta_y;
  best_mbmi.tx_size = best_tx_size;

  // Searches palette
  if (try_palette) {
    av1_rd_pick_palette_intra_sby(
        cpi, x, bsize, bmode_costs[DC_PRED], &best_mbmi,
        x->palette_buffer->best_palette_color_map, &best_rd, rate,
        rate_tokenonly, distortion, skippable, &beat_best_rd, ctx,
        ctx->tx_type_map);
  }

  // Searches filter_intra
  if (beat_best_rd && cpi->sf.intra_sf.prune_filter_intra_level < 2 &&
      av1_filter_intra_allowed_bsize(&cpi->common, bsize)) {
    if (rd_pick_filter_intra_sby(cpi, x, rate, rate_tokenonly, distortion,
                                 skippable, bsize, bmode_costs[DC_PRED],
                                 best_mbmi.mode, &best_rd, &best_model_rd,
                                 ctx)) {
      best_mbmi = *mbmi;
    }
  }

  if (!beat_best_rd) return INT64_MAX;

  if (cpi->sf.winner_mode_sf.multi_winner_mode_type) {
    int best_mode_idx = 0;
    int block_width, block_height;
    uint8_t *color_map_dst = xd->plane[PLANE_TYPE_Y].color_index_map;
    av1_get_block_dimensions(bsize, AOM_PLANE_Y, xd, &block_width,
                             &block_height, NULL, NULL);

    for (int mode_idx = 0; mode_idx < x->winner_mode_count; mode_idx++) {
      *mbmi = x->winner_mode_stats[mode_idx].mbmi;
      if (is_winner_mode_processing_enabled(cpi, x, mbmi, 0)) {
        if (mbmi->palette_mode_info.palette_size[0] > 0) {
          uint8_t *color_map_src =
              x->winner_mode_stats[mode_idx].color_index_map;
          memcpy(color_map_dst, color_map_src,
                 block_width * block_height * sizeof(*color_map_src));
        }
        set_mode_eval_params(cpi, x, WINNER_MODE_EVAL);
        if (intra_block_yrd(cpi, x, bsize, bmode_costs, &best_rd, rate,
                            rate_tokenonly, distortion, skippable, &best_mbmi,
                            ctx))
          best_mode_idx = mode_idx;
      }
    }
    if (best_mbmi.palette_mode_info.palette_size[0] > 0) {
      uint8_t *color_map_src =
          x->winner_mode_stats[best_mode_idx].color_index_map;
      memcpy(color_map_dst, color_map_src,
             block_width * block_height * sizeof(*color_map_src));
    }
  } else {
    if (is_winner_mode_processing_enabled(cpi, x, mbmi, 0)) {
      set_mode_eval_params(cpi, x, WINNER_MODE_EVAL);
      *mbmi = best_mbmi;
      intra_block_yrd(cpi, x, bsize, bmode_costs, &best_rd, rate,
                      rate_tokenonly, distortion, skippable, &best_mbmi, ctx);
    }
  }
  *mbmi = best_mbmi;
  av1_copy_tx_type_map(xd->tx_type_map, ctx->tx_type_map, ctx->num_4x4_blk);
  return best_rd;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void store_winner_mode_stats_sby(const AV1_COMP *cpi, MACROBLOCK *x,
                                        MB_MODE_INFO *mbmi, BLOCK_SIZE bsize,
                                        int64_t this_rd,
                                        int multi_winner_mode_type) {
  store_winner_mode_stats(cpi, x, mbmi, NULL, NULL, NULL, 0, NULL, bsize,
                          this_rd, multi_winner_mode_type,
                          /*txfm_search_done=*/1);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int64_t intra_model_yrd_fallback(const AV1_COMP *cpi, MACROBLOCK *x,
                                        BLOCK_SIZE bsize, TX_SIZE tx_size) {
  return intra_model_rd(&cpi->common, x, 0, bsize, tx_size, /*use_hadamard=*/1,
                        cpi->do_border_pad);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int tx_size_cost_sby(const MACROBLOCK *x, BLOCK_SIZE bsize,
                            TX_SIZE tx_size) {
  return tx_size_cost(x, bsize, tx_size);
}

#if HAVE_AVX2
static inline int64_t intra_model_yrd_lbd_multi_avx2(
    const AV1_COMMON *cm, MACROBLOCK *const x, TX_SIZE tx_size, int stepr,
    int stepc, int max_blocks_high, int max_blocks_wide) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const uint8_t *const src_base = x->plane[0].src.buf;
  const int src_stride = x->plane[0].src.stride;
  const uint8_t *const dst_base = xd->plane[0].dst.buf;
  const int dst_stride = xd->plane[0].dst.stride;
  int64_t satd_cost = 0;
  for (int row = 0; row < max_blocks_high; row += stepr) {
    const uint8_t *const src_row = src_base + (row << 2) * src_stride;
    const uint8_t *const dst_row = dst_base + (row << 2) * dst_stride;
    for (int col = 0; col < max_blocks_wide; col += stepc) {
      av1_predict_intra_block_facade(cm, xd, 0, col, row, tx_size);
      satd_cost +=
          aom_hadamard_sub_satd_avx2(tx_size, src_row + (col << 2), src_stride,
                                     dst_row + (col << 2), dst_stride);
    }
  }
  return satd_cost;
}
#endif

// Finds the best non-intrabc mode on an intra frame.
int64_t av1_rd_pick_intra_sby_mode(const AV1_COMP *const cpi, MACROBLOCK *x,
                                   int *rate, int *rate_tokenonly,
                                   int64_t *distortion, uint8_t *skippable,
                                   BLOCK_SIZE bsize, int64_t best_rd,
                                   PICK_MODE_CONTEXT *ctx) {
  static const uint8_t kBaseLumaModeOrder[INTRA_MODES] = {
    DC_PRED,       H_PRED,        V_PRED,    SMOOTH_PRED, PAETH_PRED,
    SMOOTH_V_PRED, SMOOTH_H_PRED, D135_PRED, D203_PRED,   D157_PRED,
    D67_PRED,      D113_PRED,     D45_PRED,
  };
  static const int8_t kDeltaAngles[2][6] = {
    { -3, -2, -1, 1, 2, 3 },
    { -2, 2, -3, -1, 1, 3 },
  };
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  assert(!is_inter_block(mbmi));
  const BLOCK_SIZE mbmi_bsize = mbmi->bsize;
  const IntraModeCfg *const intra_mode_cfg = &cpi->oxcf.intra_mode_cfg;
  const INTRA_MODE_SPEED_FEATURES *const intra_sf = &cpi->sf.intra_sf;
  const int allow_pal_bsize = av1_allow_palette(
      cpi->common.features.allow_screen_content_tools, mbmi_bsize);
  const int try_palette = cpi->oxcf.tool_cfg.enable_palette && allow_pal_bsize;
  const MB_MODE_INFO *const above_mi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mi = xd->left_mbmi;
  assert(!above_mi || !is_inter_block(above_mi) || is_intrabc_block(above_mi));
  assert(!left_mi || !is_inter_block(left_mi) || is_intrabc_block(left_mi));
  const PREDICTION_MODE above_mode = above_mi ? above_mi->mode : DC_PRED;
  const PREDICTION_MODE left_mode = left_mi ? left_mi->mode : DC_PRED;
  assert(above_mode < INTRA_MODES && left_mode < INTRA_MODES);
  const int *const bmode_costs =
      x->mode_costs.y_mode_costs[intra_mode_context[above_mode]]
                                [intra_mode_context[left_mode]];

  PREDICTION_MODE best_mode = mbmi->mode;
  int8_t best_angle_delta_y = 0;
  TX_SIZE best_tx_size = mbmi->tx_size;

  mbmi->angle_delta[PLANE_TYPE_Y] = 0;
  mbmi->filter_intra_mode_info.use_filter_intra = 0;
  mbmi->palette_mode_info.palette_size[0] = 0;

  // Set params for mode evaluation
  set_mode_eval_params(cpi, x, MODE_EVAL);
  x->winner_mode_count = 0;

  const TX_SIZE max_tx = max_txsize_lookup[bsize];
  uint32_t allowed_mode_mask = intra_sf->intra_y_mode_mask[max_tx];
  if (!intra_mode_cfg->enable_diagonal_intra)
    allowed_mode_mask &=
        ~((1u << D45_PRED) | (1u << D135_PRED) | (1u << D113_PRED) |
          (1u << D157_PRED) | (1u << D203_PRED) | (1u << D67_PRED));
  if (!intra_mode_cfg->enable_directional_intra)
    allowed_mode_mask &=
        ~((1u << V_PRED) | (1u << H_PRED) | (1u << D45_PRED) |
          (1u << D135_PRED) | (1u << D113_PRED) | (1u << D157_PRED) |
          (1u << D203_PRED) | (1u << D67_PRED));
  if (!intra_mode_cfg->enable_smooth_intra || intra_sf->disable_smooth_intra) {
    allowed_mode_mask &= ~((1u << SMOOTH_H_PRED) | (1u << SMOOTH_V_PRED));
    if (!intra_mode_cfg->enable_smooth_intra ||
        intra_sf->prune_filter_intra_level == 0)
      allowed_mode_mask &= ~(1u << SMOOTH_PRED);
  }
  if (!intra_mode_cfg->enable_paeth_intra)
    allowed_mode_mask &= ~(1u << PAETH_PRED);
  if (x->use_mb_mode_cache) allowed_mode_mask &= (1u << x->mb_mode_cache->mode);
  allowed_mode_mask &= (1u << INTRA_MODES) - 1u;

  // Run HoG only if at least one directional mode survived static/cache masks.
  if (intra_sf->intra_pruning_with_hog && (allowed_mode_mask & 0x01FEu) != 0) {
    static const float thresh[4] = { -1.2f, -1.2f, -0.6f, 0.4f };
    const uint32_t skip_bits = prune_intra_mode_with_hog_fast(
        x, bsize, cpi->common.seq_params->sb_size,
        thresh[intra_sf->intra_pruning_with_hog - 1], /*is_chroma=*/0);
    allowed_mode_mask &= ~(skip_bits << V_PRED);
  }

  int64_t best_model_rd = INT64_MAX;
  int64_t top_model[TOP_INTRA_MODEL_COUNT] = { INT64_MAX, INT64_MAX, INT64_MAX,
                                               INT64_MAX };
  int64_t dir_zero_rd[DIRECTIONAL_MODES] = { INT64_MAX, INT64_MAX, INT64_MAX,
                                             INT64_MAX, INT64_MAX, INT64_MAX,
                                             INT64_MAX, INT64_MAX };

  const TX_SIZE tx_size = AOMMIN(TX_32X32, max_tx);
  const int max_model_cnt = intra_sf->top_intra_model_count_allowed;
  const int base_model_rd_idx = max_model_cnt - 1;
  uint32_t reduce_model_rd_idx_mask = 0;
  if (intra_sf->adapt_top_model_rd_count_using_neighbors &&
      base_model_rd_idx > 0) {
    const uint32_t neq_left =
        xd->left_available
            ? (xd->left_mbmi->mode < INTRA_MODES ? ~(1u << xd->left_mbmi->mode)
                                                 : 0xFFFFu)
            : 0u;
    const uint32_t neq_above = xd->up_available
                                   ? (xd->above_mbmi->mode < INTRA_MODES
                                          ? ~(1u << xd->above_mbmi->mode)
                                          : 0xFFFFu)
                                   : 0u;
    reduce_model_rd_idx_mask =
        (x->qindex <= 127) ? (neq_left | neq_above) : (neq_left & neq_above);
  }

  const int base_non_dc_cost =
      av1_allow_intrabc(&cpi->common) ? x->mode_costs.intrabc_cost[0] : 0;
  int dc_extra_cost = base_non_dc_cost;
  if (av1_allow_palette(cpi->common.features.allow_screen_content_tools,
                        bsize)) {
    dc_extra_cost +=
        x->mode_costs.palette_y_mode_cost[av1_get_palette_bsize_ctx(bsize)]
                                         [av1_get_palette_mode_ctx(xd)][0];
  }
  if (av1_filter_intra_allowed_bsize(&cpi->common, bsize)) {
    dc_extra_cost += x->mode_costs.filter_intra_cost[bsize][0];
  }
  const int use_angle_delta_bsize = av1_use_angle_delta(bsize);
  const int use_angle_delta =
      use_angle_delta_bsize && intra_mode_cfg->enable_angle_delta;
  const int use_var_factor =
      (cpi->oxcf.mode == ALLINTRA) && (cpi->oxcf.speed < 4);
  const int deduct_txsize_cost =
      !xd->lossless[mbmi->segment_id] && block_signals_txsize(mbmi_bsize);
  const int multi_winner_mode_type =
      cpi->sf.winner_mode_sf.multi_winner_mode_type;
  const int64_t rdmult = x->rdmult;
  int beat_best_rd = 0;

#if HAVE_AVX2
  int model_mode = 0;
  int stepr = 0, stepc = 0, max_blocks_high = 0, max_blocks_wide = 0;
  if (LIKELY(!is_cur_buf_hbd(xd) &&
             (!cpi->do_border_pad ||
              (x->pix_to_bottom_edge >= 0 && x->pix_to_right_edge >= 0)))
#if CONFIG_RUNTIME_CPU_DETECT && !defined(aom_satd)
      && LIKELY(aom_satd == aom_satd_avx2)
#endif
  ) {
    stepr = tx_size_high_unit[tx_size];
    stepc = tx_size_wide_unit[tx_size];
    max_blocks_high = max_block_high(xd, bsize, 0);
    max_blocks_wide = max_block_wide(xd, bsize, 0);
    model_mode = (max_blocks_high <= stepr && max_blocks_wide <= stepc) ? 2 : 1;
  }
#endif

  // Loop 1: Evaluate base intra modes (angle_delta == 0).
  uint32_t rem_base = allowed_mode_mask;
  for (int idx = 0; rem_base != 0; ++idx) {
    const PREDICTION_MODE mode = (PREDICTION_MODE)kBaseLumaModeOrder[idx];
    const uint32_t bit = 1u << mode;
    if (!(rem_base & bit)) continue;
    rem_base &= ~bit;
    mbmi->mode = mode;

    int64_t this_model_rd;
#if HAVE_AVX2
    if (LIKELY(model_mode == 2)) {
      av1_predict_intra_block_facade(&cpi->common, xd, 0, 0, 0, tx_size);
      this_model_rd = aom_hadamard_sub_satd_avx2(
          tx_size, x->plane[0].src.buf, x->plane[0].src.stride,
          xd->plane[0].dst.buf, xd->plane[0].dst.stride);
    } else if (LIKELY(model_mode == 1)) {
      this_model_rd =
          intra_model_yrd_lbd_multi_avx2(&cpi->common, x, tx_size, stepr, stepc,
                                         max_blocks_high, max_blocks_wide);
    } else
#endif
    {
      this_model_rd = intra_model_yrd_fallback(cpi, x, bsize, tx_size);
    }

    const int model_rd_idx =
        base_model_rd_idx - (int)((reduce_model_rd_idx_mask >> mode) & 1u);
    if (prune_intra_y_mode(this_model_rd, &best_model_rd, top_model,
                           max_model_cnt, model_rd_idx))
      continue;

    RD_STATS this_rd_stats;
    av1_pick_uniform_tx_size_type_yrd(cpi, x, &this_rd_stats, bsize, best_rd);
    int this_rate_tokenonly = this_rd_stats.rate;
    if (this_rate_tokenonly == INT_MAX) continue;

    const int is_dir = (mode >= V_PRED && mode <= D67_PRED);
    int mode_info_cost = bmode_costs[mode];
    if (mode == DC_PRED) {
      mode_info_cost += dc_extra_cost;
    } else {
      mode_info_cost += base_non_dc_cost;
      if (use_angle_delta_bsize && is_dir) {
        mode_info_cost +=
            x->mode_costs.angle_delta_cost[mode - V_PRED][MAX_ANGLE_DELTA];
      }
    }
    const int this_rate = this_rate_tokenonly + mode_info_cost;
    int64_t this_rd = RDCOST(rdmult, this_rate, this_rd_stats.dist);
    if (UNLIKELY(use_var_factor) && this_rd != INT64_MAX) {
      this_rd = (int64_t)(this_rd * intra_rd_variance_factor(cpi, x, bsize));
    }
    if (is_dir) dir_zero_rd[mode - V_PRED] = this_rd;

    if (UNLIKELY(multi_winner_mode_type)) {
      store_winner_mode_stats_sby(cpi, x, mbmi, bsize, this_rd,
                                  multi_winner_mode_type);
    }
    if (this_rd < best_rd) {
      if (deduct_txsize_cost) {
        this_rate_tokenonly -= tx_size_cost_sby(x, bsize, mbmi->tx_size);
      }
      best_mode = mode;
      best_tx_size = mbmi->tx_size;
      best_rd = this_rd;
      beat_best_rd = 1;
      *rate = this_rate;
      *rate_tokenonly = this_rate_tokenonly;
      *distortion = this_rd_stats.dist;
      *skippable = this_rd_stats.skip_txfm;
      av1_copy_tx_type_map(ctx->tx_type_map, xd->tx_type_map, ctx->num_4x4_blk);
    }
  }

  // Loop 2: Evaluate directional delta angles (only active modes 1..8).
  uint32_t dir_mask = use_angle_delta ? (allowed_mode_mask & 0x01FEu) : 0u;
  if (dir_mask != 0) {
    const int reorder_delta_angle =
        intra_sf->prune_luma_odd_delta_angles_in_intra != 0;
    const int8_t *const angle_list = kDeltaAngles[reorder_delta_angle];
    do {
      const PREDICTION_MODE mode = (PREDICTION_MODE)ctz_32(dir_mask);
      dir_mask &= dir_mask - 1u;
      mbmi->mode = mode;

      const int mode_base_cost = bmode_costs[mode] + base_non_dc_cost;
      const int *const angle_cost_row =
          x->mode_costs.angle_delta_cost[mode - V_PRED];
      const int model_rd_idx =
          base_model_rd_idx - (int)((reduce_model_rd_idx_mask >> mode) & 1u);
      int64_t rd_ev[5] = { INT64_MAX, INT64_MAX, dir_zero_rd[mode - V_PRED],
                           INT64_MAX, INT64_MAX };

      for (int ai = 0; ai < 6; ++ai) {
        if (ai >= 2 && LIKELY(reorder_delta_angle) && best_rd != INT64_MAX) {
          const int64_t rd_thresh = best_rd + (best_rd >> 3);
          if (ai == 2 && rd_ev[1] > rd_thresh && rd_ev[2] > rd_thresh &&
              rd_ev[3] > rd_thresh) {
            break;
          }
          if (rd_ev[ai - 2] > rd_thresh && rd_ev[ai - 1] > rd_thresh) {
            continue;
          }
        }
        const int angle = angle_list[ai];
        mbmi->angle_delta[PLANE_TYPE_Y] = (int8_t)angle;

        int64_t this_model_rd;
#if HAVE_AVX2
        if (LIKELY(model_mode == 2)) {
          av1_predict_intra_block_facade(&cpi->common, xd, 0, 0, 0, tx_size);
          this_model_rd = aom_hadamard_sub_satd_avx2(
              tx_size, x->plane[0].src.buf, x->plane[0].src.stride,
              xd->plane[0].dst.buf, xd->plane[0].dst.stride);
        } else if (LIKELY(model_mode == 1)) {
          this_model_rd = intra_model_yrd_lbd_multi_avx2(
              &cpi->common, x, tx_size, stepr, stepc, max_blocks_high,
              max_blocks_wide);
        } else
#endif
        {
          this_model_rd = intra_model_yrd_fallback(cpi, x, bsize, tx_size);
        }

        if (prune_intra_y_mode(this_model_rd, &best_model_rd, top_model,
                               max_model_cnt, model_rd_idx))
          continue;

        RD_STATS this_rd_stats;
        av1_pick_uniform_tx_size_type_yrd(cpi, x, &this_rd_stats, bsize,
                                          best_rd);
        int this_rate_tokenonly = this_rd_stats.rate;
        if (this_rate_tokenonly == INT_MAX) continue;

        const int this_rate = this_rate_tokenonly + mode_base_cost +
                              angle_cost_row[MAX_ANGLE_DELTA + angle];
        int64_t this_rd = RDCOST(rdmult, this_rate, this_rd_stats.dist);
        if (UNLIKELY(use_var_factor) && this_rd != INT64_MAX) {
          this_rd =
              (int64_t)(this_rd * intra_rd_variance_factor(cpi, x, bsize));
        }
        if (ai < 2) rd_ev[1 + 2 * ai] = this_rd;

        if (UNLIKELY(multi_winner_mode_type)) {
          store_winner_mode_stats_sby(cpi, x, mbmi, bsize, this_rd,
                                      multi_winner_mode_type);
        }
        if (this_rd < best_rd) {
          if (deduct_txsize_cost) {
            this_rate_tokenonly -= tx_size_cost_sby(x, bsize, mbmi->tx_size);
          }
          best_mode = mode;
          best_angle_delta_y = (int8_t)angle;
          best_tx_size = mbmi->tx_size;
          best_rd = this_rd;
          beat_best_rd = 1;
          *rate = this_rate;
          *rate_tokenonly = this_rate_tokenonly;
          *distortion = this_rd_stats.dist;
          *skippable = this_rd_stats.skip_txfm;
          av1_copy_tx_type_map(ctx->tx_type_map, xd->tx_type_map,
                               ctx->num_4x4_blk);
        }
      }
    } while (dir_mask != 0);
  }

  const int try_filter_intra =
      intra_sf->prune_filter_intra_level < 2 &&
      av1_filter_intra_allowed_bsize(&cpi->common, bsize);
  if (UNLIKELY(try_palette || try_filter_intra || multi_winner_mode_type)) {
    return finish_intra_sby_slow_path(
        cpi, x, rate, rate_tokenonly, distortion, skippable, bsize, bmode_costs,
        ctx, best_rd, best_model_rd, beat_best_rd, best_mode,
        best_angle_delta_y, best_tx_size, try_palette);
  }

  if (!beat_best_rd) return INT64_MAX;

  mbmi->mode = best_mode;
  mbmi->angle_delta[PLANE_TYPE_Y] = best_angle_delta_y;
  mbmi->tx_size = best_tx_size;
  if (is_winner_mode_processing_enabled(cpi, x, mbmi, 0)) {
    set_mode_eval_params(cpi, x, WINNER_MODE_EVAL);
    if (!intra_block_yrd(cpi, x, bsize, bmode_costs, &best_rd, rate,
                         rate_tokenonly, distortion, skippable, NULL, ctx)) {
      mbmi->tx_size = best_tx_size;
    }
  }
  av1_copy_tx_type_map(xd->tx_type_map, ctx->tx_type_map, ctx->num_4x4_blk);
  return best_rd;
}

void av1_lowbd_generate_hog_c(const uint8_t *src, int stride, int rows,
                              int cols, float *hist) {
  lowbd_generate_hog(src, stride, rows, cols, hist);
}

void av1_lowbd_compute_gradient_info_sb_c(MACROBLOCK *const x,
                                          BLOCK_SIZE sb_size,
                                          PLANE_TYPE plane) {
  lowbd_compute_gradient_info_sb(x, sb_size, plane);
}

#if CONFIG_AV1_HIGHBITDEPTH
void av1_highbd_generate_hog_c(const uint8_t *src8, int stride, int rows,
                               int cols, float *hist) {
  highbd_generate_hog(src8, stride, rows, cols, hist);
}

void av1_highbd_compute_gradient_info_sb_c(MACROBLOCK *const x,
                                           BLOCK_SIZE sb_size,
                                           PLANE_TYPE plane) {
  highbd_compute_gradient_info_sb(x, sb_size, plane);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
