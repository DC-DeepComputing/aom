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

static AOM_FORCE_INLINE void update_coeff_last(
    int *accu_rate, int64_t *accu_dist, int si, int ci, tran_low_t abs_qc,
    int sign, TX_CLASS tx_class, int bhl, int width, int64_t rdmult, int shift,
    const int16_t *dequant, const LV_MAP_COEFF_COST *txb_costs,
    const tran_low_t *tcoeff, tran_low_t *qcoeff, tran_low_t *dqcoeff,
    const qm_val_t *iqmatrix, const qm_val_t *qmatrix) {
  const int coeff_ctx = get_lower_levels_ctx_eob(bhl, width, si);
  const tran_low_t tqc = tcoeff[ci];
  const tran_low_t dqc = dqcoeff[ci];
  const int *const base_eob = txb_costs->base_eob_cost[coeff_ctx];
  const tran_low_t abs_qc_low = abs_qc - 1;
  int rate, rate_low;
  if (abs_qc == 2) {
    rate = base_eob[1] + av1_cost_literal(1);
    rate_low = base_eob[0] + av1_cost_literal(1);
  } else {
    const int br_ctx = get_br_ctx_eob(ci, bhl, tx_class);
    const int *const lps_cost = txb_costs->lps_cost[br_ctx];
    const int base_rate = base_eob[2] + av1_cost_literal(1);
    rate = base_rate + get_br_cost(abs_qc, lps_cost);
    rate_low = (abs_qc == 3) ? (base_eob[1] + av1_cost_literal(1))
                             : (base_rate + get_br_cost(abs_qc_low, lps_cost));
  }

  int dqv = dequant[1];
  if (iqmatrix != NULL) {
    dqv = ((iqmatrix[ci] * dqv) + (1 << (AOM_QM_BITS - 1))) >> AOM_QM_BITS;
  }
  tran_low_t qc_low, dqc_low;
  get_qc_dqc_low(abs_qc, sign, dqv, shift, &qc_low, &dqc_low);

  int64_t dist_diff_0, dist_diff_low_0;
  if (qmatrix == NULL) {
    const int64_t tqc2 = (int64_t)tqc * 2;
    dist_diff_0 = ((int64_t)dqc * (dqc - tqc2)) * (1LL << (2 * shift));
    dist_diff_low_0 =
        ((int64_t)dqc_low * (dqc_low - tqc2)) * (1LL << (2 * shift));
  } else {
    const int64_t scaled_qm = (int64_t)qmatrix[ci] << shift;
    const int64_t dist0 = get_coeff_dist_scaled_qm(tqc, 0, scaled_qm);
    dist_diff_0 = get_coeff_dist_scaled_qm(tqc, dqc, scaled_qm) - dist0;
    dist_diff_low_0 = get_coeff_dist_scaled_qm(tqc, dqc_low, scaled_qm) - dist0;
  }

  const int64_t rd = RDCOST(rdmult, rate, dist_diff_0);
  const int64_t rd_low = RDCOST(rdmult, rate_low, dist_diff_low_0);

  if (rd_low < rd) {
    qcoeff[ci] = qc_low;
    dqcoeff[ci] = dqc_low;
    *accu_rate += rate_low;
    *accu_dist += dist_diff_low_0;
  } else {
    *accu_rate += rate;
    *accu_dist += dist_diff_0;
  }
}

static AOM_FORCE_INLINE void update_coeff_dc(
    int *accu_rate, TX_SIZE tx_size, TX_CLASS tx_class, int bhl,
    intptr_t stride, int64_t rdmult, int shift, int dc_sign_ctx,
    const int16_t *dequant, const LV_MAP_COEFF_COST *txb_costs,
    const tran_low_t *tcoeff, tran_low_t *qcoeff, tran_low_t *dqcoeff,
    uint8_t *levels, const qm_val_t *iqmatrix, const qm_val_t *qmatrix) {
  const tran_low_t qc = qcoeff[0];
  const int coeff_ctx =
      (tx_class == TX_CLASS_2D)
          ? 0
          : get_lower_levels_ctx(levels, 0, bhl, tx_size, tx_class);
  if (qc == 0) {
    *accu_rate += txb_costs->base_cost[coeff_ctx][0];
  } else {
    const int sign = (qc < 0) ? 1 : 0;
    const tran_low_t abs_qc = abs(qc);
    const tran_low_t tqc = tcoeff[0];
    const tran_low_t dqc = dqcoeff[0];
    const int dc_sign_cost = txb_costs->dc_sign_cost[dc_sign_ctx][sign];
    const int *const base_cost = txb_costs->base_cost[coeff_ctx];
    int rate = base_cost[AOMMIN(abs_qc, 3)] + dc_sign_cost;
    const int *lps_cost = txb_costs->lps_cost[0];
    if (abs_qc > NUM_BASE_LEVELS) {
      const int br_ctx = get_br_ctx_stride(levels, 0, bhl, stride, tx_class);
      lps_cost = txb_costs->lps_cost[br_ctx];
      rate += get_br_cost(abs_qc, lps_cost);
    }

    tran_low_t qc_low = 0, dqc_low = 0;
    tran_low_t abs_qc_low = 0;
    int rate_low;
    if (abs_qc == 1) {
      rate_low = base_cost[0];
    } else {
      const int dqv = get_dqv(dequant, 0, iqmatrix);
      get_qc_dqc_low(abs_qc, sign, dqv, shift, &qc_low, &dqc_low);
      abs_qc_low = abs_qc - 1;
      rate_low = base_cost[AOMMIN(abs_qc_low, 3)] + dc_sign_cost;
      if (abs_qc_low > NUM_BASE_LEVELS) {
        rate_low += get_br_cost(abs_qc_low, lps_cost);
      }
    }

    int64_t dist_diff_0, dist_diff_low_0;
    if (qmatrix == NULL) {
      const int64_t tqc2 = (int64_t)tqc * 2;
      dist_diff_0 = ((int64_t)dqc * (dqc - tqc2)) * (1LL << (2 * shift));
      dist_diff_low_0 =
          (abs_qc == 1)
              ? 0
              : (((int64_t)dqc_low * (dqc_low - tqc2)) * (1LL << (2 * shift)));
    } else {
      const int64_t scaled_qm = (int64_t)qmatrix[0] << shift;
      dist_diff_0 = get_coeff_dist_scaled_qm(tqc, dqc, scaled_qm);
      dist_diff_low_0 = (abs_qc == 1)
                            ? get_coeff_dist_scaled_qm(tqc, 0, scaled_qm)
                            : get_coeff_dist_scaled_qm(tqc, dqc_low, scaled_qm);
    }

    const int64_t rd = RDCOST(rdmult, rate, dist_diff_0);
    const int64_t rd_low = RDCOST(rdmult, rate_low, dist_diff_low_0);

    if (rd_low < rd) {
      qcoeff[0] = qc_low;
      dqcoeff[0] = dqc_low;
      levels[0] = AOMMIN(abs_qc_low, INT8_MAX);
      *accu_rate += rate_low;
    } else {
      *accu_rate += rate;
    }
  }
}

