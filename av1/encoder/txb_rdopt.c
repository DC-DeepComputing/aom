/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/encoder/txb_rdopt.h"
#include "av1/encoder/txb_rdopt_utils.h"
#include "av1/encoder/rdopt_utils.h"

#include "aom_ports/mem.h"
#include "av1/common/idct.h"

static AOM_FORCE_INLINE int get_br_cost_fast(tran_low_t level,
                                             const int *coeff_lps) {
  if (LIKELY(level < 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE)) {
    return coeff_lps[level - 1 - NUM_BASE_LEVELS];
  }
  const int r = level - COEFF_BASE_RANGE - NUM_BASE_LEVELS;
  return coeff_lps[COEFF_BASE_RANGE] + av1_cost_literal((get_msb(r) << 1) + 1);
}

static AOM_FORCE_INLINE int get_eob_cost_tab(
    int eob, const int *eob_cost, const LV_MAP_COEFF_COST *txb_costs) {
  if (eob < 3) {
    return eob_cost[eob - 1];
  }
  const int d = eob - 1;
  const int k = get_msb(d);
  const int bit = (d >> (k - 1)) & 1;
  return eob_cost[k + 1] + txb_costs->eob_extra_cost[k - 1][bit] +
         av1_cost_literal(k - 1);
}

static AOM_FORCE_INLINE tran_low_t update_coeff_last(
    int *accu_rate, int64_t *accu_dist, int si, int ci, tran_low_t abs_qc,
    int sign, TX_CLASS tx_class, int bhl, int eob_shift, int64_t rdmult,
    int shift, const int16_t *dequant, const LV_MAP_COEFF_COST *txb_costs,
    const tran_low_t *tcoeff, tran_low_t *qcoeff, tran_low_t *dqcoeff,
    const qm_val_t *iqmatrix, const qm_val_t *qmatrix, int need_accu_dist) {
  const int coeff_ctx = AOMMIN((si - 1) >> eob_shift, 2) + 1;
  const tran_low_t tqc = tcoeff[ci];
  const tran_low_t dqc = dqcoeff[ci];
  const int *const base_eob = txb_costs->base_eob_cost[coeff_ctx];
  const tran_low_t abs_qc_low = abs_qc - 1;
  int rate, rate_low;
  if (LIKELY(abs_qc == 2)) {
    rate = base_eob[1] + av1_cost_literal(1);
    rate_low = base_eob[0] + av1_cost_literal(1);
  } else {
    const int br_ctx = get_br_ctx_eob(ci, bhl, tx_class);
    const int *const lps_cost = txb_costs->lps_cost[br_ctx];
    const int base_rate = base_eob[2] + av1_cost_literal(1);
    if (LIKELY(abs_qc < 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE)) {
      rate = base_rate + lps_cost[abs_qc - (1 + NUM_BASE_LEVELS)];
      rate_low = (abs_qc == 3)
                     ? (base_eob[1] + av1_cost_literal(1))
                     : (base_rate + lps_cost[abs_qc - (2 + NUM_BASE_LEVELS)]);
    } else {
      rate = base_rate + get_br_cost_fast(abs_qc, lps_cost);
      rate_low = base_rate + get_br_cost_fast(abs_qc_low, lps_cost);
    }
  }
  int dqv = dequant[1];
  if (LIKELY(iqmatrix != NULL)) {
    dqv = ((iqmatrix[ci] * dqv) + (1 << (AOM_QM_BITS - 1))) >> AOM_QM_BITS;
  }
  tran_low_t qc_low, dqc_low;
  get_qc_dqc_low(abs_qc, sign, dqv, shift, &qc_low, &dqc_low);

  const int64_t rd_rate = RDCOST(rdmult, rate, 0);
  const int64_t rd_low_rate = RDCOST(rdmult, rate_low, 0);
  int64_t dist_diff_0, dist_diff_low_0;
  int64_t scaled_qm = 0;
  if (LIKELY(qmatrix != NULL)) {
    scaled_qm = (int64_t)qmatrix[ci] << shift;
    dist_diff_0 = get_coeff_dist_scaled_qm(tqc, dqc, scaled_qm);
    dist_diff_low_0 = get_coeff_dist_scaled_qm(tqc, dqc_low, scaled_qm);
  } else {
    const int64_t tqc2 = (int64_t)tqc * 2;
    dist_diff_0 = ((int64_t)dqc * (dqc - tqc2)) * (1LL << (2 * shift));
    dist_diff_low_0 =
        ((int64_t)dqc_low * (dqc_low - tqc2)) * (1LL << (2 * shift));
  }

  const int64_t rd = rd_rate + dist_diff_0 * (1 << RDDIV_BITS);
  const int64_t rd_low = rd_low_rate + dist_diff_low_0 * (1 << RDDIV_BITS);

  int64_t chosen_dist;
  tran_low_t final_abs = abs_qc;
  if (rd_low < rd) {
    qcoeff[ci] = qc_low;
    dqcoeff[ci] = dqc_low;
    *accu_rate += rate_low;
    chosen_dist = dist_diff_low_0;
    final_abs = abs_qc_low;
  } else {
    *accu_rate += rate;
    chosen_dist = dist_diff_0;
  }
  if (need_accu_dist) {
    if (LIKELY(qmatrix != NULL)) {
      chosen_dist -= get_coeff_dist_scaled_qm(tqc, 0, scaled_qm);
    }
    *accu_dist += chosen_dist;
  }
  return final_abs;
}

static inline int get_lower_levels_ctx_dc(const uint8_t *levels,
                                          intptr_t stride, TX_CLASS tx_class) {
  if (LIKELY(tx_class == TX_CLASS_2D)) return 0;
  const int stats = get_nz_mag_stride(levels, stride, tx_class);
  return NZ_MAP_CTX_0 + AOMMIN((stats + 1) >> 1, 4);
}

typedef struct {
  int64_t rdmult;
  int64_t accu_dist;
  const int *eob_cost;
  const LV_MAP_COEFF_COST *txb_costs;
  const int16_t *dequant;
  const tran_low_t *tcoeff;
  tran_low_t *qcoeff;
  tran_low_t *dqcoeff;
  uint8_t *levels;
  const qm_val_t *iqmatrix;
  const qm_val_t *qmatrix;
  int *nz_ci;
  int8_t *cc;
  const int8_t *nz_map_ctx;
  intptr_t stride;
  int eob;
  int nz_num;
  int accu_rate;
  int bhl;
  int dc_sign_ctx;
  int shift;
  int dequant_ac;
  int16_t sharpness;
  uint8_t tx_class;
  uint8_t eob_shift;
  int min_eob_cutoff;
} TxbOptCtx;

static AOM_FORCE_INLINE void update_coeff_dc(TxbOptCtx *__restrict ctx) {
  tran_low_t *const qcoeff = ctx->qcoeff;
  const tran_low_t qc = qcoeff[0];
  const TX_CLASS tx_class = (TX_CLASS)ctx->tx_class;
  const int coeff_ctx =
      get_lower_levels_ctx_dc(ctx->levels, ctx->stride, tx_class);
  const LV_MAP_COEFF_COST *const txb_costs = ctx->txb_costs;
  if (qc == 0) {
    ctx->accu_rate += txb_costs->base_cost[coeff_ctx][0];
  } else {
    const int sign = (qc < 0) ? 1 : 0;
    const tran_low_t abs_qc = abs(qc);
    const tran_low_t tqc = ctx->tcoeff[0];
    const tran_low_t dqc = ctx->dqcoeff[0];
    const int dc_sign_cost = txb_costs->dc_sign_cost[ctx->dc_sign_ctx][sign];
    const int *const base_cost = txb_costs->base_cost[coeff_ctx];
    int rate = base_cost[AOMMIN(abs_qc, 3)] + dc_sign_cost;
    uint8_t *const levels = ctx->levels;
    const int *lps_cost = txb_costs->lps_cost[0];
    if (abs_qc > NUM_BASE_LEVELS) {
      const int br_ctx =
          get_br_ctx_stride(levels, 0, ctx->bhl, ctx->stride, tx_class);
      lps_cost = txb_costs->lps_cost[br_ctx];
      rate += get_br_cost_fast(abs_qc, lps_cost);
    }

    const int shift = ctx->shift;
    tran_low_t qc_low = 0, dqc_low = 0;
    const tran_low_t abs_qc_low = abs_qc - 1;
    int rate_low;
    if (abs_qc == 1) {
      rate_low = base_cost[0];
    } else {
      const int dqv = get_dqv(ctx->dequant, 0, ctx->iqmatrix);
      get_qc_dqc_low(abs_qc, sign, dqv, shift, &qc_low, &dqc_low);
      rate_low = base_cost[AOMMIN(abs_qc_low, 3)] + dc_sign_cost;
      if (abs_qc_low > NUM_BASE_LEVELS) {
        rate_low += get_br_cost_fast(abs_qc_low, lps_cost);
      }
    }

    const int64_t rdmult = ctx->rdmult;
    const int64_t rd_rate = RDCOST(rdmult, rate, 0);
    const int64_t rd_low_rate = RDCOST(rdmult, rate_low, 0);
    const qm_val_t *const qmatrix = ctx->qmatrix;
    int64_t dist_diff_0, dist_diff_low_0;
    if (LIKELY(qmatrix != NULL)) {
      const int64_t scaled_qm = (int64_t)qmatrix[0] << shift;
      dist_diff_0 = get_coeff_dist_scaled_qm(tqc, dqc, scaled_qm);
      dist_diff_low_0 = (abs_qc == 1)
                            ? get_coeff_dist_scaled_qm(tqc, 0, scaled_qm)
                            : get_coeff_dist_scaled_qm(tqc, dqc_low, scaled_qm);
    } else {
      const int64_t tqc2 = (int64_t)tqc * 2;
      dist_diff_0 = ((int64_t)dqc * (dqc - tqc2)) * (1LL << (2 * shift));
      dist_diff_low_0 =
          (abs_qc == 1)
              ? 0
              : (((int64_t)dqc_low * (dqc_low - tqc2)) * (1LL << (2 * shift)));
    }

    const int64_t rd = rd_rate + dist_diff_0 * (1 << RDDIV_BITS);
    const int64_t rd_low = rd_low_rate + dist_diff_low_0 * (1 << RDDIV_BITS);

    if (rd_low < rd) {
      qcoeff[0] = qc_low;
      ctx->dqcoeff[0] = dqc_low;
      levels[0] = AOMMIN(abs_qc_low, INT8_MAX);
      ctx->accu_rate += rate_low;
    } else {
      ctx->accu_rate += rate;
    }
  }
}