typedef struct {
  int64_t rdmult;
  int64_t accu_dist;
  const LV_MAP_EOB_COST *txb_eob_costs;
  const LV_MAP_COEFF_COST *txb_costs;
  const int16_t *dequant;
  const tran_low_t *tcoeff;
  tran_low_t *qcoeff;
  tran_low_t *dqcoeff;
  uint8_t *levels;
  const qm_val_t *iqmatrix;
  const qm_val_t *qmatrix;
  int *nz_ci;
  intptr_t stride;
  int eob;
  int nz_num;
  int accu_rate;
  int bhl;
  int width;
  int dc_sign_ctx;
  int shift;
  int dequant_ac;
  int sharpness;
  int min_eob_cutoff;
} TxbOptCtx;

static AOM_FORCE_INLINE int get_br_ctx_simple_fast(const uint8_t *const levels,
                                                   intptr_t c, int bhl,
                                                   intptr_t stride,
                                                   TX_CLASS tx_class) {
  const intptr_t col = c >> bhl;
  const uint8_t *const lv = levels + c + (col << TX_PAD_HOR_LOG2);
  if (LIKELY(tx_class == TX_CLASS_2D)) {
    const int mag = AOMMIN((lv[1] + lv[stride] + lv[stride + 1] + 1) >> 1, 6);
    return mag + (((c & ~(stride - (TX_PAD_HOR - 1))) == 0) ? 7 : 14);
  }
  if (tx_class == TX_CLASS_HORIZ) {
    const int mag = AOMMIN((lv[1] + lv[stride] + lv[stride * 2] + 1) >> 1, 6);
    return mag + ((c < (stride - TX_PAD_HOR)) ? 7 : 14);
  }
  const int mag = AOMMIN((lv[1] + lv[stride] + lv[2] + 1) >> 1, 6);
  return mag + (((c & (stride - (TX_PAD_HOR + 1))) == 0) ? 7 : 14);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int update_coeff_simple_gt1_try_lower(intptr_t ci, tran_low_t qc,
                                             intptr_t coeff_ctx,
                                             TX_CLASS tx_class,
                                             const TxbOptCtx *__restrict ctx) {
  const LV_MAP_COEFF_COST *__restrict txb_costs = ctx->txb_costs;
  const int *const base_cost = txb_costs->base_cost[coeff_ctx];
  const tran_low_t abs_qc = abs(qc);
  int rate, rate_low;
  if (abs_qc == 2) {
    rate = base_cost[6];
    rate_low = base_cost[5];
  } else {
    const int br_ctx = get_br_ctx_simple_fast(ctx->levels, ci, ctx->bhl,
                                              ctx->stride, tx_class);
    const int *const lps_cost = txb_costs->lps_cost[br_ctx];
    const int base_rate = base_cost[7];
    if (LIKELY(abs_qc < 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE)) {
      rate = base_rate + lps_cost[abs_qc - (1 + NUM_BASE_LEVELS)];
      rate_low = (abs_qc == 3)
                     ? base_cost[6]
                     : (base_rate + lps_cost[abs_qc - (2 + NUM_BASE_LEVELS)]);
    } else {
      rate = base_rate + get_br_cost(abs_qc, lps_cost);
      rate_low = base_rate + get_br_cost(abs_qc - 1, lps_cost);
    }
  }

  int dqv = ctx->dequant_ac;
  if (ctx->iqmatrix != NULL) {
    dqv = ((ctx->iqmatrix[ci] * dqv) + (1 << (AOM_QM_BITS - 1))) >> AOM_QM_BITS;
  }
  const int shift = ctx->shift;
  const tran_low_t abs_qc_low = abs_qc - 1;
  const tran_low_t abs_dqc_low = (abs_qc_low * dqv) >> shift;
  const tran_low_t abs_tqc = abs(ctx->tcoeff[ci]);
  const tran_low_t abs_dqc = abs(ctx->dqcoeff[ci]);
  const qm_val_t *__restrict qmatrix = ctx->qmatrix;
  int64_t dist_diff_0, dist_diff_low_0;
  if (qmatrix == NULL) {
    const int64_t abs_tqc2 = (int64_t)abs_tqc << 1;
    dist_diff_0 =
        ((int64_t)abs_dqc * (abs_dqc - abs_tqc2)) * (1LL << (2 * shift));
    dist_diff_low_0 = ((int64_t)abs_dqc_low * (abs_dqc_low - abs_tqc2)) *
                      (1LL << (2 * shift));
  } else {
    const int64_t scaled_qm = (int64_t)qmatrix[ci] << shift;
    dist_diff_0 = get_coeff_dist_scaled_qm(abs_tqc, abs_dqc, scaled_qm);
    dist_diff_low_0 = get_coeff_dist_scaled_qm(abs_tqc, abs_dqc_low, scaled_qm);
  }
  const int64_t rdmult = ctx->rdmult;
  const int64_t rd = RDCOST(rdmult, rate, dist_diff_0);
  const int64_t rd_low = RDCOST(rdmult, rate_low, dist_diff_low_0);
  if (rd_low < rd) {
    const int sign = (qc < 0) ? 1 : 0;
    ctx->qcoeff[ci] = (-sign ^ abs_qc_low) + sign;
    ctx->dqcoeff[ci] = (-sign ^ abs_dqc_low) + sign;
    ctx->levels[get_padded_idx((int)ci, ctx->bhl)] =
        AOMMIN(abs_qc_low, INT8_MAX);
    return rate_low;
  }
  return rate;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int update_coeff_simple_gt1(intptr_t ci, tran_low_t qc,
                                   intptr_t coeff_ctx, TX_CLASS tx_class,
                                   const TxbOptCtx *__restrict ctx) {
  const tran_low_t abs_tqc = abs(ctx->tcoeff[ci]);
  const tran_low_t abs_dqc = abs(ctx->dqcoeff[ci]);
  if (abs_dqc >= abs_tqc) {
    return update_coeff_simple_gt1_try_lower(ci, qc, coeff_ctx, tx_class, ctx);
  }
  const LV_MAP_COEFF_COST *__restrict txb_costs = ctx->txb_costs;
  const int *const base_cost = txb_costs->base_cost[coeff_ctx];
  const tran_low_t abs_qc = abs(qc);
  if (LIKELY(abs_qc == 2)) return base_cost[6];
  const int br_ctx =
      get_br_ctx_simple_fast(ctx->levels, ci, ctx->bhl, ctx->stride, tx_class);
  const int *const lps_cost = txb_costs->lps_cost[br_ctx];
  const int base_rate = base_cost[7];
  if (LIKELY(abs_qc < 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE)) {
    return base_rate + lps_cost[abs_qc - (1 + NUM_BASE_LEVELS)];
  }
  return base_rate + get_br_cost(abs_qc, lps_cost);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int update_coeff_simple_nz(intptr_t ci, tran_low_t qc,
                                  intptr_t coeff_ctx, TX_CLASS tx_class,
                                  const TxbOptCtx *__restrict ctx) {
  if ((unsigned int)(qc + 1) > 2) {
    return update_coeff_simple_gt1(ci, qc, coeff_ctx, tx_class, ctx);
  }
  const tran_low_t abs_tqc = abs(ctx->tcoeff[ci]);
  const tran_low_t abs_dqc = abs(ctx->dqcoeff[ci]);
  const LV_MAP_COEFF_COST *__restrict txb_costs = ctx->txb_costs;
  const int *const base_cost = txb_costs->base_cost[coeff_ctx];
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
    return rate_low;
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
  const int sign_cost = (ci == 0)
                            ? txb_costs->dc_sign_cost[ctx->dc_sign_ctx][sign]
                            : av1_cost_literal(1);
  const int *const base_cost = txb_costs->base_cost[coeff_ctx];
  int rate, rate_low;
  if (abs_qc == 2) {
    rate = base_cost[2] + sign_cost;
    rate_low = base_cost[1] + sign_cost;
  } else {
    const int br_ctx = get_br_ctx_stride(levels, ci, bhl, stride, tx_class);
    const int *const lps_cost = txb_costs->lps_cost[br_ctx];
    const int base_rate = base_cost[3] + sign_cost;
    rate = base_rate + get_br_cost(abs_qc, lps_cost);
    rate_low = (abs_qc == 3) ? (base_cost[2] + sign_cost)
                             : (base_rate + get_br_cost(abs_qc_low, lps_cost));
  }

  const int min_eob_cutoff = ctx->min_eob_cutoff;
  const int sharpness = ctx->sharpness;
  const int new_eob = si + 1;
  const int allow_new_eob = (sharpness == 0 || new_eob >= min_eob_cutoff);
  int rate_coeff_eob = 0, rate_coeff_eob_low = 0;
  if (allow_new_eob) {
    const int coeff_ctx_new_eob = get_lower_levels_ctx_eob(bhl, ctx->width, si);
    const int new_eob_cost =
        get_eob_cost(new_eob, ctx->txb_eob_costs, txb_costs, tx_class);
    const int *const base_eob_cost =
        txb_costs->base_eob_cost[coeff_ctx_new_eob];
    if (abs_qc == 2) {
      rate_coeff_eob = new_eob_cost + base_eob_cost[1] + sign_cost;
      rate_coeff_eob_low = new_eob_cost + base_eob_cost[0] + sign_cost;
    } else {
      const int br_ctx_eob = get_br_ctx_eob(ci, bhl, tx_class);
      const int *const lps_cost_eob = txb_costs->lps_cost[br_ctx_eob];
      const int base_eob_rate = new_eob_cost + base_eob_cost[2] + sign_cost;
      rate_coeff_eob = base_eob_rate + get_br_cost(abs_qc, lps_cost_eob);
      rate_coeff_eob_low =
          (abs_qc == 3)
              ? (new_eob_cost + base_eob_cost[1] + sign_cost)
              : (base_eob_rate + get_br_cost(abs_qc_low, lps_cost_eob));
    }
  }

  const tran_low_t tqc = ctx->tcoeff[ci];
  const tran_low_t dqc = ctx->dqcoeff[ci];
  const int shift = ctx->shift;
  const qm_val_t *__restrict qmatrix = ctx->qmatrix;
  const int dqv = get_dqv(ctx->dequant, ci, ctx->iqmatrix);
  tran_low_t qc_low, dqc_low;
  get_qc_dqc_low(abs_qc, sign, dqv, shift, &qc_low, &dqc_low);

  int64_t dist, dist_low;
  if (qmatrix == NULL) {
    const int64_t tqc2 = (int64_t)tqc * 2;
    dist = ((int64_t)dqc * (dqc - tqc2)) * (1LL << (2 * shift));
    dist_low = ((int64_t)dqc_low * (dqc_low - tqc2)) * (1LL << (2 * shift));
  } else {
    const int64_t scaled_qm = (int64_t)qmatrix[ci] << shift;
    const int64_t dist0 = get_coeff_dist_scaled_qm(tqc, 0, scaled_qm);
    dist = get_coeff_dist_scaled_qm(tqc, dqc, scaled_qm) - dist0;
    dist_low = get_coeff_dist_scaled_qm(tqc, dqc_low, scaled_qm) - dist0;
  }

  const int64_t dist_orig = dist;
  const int64_t rdmult = ctx->rdmult;
  int accu_rate = ctx->accu_rate;
  const int64_t accu_dist = ctx->accu_dist;
  int64_t rd = RDCOST(rdmult, accu_rate + rate, accu_dist + dist);
  int lower_level = 0;

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

  if (allow_new_eob) {
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

  if (lower_level) {
    ctx->qcoeff[ci] = qc_low;
    ctx->dqcoeff[ci] = dqc_low;
    levels[get_padded_idx(ci, bhl)] = AOMMIN(abs_qc_low, INT8_MAX);
  }
  ctx->nz_ci[ctx->nz_num] = ci;
  ++ctx->nz_num;
  return accu_rate;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int update_coeff_eob_eq1(int si, int ci, tran_low_t qc, int coeff_ctx,
                                TX_CLASS tx_class, TxbOptCtx *__restrict ctx) {
  const LV_MAP_COEFF_COST *__restrict txb_costs = ctx->txb_costs;
  const int sign_cost = (ci == 0)
                            ? txb_costs->dc_sign_cost[ctx->dc_sign_ctx][qc < 0]
                            : av1_cost_literal(1);
  const int *const base_cost = txb_costs->base_cost[coeff_ctx];
  int rate = base_cost[1] + sign_cost;
  const int bhl = ctx->bhl;
  const int new_eob = si + 1;
  const int coeff_ctx_new_eob = get_lower_levels_ctx_eob(bhl, ctx->width, si);
  const int new_eob_cost =
      get_eob_cost(new_eob, ctx->txb_eob_costs, txb_costs, tx_class);
  const int rate_coeff_eob =
      new_eob_cost + txb_costs->base_eob_cost[coeff_ctx_new_eob][0] + sign_cost;

  const tran_low_t tqc = ctx->tcoeff[ci];
  const tran_low_t dqc = ctx->dqcoeff[ci];
  const int shift = ctx->shift;
  const qm_val_t *__restrict qmatrix = ctx->qmatrix;
  int64_t dist;
  if (qmatrix == NULL) {
    const int64_t tqc2 = (int64_t)tqc * 2;
    dist = ((int64_t)dqc * (dqc - tqc2)) * (1LL << (2 * shift));
  } else {
    const int64_t scaled_qm = (int64_t)qmatrix[ci] << shift;
    const int64_t dist0 = get_coeff_dist_scaled_qm(tqc, 0, scaled_qm);
    dist = get_coeff_dist_scaled_qm(tqc, dqc, scaled_qm) - dist0;
  }

  const int64_t dist_orig = dist;
  const int64_t rdmult = ctx->rdmult;
  int accu_rate = ctx->accu_rate;
  const int64_t accu_dist = ctx->accu_dist;
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
  } else {
    ctx->nz_ci[ctx->nz_num] = ci;
    ++ctx->nz_num;
  }
  return accu_rate;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int update_coeff_eob_nz(int si, int ci, tran_low_t qc, int coeff_ctx,
                               TX_CLASS tx_class, TxbOptCtx *__restrict ctx) {
  const tran_low_t abs_qc = abs(qc);
  if (ctx->sharpness != 0 && si + 1 < ctx->min_eob_cutoff && abs_qc <= 2) {
    const LV_MAP_COEFF_COST *__restrict txb_costs = ctx->txb_costs;
    const int rate = (ci == 0)
                         ? (txb_costs->base_cost[coeff_ctx][abs_qc] +
                            txb_costs->dc_sign_cost[ctx->dc_sign_ctx][qc < 0])
                         : txb_costs->base_cost[coeff_ctx][4 + abs_qc];
    ctx->nz_ci[ctx->nz_num] = ci;
    ++ctx->nz_num;
    return ctx->accu_rate + rate;
  }
  if (abs_qc > 1) {
    return update_coeff_eob_gt1(si, ci, qc, coeff_ctx, tx_class, ctx);
  }
  return update_coeff_eob_eq1(si, ci, qc, coeff_ctx, tx_class, ctx);
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
      // no need to set up levels because this is the last step
      // levels[get_padded_idx(ci, bhl)] = 0;
    }
    *accu_rate = 0;
    *eob = 0;
  }
}

// TODO(angiebird): use this function whenever it's possible
static inline int get_tx_type_cost(const MACROBLOCK *x, const MACROBLOCKD *xd,
                                   int plane, TX_SIZE tx_size, TX_TYPE tx_type,
                                   int reduced_tx_set_used) {
  if (plane > 0) return 0;

  const TX_SIZE square_tx_size = txsize_sqr_map[tx_size];

  const MB_MODE_INFO *mbmi = xd->mi[0];
  const int is_inter = is_inter_block(mbmi);
  const TxSetType set_type =
      av1_get_ext_tx_set_type(tx_size, is_inter, reduced_tx_set_used);
  if (av1_num_ext_tx_set[set_type] > 1 &&
      !xd->lossless[xd->mi[0]->segment_id]) {
    const int ext_tx_set = ext_tx_set_index[is_inter][set_type];
    if (is_inter) {
      if (ext_tx_set > 0)
        return x->mode_costs
            .inter_tx_type_costs[ext_tx_set][square_tx_size][tx_type];
    } else {
      if (ext_tx_set > 0) {
        PREDICTION_MODE intra_dir;
        if (mbmi->filter_intra_mode_info.use_filter_intra)
          intra_dir = fimode_to_intradir[mbmi->filter_intra_mode_info
                                             .filter_intra_mode];
        else
          intra_dir = mbmi->mode;
        return x->mode_costs.intra_tx_type_costs[ext_tx_set][square_tx_size]
                                                [intra_dir][tx_type];
      }
    }
  }
  return 0;
}

// Recomputes the cached lower-level contexts of every position whose
// get_nz_mag_stride() neighborhood contains position c (called after
// levels[c] changes). (dc, dr) are offsets in (col, row) = (ci >> bhl,
// ci & (height - 1)) units, matching get_nz_mag_stride().
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void patch_ctx_neighbors_slow(int8_t *__restrict cc, const uint8_t *lv_c,
                                     int c, int bhl, intptr_t stride,
                                     const int8_t *nz_map_ctx,
                                     TX_CLASS tx_class) {
  const unsigned int col = (unsigned int)c >> bhl;
  const int step = 1 << bhl;
  const unsigned int row = (unsigned int)c & (unsigned int)(step - 1);
  if (LIKELY(tx_class == TX_CLASS_2D)) {
#define PATCH_2D(dc, dr)                                          \
  do {                                                            \
    const int p = c - (dc) * step - (dr);                         \
    const uint8_t *const lv = lv_c - (dc) * stride - (dr);        \
    const int stats = get_nz_mag_stride(lv, stride, TX_CLASS_2D); \
    const int ctx = AOMMIN((stats + 1) >> 1, 4);                  \
    cc[p] = (int8_t)(ctx + nz_map_ctx[p]);                        \
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
    const int ctx = AOMMIN((stats + 1) >> 1, 4);                    \
    cc[p] = (int8_t)(ctx + nz_map_ctx_offset_1d[row - (dr)]);       \
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
    const int ctx = AOMMIN((stats + 1) >> 1, 4);                     \
    cc[p] = (int8_t)(ctx + nz_map_ctx_offset_1d[col - (dc)]);        \
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
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void patch_ctx_neighbors(int8_t *__restrict cc, const uint8_t *levels,
                                int c, int bhl, intptr_t stride,
                                const int8_t *nz_map_ctx, TX_CLASS tx_class) {
  const unsigned int col = (unsigned int)c >> bhl;
  const uint8_t *const lv_c = levels + c + (col << TX_PAD_HOR_LOG2);
  if (lv_c[0] >= 3) return;
  patch_ctx_neighbors_slow(cc, lv_c, c, bhl, stride, nz_map_ctx, tx_class);
}

// Lower-level contexts are read from cc[] (precomputed by
// av1_get_nz_map_contexts) and cc[] is kept in sync whenever
// update_coeff_eob_nz() modifies levels[].
static AOM_FORCE_INLINE void update_coeff_eob_facade(
    int *si, TX_SIZE tx_size, TX_CLASS tx_class, int bhl, intptr_t stride,
    const int16_t *scan, const LV_MAP_COEFF_COST *txb_costs,
    const tran_low_t *qcoeff, const uint8_t *levels, int max_nz_num, int8_t *cc,
    TxbOptCtx *ctx) {
  int s = *si;
  int local_rate = ctx->accu_rate;
  const int8_t *const nz_map_ctx =
      (tx_class == TX_CLASS_2D) ? av1_nz_map_ctx_offset[tx_size] : NULL;
  while (s >= 1 && ctx->nz_num <= max_nz_num) {
    int ci;
    tran_low_t qc;
    const int (*local_base_cost)[8] = txb_costs->base_cost;
    const tran_low_t *local_qcoeff = qcoeff;
    do {
      ci = scan[s];
      qc = local_qcoeff[ci];
      if (qc != 0) break;
      local_rate += local_base_cost[(uint8_t)cc[ci]][0];
    } while (--s >= 1);
    if (s < 1) break;
    ctx->accu_rate = local_rate;
    const int old_eob = ctx->eob;
    const int old_nz_num = ctx->nz_num;
    const int old_nz0 = ctx->nz_ci[0];
    const int old_nz1 = ctx->nz_ci[1];
    local_rate = update_coeff_eob_nz(s, ci, qc, (uint8_t)cc[ci], tx_class, ctx);
    if (UNLIKELY(ctx->eob != old_eob)) {
      if (old_nz_num > 0) {
        patch_ctx_neighbors(cc, levels, old_nz0, bhl, stride, nz_map_ctx,
                            tx_class);
      }
      if (old_nz_num > 1) {
        patch_ctx_neighbors(cc, levels, old_nz1, bhl, stride, nz_map_ctx,
                            tx_class);
      }
    }
    if (qcoeff[ci] != qc) {
      patch_ctx_neighbors(cc, levels, ci, bhl, stride, nz_map_ctx, tx_class);
    }
    --s;
  }
  ctx->accu_rate = local_rate;
  *si = s;
}

// Lower-level contexts are read from cc[] and cc[] is kept in sync whenever a
// coefficient level is lowered.
static AOM_FORCE_INLINE void update_coeff_simple_facade(
    int *si, TX_SIZE tx_size, TX_CLASS tx_class, int bhl, intptr_t stride,
    const int16_t *scan, const LV_MAP_COEFF_COST *txb_costs,
    const tran_low_t *qcoeff, const uint8_t *levels, int sharpness, int8_t *cc,
    TxbOptCtx *ctx) {
  int s = *si;
  int local_rate = ctx->accu_rate;
  const int8_t *const nz_map_ctx =
      (tx_class == TX_CLASS_2D) ? av1_nz_map_ctx_offset[tx_size] : NULL;
  if (sharpness != 0) {
    while (s >= 1) {
      int ci;
      tran_low_t qc;
      const int *local_base_cost4 = &txb_costs->base_cost[0][4];
      const tran_low_t *local_qcoeff = qcoeff;
      do {
        ci = scan[s];
        qc = local_qcoeff[ci];
        if (UNLIKELY((unsigned int)(qc + 1) > 2)) break;
        local_rate +=
            local_base_cost4[((size_t)(uint8_t)cc[ci] << 3) + (qc & 1)];
      } while (--s >= 1);
      if (s < 1) break;
      local_rate +=
          update_coeff_simple_gt1(ci, qc, (uint8_t)cc[ci], tx_class, ctx);
      if (qcoeff[ci] != qc) {
        patch_ctx_neighbors(cc, levels, ci, bhl, stride, nz_map_ctx, tx_class);
      }
      --s;
    }
  } else {
    while (s >= 1) {
      int ci;
      tran_low_t qc;
      const int (*local_base_cost)[8] = txb_costs->base_cost;
      const tran_low_t *local_qcoeff = qcoeff;
      do {
        ci = scan[s];
        qc = local_qcoeff[ci];
        if (qc != 0) break;
        local_rate += local_base_cost[(uint8_t)cc[ci]][0];
      } while (--s >= 1);
      if (s < 1) break;
      local_rate +=
          update_coeff_simple_nz(ci, qc, (uint8_t)cc[ci], tx_class, ctx);
      if (qcoeff[ci] != qc) {
        patch_ctx_neighbors(cc, levels, ci, bhl, stride, nz_map_ctx, tx_class);
      }
      --s;
    }
  }
  ctx->accu_rate = local_rate;
  *si = 0;
}

static inline int64_t get_txb_rdmult(const struct AV1_COMP *cpi,
                                     const MACROBLOCK *x, const MACROBLOCKD *xd,
                                     int plane, PLANE_TYPE plane_type,
                                     int sharpness, int *is_noise_pattern_out) {
  const MB_MODE_INFO *mbmi = xd->mi[0];
  const int is_inter = is_inter_block(mbmi);
  int is_noise_pattern = 0;
#if CONFIG_AV1_HIGHBITDEPTH
  if (xd->bd > 8 && sharpness == 3 && plane == AOM_PLANE_Y) {
    int64_t src_var, rec_var;
    av1_get_variance_stats(x, &src_var, &rec_var);
    if (src_var > rec_var) {
      const int num_pixels =
          block_size_wide[mbmi->bsize] * block_size_high[mbmi->bsize];
      const int64_t src_var_per_px = src_var / num_pixels;
      if (src_var_per_px < 64) {
        is_noise_pattern = 1;
      }
    }
  }
#endif
  if (is_noise_pattern_out != NULL) *is_noise_pattern_out = is_noise_pattern;
  const int rshift = (cpi->oxcf.tune_cfg.tuning == AOM_TUNE_IQ ||
                      cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2)
                         ? 7
                         : (is_noise_pattern ? 6 : 5);
  const int (*trellis_rd_mult)[2] = cpi->sf.tx_sf.use_chroma_trellis_rd_mult
                                        ? plane_rd_mult_chroma
                                        : plane_rd_mult;
  return ROUND_POWER_OF_TWO(
      (int64_t)x->rdmult * (8 - sharpness) *
          (trellis_rd_mult[is_inter][plane_type] << (2 * (xd->bd - 8))),
      rshift);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
int av1_optimize_txb_multi(const struct AV1_COMP *cpi, MACROBLOCK *x, int plane,
                           int block, TX_SIZE tx_size, TX_TYPE tx_type,
                           const TXB_CTX *const txb_ctx, int *rate_cost,
                           int sharpness) {
  MACROBLOCKD *xd = &x->e_mbd;
  const struct macroblock_plane *p = &x->plane[plane];
  int eob = p->eobs[block];
  const PLANE_TYPE plane_type = get_plane_type(plane);
  int is_noise_pattern = 0;
  const int64_t rdmult = get_txb_rdmult(cpi, x, xd, plane, plane_type,
                                        sharpness, &is_noise_pattern);
  const SCAN_ORDER *scan_order = get_scan(tx_size, tx_type);
  const int16_t *scan = scan_order->scan;
  const int shift = av1_get_tx_scale(tx_size);
  const int16_t *dequant = p->dequant_QTX;
  const qm_val_t *iqmatrix =
      tx_type < IDTX
          ? xd->plane[plane].seg_iqmatrix[xd->mi[0]->segment_id][tx_size]
          : NULL;
  const qm_val_t *qmatrix =
      (cpi->oxcf.tune_cfg.dist_metric == AOM_DIST_METRIC_QM_PSNR &&
       tx_type < IDTX)
          ? xd->plane[plane].seg_qmatrix[xd->mi[0]->segment_id][tx_size]
          : NULL;
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *qcoeff = p->qcoeff + block_offset;
  tran_low_t *dqcoeff = p->dqcoeff + block_offset;
  const tran_low_t *tcoeff = p->coeff + block_offset;
  const CoeffCosts *coeff_costs = &x->coeff_costs;

  const AV1_COMMON *cm = &cpi->common;
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  const TX_CLASS tx_class = tx_type_to_class[tx_type];
  const int bhl = get_txb_bhl(tx_size);
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  assert(height == (1 << bhl));
  const LV_MAP_COEFF_COST *txb_costs =
      &coeff_costs->coeff_costs[txs_ctx][plane_type];
  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const LV_MAP_EOB_COST *txb_eob_costs =
      &coeff_costs->eob_costs[eob_multi_size][plane_type];

  const int non_skip_cost = txb_costs->txb_skip_cost[txb_ctx->txb_skip_ctx][0];
  const int skip_cost = txb_costs->txb_skip_cost[txb_ctx->txb_skip_ctx][1];
  const int eob_cost = get_eob_cost(eob, txb_eob_costs, txb_costs, tx_class);
  int accu_rate = eob_cost;
  int64_t accu_dist = 0;
  int si = eob - 1;
  const int ci = scan[si];
  const tran_low_t qc = qcoeff[ci];
  const tran_low_t abs_qc = abs(qc);
  const int sign = qc < 0;
  const int max_nz_num = 2;
  int nz_ci[3] = { ci, 0, 0 };
  if (abs_qc >= 2) {
    update_coeff_last(&accu_rate, &accu_dist, si, ci, abs_qc, sign, tx_class,
                      bhl, width, rdmult, shift, dequant, txb_costs, tcoeff,
                      qcoeff, dqcoeff, iqmatrix, qmatrix);
    --si;
  } else {
    assert(abs_qc == 1);
    const int coeff_ctx = get_lower_levels_ctx_eob(bhl, width, si);
    accu_rate += txb_costs->base_eob_cost[coeff_ctx][0] + av1_cost_literal(1);
    const tran_low_t tqc = tcoeff[ci];
    const tran_low_t dqc = dqcoeff[ci];
    if (qmatrix == NULL) {
      accu_dist +=
          ((int64_t)dqc * (dqc - (int64_t)tqc * 2)) * (1LL << (2 * shift));
    } else {
      const int64_t scaled_qm = (int64_t)qmatrix[ci] << shift;
      accu_dist += get_coeff_dist_scaled_qm(tqc, dqc, scaled_qm) -
                   get_coeff_dist_scaled_qm(tqc, 0, scaled_qm);
    }
    --si;
  }

  const intptr_t stride = (1 << bhl) + TX_PAD_HOR;
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, height);
  int ac_nz_num = 1;
  int eob_facade_si = 0;
  if (si == 0 && (qcoeff[0] == 0 ||
                  (sharpness != 0 && (unsigned int)(qcoeff[0] + 2) <= 4))) {
    if (tx_class != TX_CLASS_2D) {
      av1_txb_init_levels(qcoeff, width, height, levels);
    }
    const int coeff_ctx =
        (tx_class == TX_CLASS_2D)
            ? 0
            : get_lower_levels_ctx(levels, 0, bhl, tx_size, tx_class);
    const tran_low_t qc_dc = qcoeff[0];
    if (qc_dc == 0) {
      accu_rate += txb_costs->base_cost[coeff_ctx][0];
      if (sharpness == 0) {
        update_skip(&accu_rate, accu_dist, &eob, 1, nz_ci, rdmult, skip_cost,
                    non_skip_cost, qcoeff, dqcoeff);
      }
    } else {
      accu_rate += txb_costs->base_cost[coeff_ctx][abs(qc_dc)] +
                   txb_costs->dc_sign_cost[txb_ctx->dc_sign_ctx][qc_dc < 0];
    }
  } else {
    if (si >= 1 || tx_class != TX_CLASS_2D ||
        (unsigned int)(qcoeff[0] + 2) > 4) {
      av1_txb_init_levels(qcoeff, width, height, levels);
    }

    TxbOptCtx ctx = { rdmult,
                      accu_dist,
                      txb_eob_costs,
                      txb_costs,
                      dequant,
                      tcoeff,
                      qcoeff,
                      dqcoeff,
                      levels,
                      iqmatrix,
                      qmatrix,
                      nz_ci,
                      stride,
                      eob,
                      1,
                      accu_rate,
                      bhl,
                      width,
                      txb_ctx->dc_sign_ctx,
                      shift,
                      dequant[1],
                      sharpness,
                      is_noise_pattern ? 8 : 5 };
    TxbOptCtx *ctx_ptr = &ctx;

    if (si >= 1) {
      // Compute every lower-level context in one SIMD pass instead of a
      // 5-neighbor scalar gather per scanned position.
      DECLARE_ALIGNED(16, int8_t, coeff_ctx_buf[32 * 32]);
      av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class,
                              coeff_ctx_buf);
#define UPDATE_COEFF_PASSES_CASE(tx_class_literal)                             \
  case tx_class_literal:                                                       \
    update_coeff_eob_facade(&si, tx_size, tx_class_literal, bhl, stride, scan, \
                            txb_costs, qcoeff, levels, max_nz_num,             \
                            coeff_ctx_buf, ctx_ptr);                           \
    ac_nz_num = ctx_ptr->nz_num;                                               \
    eob_facade_si = si;                                                        \
    if (si >= 1) {                                                             \
      update_coeff_simple_facade(&si, tx_size, tx_class_literal, bhl, stride,  \
                                 scan, txb_costs, qcoeff, levels, sharpness,   \
                                 coeff_ctx_buf, ctx_ptr);                      \
    }                                                                          \
    break
      switch (tx_class) {
        UPDATE_COEFF_PASSES_CASE(TX_CLASS_2D);
        UPDATE_COEFF_PASSES_CASE(TX_CLASS_HORIZ);
        UPDATE_COEFF_PASSES_CASE(TX_CLASS_VERT);
#undef UPDATE_COEFF_PASSES_CASE
        default: assert(false);
      }
    }

    if (ac_nz_num <= max_nz_num) {
      const int coeff_ctx =
          (tx_class == TX_CLASS_2D)
              ? 0
              : get_lower_levels_ctx(levels, 0, bhl, tx_size, tx_class);
      const tran_low_t qc_dc = qcoeff[0];
      if (qc_dc == 0) {
        ctx_ptr->accu_rate += txb_costs->base_cost[coeff_ctx][0];
      } else if (sharpness != 0 && (unsigned int)(qc_dc + 2) <= 4) {
        ctx_ptr->accu_rate +=
            txb_costs->base_cost[coeff_ctx][abs(qc_dc)] +
            txb_costs->dc_sign_cost[txb_ctx->dc_sign_ctx][qc_dc < 0];
      } else {
        ctx_ptr->accu_rate =
            update_coeff_eob_nz(0, 0, qc_dc, coeff_ctx, tx_class, ctx_ptr);
      }
      if (sharpness == 0 && ctx_ptr->nz_num <= max_nz_num) {
        update_skip(&ctx_ptr->accu_rate, ctx_ptr->accu_dist, &ctx_ptr->eob,
                    ctx_ptr->nz_num, nz_ci, rdmult, skip_cost, non_skip_cost,
                    qcoeff, dqcoeff);
      }
    } else {
      // DC position (si == 0)
      // no need to update accu_dist because it's not used after this point
      update_coeff_dc(&ctx_ptr->accu_rate, tx_size, tx_class, bhl, stride,
                      rdmult, shift, txb_ctx->dc_sign_ctx, dequant, txb_costs,
                      tcoeff, qcoeff, dqcoeff, levels, iqmatrix, qmatrix);
    }
    accu_rate = ctx_ptr->accu_rate;
    eob = ctx_ptr->eob;
  }

  if (eob == 0) {
    accu_rate += skip_cost;
    p->eobs[block] = 0;
    p->txb_entropy_ctx[block] = 0;
  } else {
    const int tx_type_cost = get_tx_type_cost(x, xd, plane, tx_size, tx_type,
                                              cm->features.reduced_tx_set_used);
    accu_rate += non_skip_cost + tx_type_cost;
    p->eobs[block] = eob;
    const tran_low_t dc_val = qcoeff[0];
    const int dc_sign = ((dc_val < 0) + ((dc_val > 0) << 1))
                        << COEFF_CONTEXT_BITS;
    int cul_level = abs(dc_val);
    if (eob > 1 && cul_level < COEFF_CONTEXT_MASK) {
      cul_level += abs(qcoeff[nz_ci[0]]);
      if (ac_nz_num >= 2) cul_level += abs(qcoeff[nz_ci[1]]);
      if (ac_nz_num >= 3) {
        cul_level += abs(qcoeff[nz_ci[2]]);
        for (int c = 1; c <= eob_facade_si && cul_level < COEFF_CONTEXT_MASK;
             ++c) {
          cul_level += abs(qcoeff[scan[c]]);
        }
      }
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

  MACROBLOCKD *xd = &x->e_mbd;
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const MB_MODE_INFO *mbmi = xd->mi[0];
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *qcoeff = p->qcoeff + block_offset;
  tran_low_t *dqcoeff = p->dqcoeff + block_offset;
  const CoeffCosts *coeff_costs = &x->coeff_costs;
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
    if (sharpness == 0) {
      const int64_t rdmult =
          get_txb_rdmult(cpi, x, xd, plane, plane_type, sharpness, NULL);
      const tran_low_t tqc = p->coeff[block_offset];
      const tran_low_t dqc = dqcoeff[0];
      const int shift = av1_get_tx_scale(tx_size);
      const qm_val_t *qmatrix =
          (cpi->oxcf.tune_cfg.dist_metric == AOM_DIST_METRIC_QM_PSNR &&
           tx_type < IDTX)
              ? xd->plane[plane].seg_qmatrix[mbmi->segment_id][tx_size]
              : NULL;
      int64_t accu_dist;
      if (qmatrix == NULL) {
        accu_dist =
            ((int64_t)dqc * (dqc - (int64_t)tqc * 2)) * (1LL << (2 * shift));
      } else {
        const int64_t scaled_qm = (int64_t)qmatrix[0] << shift;
        const int64_t dist0 = get_coeff_dist_scaled_qm(tqc, 0, scaled_qm);
        accu_dist = get_coeff_dist_scaled_qm(tqc, dqc, scaled_qm) - dist0;
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
    const int64_t rdmult =
        get_txb_rdmult(cpi, x, xd, plane, plane_type, sharpness, NULL);
    const tran_low_t tqc = p->coeff[block_offset];
    const tran_low_t dqc = dqcoeff[0];
    const int shift = av1_get_tx_scale(tx_size);
    const qm_val_t *iqmatrix =
        tx_type < IDTX
            ? xd->plane[plane].seg_iqmatrix[mbmi->segment_id][tx_size]
            : NULL;
    const qm_val_t *qmatrix =
        (cpi->oxcf.tune_cfg.dist_metric == AOM_DIST_METRIC_QM_PSNR &&
         tx_type < IDTX)
            ? xd->plane[plane].seg_qmatrix[mbmi->segment_id][tx_size]
            : NULL;
    const int dc_sign_cost =
        txb_costs->dc_sign_cost[txb_ctx->dc_sign_ctx][sign];
    const int dqv = get_dqv(p->dequant_QTX, 0, iqmatrix);
    tran_low_t qc_low, dqc_low;
    get_qc_dqc_low(abs_qc, sign, dqv, shift, &qc_low, &dqc_low);
    const tran_low_t abs_qc_low = abs_qc - 1;
    int rate, rate_low;
    if (LIKELY(abs_qc <= NUM_BASE_LEVELS)) {
      rate = txb_costs->base_eob_cost[0][1] + dc_sign_cost;
      rate_low = txb_costs->base_eob_cost[0][0] + dc_sign_cost;
    } else {
      rate = txb_costs->base_eob_cost[0][2] + dc_sign_cost +
             get_br_cost(abs_qc, txb_costs->lps_cost[0]);
      rate_low =
          txb_costs->base_eob_cost[0][AOMMIN(abs_qc_low, 3) - 1] + dc_sign_cost;
      if (abs_qc_low > NUM_BASE_LEVELS) {
        rate_low += get_br_cost(abs_qc_low, txb_costs->lps_cost[0]);
      }
    }
    int64_t dist_diff_0, dist_diff_low_0;
    if (qmatrix == NULL) {
      const int64_t tqc2 = (int64_t)tqc * 2;
      dist_diff_0 = ((int64_t)dqc * (dqc - tqc2)) * (1LL << (2 * shift));
      dist_diff_low_0 =
          ((int64_t)dqc_low * (dqc_low - tqc2)) * (1LL << (2 * shift));
    } else {
      const int64_t scaled_qm = (int64_t)qmatrix[0] << shift;
      const int64_t dist0 = get_coeff_dist_scaled_qm(tqc, 0, scaled_qm);
      dist_diff_0 = get_coeff_dist_scaled_qm(tqc, dqc, scaled_qm) - dist0;
      dist_diff_low_0 =
          get_coeff_dist_scaled_qm(tqc, dqc_low, scaled_qm) - dist0;
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
    if (sharpness == 0) {
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

  const int tx_type_cost = get_tx_type_cost(
      x, xd, plane, tx_size, tx_type, cpi->common.features.reduced_tx_set_used);
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
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
  const int16_t *const scan = scan_order->scan;
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, height);
  DECLARE_ALIGNED(16, int8_t, coeff_contexts[1024]);

  av1_txb_init_levels(qcoeff, width, height, levels);
  av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class, coeff_contexts);

  const int bhl = get_txb_bhl(tx_size);
  const intptr_t stride = height + TX_PAD_HOR;
  const int (*lps_cost)[16] = coeff_costs->lps_cost;
  int c = eob - 1;
  {
    const int pos = scan[c];
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
                get_br_cost(level, lps_cost[ctx]);
      }
    }
  }
  const int *const base_cost4 = &coeff_costs->base_cost[0][4];
  for (c = eob - 2; c >= 1; --c) {
    const int pos = scan[c];
    const size_t ctx8 = (size_t)(uint8_t)coeff_contexts[pos] << 3;
    const unsigned int level = (unsigned int)abs(qcoeff[pos]);
    if (LIKELY(level <= NUM_BASE_LEVELS)) {
      cost += base_cost4[ctx8 + level];
    } else {
      const int ctx =
          get_br_ctx_simple_fast(levels, pos, bhl, stride, tx_class);
      cost += base_cost4[ctx8 + 3] + get_br_cost(level, lps_cost[ctx]);
    }
  }
  // c == 0 after previous loop
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
        cost += base_cost[coeff_ctx][3] + get_br_cost(level, lps_cost[ctx]);
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

  // coeffs
  int c = eob - 1;
  // eob
  {
    const int pos = scan[c];
    const tran_low_t v = abs(qcoeff[pos]) - 1;
    cost += (v << (AV1_PROB_COST_SHIFT + 2));
  }
  // other coeffs
  for (c = eob - 2; c >= 0; c--) {
    const int pos = scan[c];
    const tran_low_t v = abs(qcoeff[pos]);
    const int idx = AOMMIN(v, 14);

    cost += costLUT[idx];
  }

  // const_term does not contain DC, and log(e) does not contain eob, so both
  // (eob-1)
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
              get_br_cost(level, coeff_costs->lps_cost[0]);
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
                get_br_cost(level1, coeff_costs->lps_cost[7]);
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
                get_br_cost(level0, coeff_costs->lps_cost[ctx0]);
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