// Recomputes the cached lower-level contexts of every position whose
// get_nz_mag_stride() neighborhood contains position c (called after
// levels[c] changes). (dc, dr) are offsets in (col, row) = (ci >> bhl,
// ci & (height - 1)) units, matching get_nz_mag_stride().
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int patch_ctx_neighbors(int c, const TxbOptCtx *__restrict ctx,
                               int ret_rate) {
  const int bhl = ctx->bhl;
  const unsigned int col = (unsigned int)c >> bhl;
  const uint8_t *const lv_c = ctx->levels + c + (col << TX_PAD_HOR_LOG2);
  int8_t *__restrict cc = ctx->cc;
  const intptr_t stride = ctx->stride;
  const TX_CLASS tx_class = (TX_CLASS)ctx->tx_class;
  const int step = 1 << bhl;
  const unsigned int row = (unsigned int)c & (unsigned int)(step - 1);
  if (LIKELY(tx_class == TX_CLASS_2D)) {
    const int8_t *const nz_map_ctx = ctx->nz_map_ctx;
#define PATCH_2D(dc, dr)                                          \
  do {                                                            \
    const int p = c - (dc) * step - (dr);                         \
    const uint8_t *const lv = lv_c - (dc) * stride - (dr);        \
    const int stats = get_nz_mag_stride(lv, stride, TX_CLASS_2D); \
    const int ctx_val = AOMMIN((stats + 1) >> 1, 4);              \
    cc[p] = (int8_t)(ctx_val + nz_map_ctx[p]);                    \
  } while (0)
    if (col != 0) {
      PATCH_2D(1, 0);
      if (col >= 2) PATCH_2D(2, 0);
      if (row != 0) PATCH_2D(1, 1);
    }
    if (row != 0) {
      PATCH_2D(0, 1);
      if (row >= 2) PATCH_2D(0, 2);
    }
#undef PATCH_2D
  } else if (tx_class == TX_CLASS_VERT) {
#define PATCH_V(dc, dr)                                             \
  do {                                                              \
    const int p = c - (dc) * step - (dr);                           \
    const uint8_t *const lv = lv_c - (dc) * stride - (dr);          \
    const int stats = get_nz_mag_stride(lv, stride, TX_CLASS_VERT); \
    const int ctx_val = AOMMIN((stats + 1) >> 1, 4);                \
    cc[p] = (int8_t)(ctx_val + nz_map_ctx_offset_1d[row - (dr)]);   \
  } while (0)
    if (col != 0) PATCH_V(1, 0);
    if (row != 0) {
      PATCH_V(0, 1);
      if (row >= 2) PATCH_V(0, 2);
      if (row >= 3) PATCH_V(0, 3);
      if (row >= 4) PATCH_V(0, 4);
    }
#undef PATCH_V
  } else {
#define PATCH_H(dc, dr)                                              \
  do {                                                               \
    const int p = c - (dc) * step - (dr);                            \
    const uint8_t *const lv = lv_c - (dc) * stride - (dr);           \
    const int stats = get_nz_mag_stride(lv, stride, TX_CLASS_HORIZ); \
    const int ctx_val = AOMMIN((stats + 1) >> 1, 4);                 \
    cc[p] = (int8_t)(ctx_val + nz_map_ctx_offset_1d[col - (dc)]);    \
  } while (0)
    if (row != 0) PATCH_H(0, 1);
    if (col != 0) {
      PATCH_H(1, 0);
      if (col >= 2) PATCH_H(2, 0);
      if (col >= 3) PATCH_H(3, 0);
      if (col >= 4) PATCH_H(4, 0);
    }
#undef PATCH_H
  }
  return ret_rate;
}

static AOM_FORCE_INLINE int get_br_ctx_simple_fast(const uint8_t *const levels,
                                                   intptr_t c, int bhl,
                                                   intptr_t stride,
                                                   TX_CLASS tx_class) {
  const intptr_t col = c >> bhl;
  const uint8_t *lv = levels + c + (col << TX_PAD_HOR_LOG2);
  if (LIKELY(tx_class == TX_CLASS_2D)) {
    const int mag = AOMMIN((lv[1] + lv[stride] + lv[stride + 1] + 1) >> 1, 6);
    return mag + (((c & ~(stride - (TX_PAD_HOR - 1))) == 0) ? 7 : 14);
  }
  if (tx_class == TX_CLASS_VERT) {
    const int mag = AOMMIN((lv[1] + lv[2] + lv[stride] + 1) >> 1, 6);
    return mag + (((c & (stride - (TX_PAD_HOR + 1))) == 0) ? 7 : 14);
  }
  const int mag = AOMMIN((lv[1] + lv[stride] + lv[stride * 2] + 1) >> 1, 6);
  return mag + ((c < stride - TX_PAD_HOR) ? 7 : 14);
}

static AOM_FORCE_INLINE uint64_t pack_rate_cul(int rate, tran_low_t abs_val) {
  const unsigned int capped = (unsigned int)AOMMIN(abs_val, COEFF_CONTEXT_MASK);
  return (uint64_t)(uint32_t)rate | ((uint64_t)capped << 32);
}

static AOM_FORCE_INLINE int eval_better_qm_exact(tran_low_t tqc, tran_low_t dqc,
                                                 tran_low_t dqc_low,
                                                 int64_t scaled_qm, int rate,
                                                 int rate_low, int64_t rdmult) {
  const int64_t dist_diff = get_coeff_dist_scaled_qm(tqc, dqc, scaled_qm) -
                            get_coeff_dist_scaled_qm(tqc, dqc_low, scaled_qm);
  const int64_t rd_thresh =
      RDCOST(rdmult, rate_low, 0) - RDCOST(rdmult, rate, 0);
  return dist_diff * (1 << RDDIV_BITS) > rd_thresh;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static uint64_t finish_try_lower(intptr_t ci, unsigned int abs_qc, int rate,
                                 int rate_low, tran_low_t dqc_low,
                                 const TxbOptCtx *__restrict ctx) {
  const tran_low_t tqc = ctx->tcoeff[ci];
  const tran_low_t dqc = ctx->dqcoeff[ci];
  const int sign_mask = tqc >> 31;
  if (UNLIKELY(abs_qc >= 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE)) {
    const tran_low_t full_abs = (tran_low_t)abs_qc;
    const int br_ctx = get_br_ctx_simple_fast(
        ctx->levels, ci, ctx->bhl, ctx->stride, (TX_CLASS)ctx->tx_class);
    const int *const lps_flat = ctx->txb_costs->lps_cost[br_ctx];
    const int base_rate = rate;
    const int full_rate = base_rate + get_br_cost_fast(full_abs, lps_flat);
    if ((dqc ^ sign_mask) < (tqc ^ sign_mask)) {
      return pack_rate_cul(full_rate, full_abs);
    }
    const tran_low_t abs_qc_low = full_abs - 1;
    const int full_rate_low =
        base_rate + get_br_cost_fast(abs_qc_low, lps_flat);
    int dqv = ctx->dequant_ac;
    const qm_val_t *const iqmatrix = ctx->iqmatrix;
    if (iqmatrix != NULL) {
      dqv = ((iqmatrix[ci] * dqv) + (1 << (AOM_QM_BITS - 1))) >> AOM_QM_BITS;
    }
    const int shift = ctx->shift;
    const tran_low_t abs_dqc_low = (abs_qc_low * dqv) >> shift;
    const tran_low_t full_dqc_low = (abs_dqc_low ^ sign_mask) - sign_mask;
    const qm_val_t *const qmatrix = ctx->qmatrix;
    const int64_t rdmult = ctx->rdmult;
    int better;
    if (qmatrix != NULL) {
      const int64_t scaled_qm = (int64_t)qmatrix[ci] << shift;
      better = eval_better_qm_exact(tqc, dqc, full_dqc_low, scaled_qm,
                                    full_rate, full_rate_low, rdmult);
    } else {
      const int64_t dist_diff = ((int64_t)(dqc - full_dqc_low) *
                                 (dqc + full_dqc_low - ((int64_t)tqc * 2))) *
                                (1LL << (2 * shift));
      const int64_t rd_thresh =
          RDCOST(rdmult, full_rate_low, 0) - RDCOST(rdmult, full_rate, 0);
      better = dist_diff * (1 << RDDIV_BITS) > rd_thresh;
    }
    if (better) {
      ctx->qcoeff[ci] = (abs_qc_low ^ sign_mask) - sign_mask;
      ctx->dqcoeff[ci] = full_dqc_low;
      ctx->levels[get_padded_idx((int)ci, ctx->bhl)] =
          AOMMIN(abs_qc_low, INT8_MAX);
      return pack_rate_cul(full_rate_low, abs_qc_low);
    }
    return pack_rate_cul(full_rate, full_abs);
  }
  if ((dqc ^ sign_mask) >= (tqc ^ sign_mask)) {
    const int shift = ctx->shift;
    const int64_t rdmult = ctx->rdmult;
    const int64_t approx_rd =
        ((int64_t)(rate - rate_low) * rdmult) >> AV1_PROB_COST_SHIFT;
    const int64_t sum_term = (int64_t)tqc * 2 - (int64_t)dqc - (int64_t)dqc_low;
    const qm_val_t *const qmatrix = ctx->qmatrix;
    int better;
    if (qmatrix != NULL) {
      const int64_t scaled_qm = (int64_t)qmatrix[ci] << shift;
      const int64_t sq_qm = scaled_qm * scaled_qm;
      const int64_t approx_dd =
          (((int64_t)(dqc - dqc_low) * sum_term) * sq_qm) >> (2 * AOM_QM_BITS);
      const int64_t margin = approx_rd - (approx_dd * (1 << RDDIV_BITS));
      better =
          (margin >= 0) && (LIKELY(margin > (1 << RDDIV_BITS)) ||
                            eval_better_qm_exact(tqc, dqc, dqc_low, scaled_qm,
                                                 rate, rate_low, rdmult));
    } else {
      const int64_t dd_pos = ((int64_t)(dqc - dqc_low) * sum_term) *
                             (1LL << (2 * shift + RDDIV_BITS));
      better = (approx_rd >= dd_pos) && (LIKELY(approx_rd > dd_pos) ||
                                         -dd_pos > RDCOST(rdmult, rate_low, 0) -
                                                       RDCOST(rdmult, rate, 0));
    }
    if (better) {
      const unsigned int abs_qc_low = abs_qc - 1;
      ctx->qcoeff[ci] = ((tran_low_t)abs_qc_low ^ sign_mask) - sign_mask;
      ctx->dqcoeff[ci] = dqc_low;
      ctx->levels[ci + ((ci >> ctx->bhl) << TX_PAD_HOR_LOG2)] =
          (uint8_t)abs_qc_low;
      const int out_rate = (abs_qc <= 3)
                               ? patch_ctx_neighbors((int)ci, ctx, rate_low)
                               : rate_low;
      return (uint64_t)(uint32_t)out_rate | ((uint64_t)abs_qc_low << 32);
    }
  }
  return (uint64_t)(uint32_t)rate | ((uint64_t)abs_qc << 32);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static uint64_t update_coeff_simple_gt1_try_lower(
    intptr_t ci, unsigned int abs_qc, const int *bc4,
    const TxbOptCtx *__restrict ctx) {
  if (UNLIKELY(abs_qc >= 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE)) {
    return finish_try_lower(ci, abs_qc, bc4[3], 0, 0, ctx);
  }
  const int br_ctx = get_br_ctx_simple_fast(
      ctx->levels, ci, ctx->bhl, ctx->stride, (TX_CLASS)ctx->tx_class);
  const int *const lps_flat = ctx->txb_costs->lps_cost[br_ctx];
  const int base_rate = bc4[3];
  int rate_m1 =
      base_rate + lps_flat[((int)abs_qc - (2 + NUM_BASE_LEVELS)) & 15];
  const int rate = base_rate + lps_flat[abs_qc - (1 + NUM_BASE_LEVELS)];
  int c2 = bc4[2];
  const int rate_low = (abs_qc == 3) ? c2 : rate_m1;

  const tran_low_t tqc = ctx->tcoeff[ci];
  const tran_low_t dqc = ctx->dqcoeff[ci];
  int dqv = ctx->dequant_ac;
  const qm_val_t *const iqmatrix = ctx->iqmatrix;
  if (iqmatrix != NULL) {
    dqv = ((iqmatrix[ci] * dqv) + (1 << (AOM_QM_BITS - 1))) >> AOM_QM_BITS;
  }
  const int shift = ctx->shift;
  const int sign_mask = tqc >> 31;
  const tran_low_t abs_dqc_low =
      (tran_low_t)(((int)(abs_qc - 1) * dqv) >> shift);
  const tran_low_t dqc_low = (abs_dqc_low ^ sign_mask) - sign_mask;
  const int64_t sum_term = (int64_t)tqc * 2 - (int64_t)dqc - (int64_t)dqc_low;
  const int64_t approx_rd =
      ((int64_t)(rate - rate_low) * ctx->rdmult) >> AV1_PROB_COST_SHIFT;
  const qm_val_t *const qmatrix = ctx->qmatrix;
  int64_t thresh_dd;
  if (qmatrix != NULL) {
    const int64_t scaled_qm = (int64_t)qmatrix[ci] << shift;
    const int64_t sq_qm = scaled_qm * scaled_qm;
    thresh_dd =
        ((((int64_t)(dqc - dqc_low) * sum_term) * sq_qm) >> (2 * AOM_QM_BITS)) *
        (1 << RDDIV_BITS);
  } else {
    thresh_dd = ((int64_t)(dqc - dqc_low) * sum_term) *
                (1LL << (2 * shift + RDDIV_BITS));
  }
  if (UNLIKELY(approx_rd >= thresh_dd)) {
    return finish_try_lower(ci, abs_qc, rate, rate_low, dqc_low, ctx);
  }
  return (uint64_t)(uint32_t)rate | ((uint64_t)abs_qc << 32);
}

static AOM_FORCE_INLINE uint64_t update_coeff_simple_eq2_qm(
    intptr_t ci, const int *bc4, const TxbOptCtx *__restrict ctx) {
  const tran_low_t tqc = ctx->tcoeff[ci];
  const tran_low_t dqc = ctx->dqcoeff[ci];
  const int raw_dqv =
      (ctx->iqmatrix[ci] * ctx->dequant_ac) + (1 << (AOM_QM_BITS - 1));
  const int shift = ctx->shift;
  const int64_t scaled_qm = (int64_t)ctx->qmatrix[ci] << shift;
  const int64_t sq_qm = scaled_qm * scaled_qm;
  const int sign_mask = tqc >> 31;
  const tran_low_t dqc_low =
      ((raw_dqv >> (AOM_QM_BITS + shift)) ^ sign_mask) - sign_mask;
  const int64_t sum_term = (int64_t)tqc * 2 - (int64_t)dqc - (int64_t)dqc_low;
  const int64_t approx_dd =
      (((int64_t)(dqc - dqc_low) * sum_term) * sq_qm) >> (2 * AOM_QM_BITS);
  const int rate = bc4[2];
  const int rate_low = bc4[1];
  const int64_t approx_rd =
      ((int64_t)(rate - rate_low) * ctx->rdmult) >> AV1_PROB_COST_SHIFT;
  if (UNLIKELY(approx_rd >= approx_dd * (1 << RDDIV_BITS))) {
    return finish_try_lower(ci, 2, rate, rate_low, dqc_low, ctx);
  }
  return (uint64_t)(uint32_t)rate | (2ULL << 32);
}

static AOM_FORCE_INLINE uint64_t update_coeff_simple_eq2_noqm(
    intptr_t ci, const int *bc4, const TxbOptCtx *__restrict ctx) {
  const tran_low_t tqc = ctx->tcoeff[ci];
  const tran_low_t dqc = ctx->dqcoeff[ci];
  int dqv = ctx->dequant_ac;
  if (UNLIKELY(ctx->iqmatrix != NULL)) {
    dqv = ((ctx->iqmatrix[ci] * dqv) + (1 << (AOM_QM_BITS - 1))) >> AOM_QM_BITS;
  }
  const int shift = ctx->shift;
  const int sign_mask = tqc >> 31;
  const tran_low_t dqc_low = ((dqv >> shift) ^ sign_mask) - sign_mask;
  const int rate = bc4[2];
  const int rate_low = bc4[1];
  if (UNLIKELY(ctx->qmatrix != NULL)) {
    return finish_try_lower(ci, 2, rate, rate_low, dqc_low, ctx);
  }
  const int64_t sum_term = (int64_t)tqc * 2 - (int64_t)dqc - (int64_t)dqc_low;
  const int64_t dd_pos =
      ((int64_t)(dqc - dqc_low) * sum_term) * (1LL << (2 * shift + RDDIV_BITS));
  const int64_t approx_rd =
      ((int64_t)(rate - rate_low) * ctx->rdmult) >> AV1_PROB_COST_SHIFT;
  if (UNLIKELY(approx_rd >= dd_pos)) {
    return finish_try_lower(ci, 2, rate, rate_low, dqc_low, ctx);
  }
  return (uint64_t)(uint32_t)rate | (2ULL << 32);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int update_coeff_simple_nz(intptr_t ci, tran_low_t qc,
                                  intptr_t coeff_ctx,
                                  const TxbOptCtx *__restrict ctx) {
  const LV_MAP_COEFF_COST *__restrict txb_costs = ctx->txb_costs;
  const int *const base_cost = txb_costs->base_cost[coeff_ctx];
  if ((unsigned int)(qc + 1) > 2) {
    const unsigned int abs_qc = (unsigned int)abs(qc);
    if (abs_qc == 2) {
      return (int)(uint32_t)update_coeff_simple_eq2_noqm(ci, &base_cost[4],
                                                         ctx);
    }
    return (int)(uint32_t)update_coeff_simple_gt1_try_lower(ci, abs_qc,
                                                            &base_cost[4], ctx);
  }
  const tran_low_t abs_tqc = abs(ctx->tcoeff[ci]);
  const tran_low_t abs_dqc = abs(ctx->dqcoeff[ci]);
  const int rate = base_cost[5];
  if (abs_dqc < abs_tqc) return rate;

  const qm_val_t *__restrict qmatrix = ctx->qmatrix;
  const int shift = ctx->shift;
  int64_t dist_diff_0;
  if (qmatrix == NULL) {
    dist_diff_0 = ((int64_t)abs_dqc * (abs_dqc - ((int64_t)abs_tqc * 2))) *
                  (1LL << (2 * shift));
  } else {
    const int64_t scaled_qm = (int64_t)qmatrix[ci] << shift;
    const int64_t dist0 = get_coeff_dist_scaled_qm(abs_tqc, 0, scaled_qm);
    dist_diff_0 = get_coeff_dist_scaled_qm(abs_tqc, abs_dqc, scaled_qm) - dist0;
  }
  const int rate_low = base_cost[0];
  const int64_t rdmult = ctx->rdmult;
  const int64_t rd = RDCOST(rdmult, rate, dist_diff_0);
  const int64_t rd_low = RDCOST(rdmult, rate_low, 0);
  if (rd_low < rd) {
    ctx->qcoeff[ci] = 0;
    ctx->dqcoeff[ci] = 0;
    ctx->levels[get_padded_idx((int)ci, ctx->bhl)] = 0;
    return patch_ctx_neighbors((int)ci, ctx, rate_low);
  }
  return rate;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int update_coeff_eob_gt1(int si, int ci, tran_low_t qc, int coeff_ctx,
                                TX_CLASS tx_class, TxbOptCtx *__restrict ctx) {
  const LV_MAP_COEFF_COST *__restrict txb_costs = ctx->txb_costs;
  const tran_low_t abs_qc = abs(qc);
  const tran_low_t abs_qc_low = abs_qc - 1;
  const int bhl = ctx->bhl;
  const intptr_t stride = ctx->stride;
  uint8_t *__restrict levels = ctx->levels;
  const int sign = (qc < 0) ? 1 : 0;
  const int sign_cost = UNLIKELY(ci == 0)
                            ? txb_costs->dc_sign_cost[ctx->dc_sign_ctx][sign]
                            : av1_cost_literal(1);
  const int *const base_cost = txb_costs->base_cost[coeff_ctx];
  const tran_low_t tqc = ctx->tcoeff[ci];
  const tran_low_t dqc = ctx->dqcoeff[ci];
  const int shift = ctx->shift;
  int rate, rate_low;
  if (LIKELY(abs_qc == 2)) {
    rate = base_cost[2] + sign_cost;
    rate_low = base_cost[1] + sign_cost;
  } else {
    const int br_ctx = get_br_ctx_stride(levels, ci, bhl, stride, tx_class);
    const int *const lps_cost = txb_costs->lps_cost[br_ctx];
    const int base_rate = base_cost[3] + sign_cost;
    if (LIKELY(abs_qc < 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE)) {
      rate = base_rate + lps_cost[abs_qc - (1 + NUM_BASE_LEVELS)];
      rate_low = (abs_qc == 3)
                     ? (base_cost[2] + sign_cost)
                     : (base_rate + lps_cost[abs_qc - (2 + NUM_BASE_LEVELS)]);
    } else {
      rate = base_rate + get_br_cost_fast(abs_qc, lps_cost);
      rate_low = base_rate + get_br_cost_fast(abs_qc_low, lps_cost);
    }
  }
  int dqv = UNLIKELY(ci == 0) ? ctx->dequant[0] : ctx->dequant_ac;
  if (LIKELY(ctx->iqmatrix != NULL)) {
    dqv = ((ctx->iqmatrix[ci] * dqv) + (1 << (AOM_QM_BITS - 1))) >> AOM_QM_BITS;
  }
  tran_low_t qc_low, dqc_low;
  get_qc_dqc_low(abs_qc, sign, dqv, shift, &qc_low, &dqc_low);

  const qm_val_t *__restrict qmatrix = ctx->qmatrix;
  int64_t dist, dist_low;
  if (LIKELY(qmatrix != NULL)) {
    const int64_t scaled_qm = (int64_t)qmatrix[ci] << shift;
    const int64_t dist0 = get_coeff_dist_scaled_qm(tqc, 0, scaled_qm);
    dist = get_coeff_dist_scaled_qm(tqc, dqc, scaled_qm) - dist0;
    dist_low = get_coeff_dist_scaled_qm(tqc, dqc_low, scaled_qm) - dist0;
  } else {
    const int64_t tqc2 = (int64_t)tqc * 2;
    dist = ((int64_t)dqc * (dqc - tqc2)) * (1LL << (2 * shift));
    dist_low = ((int64_t)dqc_low * (dqc_low - tqc2)) * (1LL << (2 * shift));
  }

  const int64_t dist_orig = dist;
  const int64_t rdmult = ctx->rdmult;
  int accu_rate = ctx->accu_rate;
  const int64_t accu_dist = ctx->accu_dist;
  int64_t rd = RDCOST(rdmult, accu_rate + rate, accu_dist + dist);
  int lower_level = 0;

  const int min_eob_cutoff = ctx->min_eob_cutoff;
  const int sharpness = ctx->sharpness;
  const int qc_threshold = (si <= min_eob_cutoff) ? 2 : 1;
  const int allow_lower_qc = sharpness ? (abs_qc > qc_threshold) : 1;
  if (allow_lower_qc) {
    const int64_t rd_low =
        RDCOST(rdmult, accu_rate + rate_low, accu_dist + dist_low);
    if (rd_low < rd) {
      lower_level = 1;
      rd = rd_low;
      rate = rate_low;
      dist = dist_low;
    }
  }

  const int new_eob = si + 1;
  if (sharpness == 0 || new_eob >= min_eob_cutoff) {
    const int coeff_ctx_new_eob = AOMMIN((si - 1) >> ctx->eob_shift, 2) + 1;
    const int new_eob_cost =
        get_eob_cost_tab(new_eob, ctx->eob_cost, txb_costs);
    const int *const base_eob_cost =
        txb_costs->base_eob_cost[coeff_ctx_new_eob];
    int rate_coeff_eob, rate_coeff_eob_low;
    if (LIKELY(abs_qc == 2)) {
      rate_coeff_eob = new_eob_cost + base_eob_cost[1] + sign_cost;
      rate_coeff_eob_low = new_eob_cost + base_eob_cost[0] + sign_cost;
    } else {
      const int br_ctx_eob = get_br_ctx_eob(ci, bhl, tx_class);
      const int *const lps_cost_eob = txb_costs->lps_cost[br_ctx_eob];
      const int base_eob_rate = new_eob_cost + base_eob_cost[2] + sign_cost;
      if (LIKELY(abs_qc < 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE)) {
        rate_coeff_eob =
            base_eob_rate + lps_cost_eob[abs_qc - (1 + NUM_BASE_LEVELS)];
        rate_coeff_eob_low =
            (abs_qc == 3) ? (new_eob_cost + base_eob_cost[1] + sign_cost)
                          : (base_eob_rate +
                             lps_cost_eob[abs_qc - (2 + NUM_BASE_LEVELS)]);
      } else {
        rate_coeff_eob = base_eob_rate + get_br_cost_fast(abs_qc, lps_cost_eob);
        rate_coeff_eob_low =
            base_eob_rate + get_br_cost_fast(abs_qc_low, lps_cost_eob);
      }
    }
    int lower_level_new_eob = 0;
    int64_t dist_new_eob = dist_orig;
    int64_t rd_new_eob = RDCOST(rdmult, rate_coeff_eob, dist_new_eob);
    const int64_t rd_new_eob_low = RDCOST(rdmult, rate_coeff_eob_low, dist_low);
    if (rd_new_eob_low < rd_new_eob) {
      lower_level_new_eob = 1;
      rd_new_eob = rd_new_eob_low;
      rate_coeff_eob = rate_coeff_eob_low;
      dist_new_eob = dist_low;
    }

    if (rd_new_eob < rd) {
      tran_low_t *__restrict qcoeff = ctx->qcoeff;
      tran_low_t *__restrict dqcoeff = ctx->dqcoeff;
      const int *__restrict nz_ci = ctx->nz_ci;
      const int nz_num = ctx->nz_num;
      for (int ni = 0; ni < nz_num; ++ni) {
        int last_ci = nz_ci[ni];
        levels[get_padded_idx(last_ci, bhl)] = 0;
        qcoeff[last_ci] = 0;
        dqcoeff[last_ci] = 0;
      }
      ctx->eob = new_eob;
      ctx->nz_num = 0;
      accu_rate = rate_coeff_eob;
      ctx->accu_dist = dist_new_eob;
      lower_level = lower_level_new_eob;
    } else {
      accu_rate += rate;
      ctx->accu_dist = accu_dist + dist;
    }
  } else {
    accu_rate += rate;
    ctx->accu_dist = accu_dist + dist;
  }

  ctx->nz_ci[ctx->nz_num] = ci;
  ++ctx->nz_num;
  if (lower_level) {
    ctx->qcoeff[ci] = qc_low;
    ctx->dqcoeff[ci] = dqc_low;
    levels[get_padded_idx(ci, bhl)] = AOMMIN(abs_qc_low, INT8_MAX);
    return (abs_qc <= 3) ? ~accu_rate : accu_rate;
  }
  return accu_rate;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int update_coeff_eob_eq1(int si, int ci, int coeff_ctx,
                                TxbOptCtx *__restrict ctx) {
  const LV_MAP_COEFF_COST *__restrict txb_costs = ctx->txb_costs;
  const int sign_cost =
      UNLIKELY(ci == 0)
          ? txb_costs->dc_sign_cost[ctx->dc_sign_ctx][ctx->qcoeff[0] < 0]
          : av1_cost_literal(1);
  const int *const base_cost = txb_costs->base_cost[coeff_ctx];
  int rate = base_cost[1] + sign_cost;
  const int bhl = ctx->bhl;
  const int new_eob = si + 1;
  const int coeff_ctx_new_eob = AOMMIN((si - 1) >> ctx->eob_shift, 2) + 1;
  const int new_eob_cost = get_eob_cost_tab(new_eob, ctx->eob_cost, txb_costs);
  const int rate_coeff_eob =
      new_eob_cost + txb_costs->base_eob_cost[coeff_ctx_new_eob][0] + sign_cost;
  const int64_t rdmult = ctx->rdmult;
  int accu_rate = ctx->accu_rate;
  const int64_t accu_dist = ctx->accu_dist;

  const tran_low_t tqc = ctx->tcoeff[ci];
  const tran_low_t dqc = ctx->dqcoeff[ci];
  const int shift = ctx->shift;
  const qm_val_t *__restrict qmatrix = ctx->qmatrix;
  int64_t dist;
  if (LIKELY(qmatrix != NULL)) {
    const int64_t scaled_qm = (int64_t)qmatrix[ci] << shift;
    const int64_t dist0 = get_coeff_dist_scaled_qm(tqc, 0, scaled_qm);
    dist = get_coeff_dist_scaled_qm(tqc, dqc, scaled_qm) - dist0;
  } else {
    const int64_t tqc2 = (int64_t)tqc * 2;
    dist = ((int64_t)dqc * (dqc - tqc2)) * (1LL << (2 * shift));
  }

  const int64_t dist_orig = dist;
  int64_t rd = RDCOST(rdmult, accu_rate + rate, accu_dist + dist);
  int lower_level = 0;

  if (UNLIKELY(ctx->sharpness == 0)) {
    const int rate_low = base_cost[0];
    const int64_t rd_low = RDCOST(rdmult, accu_rate + rate_low, accu_dist);
    if (rd_low < rd) {
      lower_level = 1;
      rd = rd_low;
      rate = rate_low;
      dist = 0;
    }
  }

  const int64_t rd_new_eob = RDCOST(rdmult, rate_coeff_eob, dist_orig);
  uint8_t *__restrict levels = ctx->levels;
  if (rd_new_eob < rd) {
    tran_low_t *__restrict qcoeff = ctx->qcoeff;
    tran_low_t *__restrict dqcoeff = ctx->dqcoeff;
    const int *__restrict nz_ci = ctx->nz_ci;
    const int nz_num = ctx->nz_num;
    for (int ni = 0; ni < nz_num; ++ni) {
      int last_ci = nz_ci[ni];
      levels[get_padded_idx(last_ci, bhl)] = 0;
      qcoeff[last_ci] = 0;
      dqcoeff[last_ci] = 0;
    }
    ctx->eob = new_eob;
    ctx->nz_num = 1;
    ctx->nz_ci[0] = ci;
    ctx->accu_dist = dist_orig;
    return rate_coeff_eob;
  }

  accu_rate += rate;
  ctx->accu_dist = accu_dist + dist;
  if (lower_level) {
    ctx->qcoeff[ci] = 0;
    ctx->dqcoeff[ci] = 0;
    levels[get_padded_idx(ci, bhl)] = 0;
    return ~accu_rate;
  }
  ctx->nz_ci[ctx->nz_num] = ci;
  ++ctx->nz_num;
  return accu_rate;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int update_coeff_eob_nz(int si, int ci, tran_low_t qc, int coeff_ctx,
                               TxbOptCtx *__restrict ctx) {
  const int old_eob = ctx->eob;
  const int old_nz_num = ctx->nz_num;
  const int old_nz0 = ctx->nz_ci[0];
  const int old_nz1 = (old_nz_num > 1) ? ctx->nz_ci[1] : 0;
  int rate = ((unsigned int)(qc + 1) > 2)
                 ? update_coeff_eob_gt1(si, ci, qc, coeff_ctx,
                                        (TX_CLASS)ctx->tx_class, ctx)
                 : update_coeff_eob_eq1(si, ci, coeff_ctx, ctx);
  if (UNLIKELY(ctx->eob != old_eob) && LIKELY(ci != 0)) {
    if (old_nz_num > 0) (void)patch_ctx_neighbors(old_nz0, ctx, 0);
    if (old_nz_num > 1) (void)patch_ctx_neighbors(old_nz1, ctx, 0);
  }
  if (UNLIKELY(rate < 0)) {
    rate = ~rate;
    if (LIKELY(ci != 0)) return patch_ctx_neighbors(ci, ctx, rate);
  }
  return rate;
}

static inline void update_skip(int *accu_rate, int64_t accu_dist, int *eob,
                               int nz_num, int *nz_ci, int64_t rdmult,
                               int skip_cost, int non_skip_cost,
                               tran_low_t *qcoeff, tran_low_t *dqcoeff) {
  const int64_t rd = RDCOST(rdmult, *accu_rate + non_skip_cost, accu_dist);
  const int64_t rd_new_eob = RDCOST(rdmult, skip_cost, 0);
  if (rd_new_eob < rd) {
    for (int i = 0; i < nz_num; ++i) {
      const int ci = nz_ci[i];
      qcoeff[ci] = 0;
      dqcoeff[ci] = 0;
    }
    *accu_rate = 0;
    *eob = 0;
  }
}

static inline int get_tx_type_cost_mbmi(const MACROBLOCK *x,
                                        const MACROBLOCKD *xd,
                                        const MB_MODE_INFO *mbmi, int is_inter,
                                        int plane, TX_SIZE tx_size,
                                        TX_TYPE tx_type,
                                        int reduced_tx_set_used) {
  if (plane > 0) return 0;

  const TX_SIZE square_tx_size = txsize_sqr_map[tx_size];
  const TxSetType set_type =
      av1_get_ext_tx_set_type(tx_size, is_inter, reduced_tx_set_used);
  if (av1_num_ext_tx_set[set_type] > 1 && !xd->lossless[mbmi->segment_id]) {
    const int ext_tx_set = ext_tx_set_index[is_inter][set_type];
    if (ext_tx_set > 0) {
      if (is_inter) {
        return x->mode_costs
            .inter_tx_type_costs[ext_tx_set][square_tx_size][tx_type];
      }
      const PREDICTION_MODE intra_dir =
          mbmi->filter_intra_mode_info.use_filter_intra
              ? fimode_to_intradir[mbmi->filter_intra_mode_info
                                       .filter_intra_mode]
              : mbmi->mode;
      return x->mode_costs
          .intra_tx_type_costs[ext_tx_set][square_tx_size][intra_dir][tx_type];
    }
  }
  return 0;
}

static inline int get_tx_type_cost(const MACROBLOCK *x, const MACROBLOCKD *xd,
                                   int plane, TX_SIZE tx_size, TX_TYPE tx_type,
                                   int reduced_tx_set_used) {
  if (plane > 0) return 0;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  return get_tx_type_cost_mbmi(x, xd, mbmi, is_inter_block(mbmi), plane,
                               tx_size, tx_type, reduced_tx_set_used);
}

static AOM_FORCE_INLINE int64_t calc_coeff_dist0(intptr_t ci,
                                                 const tran_low_t *tcoeff,
                                                 const tran_low_t *dqcoeff,
                                                 const qm_val_t *qmatrix,
                                                 int shift) {
  const tran_low_t tqc = tcoeff[ci];
  const tran_low_t dqc = dqcoeff[ci];
  if (LIKELY(qmatrix != NULL)) {
    const int64_t scaled_qm = (int64_t)qmatrix[ci] << shift;
    const int64_t dist0 = get_coeff_dist_scaled_qm(tqc, 0, scaled_qm);
    return get_coeff_dist_scaled_qm(tqc, dqc, scaled_qm) - dist0;
  }
  return ((int64_t)dqc * (dqc - (int64_t)tqc * 2)) * (1LL << (2 * shift));
}

// Lower-level contexts are read from cc[] (precomputed by
// av1_get_nz_map_contexts) and cc[] is kept in sync whenever
// update_coeff_eob_nz() modifies levels[].
static AOM_FORCE_INLINE void update_coeff_eob_facade(
    int *si, const int16_t *scan, const LV_MAP_COEFF_COST *txb_costs,
    const tran_low_t *qcoeff, int max_nz_num, int sharpness, const int8_t *cc,
    TxbOptCtx *ctx) {
  int s = *si;
  int local_rate = ctx->accu_rate;
  int nz_num = ctx->nz_num;
  const int *local_base_cost4 = &txb_costs->base_cost[0][4];
  const tran_low_t *local_qcoeff = qcoeff;
  if (LIKELY(sharpness != 0)) {
    const int min_s_phase1 = ctx->min_eob_cutoff - 1;
    const int16_t *scan_p = scan + s;
    const int16_t *const p1_end = scan + min_s_phase1;
    if (scan_p >= p1_end) {
      int64_t local_dist = 0;
      int need_last_dist = 1;
      do {
        intptr_t ci;
        tran_low_t qc;
        for (;;) {
          ci = *scan_p;
          qc = local_qcoeff[ci];
          if (qc != 0) break;
          local_rate += local_base_cost4[(size_t)(uint8_t)cc[ci] << 3];
          if (--scan_p < p1_end) goto phase1_done;
        }
        if (need_last_dist) {
          need_last_dist = 0;
          local_dist = calc_coeff_dist0(ctx->nz_ci[0], ctx->tcoeff,
                                        ctx->dqcoeff, ctx->qmatrix, ctx->shift);
        }
        const int cur_s = (int)(scan_p - scan);
        const int coeff_ctx = (uint8_t)cc[ci];
        if (LIKELY((unsigned int)(qc + 1) <= 2)) {
          const int coeff_ctx_new_eob =
              AOMMIN((cur_s - 1) >> ctx->eob_shift, 2) + 1;
          const int k = get_msb(cur_s);
          const int bit = (cur_s >> (k - 1)) & 1;
          const int rate_coeff_eob =
              ctx->eob_cost[k + 1] + txb_costs->eob_extra_cost[k - 1][bit] +
              txb_costs->base_eob_cost[coeff_ctx_new_eob][0] +
              av1_cost_literal(k);
          const int rate_keep =
              local_rate + local_base_cost4[((size_t)coeff_ctx << 3) + 1];
          const int64_t rdmult = ctx->rdmult;
          const int64_t approx_dr =
              ((int64_t)(rate_coeff_eob - rate_keep) * rdmult) >>
              AV1_PROB_COST_SHIFT;
          const int64_t d_eob = approx_dr - local_dist * (1 << RDDIV_BITS);
          if (LIKELY(d_eob >= 0) ||
              (UNLIKELY(d_eob == -1) &&
               RDCOST(rdmult, rate_coeff_eob, 0) >=
                   RDCOST(rdmult, rate_keep, local_dist))) {
            if (nz_num < 2 && scan_p > p1_end) {
              local_dist += calc_coeff_dist0(ci, ctx->tcoeff, ctx->dqcoeff,
                                             ctx->qmatrix, ctx->shift);
            }
            ctx->nz_ci[nz_num] = (int)ci;
            ++nz_num;
            local_rate = rate_keep;
            --scan_p;
            continue;
          }
        }
        ctx->accu_rate = local_rate;
        ctx->accu_dist = local_dist;
        ctx->nz_num = nz_num;
        local_rate = update_coeff_eob_nz(cur_s, (int)ci, qc, coeff_ctx, ctx);
        local_dist = ctx->accu_dist;
        nz_num = ctx->nz_num;
        --scan_p;
      } while (scan_p >= p1_end && nz_num <= max_nz_num);
    }
  phase1_done:;
    const int16_t *const p2_end = scan + 1;
    int *const nz_ci = ctx->nz_ci;
    while (scan_p >= p2_end && nz_num <= max_nz_num) {
      const intptr_t ci = *scan_p;
      const tran_low_t qc = local_qcoeff[ci];
      const unsigned int abs_qc = (unsigned int)abs(qc);
      if (UNLIKELY(abs_qc > 2)) {
        ctx->accu_rate = local_rate;
        ctx->nz_num = nz_num;
        local_rate = update_coeff_eob_nz((int)(scan_p - scan), (int)ci, qc,
                                         (uint8_t)cc[ci], ctx);
        nz_num = ctx->nz_num;
      } else {
        local_rate += local_base_cost4[((size_t)(uint8_t)cc[ci] << 3) + abs_qc];
        nz_ci[nz_num] = (int)ci;
        nz_num += (abs_qc != 0);
      }
      --scan_p;
    }
    s = (int)(scan_p - scan);
  } else {
    while (s >= 1 && nz_num <= max_nz_num) {
      intptr_t ci;
      tran_low_t qc;
      do {
        ci = scan[s];
        qc = local_qcoeff[ci];
        if (qc != 0) break;
        local_rate += local_base_cost4[(size_t)(uint8_t)cc[ci] << 3];
      } while (--s >= 1);
      if (s < 1) break;
      ctx->accu_rate = local_rate;
      ctx->nz_num = nz_num;
      local_rate = update_coeff_eob_nz(s, (int)ci, qc, (uint8_t)cc[ci], ctx);
      nz_num = ctx->nz_num;
      --s;
    }
  }
  ctx->accu_rate = local_rate;
  ctx->nz_num = nz_num;
  *si = s;
}

// Lower-level contexts are read from cc[] and cc[] is kept in sync whenever a
// coefficient level is lowered. Returns accumulated |qcoeff| over [1..si] for
// cul_level (each >2 coeff clamped to COEFF_CONTEXT_MASK).
static AOM_FORCE_INLINE unsigned int update_coeff_simple_facade(
    int si, const int16_t *scan, const LV_MAP_COEFF_COST *txb_costs,
    const tran_low_t *qcoeff, int sharpness, const int8_t *cc, TxbOptCtx *ctx) {
  const int16_t *scan_p = scan + si;
  const int16_t *scan_end = scan + 1;
  int local_rate = ctx->accu_rate;
  unsigned int simple_cul = 0;
  if (LIKELY(sharpness != 0)) {
    const int *local_base_cost4 = &txb_costs->base_cost[0][4];
    const tran_low_t *local_qcoeff = qcoeff;
    if (LIKELY(ctx->iqmatrix != NULL && ctx->qmatrix != NULL)) {
      for (;;) {
        intptr_t ci;
        tran_low_t qc;
        for (;;) {
          ci = *scan_p;
          qc = local_qcoeff[ci];
          if (UNLIKELY((unsigned int)(qc + 1) > 2)) break;
          const unsigned int is_nz = (unsigned int)(qc & 1);
          simple_cul += is_nz;
          local_rate +=
              local_base_cost4[((size_t)(uint8_t)cc[ci] << 3) + is_nz];
          if (--scan_p < scan_end) goto simple_done;
        }
        const int *const bc4 = &local_base_cost4[(size_t)(uint8_t)cc[ci] << 3];
        const unsigned int abs_qc = (unsigned int)abs(qc);
        const uint64_t packed =
            LIKELY(abs_qc == 2)
                ? update_coeff_simple_eq2_qm(ci, bc4, ctx)
                : update_coeff_simple_gt1_try_lower(ci, abs_qc, bc4, ctx);
        local_rate += (int)(uint32_t)packed;
        simple_cul += (unsigned int)(packed >> 32);
        if (--scan_p < scan_end) break;
      }
    } else {
      for (;;) {
        intptr_t ci;
        tran_low_t qc;
        for (;;) {
          ci = *scan_p;
          qc = local_qcoeff[ci];
          if (UNLIKELY((unsigned int)(qc + 1) > 2)) break;
          const unsigned int is_nz = (unsigned int)(qc & 1);
          simple_cul += is_nz;
          local_rate +=
              local_base_cost4[((size_t)(uint8_t)cc[ci] << 3) + is_nz];
          if (--scan_p < scan_end) goto simple_done;
        }
        const int *const bc4 = &local_base_cost4[(size_t)(uint8_t)cc[ci] << 3];
        const unsigned int abs_qc = (unsigned int)abs(qc);
        const uint64_t packed =
            LIKELY(abs_qc == 2)
                ? update_coeff_simple_eq2_noqm(ci, bc4, ctx)
                : update_coeff_simple_gt1_try_lower(ci, abs_qc, bc4, ctx);
        local_rate += (int)(uint32_t)packed;
        simple_cul += (unsigned int)(packed >> 32);
        if (--scan_p < scan_end) break;
      }
    }
  } else {
    const int (*local_base_cost)[8] = txb_costs->base_cost;
    const tran_low_t *local_qcoeff = qcoeff;
    for (;;) {
      intptr_t ci;
      tran_low_t qc;
      for (;;) {
        ci = *scan_p;
        qc = local_qcoeff[ci];
        if (qc != 0) break;
        local_rate += local_base_cost[(uint8_t)cc[ci]][0];
        if (--scan_p < scan_end) goto simple_done;
      }
      local_rate += update_coeff_simple_nz(ci, qc, (uint8_t)cc[ci], ctx);
      const unsigned int final_abs = (unsigned int)abs(local_qcoeff[ci]);
      simple_cul += AOMMIN(final_abs, COEFF_CONTEXT_MASK);
      if (--scan_p < scan_end) break;
    }
  }
simple_done:
  ctx->accu_rate = local_rate;
  return simple_cul;
}

#if CONFIG_AV1_HIGHBITDEPTH
static inline int64_t get_txb_rdmult_hbd(const MACROBLOCK *x,
                                         const MACROBLOCKD *xd,
                                         const MB_MODE_INFO *mbmi, int plane,
                                         int sharpness, int base_mult,
                                         int is_iq_tune,
                                         int *is_noise_pattern_out) {
  int is_noise_pattern = 0;
  if (sharpness == 3 && plane == AOM_PLANE_Y) {
    int64_t src_var, rec_var;
    av1_get_variance_stats(x, &src_var, &rec_var);
    if (src_var > rec_var) {
      const int num_pixels =
          block_size_wide[mbmi->bsize] * block_size_high[mbmi->bsize];
      if (src_var / num_pixels < 64) {
        is_noise_pattern = 1;
      }
    }
  }
  if (is_noise_pattern_out != NULL) *is_noise_pattern_out = is_noise_pattern;
  const int rshift = is_iq_tune ? 7 : (is_noise_pattern ? 6 : 5);
  return ROUND_POWER_OF_TWO(
      (int64_t)x->rdmult * (8 - sharpness) * (base_mult << (2 * (xd->bd - 8))),
      rshift);
}
#endif

static inline int64_t get_txb_rdmult_mbmi(
    const struct AV1_COMP *cpi, const MACROBLOCK *x, const MACROBLOCKD *xd,
    const MB_MODE_INFO *mbmi, int is_inter, int plane, PLANE_TYPE plane_type,
    int sharpness, int *is_noise_pattern_out) {
  const int (*trellis_rd_mult)[2] = cpi->sf.tx_sf.use_chroma_trellis_rd_mult
                                        ? plane_rd_mult_chroma
                                        : plane_rd_mult;
  const int base_mult = trellis_rd_mult[is_inter][plane_type];
  const int is_iq_tune = (cpi->oxcf.tune_cfg.tuning == AOM_TUNE_IQ ||
                          cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2);
#if CONFIG_AV1_HIGHBITDEPTH
  if (UNLIKELY(xd->bd > 8)) {
    return get_txb_rdmult_hbd(x, xd, mbmi, plane, sharpness, base_mult,
                              is_iq_tune, is_noise_pattern_out);
  }
#else
  (void)xd;
  (void)mbmi;
  (void)plane;
#endif
  if (is_noise_pattern_out != NULL) *is_noise_pattern_out = 0;
  const int64_t num = (int64_t)x->rdmult * (8 - sharpness) * base_mult;
  return is_iq_tune ? ROUND_POWER_OF_TWO(num, 7) : ROUND_POWER_OF_TWO(num, 5);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
int av1_optimize_txb_multi(const struct AV1_COMP *cpi, MACROBLOCK *x, int plane,
                           int block, TX_SIZE tx_size, TX_TYPE tx_type,
                           const TXB_CTX *const txb_ctx, int *rate_cost,
                           int sharpness) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblock_plane *p = &x->plane[plane];
  int eob = p->eobs[block];
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const int is_inter = is_inter_block(mbmi);
  const int seg_id = mbmi->segment_id;
  int is_noise_pattern = 0;
  const int64_t rdmult =
      get_txb_rdmult_mbmi(cpi, x, xd, mbmi, is_inter, plane, plane_type,
                          sharpness, &is_noise_pattern);
  const int16_t *const scan = get_scan(tx_size, tx_type)->scan;
  const int shift = av1_get_tx_scale(tx_size);
  const int16_t *const dequant = p->dequant_QTX;
  const qm_val_t *iqmatrix = NULL;
  const qm_val_t *qmatrix = NULL;
  if (LIKELY(tx_type < IDTX)) {
    iqmatrix = xd->plane[plane].seg_iqmatrix[seg_id][tx_size];
    if (LIKELY(cpi->oxcf.tune_cfg.dist_metric == AOM_DIST_METRIC_QM_PSNR)) {
      qmatrix = xd->plane[plane].seg_qmatrix[seg_id][tx_size];
    }
  }
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *const qcoeff = p->qcoeff + block_offset;
  tran_low_t *const dqcoeff = p->dqcoeff + block_offset;
  const tran_low_t *const tcoeff = p->coeff + block_offset;
  const CoeffCosts *const coeff_costs = &x->coeff_costs;

  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  const TX_CLASS tx_class = tx_type_to_class[tx_type];
  const int bhl = get_txb_bhl(tx_size);
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  const LV_MAP_COEFF_COST *txb_costs =
      &coeff_costs->coeff_costs[txs_ctx][plane_type];
  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const int eob_shift = eob_multi_size + 1;
  const LV_MAP_EOB_COST *txb_eob_costs =
      &coeff_costs->eob_costs[eob_multi_size][plane_type];
  const int *const eob_cost_tab =
      txb_eob_costs->eob_cost[tx_class != TX_CLASS_2D];

  const int non_skip_cost = txb_costs->txb_skip_cost[txb_ctx->txb_skip_ctx][0];
  const int non_skip_total_cost =
      non_skip_cost +
      get_tx_type_cost_mbmi(x, xd, mbmi, is_inter, plane, tx_size, tx_type,
                            cpi->common.features.reduced_tx_set_used);
  int accu_rate = get_eob_cost_tab(eob, eob_cost_tab, txb_costs);
  int64_t accu_dist = 0;
  int si = eob - 1;
  const int ci = scan[si];
  const tran_low_t qc = qcoeff[ci];
  tran_low_t last_abs_qc = abs(qc);
  const int sign = qc < 0;
  const int min_eob_cutoff = is_noise_pattern ? 8 : 5;
  const int need_accu_dist = (int)UNLIKELY(sharpness == 0);
  if (last_abs_qc >= 2) {
    last_abs_qc = update_coeff_last(&accu_rate, &accu_dist, si, ci, last_abs_qc,
                                    sign, tx_class, bhl, eob_shift, rdmult,
                                    shift, dequant, txb_costs, tcoeff, qcoeff,
                                    dqcoeff, iqmatrix, qmatrix, need_accu_dist);
  } else {
    assert(last_abs_qc == 1);
    const int coeff_ctx = AOMMIN((si - 1) >> eob_shift, 2) + 1;
    accu_rate += txb_costs->base_eob_cost[coeff_ctx][0] + av1_cost_literal(1);
    if (need_accu_dist) {
      accu_dist += calc_coeff_dist0(ci, tcoeff, dqcoeff, qmatrix, shift);
    }
  }
  --si;

  if (si <= 1 && LIKELY(sharpness != 0 && tx_class == TX_CLASS_2D)) {
    const tran_low_t qc_dc = qcoeff[0];
    if (LIKELY((unsigned int)(qc_dc + 2) <= 4)) {
      if (si == 0) {
        const unsigned int abs_dc = (unsigned int)abs(qc_dc);
        if (qc_dc == 0) {
          accu_rate += txb_costs->base_cost[0][0];
        } else {
          accu_rate += txb_costs->base_cost[0][abs_dc] +
                       txb_costs->dc_sign_cost[txb_ctx->dc_sign_ctx][qc_dc < 0];
        }
        const int dc_sign = ((qc_dc < 0) + ((qc_dc > 0) << 1))
                            << COEFF_CONTEXT_BITS;
        const unsigned int cul = abs_dc + (unsigned int)last_abs_qc;
        p->txb_entropy_ctx[block] =
            (uint8_t)(AOMMIN(cul, COEFF_CONTEXT_MASK) | dc_sign);
        *rate_cost = accu_rate + non_skip_total_cost;
        return 2;
      }
      const int ci1 = scan[1];
      const tran_low_t qc1 = qcoeff[ci1];
      if (LIKELY((unsigned int)(qc1 + 2) <= 4)) {
        const int dc = (ci >> bhl) - (ci1 >> bhl);
        const int dr = (ci & (height - 1)) - (ci1 & (height - 1));
        const int ctx_val1 =
            ((dc | dr) >= 0 && dc + dr <= 2) ? (1 + (last_abs_qc >= 3)) : 0;
        const int coeff_ctx1 = ctx_val1 + av1_nz_map_ctx_offset[tx_size][ci1];
        const unsigned int abs_qc1 = (unsigned int)abs(qc1);
        const unsigned int abs_dc = (unsigned int)abs(qc_dc);
        accu_rate += txb_costs->base_cost[coeff_ctx1][4 + abs_qc1];
        if (qc_dc == 0) {
          accu_rate += txb_costs->base_cost[0][0];
        } else {
          accu_rate += txb_costs->base_cost[0][abs_dc] +
                       txb_costs->dc_sign_cost[txb_ctx->dc_sign_ctx][qc_dc < 0];
        }
        const int dc_sign = ((qc_dc < 0) + ((qc_dc > 0) << 1))
                            << COEFF_CONTEXT_BITS;
        const unsigned int cul = abs_dc + (unsigned int)last_abs_qc + abs_qc1;
        p->txb_entropy_ctx[block] =
            (uint8_t)(AOMMIN(cul, COEFF_CONTEXT_MASK) | dc_sign);
        *rate_cost = accu_rate + non_skip_total_cost;
        return 3;
      }
    }
  }

  const int max_nz_num = 2;
  int nz_ci[4];
  nz_ci[0] = ci;
  const intptr_t stride = (1 << bhl) + TX_PAD_HOR;
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, height);
  int ac_nz_num = 1;
  unsigned int simple_cul = 0;
  if (UNLIKELY(si == 0 &&
               (qcoeff[0] == 0 ||
                (sharpness != 0 && (unsigned int)(qcoeff[0] + 2) <= 4)))) {
    av1_txb_init_levels(qcoeff, width, height, levels);
    const int coeff_ctx = get_lower_levels_ctx_dc(levels, stride, tx_class);
    const tran_low_t qc_dc = qcoeff[0];
    if (qc_dc == 0) {
      accu_rate += txb_costs->base_cost[coeff_ctx][0];
      if (sharpness == 0) {
        const int skip_cost =
            txb_costs->txb_skip_cost[txb_ctx->txb_skip_ctx][1];
        update_skip(&accu_rate, accu_dist, &eob, 1, nz_ci, rdmult, skip_cost,
                    non_skip_cost, qcoeff, dqcoeff);
      }
    } else {
      accu_rate += txb_costs->base_cost[coeff_ctx][abs(qc_dc)] +
                   txb_costs->dc_sign_cost[txb_ctx->dc_sign_ctx][qc_dc < 0];
    }
  } else {
    DECLARE_ALIGNED(16, int8_t, coeff_ctx_buf[32 * 32]);
    TxbOptCtx ctx = { rdmult,
                      accu_dist,
                      eob_cost_tab,
                      txb_costs,
                      dequant,
                      tcoeff,
                      qcoeff,
                      dqcoeff,
                      levels,
                      iqmatrix,
                      qmatrix,
                      nz_ci,
                      coeff_ctx_buf,
                      (tx_class == TX_CLASS_2D) ? av1_nz_map_ctx_offset[tx_size]
                                                : NULL,
                      stride,
                      eob,
                      1,
                      accu_rate,
                      bhl,
                      txb_ctx->dc_sign_ctx,
                      shift,
                      dequant[1],
                      (int16_t)sharpness,
                      (uint8_t)tx_class,
                      (uint8_t)eob_shift,
                      min_eob_cutoff };
    TxbOptCtx *ctx_ptr = &ctx;
    av1_txb_init_levels(qcoeff, width, height, levels);

    if (LIKELY(si >= 1)) {
      av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class,
                              coeff_ctx_buf);
      update_coeff_eob_facade(&si, scan, txb_costs, qcoeff, max_nz_num,
                              sharpness, coeff_ctx_buf, ctx_ptr);
      if (si >= 1) {
        simple_cul = update_coeff_simple_facade(
            si, scan, txb_costs, qcoeff, sharpness, coeff_ctx_buf, ctx_ptr);
      }
      ac_nz_num = ctx_ptr->nz_num;
    }

    if (ac_nz_num <= max_nz_num) {
      const LV_MAP_COEFF_COST *const dc_costs = ctx_ptr->txb_costs;
      const int coeff_ctx = get_lower_levels_ctx_dc(
          ctx_ptr->levels, ctx_ptr->stride, (TX_CLASS)ctx_ptr->tx_class);
      const tran_low_t qc_dc = ctx_ptr->qcoeff[0];
      if (qc_dc == 0) {
        ctx_ptr->accu_rate += dc_costs->base_cost[coeff_ctx][0];
      } else if (ctx_ptr->sharpness != 0 && (unsigned int)(qc_dc + 2) <= 4) {
        ctx_ptr->accu_rate +=
            dc_costs->base_cost[coeff_ctx][abs(qc_dc)] +
            dc_costs->dc_sign_cost[ctx_ptr->dc_sign_ctx][qc_dc < 0];
      } else {
        ctx_ptr->accu_rate =
            update_coeff_eob_nz(0, 0, qc_dc, coeff_ctx, ctx_ptr);
      }
      if (UNLIKELY(ctx_ptr->sharpness == 0) && ctx_ptr->nz_num <= max_nz_num) {
        const int skip_cost =
            ctx_ptr->txb_costs->txb_skip_cost[txb_ctx->txb_skip_ctx][1];
        update_skip(&ctx_ptr->accu_rate, ctx_ptr->accu_dist, &ctx_ptr->eob,
                    ctx_ptr->nz_num, nz_ci, ctx_ptr->rdmult, skip_cost,
                    non_skip_cost, ctx_ptr->qcoeff, ctx_ptr->dqcoeff);
      }
    } else {
      update_coeff_dc(ctx_ptr);
    }
    accu_rate = ctx_ptr->accu_rate;
    eob = ctx_ptr->eob;
  }

  if (UNLIKELY(eob == 0)) {
    accu_rate += txb_costs->txb_skip_cost[txb_ctx->txb_skip_ctx][1];
    p->eobs[block] = 0;
    p->txb_entropy_ctx[block] = 0;
  } else {
    accu_rate += non_skip_total_cost;
    p->eobs[block] = eob;
    const tran_low_t dc_val = qcoeff[0];
    const int dc_sign = ((dc_val < 0) + ((dc_val > 0) << 1))
                        << COEFF_CONTEXT_BITS;
    unsigned int cul_level = (unsigned int)abs(dc_val) + simple_cul;
    if (eob > 1 && cul_level < COEFF_CONTEXT_MASK) {
      cul_level += (unsigned int)abs(qcoeff[nz_ci[0]]);
      if (ac_nz_num >= 2) cul_level += (unsigned int)abs(qcoeff[nz_ci[1]]);
      if (ac_nz_num >= 3) cul_level += (unsigned int)abs(qcoeff[nz_ci[2]]);
    }
    p->txb_entropy_ctx[block] =
        (uint8_t)(AOMMIN(cul_level, COEFF_CONTEXT_MASK) | dc_sign);
  }

  *rate_cost = accu_rate;
  return eob;
}

int av1_optimize_txb(const struct AV1_COMP *cpi, MACROBLOCK *x, int plane,
                     int block, TX_SIZE tx_size, TX_TYPE tx_type,
                     const TXB_CTX *const txb_ctx, int *rate_cost,
                     int sharpness) {
  const struct macroblock_plane *p = &x->plane[plane];
  const int eob = p->eobs[block];
  assert(eob > 0);
  if (eob > 1) {
    return av1_optimize_txb_multi(cpi, x, plane, block, tx_size, tx_type,
                                  txb_ctx, rate_cost, sharpness);
  }

  MACROBLOCKD *const xd = &x->e_mbd;
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const int is_inter = is_inter_block(mbmi);
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *const qcoeff = p->qcoeff + block_offset;
  tran_low_t *const dqcoeff = p->dqcoeff + block_offset;
  const CoeffCosts *const coeff_costs = &x->coeff_costs;
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  const TX_CLASS tx_class = tx_type_to_class[tx_type];
  const LV_MAP_COEFF_COST *txb_costs =
      &coeff_costs->coeff_costs[txs_ctx][plane_type];
  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const LV_MAP_EOB_COST *txb_eob_costs =
      &coeff_costs->eob_costs[eob_multi_size][plane_type];

  const int non_skip_cost = txb_costs->txb_skip_cost[txb_ctx->txb_skip_ctx][0];
  const int skip_cost = txb_costs->txb_skip_cost[txb_ctx->txb_skip_ctx][1];
  const int eob_cost = txb_eob_costs->eob_cost[tx_class != TX_CLASS_2D][0];
  const tran_low_t qc = qcoeff[0];
  const tran_low_t abs_qc = abs(qc);
  const int sign = qc < 0;
  int accu_rate = eob_cost;

  tran_low_t final_abs_qc = abs_qc;
  if (abs_qc == 1) {
    accu_rate += txb_costs->base_eob_cost[0][0] +
                 txb_costs->dc_sign_cost[txb_ctx->dc_sign_ctx][sign];
    if (UNLIKELY(sharpness == 0)) {
      const int64_t rdmult = get_txb_rdmult_mbmi(
          cpi, x, xd, mbmi, is_inter, plane, plane_type, sharpness, NULL);
      const tran_low_t tqc = p->coeff[block_offset];
      const tran_low_t dqc = dqcoeff[0];
      const int shift = av1_get_tx_scale(tx_size);
      const qm_val_t *qmatrix =
          (cpi->oxcf.tune_cfg.dist_metric == AOM_DIST_METRIC_QM_PSNR &&
           tx_type < IDTX)
              ? xd->plane[plane].seg_qmatrix[mbmi->segment_id][tx_size]
              : NULL;
      int64_t accu_dist;
      if (LIKELY(qmatrix != NULL)) {
        const int64_t scaled_qm = (int64_t)qmatrix[0] << shift;
        const int64_t dist0 = get_coeff_dist_scaled_qm(tqc, 0, scaled_qm);
        accu_dist = get_coeff_dist_scaled_qm(tqc, dqc, scaled_qm) - dist0;
      } else {
        accu_dist =
            ((int64_t)dqc * (dqc - (int64_t)tqc * 2)) * (1LL << (2 * shift));
      }
      if (RDCOST(rdmult, skip_cost, 0) <
          RDCOST(rdmult, accu_rate + non_skip_cost, accu_dist)) {
        qcoeff[0] = 0;
        dqcoeff[0] = 0;
        p->eobs[block] = 0;
        p->txb_entropy_ctx[block] = 0;
        *rate_cost = skip_cost;
        return 0;
      }
    }
  } else {
    const int64_t rdmult = get_txb_rdmult_mbmi(
        cpi, x, xd, mbmi, is_inter, plane, plane_type, sharpness, NULL);
    const tran_low_t tqc = p->coeff[block_offset];
    const tran_low_t dqc = dqcoeff[0];
    const int shift = av1_get_tx_scale(tx_size);
    const qm_val_t *qmatrix =
        (cpi->oxcf.tune_cfg.dist_metric == AOM_DIST_METRIC_QM_PSNR &&
         tx_type < IDTX)
            ? xd->plane[plane].seg_qmatrix[mbmi->segment_id][tx_size]
            : NULL;
    const int dc_sign_cost =
        txb_costs->dc_sign_cost[txb_ctx->dc_sign_ctx][sign];
    const tran_low_t abs_qc_low = abs_qc - 1;
    const qm_val_t *iqmatrix =
        tx_type < IDTX
            ? xd->plane[plane].seg_iqmatrix[mbmi->segment_id][tx_size]
            : NULL;
    const int dqv = get_dqv(p->dequant_QTX, 0, iqmatrix);
    tran_low_t qc_low, dqc_low;
    get_qc_dqc_low(abs_qc, sign, dqv, shift, &qc_low, &dqc_low);
    int rate, rate_low;
    if (LIKELY(abs_qc == 2)) {
      rate = txb_costs->base_eob_cost[0][1] + dc_sign_cost;
      rate_low = txb_costs->base_eob_cost[0][0] + dc_sign_cost;
    } else {
      rate = txb_costs->base_eob_cost[0][2] + dc_sign_cost +
             get_br_cost_fast(abs_qc, txb_costs->lps_cost[0]);
      rate_low =
          txb_costs->base_eob_cost[0][AOMMIN(abs_qc_low, 3) - 1] + dc_sign_cost;
      if (abs_qc_low > NUM_BASE_LEVELS) {
        rate_low += get_br_cost_fast(abs_qc_low, txb_costs->lps_cost[0]);
      }
    }
    int64_t dist_diff_0, dist_diff_low_0;
    if (LIKELY(qmatrix != NULL)) {
      const int64_t scaled_qm = (int64_t)qmatrix[0] << shift;
      const int64_t dist0 = get_coeff_dist_scaled_qm(tqc, 0, scaled_qm);
      dist_diff_0 = get_coeff_dist_scaled_qm(tqc, dqc, scaled_qm) - dist0;
      dist_diff_low_0 =
          get_coeff_dist_scaled_qm(tqc, dqc_low, scaled_qm) - dist0;
    } else {
      const int64_t tqc2 = (int64_t)tqc * 2;
      dist_diff_0 = ((int64_t)dqc * (dqc - tqc2)) * (1LL << (2 * shift));
      dist_diff_low_0 =
          ((int64_t)dqc_low * (dqc_low - tqc2)) * (1LL << (2 * shift));
    }
    const int64_t rd = RDCOST(rdmult, rate, dist_diff_0);
    const int64_t rd_low = RDCOST(rdmult, rate_low, dist_diff_low_0);
    int64_t accu_dist;
    if (rd_low < rd) {
      qcoeff[0] = qc_low;
      dqcoeff[0] = dqc_low;
      final_abs_qc = abs_qc_low;
      accu_rate += rate_low;
      accu_dist = dist_diff_low_0;
    } else {
      accu_rate += rate;
      accu_dist = dist_diff_0;
    }
    if (UNLIKELY(sharpness == 0)) {
      if (RDCOST(rdmult, skip_cost, 0) <
          RDCOST(rdmult, accu_rate + non_skip_cost, accu_dist)) {
        qcoeff[0] = 0;
        dqcoeff[0] = 0;
        p->eobs[block] = 0;
        p->txb_entropy_ctx[block] = 0;
        *rate_cost = skip_cost;
        return 0;
      }
    }
  }

  const int tx_type_cost =
      get_tx_type_cost_mbmi(x, xd, mbmi, is_inter, plane, tx_size, tx_type,
                            cpi->common.features.reduced_tx_set_used);
  *rate_cost = accu_rate + non_skip_cost + tx_type_cost;
  p->eobs[block] = 1;
  p->txb_entropy_ctx[block] =
      (uint8_t)(((2 - sign) << COEFF_CONTEXT_BITS) |
                AOMMIN(final_abs_qc, COEFF_CONTEXT_MASK));
  return 1;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int warehouse_efficients_txb_multi(
    const tran_low_t *const qcoeff, const TX_SIZE tx_size,
    const TXB_CTX *const txb_ctx, const int eob,
    const LV_MAP_COEFF_COST *const coeff_costs, const TX_TYPE tx_type,
    const TX_CLASS tx_class, int cost) {
  const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
  const int16_t *const scan = scan_order->scan;
  const int height = get_txb_high(tx_size);
  const int bhl = get_txb_bhl(tx_size);
  if (eob <= 4 && LIKELY(tx_class == TX_CLASS_2D)) {
    const int pos1 = scan[1];
    const tran_low_t v1 = qcoeff[pos1];
    const tran_low_t v0 = qcoeff[0];
    if (LIKELY((unsigned int)(v1 + 2) <= 4 && (unsigned int)(v0 + 2) <= 4)) {
      if (eob == 3) {
        const int pos2 = scan[2];
        const tran_low_t v2 = qcoeff[pos2];
        int ctx_val1 = 0;
        if (LIKELY(v2 != 0)) {
          const unsigned int level2 = (unsigned int)abs(v2);
          if (LIKELY(level2 <= NUM_BASE_LEVELS)) {
            cost +=
                coeff_costs->base_eob_cost[1][level2 - 1] + av1_cost_literal(1);
          } else {
            const int br2 = get_br_ctx_eob(pos2, bhl, TX_CLASS_2D);
            cost += coeff_costs->base_eob_cost[1][2] + av1_cost_literal(1) +
                    get_br_cost_fast((tran_low_t)level2,
                                     coeff_costs->lps_cost[br2]);
          }
          const int dc = (pos2 >> bhl) - (pos1 >> bhl);
          const int dr = (pos2 & (height - 1)) - (pos1 & (height - 1));
          if ((dc | dr) >= 0 && dc + dr <= 2) {
            ctx_val1 = 1 + (level2 >= 3);
          }
        }
        const int coeff_ctx1 = ctx_val1 + av1_nz_map_ctx_offset[tx_size][pos1];
        cost += coeff_costs->base_cost[coeff_ctx1][4 + (unsigned int)abs(v1)];
        if (!v0) {
          cost += coeff_costs->base_cost[0][0];
        } else {
          cost += coeff_costs->dc_sign_cost[txb_ctx->dc_sign_ctx][v0 < 0] +
                  coeff_costs->base_cost[0][(unsigned int)abs(v0)];
        }
        return cost;
      }
      const int pos2 = scan[2];
      const tran_low_t v2 = qcoeff[pos2];
      if (LIKELY((unsigned int)(v2 + 2) <= 4)) {
        const int pos3 = scan[3];
        const tran_low_t v3 = qcoeff[pos3];
        const int mask = height - 1;
        const int c2 = pos2 >> bhl, r2 = pos2 & mask;
        const int c1 = pos1 >> bhl, r1 = pos1 & mask;
        int ctx_val2 = 0;
        int mag1 = 0;
        if (LIKELY(v3 != 0)) {
          const unsigned int level3 = (unsigned int)abs(v3);
          const int eob_ctx3 = (tx_size == TX_4X4) ? 2 : 1;
          if (LIKELY(level3 <= NUM_BASE_LEVELS)) {
            cost += coeff_costs->base_eob_cost[eob_ctx3][level3 - 1] +
                    av1_cost_literal(1);
          } else {
            const int br3 = get_br_ctx_eob(pos3, bhl, TX_CLASS_2D);
            cost += coeff_costs->base_eob_cost[eob_ctx3][2] +
                    av1_cost_literal(1) +
                    get_br_cost_fast((tran_low_t)level3,
                                     coeff_costs->lps_cost[br3]);
          }
          const int c3 = pos3 >> bhl, r3 = pos3 & mask;
          const int dc32 = c3 - c2, dr32 = r3 - r2;
          if ((dc32 | dr32) >= 0 && dc32 + dr32 <= 2) {
            ctx_val2 = 1 + (level3 >= 3);
          }
          const int dc31 = c3 - c1, dr31 = r3 - r1;
          if ((dc31 | dr31) >= 0 && dc31 + dr31 <= 2) {
            mag1 = (int)AOMMIN(level3, 3);
          }
        }
        const unsigned int level2 = (unsigned int)abs(v2);
        const int dc21 = c2 - c1, dr21 = r2 - r1;
        if (v2 != 0 && (dc21 | dr21) >= 0 && dc21 + dr21 <= 2) {
          mag1 += (int)level2;
        }
        const int8_t *const nz_off = av1_nz_map_ctx_offset[tx_size];
        cost += coeff_costs->base_cost[ctx_val2 + nz_off[pos2]][4 + level2] +
                coeff_costs->base_cost[((mag1 + 1) >> 1) + nz_off[pos1]]
                                      [4 + (unsigned int)abs(v1)];
        if (!v0) {
          cost += coeff_costs->base_cost[0][0];
        } else {
          cost += coeff_costs->dc_sign_cost[txb_ctx->dc_sign_ctx][v0 < 0] +
                  coeff_costs->base_cost[0][(unsigned int)abs(v0)];
        }
        return cost;
      }
    }
  }

  const int width = get_txb_wide(tx_size);
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, height);
  DECLARE_ALIGNED(16, int8_t, coeff_contexts[1024]);

  av1_txb_init_levels(qcoeff, width, height, levels);
  av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class, coeff_contexts);

  const intptr_t stride = height + TX_PAD_HOR;
  const int (*lps_cost)[16] = coeff_costs->lps_cost;
  {
    const int pos = scan[eob - 1];
    const tran_low_t v = qcoeff[pos];

    if (v) {
      const unsigned int level = (unsigned int)abs(v);
      const int coeff_ctx = (uint8_t)coeff_contexts[pos];
      if (LIKELY(level <= NUM_BASE_LEVELS)) {
        cost += coeff_costs->base_eob_cost[coeff_ctx][level - 1] +
                av1_cost_literal(1);
      } else {
        const int ctx = get_br_ctx_eob(pos, bhl, tx_class);
        cost += coeff_costs->base_eob_cost[coeff_ctx][2] + av1_cost_literal(1) +
                get_br_cost_fast(level, lps_cost[ctx]);
      }
    }
  }
  const int *const base_cost4 = &coeff_costs->base_cost[0][4];
  const uint8_t *local_levels = levels;
  const int16_t *scan_p = scan + eob - 2;
  const int16_t *const scan_end = scan + 1;
  int cost2 = 0;
  for (; scan_p >= scan_end + 1; scan_p -= 2) {
    const intptr_t pos0 = scan_p[0];
    const size_t ctx8_0 = (size_t)(uint8_t)coeff_contexts[pos0] << 3;
    const size_t level0 = (unsigned int)abs(qcoeff[pos0]);
    if (LIKELY(level0 <= NUM_BASE_LEVELS)) {
      cost += base_cost4[ctx8_0 + level0];
    } else {
      const int ctx =
          get_br_ctx_simple_fast(local_levels, pos0, bhl, stride, tx_class);
      cost += base_cost4[ctx8_0 + 3] +
              get_br_cost_fast((tran_low_t)level0, lps_cost[ctx]);
    }
    const intptr_t pos1 = scan_p[-1];
    const size_t ctx8_1 = (size_t)(uint8_t)coeff_contexts[pos1] << 3;
    const size_t level1 = (unsigned int)abs(qcoeff[pos1]);
    if (LIKELY(level1 <= NUM_BASE_LEVELS)) {
      cost2 += base_cost4[ctx8_1 + level1];
    } else {
      const int ctx =
          get_br_ctx_simple_fast(local_levels, pos1, bhl, stride, tx_class);
      cost2 += base_cost4[ctx8_1 + 3] +
               get_br_cost_fast((tran_low_t)level1, lps_cost[ctx]);
    }
  }
  if (scan_p == scan_end) {
    const intptr_t pos = *scan_p;
    const size_t ctx8 = (size_t)(uint8_t)coeff_contexts[pos] << 3;
    const size_t level = (unsigned int)abs(qcoeff[pos]);
    if (LIKELY(level <= NUM_BASE_LEVELS)) {
      cost += base_cost4[ctx8 + level];
    } else {
      const int ctx =
          get_br_ctx_simple_fast(local_levels, pos, bhl, stride, tx_class);
      cost += base_cost4[ctx8 + 3] +
              get_br_cost_fast((tran_low_t)level, lps_cost[ctx]);
    }
  }
  cost += cost2;
  {
    const int (*base_cost)[8] = coeff_costs->base_cost;
    const tran_low_t v = qcoeff[0];
    const int coeff_ctx = (uint8_t)coeff_contexts[0];
    if (!v) {
      cost += base_cost[coeff_ctx][0];
    } else {
      const unsigned int level = (unsigned int)abs(v);
      const int sign01 = (v < 0) ? 1 : 0;
      const int dc_sign_ctx = txb_ctx->dc_sign_ctx;
      cost += coeff_costs->dc_sign_cost[dc_sign_ctx][sign01];
      if (LIKELY(level <= NUM_BASE_LEVELS)) {
        cost += base_cost[coeff_ctx][level];
      } else {
        const int ctx = get_br_ctx_stride(levels, 0, bhl, stride, tx_class);
        cost +=
            base_cost[coeff_ctx][3] + get_br_cost_fast(level, lps_cost[ctx]);
      }
    }
  }
  return cost;
}

/*!\brief Estimate the entropy cost of transform coefficients using Laplacian
 * distribution.
 *
 * \ingroup coefficient_coding
 *
 * This function assumes each transform coefficient is of its own Laplacian
 * distribution and the coefficient is the only observation of the Laplacian
 * distribution.
 *
 * Based on that, each coefficient's coding cost can be estimated by computing
 * the entropy of the corresponding Laplacian distribution.
 *
 * This function then return the sum of the estimated entropy cost for all
 * coefficients in the transform block.
 *
 * Note that the entropy cost of end of block (eob) and transform type (tx_type)
 * are not included.
 *
 * \param[in]    x              Pointer to structure holding the data for the
                                current encoding macroblock
 * \param[in]    plane          The index of the current plane
 * \param[in]    block          The index of the current transform block in the
 * macroblock. It's defined by number of 4x4 units that have been coded before
 * the currernt transform block
 * \param[in]    tx_size        The transform size
 * \param[in]    tx_type        The transform type
 * \return       int            Estimated entropy cost of coefficients in the
 * transform block.
 */
static int av1_cost_coeffs_txb_estimate(const MACROBLOCK *x, const int plane,
                                        const int block, const TX_SIZE tx_size,
                                        const TX_TYPE tx_type) {
  assert(plane == 0);

  int cost = 0;
  const struct macroblock_plane *p = &x->plane[plane];
  const SCAN_ORDER *scan_order = get_scan(tx_size, tx_type);
  const int16_t *scan = scan_order->scan;
  tran_low_t *qcoeff = p->qcoeff + BLOCK_OFFSET(block);

  int eob = p->eobs[block];

  int c = eob - 1;
  {
    const int pos = scan[c];
    const tran_low_t v = abs(qcoeff[pos]) - 1;
    cost += (v << (AV1_PROB_COST_SHIFT + 2));
  }
  for (c = eob - 2; c >= 0; c--) {
    const int pos = scan[c];
    const tran_low_t v = abs(qcoeff[pos]);
    const int idx = AOMMIN(v, 14);

    cost += costLUT[idx];
  }

  cost += (const_term + loge_par) * (eob - 1);

  return cost;
}

static AOM_FORCE_INLINE int warehouse_efficients_txb_laplacian(
    const MACROBLOCK *x, const int plane, const int block,
    const TX_SIZE tx_size, const TXB_CTX *const txb_ctx, const int eob,
    const PLANE_TYPE plane_type, const LV_MAP_COEFF_COST *const coeff_costs,
    const MACROBLOCKD *const xd, const TX_TYPE tx_type, const TX_CLASS tx_class,
    int reduced_tx_set_used) {
  const int txb_skip_ctx = txb_ctx->txb_skip_ctx;

  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const LV_MAP_EOB_COST *const eob_costs =
      &x->coeff_costs.eob_costs[eob_multi_size][plane_type];
  int cost = coeff_costs->txb_skip_cost[txb_skip_ctx][0];

  cost += get_tx_type_cost(x, xd, plane, tx_size, tx_type, reduced_tx_set_used);

  cost += get_eob_cost(eob, eob_costs, coeff_costs, tx_class);

  cost += av1_cost_coeffs_txb_estimate(x, plane, block, tx_size, tx_type);
  return cost;
}

int av1_cost_coeffs_txb(const MACROBLOCK *x, const int plane, const int block,
                        const TX_SIZE tx_size, const TX_TYPE tx_type,
                        const TXB_CTX *const txb_ctx, int reduced_tx_set_used) {
  const struct macroblock_plane *p = &x->plane[plane];
  const int eob = p->eobs[block];
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const LV_MAP_COEFF_COST *const coeff_costs =
      &x->coeff_costs.coeff_costs[txs_ctx][plane_type];
  if (eob == 0) {
    return coeff_costs->txb_skip_cost[txb_ctx->txb_skip_ctx][1];
  }

  const MACROBLOCKD *const xd = &x->e_mbd;
  const TX_CLASS tx_class = tx_type_to_class[tx_type];
  const tran_low_t *const qcoeff = p->qcoeff + BLOCK_OFFSET(block);
  const int txb_skip_ctx = txb_ctx->txb_skip_ctx;
  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const LV_MAP_EOB_COST *const eob_costs =
      &x->coeff_costs.eob_costs[eob_multi_size][plane_type];
  int cost = coeff_costs->txb_skip_cost[txb_skip_ctx][0];
  cost += get_tx_type_cost(x, xd, plane, tx_size, tx_type, reduced_tx_set_used);
  cost += get_eob_cost(eob, eob_costs, coeff_costs, tx_class);

  if (eob == 1) {
    const tran_low_t v = qcoeff[0];
    const int sign = AOMSIGN(v);
    const unsigned int level = (unsigned int)((v ^ sign) - sign);
    const int sign01 = -sign;
    cost += coeff_costs->dc_sign_cost[txb_ctx->dc_sign_ctx][sign01];
    if (LIKELY(level <= NUM_BASE_LEVELS)) {
      cost += coeff_costs->base_eob_cost[0][level - 1];
    } else {
      cost += coeff_costs->base_eob_cost[0][2] +
              get_br_cost_fast(level, coeff_costs->lps_cost[0]);
    }
    return cost;
  }

  if (eob == 2) {
    const int16_t *const scan = get_scan(tx_size, tx_type)->scan;
    const int pos1 = scan[1];
    const tran_low_t v1 = qcoeff[pos1];
    unsigned int level1 = 0;
    uintptr_t min_lvl1 = 0;
    if (v1) {
      level1 = (unsigned int)abs(v1);
      if (LIKELY(level1 <= NUM_BASE_LEVELS)) {
        min_lvl1 = level1;
        cost += coeff_costs->base_eob_cost[1][level1 - 1] + av1_cost_literal(1);
      } else {
        min_lvl1 = 3;
        cost += coeff_costs->base_eob_cost[1][2] + av1_cost_literal(1) +
                get_br_cost_fast(level1, coeff_costs->lps_cost[7]);
      }
    }
    const int coeff_ctx0 = (tx_class == TX_CLASS_2D)
                               ? 0
                               : (int)(NZ_MAP_CTX_0 + ((min_lvl1 + 1) >> 1));
    const tran_low_t v0 = qcoeff[0];
    if (!v0) {
      cost += coeff_costs->base_cost[coeff_ctx0][0];
    } else {
      const unsigned int level0 = (unsigned int)abs(v0);
      const int sign01 = (v0 < 0) ? 1 : 0;
      cost += coeff_costs->dc_sign_cost[txb_ctx->dc_sign_ctx][sign01];
      if (LIKELY(level0 <= NUM_BASE_LEVELS)) {
        cost += coeff_costs->base_cost[coeff_ctx0][level0];
      } else {
        const unsigned int ctx0 = AOMMIN((level1 + 1) >> 1, 6);
        cost += coeff_costs->base_cost[coeff_ctx0][3] +
                get_br_cost_fast(level0, coeff_costs->lps_cost[ctx0]);
      }
    }
    return cost;
  }

  return warehouse_efficients_txb_multi(qcoeff, tx_size, txb_ctx, eob,
                                        coeff_costs, tx_type, tx_class, cost);
}

int av1_cost_coeffs_txb_laplacian(const MACROBLOCK *x, const int plane,
                                  const int block, const TX_SIZE tx_size,
                                  const TX_TYPE tx_type,
                                  const TXB_CTX *const txb_ctx,
                                  const int reduced_tx_set_used,
                                  const int adjust_eob) {
  const struct macroblock_plane *p = &x->plane[plane];
  int eob = p->eobs[block];

  if (adjust_eob) {
    const SCAN_ORDER *scan_order = get_scan(tx_size, tx_type);
    const int16_t *scan = scan_order->scan;
    tran_low_t *tcoeff = p->coeff + BLOCK_OFFSET(block);
    tran_low_t *qcoeff = p->qcoeff + BLOCK_OFFSET(block);
    tran_low_t *dqcoeff = p->dqcoeff + BLOCK_OFFSET(block);
    update_coeff_eob_fast(&eob, av1_get_tx_scale(tx_size), p->dequant_QTX, scan,
                          tcoeff, qcoeff, dqcoeff);
    p->eobs[block] = eob;
  }

  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const LV_MAP_COEFF_COST *const coeff_costs =
      &x->coeff_costs.coeff_costs[txs_ctx][plane_type];
  if (eob == 0) {
    return coeff_costs->txb_skip_cost[txb_ctx->txb_skip_ctx][1];
  }

  const MACROBLOCKD *const xd = &x->e_mbd;
  const TX_CLASS tx_class = tx_type_to_class[tx_type];

  return warehouse_efficients_txb_laplacian(
      x, plane, block, tx_size, txb_ctx, eob, plane_type, coeff_costs, xd,
      tx_type, tx_class, reduced_tx_set_used);
}
