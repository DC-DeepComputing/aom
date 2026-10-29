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

#include "aom_ports/mem.h"
#include "av1/common/idct.h"

static inline void update_coeff_general(
    int *accu_rate, int64_t *accu_dist, int si, int eob, TX_SIZE tx_size,
    TX_CLASS tx_class, int bhl, int width, int64_t rdmult, int shift,
    int dc_sign_ctx, const int16_t *dequant, const int16_t *scan,
    const LV_MAP_COEFF_COST *txb_costs, const tran_low_t *tcoeff,
    tran_low_t *qcoeff, tran_low_t *dqcoeff, uint8_t *levels,
    const qm_val_t *iqmatrix, const qm_val_t *qmatrix) {
  const int dqv = get_dqv(dequant, scan[si], iqmatrix);
  const int ci = scan[si];
  const tran_low_t qc = qcoeff[ci];
  const int is_last = si == (eob - 1);
  const int coeff_ctx = get_lower_levels_ctx_general(
      is_last, si, bhl, width, levels, ci, tx_size, tx_class);
  if (qc == 0) {
    *accu_rate += get_zero_coeff_cost(txb_costs->base_cost[coeff_ctx][0], txb_costs);
  } else {
    const int sign = (qc < 0) ? 1 : 0;
    const tran_low_t abs_qc = abs(qc);
    const tran_low_t tqc = tcoeff[ci];
    const tran_low_t dqc = dqcoeff[ci];
    const int64_t dist = get_coeff_dist(tqc, dqc, shift, qmatrix, ci);
    const int64_t dist0 = get_coeff_dist(tqc, 0, shift, qmatrix, ci);
    const int rate =
        get_coeff_cost_general(is_last, ci, abs_qc, sign, coeff_ctx,
                               dc_sign_ctx, txb_costs, bhl, tx_class, levels);
    const int64_t rd = RDCOST(rdmult, rate, dist);

    tran_low_t qc_low, dqc_low;
    tran_low_t abs_qc_low;
    int64_t dist_low, rd_low;
    int rate_low;
    if (abs_qc == 1) {
      abs_qc_low = qc_low = dqc_low = 0;
      dist_low = dist0;
      rate_low = get_zero_coeff_cost(txb_costs->base_cost[coeff_ctx][0], txb_costs);
    } else {
      get_qc_dqc_low(abs_qc, sign, dqv, shift, &qc_low, &dqc_low);
      abs_qc_low = abs_qc - 1;
      dist_low = get_coeff_dist(tqc, dqc_low, shift, qmatrix, ci);
      rate_low =
          get_coeff_cost_general(is_last, ci, abs_qc_low, sign, coeff_ctx,
                                 dc_sign_ctx, txb_costs, bhl, tx_class, levels);
    }

    rd_low = RDCOST(rdmult, rate_low, dist_low);
    if (rd_low < rd) {
      qcoeff[ci] = qc_low;
      dqcoeff[ci] = dqc_low;
      levels[get_padded_idx(ci, bhl)] = AOMMIN(abs_qc_low, INT8_MAX);
      *accu_rate += rate_low;
      *accu_dist += dist_low - dist0;
    } else {
      *accu_rate += rate;
      *accu_dist += dist - dist0;
    }
  }
}

static AOM_FORCE_INLINE void update_coeff_simple(
    int *accu_rate, int si, int eob, TX_SIZE tx_size, TX_CLASS tx_class,
    int bhl, int64_t rdmult, int shift, const int16_t *dequant,
    const int16_t *scan, const LV_MAP_COEFF_COST *txb_costs,
    const tran_low_t *tcoeff, tran_low_t *qcoeff, tran_low_t *dqcoeff,
    uint8_t *levels, int sharpness, const qm_val_t *iqmatrix,
    const qm_val_t *qmatrix) {
  (void)eob;
  // this simple version assumes the coeff's scan_idx is not DC (scan_idx != 0)
  // and not the last (scan_idx != eob - 1)
  assert(si != eob - 1);
  assert(si > 0);
  const int ci = scan[si];
  const tran_low_t qc = qcoeff[ci];
  const int coeff_ctx =
      get_lower_levels_ctx(levels, ci, bhl, tx_size, tx_class);
  if (qc == 0) {
    *accu_rate += get_zero_coeff_cost(txb_costs->base_cost[coeff_ctx][0], txb_costs);
  } else {
    const tran_low_t abs_qc = abs(qc);
    const tran_low_t abs_tqc = abs(tcoeff[ci]);
    const tran_low_t abs_dqc = abs(dqcoeff[ci]);
    int rate_low = 0;
    const int rate = get_two_coeff_cost_simple(
        ci, abs_qc, coeff_ctx, txb_costs, bhl, tx_class, levels, &rate_low);
    if (abs_dqc < abs_tqc) {
      *accu_rate += rate;
      return;
    }

    const int dqv = get_dqv(dequant, scan[si], iqmatrix);
    const int64_t dist = get_coeff_dist(abs_tqc, abs_dqc, shift, qmatrix, ci);
    const int64_t rd = RDCOST(rdmult, rate, dist);

    const tran_low_t abs_qc_low = abs_qc - 1;
    const tran_low_t abs_dqc_low = (abs_qc_low * dqv) >> shift;
    const int64_t dist_low =
        get_coeff_dist(abs_tqc, abs_dqc_low, shift, qmatrix, ci);
    const int64_t rd_low = RDCOST(simple_low_rdmult, rate_low, dist_low);

    int allow_lower_qc = sharpness ? (abs_qc > 1) : 1;

    if (rd_low < rd && allow_lower_qc) {
      const int sign = (qc < 0) ? 1 : 0;
      qcoeff[ci] = (-sign ^ abs_qc_low) + sign;
      dqcoeff[ci] = (-sign ^ abs_dqc_low) + sign;
      levels[get_padded_idx(ci, bhl)] = AOMMIN(abs_qc_low, INT8_MAX);
      *accu_rate += rate_low;
    } else {
      *accu_rate += rate;
    }
  }
}

static AOM_FORCE_INLINE void update_coeff_eob(
    int *accu_rate, int64_t *accu_dist, int *eob, int *nz_num, int *nz_ci,
    int si, TX_SIZE tx_size, TX_CLASS tx_class, int bhl, int width,
    int dc_sign_ctx, int64_t rdmult, int shift, const int16_t *dequant,
    const int16_t *scan, const LV_MAP_EOB_COST *txb_eob_costs,
    const LV_MAP_COEFF_COST *txb_costs, const tran_low_t *tcoeff,
    tran_low_t *qcoeff, tran_low_t *dqcoeff, uint8_t *levels, int sharpness,
    const qm_val_t *iqmatrix, const qm_val_t *qmatrix) {
  assert(si != *eob - 1);
  const int ci = scan[si];
  const tran_low_t qc = qcoeff[ci];
  const int coeff_ctx =
      get_lower_levels_ctx(levels, ci, bhl, tx_size, tx_class);
  if (qc == 0) {
    *accu_rate += get_zero_coeff_cost(txb_costs->base_cost[coeff_ctx][0], txb_costs);
  } else {
    const int dqv = get_dqv(dequant, scan[si], iqmatrix);
    int lower_level = 0;
    const tran_low_t abs_qc = abs(qc);
    const tran_low_t tqc = tcoeff[ci];
    const tran_low_t dqc = dqcoeff[ci];
    const int sign = (qc < 0) ? 1 : 0;
    const int64_t dist0 = get_coeff_dist(tqc, 0, shift, qmatrix, ci);
    int64_t dist = get_coeff_dist(tqc, dqc, shift, qmatrix, ci) - dist0;
    int rate =
        get_coeff_cost_general(0, ci, abs_qc, sign, coeff_ctx, dc_sign_ctx,
                               txb_costs, bhl, tx_class, levels);
    int64_t rd = RDCOST(rdmult, *accu_rate + rate, *accu_dist + dist);

    tran_low_t qc_low, dqc_low;
    tran_low_t abs_qc_low;
    int64_t dist_low, rd_low;
    int rate_low;

    if (abs_qc == 1) {
      abs_qc_low = 0;
      dqc_low = qc_low = 0;
      dist_low = 0;
      rate_low = get_zero_coeff_cost(txb_costs->base_cost[coeff_ctx][0], txb_costs);
      rd_low = RDCOST(rdmult, *accu_rate + rate_low, *accu_dist);
    } else {
      get_qc_dqc_low(abs_qc, sign, dqv, shift, &qc_low, &dqc_low);
      abs_qc_low = abs_qc - 1;
      dist_low = get_coeff_dist(tqc, dqc_low, shift, qmatrix, ci) - dist0;
      rate_low =
          get_coeff_cost_general(0, ci, abs_qc_low, sign, coeff_ctx,
                                 dc_sign_ctx, txb_costs, bhl, tx_class, levels);
      rd_low = RDCOST(rdmult, *accu_rate + rate_low, *accu_dist + dist_low);
    }

    int lower_level_new_eob = 0;
    const int new_eob = si + 1;
    const int coeff_ctx_new_eob = get_lower_levels_ctx_eob(bhl, width, si);
    const int new_eob_cost =
        get_eob_cost(new_eob, txb_eob_costs, txb_costs, tx_class);
    int rate_coeff_eob =
        new_eob_cost + get_coeff_cost_eob(ci, abs_qc, sign, coeff_ctx_new_eob,
                                          dc_sign_ctx, txb_costs, bhl,
                                          tx_class);
    int64_t dist_new_eob = dist;
    int64_t rd_new_eob = RDCOST(rdmult, rate_coeff_eob, dist_new_eob);

    if (abs_qc_low > 0) {
      const int rate_coeff_eob_low =
          new_eob_cost + get_coeff_cost_eob(ci, abs_qc_low, sign,
                                            coeff_ctx_new_eob, dc_sign_ctx,
                                            txb_costs, bhl, tx_class);
      const int64_t dist_new_eob_low = dist_low;
      const int64_t rd_new_eob_low =
          RDCOST(rdmult, rate_coeff_eob_low, dist_new_eob_low);
      if (rd_new_eob_low < rd_new_eob) {
        lower_level_new_eob = 1;
        rd_new_eob = rd_new_eob_low;
        rate_coeff_eob = rate_coeff_eob_low;
        dist_new_eob = dist_new_eob_low;
      }
    }

    const int qc_threshold = (si <= 5) ? 2 : 1;
    const int allow_lower_qc = sharpness ? abs_qc > qc_threshold : 1;

    if (allow_lower_qc) {
      if (rd_low < rd) {
        lower_level = 1;
        rd = rd_low;
        rate = rate_low;
        dist = dist_low;
      }
    }

    if ((sharpness == 0 || new_eob >= 5) && rd_new_eob < rd) {
      for (int ni = 0; ni < *nz_num; ++ni) {
        int last_ci = nz_ci[ni];
        levels[get_padded_idx(last_ci, bhl)] = 0;
        qcoeff[last_ci] = 0;
        dqcoeff[last_ci] = 0;
      }
      *eob = new_eob;
      *nz_num = 0;
      *accu_rate = rate_coeff_eob;
      *accu_dist = dist_new_eob;
      lower_level = lower_level_new_eob;
    } else {
      *accu_rate += rate;
      *accu_dist += dist;
    }

    if (lower_level) {
      qcoeff[ci] = qc_low;
      dqcoeff[ci] = dqc_low;
      levels[get_padded_idx(ci, bhl)] = AOMMIN(abs_qc_low, INT8_MAX);
    }
    if (qcoeff[ci]) {
      nz_ci[*nz_num] = ci;
      ++*nz_num;
    }
  }
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
static int get_tx_type_cost(const MACROBLOCK *x, const MACROBLOCKD *xd,
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

static AOM_FORCE_INLINE void update_coeff_eob_facade(
    int *accu_rate, int64_t *accu_dist, int *eob, int *nz_num, int *nz_ci,
    int *si, TX_SIZE tx_size, TX_CLASS tx_class, int bhl, int width,
    int dc_sign_ctx, int64_t rdmult, int shift, const int16_t *dequant,
    const int16_t *scan, const LV_MAP_EOB_COST *txb_eob_costs,
    const LV_MAP_COEFF_COST *txb_costs, const tran_low_t *tcoeff,
    tran_low_t *qcoeff, tran_low_t *dqcoeff, uint8_t *levels, int sharpness,
    const qm_val_t *iqmatrix, const qm_val_t *qmatrix, int max_nz_num) {
  for (; *si >= 0 && *nz_num <= max_nz_num; --*si) {
    update_coeff_eob(accu_rate, accu_dist, eob, nz_num, nz_ci, *si, tx_size,
                     tx_class, bhl, width, dc_sign_ctx, rdmult, shift, dequant,
                     scan, txb_eob_costs, txb_costs, tcoeff, qcoeff, dqcoeff,
                     levels, sharpness, iqmatrix, qmatrix);
  }
}

static AOM_FORCE_INLINE void update_coeff_simple_facade(
    int *accu_rate, int *si, int eob, TX_SIZE tx_size, TX_CLASS tx_class,
    int bhl, int64_t rdmult, int shift, const int16_t *dequant,
    const int16_t *scan, const LV_MAP_COEFF_COST *txb_costs,
    const tran_low_t *tcoeff, tran_low_t *qcoeff, tran_low_t *dqcoeff,
    uint8_t *levels, int sharpness, const qm_val_t *iqmatrix,
    const qm_val_t *qmatrix) {
  for (; *si >= 1; --*si) {
    update_coeff_simple(accu_rate, *si, eob, tx_size, tx_class, bhl, rdmult,
                        shift, dequant, scan, txb_costs, tcoeff, qcoeff,
                        dqcoeff, levels, sharpness, iqmatrix, qmatrix);
  }
}

int av1_optimize_txb(const struct AV1_COMP *cpi, MACROBLOCK *x, int plane,
                     int block, TX_SIZE tx_size, TX_TYPE tx_type,
                     const TXB_CTX *const txb_ctx, int *rate_cost,
                     int sharpness) {
  MACROBLOCKD *xd = &x->e_mbd;
  const struct macroblock_plane *p = &x->plane[plane];
  const SCAN_ORDER *scan_order = get_scan(tx_size, tx_type);
  const int16_t *scan = scan_order->scan;
  const int shift = av1_get_tx_scale(tx_size);
  int eob = p->eobs[block];
  const int16_t *dequant = p->dequant_QTX;
  const qm_val_t *iqmatrix =
      av1_get_iqmatrix(&cpi->common.quant_params, xd, plane, tx_size, tx_type);
  const qm_val_t *qmatrix =
      cpi->oxcf.tune_cfg.dist_metric == AOM_DIST_METRIC_QM_PSNR
          ? av1_get_qmatrix(&cpi->common.quant_params, xd, plane, tx_size,
                            tx_type)
          : NULL;
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *qcoeff = p->qcoeff + block_offset;
  tran_low_t *dqcoeff = p->dqcoeff + block_offset;
  const tran_low_t *tcoeff = p->coeff + block_offset;
  const CoeffCosts *coeff_costs = &x->coeff_costs;

  // This function is not called if eob = 0.
  assert(eob > 0);

  const AV1_COMMON *cm = &cpi->common;
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  const TX_CLASS tx_class = tx_type_to_class[tx_type];
  const MB_MODE_INFO *mbmi = xd->mi[0];
  const int bhl = get_txb_bhl(tx_size);
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  assert(height == (1 << bhl));
  const int is_inter = is_inter_block(mbmi);
  const LV_MAP_COEFF_COST *txb_costs =
      &coeff_costs->coeff_costs[txs_ctx][plane_type];
  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const LV_MAP_EOB_COST *txb_eob_costs =
      &coeff_costs->eob_costs[eob_multi_size][plane_type];

  // For the IQ and SSIMULACRA 2 tunings, increase rshift from 2 to 4.
  // This biases trellis quantization towards keeping more coefficients, and
  // together with the IQ and SSIMULACRA2 rdmult adjustment in
  // av1_compute_rd_mult_based_on_qindex(), this helps preserve image
  // features (like repeating patterns and camera noise/film grain), which
  // improves SSIMULACRA 2 scores.
  const int rshift =
      (cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2)
          ? 6
          : ((cpi->oxcf.tune_cfg.tuning == AOM_TUNE_IQ) ? 7 : 5);

  const int(*trellis_rd_mult)[2] = cpi->sf.tx_sf.use_chroma_trellis_rd_mult
                                       ? plane_rd_mult_chroma
                                       : plane_rd_mult;
  const int64_t rdmult = ROUND_POWER_OF_TWO(
      (int64_t)x->rdmult * (8 - sharpness) *
          (trellis_rd_mult[is_inter][plane_type] << (2 * (xd->bd - 8))),
      rshift);

  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, height);

  if (eob > 1) av1_txb_init_levels(qcoeff, width, height, levels);

  // TODO(angirbird): check iqmatrix

  const int non_skip_cost = get_txb_non_skip_cost(txb_costs, txb_ctx->txb_skip_ctx);
  const int skip_cost = get_txb_skip_cost(txb_costs, txb_ctx->txb_skip_ctx);
  const int eob_cost = get_eob_cost(eob, txb_eob_costs, txb_costs, tx_class);
  int accu_rate = eob_cost;
  int64_t accu_dist = 0;
  int si = eob - 1;
  const int ci = scan[si];
  const tran_low_t qc = qcoeff[ci];
  const tran_low_t abs_qc = abs(qc);
  const int sign = qc < 0;
  const int max_nz_num = 2;
  int nz_num = 1;
  int nz_ci[3] = { ci, 0, 0 };
  if (abs_qc >= 2) {
    update_coeff_general(&accu_rate, &accu_dist, si, eob, tx_size, tx_class,
                         bhl, width, rdmult, shift, txb_ctx->dc_sign_ctx,
                         dequant, scan, txb_costs, tcoeff, qcoeff, dqcoeff,
                         levels, iqmatrix, qmatrix);
    --si;
  } else {
    assert(abs_qc == 1);
    const int coeff_ctx = get_lower_levels_ctx_eob(bhl, width, si);
    accu_rate +=
        get_coeff_cost_eob(ci, abs_qc, sign, coeff_ctx, txb_ctx->dc_sign_ctx,
                           txb_costs, bhl, tx_class);
    const tran_low_t tqc = tcoeff[ci];
    const tran_low_t dqc = dqcoeff[ci];
    const int64_t dist = get_coeff_dist(tqc, dqc, shift, qmatrix, ci);
    const int64_t dist0 = get_coeff_dist(tqc, 0, shift, qmatrix, ci);
    accu_dist += dist - dist0;
    --si;
  }

#define UPDATE_COEFF_EOB_CASE(tx_class_literal)                            \
  case tx_class_literal:                                                   \
    update_coeff_eob_facade(                                               \
        &accu_rate, &accu_dist, &eob, &nz_num, nz_ci, &si, tx_size,        \
        tx_class_literal, bhl, width, txb_ctx->dc_sign_ctx, rdmult, shift, \
        dequant, scan, txb_eob_costs, txb_costs, tcoeff, qcoeff, dqcoeff,  \
        levels, sharpness, iqmatrix, qmatrix, max_nz_num);                 \
    break
  switch (tx_class) {
    UPDATE_COEFF_EOB_CASE(TX_CLASS_2D);
    UPDATE_COEFF_EOB_CASE(TX_CLASS_HORIZ);
    UPDATE_COEFF_EOB_CASE(TX_CLASS_VERT);
#undef UPDATE_COEFF_EOB_CASE
    default: assert(false);
  }

  if (si == -1 && nz_num <= max_nz_num && sharpness == 0) {
    update_skip(&accu_rate, accu_dist, &eob, nz_num, nz_ci, rdmult, skip_cost,
                non_skip_cost, qcoeff, dqcoeff);
  }

#define UPDATE_COEFF_SIMPLE_CASE(tx_class_literal)                            \
  case tx_class_literal:                                                      \
    update_coeff_simple_facade(&accu_rate, &si, eob, tx_size,                 \
                               tx_class_literal, bhl, rdmult, shift, dequant, \
                               scan, txb_costs, tcoeff, qcoeff, dqcoeff,      \
                               levels, sharpness, iqmatrix, qmatrix);         \
    break
  switch (tx_class) {
    UPDATE_COEFF_SIMPLE_CASE(TX_CLASS_2D);
    UPDATE_COEFF_SIMPLE_CASE(TX_CLASS_HORIZ);
    UPDATE_COEFF_SIMPLE_CASE(TX_CLASS_VERT);
#undef UPDATE_COEFF_SIMPLE_CASE
    default: assert(false);
  }

  // DC position
  if (si == 0) {
    // no need to update accu_dist because it's not used after this point
    int64_t dummy_dist = 0;
    update_coeff_general(&accu_rate, &dummy_dist, si, eob, tx_size, tx_class,
                         bhl, width, rdmult, shift, txb_ctx->dc_sign_ctx,
                         dequant, scan, txb_costs, tcoeff, qcoeff, dqcoeff,
                         levels, iqmatrix, qmatrix);
  }

  const int tx_type_cost = get_tx_type_cost(x, xd, plane, tx_size, tx_type,
                                            cm->features.reduced_tx_set_used);
  if (eob == 0)
    accu_rate += skip_cost;
  else
    accu_rate += non_skip_cost + tx_type_cost;

  p->eobs[block] = eob;
  p->txb_entropy_ctx[block] =
      av1_get_txb_entropy_context(qcoeff, scan_order, p->eobs[block]);

  *rate_cost = accu_rate;
  return eob;
}

static AOM_FORCE_INLINE int warehouse_efficients_txb(
    const MACROBLOCK *x, const int plane, const int block,
    const TX_SIZE tx_size, const TXB_CTX *const txb_ctx,
    const struct macroblock_plane *p, const int eob,
    const PLANE_TYPE plane_type, const LV_MAP_COEFF_COST *const coeff_costs,
    const MACROBLOCKD *const xd, const TX_TYPE tx_type, const TX_CLASS tx_class,
    int reduced_tx_set_used) {
  const tran_low_t *const qcoeff = p->qcoeff + BLOCK_OFFSET(block);
  const int txb_skip_ctx = txb_ctx->txb_skip_ctx;
  const int bhl = get_txb_bhl(tx_size);
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
  const int16_t *const scan = scan_order->scan;
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, height);
  DECLARE_ALIGNED(16, int8_t, coeff_contexts[MAX_TX_SQUARE]);
  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const LV_MAP_EOB_COST *const eob_costs =
      &x->coeff_costs.eob_costs[eob_multi_size][plane_type];
  int cost = coeff_costs->txb_skip_cost[txb_skip_ctx][0];

  if (eob > 1) av1_txb_init_levels(qcoeff, width, height, levels);

  cost += get_tx_type_cost(x, xd, plane, tx_size, tx_type, reduced_tx_set_used);

  cost += get_eob_cost(eob, eob_costs, coeff_costs, tx_class);

  av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class, coeff_contexts);

  const int(*lps_cost)[COEFF_BASE_RANGE + 1 + COEFF_BASE_RANGE + 1] =
      coeff_costs->lps_cost;
  int c = eob - 1;
  {
    const int pos = scan[c];
    const tran_low_t v = qcoeff[pos];

    if (v) {
      const int sign = AOMSIGN(v);
      const int level = (v ^ sign) - sign;
      const int coeff_ctx = coeff_contexts[pos];
      cost += coeff_costs->base_eob_cost[coeff_ctx][AOMMIN(level, 3) - 1];
      // sign bit cost
      if (level > NUM_BASE_LEVELS) {
        const int ctx = get_br_ctx_eob(pos, bhl, tx_class);
        cost += get_br_cost(level, lps_cost[ctx]);
      }
      if (c) {
        cost += av1_cost_literal(1);
      } else {
        const int sign01 = (sign ^ sign) - sign;
        const int dc_sign_ctx = txb_ctx->dc_sign_ctx;
        cost += coeff_costs->dc_sign_cost[dc_sign_ctx][sign01];
        return cost;
      }
    }
  }
  const int(*base_cost)[8] = coeff_costs->base_cost;
  for (c = eob - 2; c >= 1; --c) {
    const int pos = scan[c];
    const int coeff_ctx = coeff_contexts[pos];
    const tran_low_t v = qcoeff[pos];
    if (!v) {
      cost += base_cost[coeff_ctx][0];
      continue;
    }
    const int level = abs(v);
    cost += base_cost[coeff_ctx][AOMMIN(level, 3)];
    // sign bit cost
    cost += av1_cost_literal(1);
    if (level > NUM_BASE_LEVELS) {
      const int ctx = get_br_ctx(levels, pos, bhl, tx_class);
      cost += get_br_cost(level, lps_cost[ctx]);
    }
  }
  // c == 0 after previous loop
  {
    const int pos = scan[c];
    const tran_low_t v = qcoeff[pos];
    const int coeff_ctx = coeff_contexts[pos];
    if (!v) {
      cost += base_cost[coeff_ctx][0];
    } else {
      const int sign = AOMSIGN(v);
      const int level = (v ^ sign) - sign;
      cost += base_cost[coeff_ctx][AOMMIN(level, 3)];
      // sign bit cost
      const int sign01 = (sign ^ sign) - sign;
      const int dc_sign_ctx = txb_ctx->dc_sign_ctx;
      cost += coeff_costs->dc_sign_cost[dc_sign_ctx][sign01];
      if (level > NUM_BASE_LEVELS) {
        const int ctx = get_br_ctx(levels, pos, bhl, tx_class);
        cost += get_br_cost(level, lps_cost[ctx]);
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

  switch (tx_class) {
    case TX_CLASS_2D:
      return warehouse_efficients_txb(x, plane, block, tx_size, txb_ctx, p, eob,
                                      plane_type, coeff_costs, xd, tx_type,
                                      TX_CLASS_2D, reduced_tx_set_used);

    case TX_CLASS_VERT:
      return warehouse_efficients_txb(x, plane, block, tx_size, txb_ctx, p, eob,
                                      plane_type, coeff_costs, xd, tx_type,
                                      TX_CLASS_VERT, reduced_tx_set_used);

    case TX_CLASS_HORIZ:
      return warehouse_efficients_txb(x, plane, block, tx_size, txb_ctx, p, eob,
                                      plane_type, coeff_costs, xd, tx_type,
                                      TX_CLASS_HORIZ, reduced_tx_set_used);

    default: assert(0 && "Invalid TX_CLASS"); return 0;
  }
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
}    const int dqv = get_dqv(dequant, scan[si], iqmatrix);
    int64_t simple_low_rdmult = rdmult;
    if (txb_costs->ssim2_px > 0) {
      const int px = txb_costs->ssim2_px;
      const int bq = txb_costs->ssim2_base_q;
      const int is_portrait = txb_costs->ssim2_is_portrait;
      int slr_opt = 0;
      if ((px >= 500000 && px < 1000000 && bq >= 75 && bq < 110) ||
          (px >= 2793000 && px < 3000000 && is_portrait && bq >= 28 && bq < 50)) {
        slr_opt = 6;
      } else if (px >= 3000000 && !is_portrait && bq >= 75 && bq < 110) {
        slr_opt = 7;
      }
      if (slr_opt == 6) {
        simple_low_rdmult = (rdmult * 63) >> 6;
      } else if (slr_opt == 7) {
        simple_low_rdmult = (rdmult * 65) >> 6;
      }

      int sq2_opt = 0;
      if (px >= 3000000 && !is_portrait && bq >= 75 && bq < 110) {
        sq2_opt = 8;
      } else if (px >= 2700000 && px < 2793000 && bq >= 75 && bq < 110) {
        sq2_opt = 12;
      } else if (px >= 2000000 && px < 2700000 && bq >= 75 && bq < 110) {
        sq2_opt = 13;
      } else if (px >= 2793000 && px < 3000000 && is_portrait && bq >= 75 && bq < 110) {
        sq2_opt = 14;
      }
      if (sq2_opt > 0) {
        if (abs_qc == 2) {
          if (sq2_opt == 12) {
            simple_low_rdmult = (simple_low_rdmult * 65) >> 6;
          }
        } else if (abs_qc >= 3) {
          if (sq2_opt == 8) {
            simple_low_rdmult = (simple_low_rdmult * 31) >> 5;
          } else if (sq2_opt == 13) {
            simple_low_rdmult = (simple_low_rdmult * 63) >> 6;
          } else if (sq2_opt == 14) {
            simple_low_rdmult = (simple_low_rdmult * 65) >> 6;
          }
        }
      }
    }
    const int64_t dist = get_coeff_dist(abs_tqc, abs_dqc, shift, qmatrix, ci);
    const int64_t rd = RDCOST(simple_low_rdmult, rate, dist);

    const tran_low_t abs_qc_low = abs_qc - 1;
    const tran_low_t abs_dqc_low = (abs_qc_low * dqv) >> shift;
    const int64_t dist_low =
        get_coeff_dist(abs_tqc, abs_dqc_low, shift, qmatrix, ci);
    const int64_t rd_low = RDCOST(simple_low_rdmult, rate_low, dist_low);

    int simple_qc_thresh = 1;
    if (txb_costs->ssim2_px > 0) {
      const int px = txb_costs->ssim2_px;
      const int bq = txb_costs->ssim2_base_q;
      const int is_portrait = txb_costs->ssim2_is_portrait;
      int sqt_opt = 0;
      if ((px >= 15000 && px < 30000 && bq >= 140 && bq < 165) ||
          (px >= 2793000 && px < 3000000 && is_portrait && bq >= 140 && bq < 165)) {
        sqt_opt = 1;
      } else if ((px >= 500000 && px < 1000000 && bq >= 75 && bq < 110) ||
                 (px >= 200000 && px < 400000 && bq >= 28 && bq < 50)) {
        sqt_opt = 8;
      } else if (px >= 2700000 && px < 2793000 && bq >= 140 && bq < 165) {
        sqt_opt = 9;
      } else if (px >= 50000 && px < 100000 && bq <= 16) {
        sqt_opt = 13;
      }
      int sqt_si = 0;
      int sqt0_si = 0;
      if (sqt_opt == 1) {
        sqt_si = 1;
      } else if (sqt_opt == 8) {
        sqt0_si = 6;
      } else if (sqt_opt == 9) {
        sqt0_si = 8;
      } else if (sqt_opt == 13) {
        sqt0_si = 16;
      }
      if (sqt_si > 0 && si <= sqt_si) {
        simple_qc_thresh = 2;
      } else if (sqt0_si > 0 && si >= sqt0_si) {
        simple_qc_thresh = 0;
      }
    }
    int allow_lower_qc = sharpness ? (abs_qc > simple_qc_thresh) : 1;

    if (rd_low < rd && allow_lower_qc) {
      const int sign = (qc < 0) ? 1 : 0;
      qcoeff[ci] = (-sign ^ abs_qc_low) + sign;
      dqcoeff[ci] = (-sign ^ abs_dqc_low) + sign;
      levels[get_padded_idx(ci, bhl)] = AOMMIN(abs_qc_low, INT8_MAX);
      *accu_rate += rate_low;
    } else {
      *accu_rate += rate;
    }
  }
}

static AOM_FORCE_INLINE void update_coeff_eob(
    int *accu_rate, int64_t *accu_dist, int *eob, int *nz_num, int *nz_ci,
    int si, TX_SIZE tx_size, TX_CLASS tx_class, int bhl, int width,
    int dc_sign_ctx, int64_t rdmult, int shift, const int16_t *dequant,
    const int16_t *scan, const LV_MAP_EOB_COST *txb_eob_costs,
    const LV_MAP_COEFF_COST *txb_costs, const tran_low_t *tcoeff,
    tran_low_t *qcoeff, tran_low_t *dqcoeff, uint8_t *levels, int sharpness,
    const qm_val_t *iqmatrix, const qm_val_t *qmatrix) {
  assert(si != *eob - 1);
  const int ci = scan[si];
  const tran_low_t qc = qcoeff[ci];
  const int coeff_ctx =
      get_lower_levels_ctx(levels, ci, bhl, tx_size, tx_class);
  if (qc == 0) {
    *accu_rate += get_zero_coeff_cost(txb_costs->base_cost[coeff_ctx][0], txb_costs);
  } else {
    const int dqv = get_dqv(dequant, scan[si], iqmatrix);
    int lower_level = 0;
    const tran_low_t abs_qc = abs(qc);
    const tran_low_t tqc = tcoeff[ci];
    const tran_low_t dqc = dqcoeff[ci];
    const int sign = (qc < 0) ? 1 : 0;
    const int64_t dist0 = get_coeff_dist(tqc, 0, shift, qmatrix, ci);
    int64_t dist = get_coeff_dist(tqc, dqc, shift, qmatrix, ci) - dist0;
    int rate =
        get_coeff_cost_general(0, ci, abs_qc, sign, coeff_ctx, dc_sign_ctx,
                               txb_costs, bhl, tx_class, levels);
    int64_t rd = RDCOST(rdmult, *accu_rate + rate, *accu_dist + dist);

    tran_low_t qc_low, dqc_low;
    tran_low_t abs_qc_low;
    int64_t dist_low, rd_low;
    int rate_low;

    if (abs_qc == 1) {
      abs_qc_low = 0;
      dqc_low = qc_low = 0;
      dist_low = 0;
      rate_low = get_eob_zero_cost(txb_costs->base_cost[coeff_ctx][0]);
      rd_low = RDCOST(rdmult, *accu_rate + rate_low, *accu_dist);
    } else {
      get_qc_dqc_low(abs_qc, sign, dqv, shift, &qc_low, &dqc_low);
      abs_qc_low = abs_qc - 1;
      dist_low = get_coeff_dist(tqc, dqc_low, shift, qmatrix, ci) - dist0;
      rate_low =
          get_coeff_cost_general(0, ci, abs_qc_low, sign, coeff_ctx,
                                 dc_sign_ctx, txb_costs, bhl, tx_class, levels);
      rd_low = RDCOST(rdmult, *accu_rate + rate_low, *accu_dist + dist_low);
    }

    int lower_level_new_eob = 0;
    const int new_eob = si + 1;
    const int coeff_ctx_new_eob = get_lower_levels_ctx_eob(bhl, width, si);
    const int new_eob_cost =
        get_eob_cost(new_eob, txb_eob_costs, txb_costs, tx_class);
    int rate_coeff_eob =
        new_eob_cost + get_coeff_cost_eob(ci, abs_qc, sign, coeff_ctx_new_eob,
                                          dc_sign_ctx, txb_costs, bhl,
                                          tx_class);
    int64_t dist_new_eob = dist;
    int64_t new_eob_rdmult = rdmult;
    int64_t eob_low_rdmult = rdmult;
    int elr_opt = 0;
    int eq2_opt = 0;
    if (txb_costs->ssim2_px > 0) {
      const int px = txb_costs->ssim2_px;
      const int bq = txb_costs->ssim2_base_q;
      const int is_portrait = txb_costs->ssim2_is_portrait;
      int ner_opt = 0;
      if ((px >= 15000 && px < 30000 && bq >= 140 && bq < 165) ||
          (px >= 500000 && px < 1000000 && bq >= 28 && bq < 50)) {
        ner_opt = 1;
      } else if ((px >= 50000 && px < 100000 && bq >= 75 && bq < 110) ||
                 (px >= 3000000 && !is_portrait && bq >= 75 && bq < 110) ||
                 (px >= 3000000 && is_portrait && bq <= 16)) {
        ner_opt = 2;
      } else if (px >= 200000 && px < 400000 && bq >= 28 && bq < 50) {
        ner_opt = 3;
      } else if (px >= 500000 && px < 1000000 && bq >= 75 && bq < 110) {
        ner_opt = 4;
      } else if (px >= 2793000 && px < 3000000 && !is_portrait && bq >= 75 && bq < 110) {
        ner_opt = 7;
      } else if ((px >= 30000 && px < 50000 && bq >= 75 && bq < 110) ||
                 (px >= 3000000 && !is_portrait && bq >= 140 && bq < 165)) {
        ner_opt = 8;
      } else if (px >= 200000 && px < 400000 && bq >= 75 && bq < 110) {
        ner_opt = 9;
      }
      if (ner_opt == 1) {
        new_eob_rdmult = (rdmult * 15) >> 4;
      } else if (ner_opt == 2) {
        new_eob_rdmult = (rdmult * 17) >> 4;
      } else if (ner_opt == 3) {
        new_eob_rdmult = (rdmult * 31) >> 5;
      } else if (ner_opt == 4) {
        new_eob_rdmult = (rdmult * 33) >> 5;
      } else if (ner_opt == 7) {
        new_eob_rdmult = (rdmult * 65) >> 6;
      } else if (ner_opt == 8) {
        new_eob_rdmult = (rdmult * 7) >> 3;
      } else if (ner_opt == 9) {
        new_eob_rdmult = (rdmult * 9) >> 3;
      }

      if (px >= 3000000 && !is_portrait && bq >= 75 && bq < 110) {
        elr_opt = 2;
      } else if ((px >= 100000 && px < 200000 && bq >= 75 && bq < 110) ||
                 (px >= 3000000 && is_portrait && bq >= 75 && bq < 110)) {
        elr_opt = 3;
      } else if (px >= 3000000 && !is_portrait && bq >= 28 && bq < 50) {
        elr_opt = 6;
      } else if ((px >= 2793000 && px < 3000000 && !is_portrait && bq >= 28 && bq < 50) ||
                 (px >= 2000000 && px < 2700000 && bq >= 28 && bq < 50)) {
        elr_opt = 7;
      } else if (px >= 500000 && px < 1000000 && bq <= 16) {
        elr_opt = 8;
      } else if (px >= 2793000 && px < 3000000 && !is_portrait && bq >= 75 && bq < 110) {
        elr_opt = 9;
      }
      if (elr_opt == 2) {
        eob_low_rdmult = (rdmult * 17) >> 4;
      } else if (elr_opt == 3) {
        eob_low_rdmult = (rdmult * 31) >> 5;
      } else if (elr_opt == 6) {
        eob_low_rdmult = (rdmult * 63) >> 6;
      } else if (elr_opt == 7) {
        eob_low_rdmult = (rdmult * 65) >> 6;
      } else if (elr_opt == 8) {
        eob_low_rdmult = (rdmult * 7) >> 3;
      } else if (elr_opt == 9) {
        eob_low_rdmult = (rdmult * 9) >> 3;
      }

      if (px >= 2793000 && px < 3000000 && !is_portrait && bq >= 75 && bq < 110) {
        eq2_opt = 4;
      } else if (px >= 2700000 && px < 2793000 && bq >= 28 && bq < 50) {
        eq2_opt = 6;
      } else if (px >= 2793000 && px < 3000000 && !is_portrait && bq >= 28 && bq < 50) {
        eq2_opt = 7;
      } else if (px >= 2000000 && px < 2700000 && bq >= 28 && bq < 50) {
        eq2_opt = 11;
      }
      if (eq2_opt > 0) {
        if (abs_qc == 2) {
          if (eq2_opt == 6 || eq2_opt == 7) {
            eob_low_rdmult =
                (eob_low_rdmult * ((eq2_opt == 6) ? 63 : 65)) >> 6;
            elr_opt = 99;
          }
        } else if (abs_qc >= 3) {
          if (eq2_opt == 4) {
            eob_low_rdmult = (eob_low_rdmult * 33) >> 5;
            elr_opt = 99;
          } else if (eq2_opt == 11) {
            eob_low_rdmult = (eob_low_rdmult * 15) >> 4;
            elr_opt = 99;
          }
        }
      }
    }
    int64_t rd_new_eob = RDCOST(new_eob_rdmult, rate_coeff_eob, dist_new_eob);

    if (abs_qc_low > 0) {
      const int rate_coeff_eob_low =
          new_eob_cost + get_coeff_cost_eob(ci, abs_qc_low, sign,
                                            coeff_ctx_new_eob, dc_sign_ctx,
                                            txb_costs, bhl, tx_class);
      const int64_t dist_new_eob_low = dist_low;
      const int64_t rd_new_eob_low =
          RDCOST(new_eob_rdmult, rate_coeff_eob_low, dist_new_eob_low);
      const int take_new_eob_low = (rd_new_eob_low < rd_new_eob);
      if (take_new_eob_low) {
        lower_level_new_eob = 1;
        rd_new_eob = rd_new_eob_low;
        rate_coeff_eob = rate_coeff_eob_low;
        dist_new_eob = dist_new_eob_low;
      }
    }

    int qc_si_thresh = 5;
    int min_new_eob = 5;
    int eob_qc0_si = 0;
    if (txb_costs->ssim2_px > 0) {
      const int px = txb_costs->ssim2_px;
      const int bq = txb_costs->ssim2_base_q;
      const int is_portrait = txb_costs->ssim2_is_portrait;
      int eq0_opt = 0;
      if (px >= 100000 && px < 200000 && bq >= 75 && bq < 110) {
        eq0_opt = 4;
      } else if (px >= 50000 && px < 100000 && bq >= 140 && bq < 165) {
        eq0_opt = 9;
      } else if (px >= 2793000 && px < 3000000 && !is_portrait && bq >= 75 && bq < 110) {
        eq0_opt = 11;
      }
      if (eq0_opt == 4) {
        eob_qc0_si = 6;
      } else if (eq0_opt == 9) {
        eob_qc0_si = 16;
      } else if (eq0_opt == 11) {
        eob_qc0_si = 20;
      }

      int mne_opt = 0;
      if ((px >= 3000000 && is_portrait && bq >= 140 && bq < 165) ||
          (px >= 2793000 && px < 3000000 && !is_portrait && bq >= 28 && bq < 50) ||
          (px >= 30000 && px < 50000 && bq >= 75 && bq < 110)) {
        mne_opt = 1;
      } else if ((px >= 2700000 && px < 2793000 && bq <= 16) ||
                 (px >= 50000 && px < 100000 && bq >= 140 && bq < 165) ||
                 (px >= 500000 && px < 1000000 && bq >= 75 && bq < 110)) {
        mne_opt = 2;
      } else if (px >= 2000000 && px < 2700000 && bq >= 140 && bq < 165) {
        mne_opt = 3;
      } else if ((px >= 3000000 && !is_portrait && bq <= 16) ||
                 (px >= 2793000 && px < 3000000 && !is_portrait &&
                  ((bq >= 140 && bq < 165) || bq <= 16)) ||
                 (px >= 200000 && px < 400000 && bq >= 28 && bq < 50)) {
        mne_opt = 4;
      }
      if (mne_opt >= 1 && mne_opt <= 4) {
        min_new_eob = mne_opt;
      }

      int qst_opt = 0;
      if ((px >= 2793000 && px < 3000000 && is_portrait && bq >= 140 && bq < 165) ||
          (px >= 100000 && px < 200000 && bq >= 140 && bq < 165)) {
        qst_opt = 1;
      } else if (px >= 200000 && px < 400000 && bq >= 75 && bq < 110) {
        qst_opt = 2;
      } else if (px >= 50000 && px < 100000 && bq >= 28 && bq < 50) {
        qst_opt = 3;
      } else if ((px >= 3000000 && !is_portrait && bq >= 28 && bq < 50) ||
                 (px >= 3000000 && is_portrait && bq >= 75 && bq < 110)) {
        qst_opt = 4;
      } else if ((px >= 2793000 && px < 3000000 && is_portrait && bq >= 75 && bq < 110) ||
                 (px >= 500000 && px < 1000000 && bq >= 28 && bq < 50)) {
        qst_opt = 6;
      } else if (px >= 2000000 && px < 2700000 && bq >= 75 && bq < 110) {
        qst_opt = 7;
      } else if (px >= 50000 && px < 100000 && bq <= 16) {
        qst_opt = 11;
      }
      if (qst_opt == 1) {
        qc_si_thresh = -1;
      } else if (qst_opt == 2) {
        qc_si_thresh = 0;
      } else if (qst_opt == 3) {
        qc_si_thresh = 1;
      } else if (qst_opt == 4) {
        qc_si_thresh = 2;
      } else if (qst_opt == 6) {
        qc_si_thresh = 7;
      } else if (qst_opt == 7) {
        qc_si_thresh = 4;
      } else if (qst_opt == 11) {
        qc_si_thresh = 10;
      }
    }
    int qc_threshold = (si <= qc_si_thresh) ? 2 : 1;
    if (eob_qc0_si > 0 && si >= eob_qc0_si) {
      qc_threshold = 0;
    }
    const int allow_lower_qc = sharpness ? abs_qc > qc_threshold : 1;

    if (allow_lower_qc) {
      int take_low = (rd_low < rd);
      if (elr_opt > 0) {
        take_low = (RDCOST(eob_low_rdmult, rate_low, dist_low) <
                    RDCOST(eob_low_rdmult, rate, dist));
      }
      if (take_low) {
        lower_level = 1;
        rd = rd_low;
        rate = rate_low;
        dist = dist_low;
      }
    }

    if ((sharpness == 0 || new_eob >= min_new_eob) && rd_new_eob < rd) {
      for (int ni = 0; ni < *nz_num; ++ni) {
        int last_ci = nz_ci[ni];
        levels[get_padded_idx(last_ci, bhl)] = 0;
        qcoeff[last_ci] = 0;
        dqcoeff[last_ci] = 0;
      }
      *eob = new_eob;
      *nz_num = 0;
      *accu_rate = rate_coeff_eob;
      *accu_dist = dist_new_eob;
      lower_level = lower_level_new_eob;
    } else {
      *accu_rate += rate;
      *accu_dist += dist;
    }

    if (lower_level) {
      qcoeff[ci] = qc_low;
      dqcoeff[ci] = dqc_low;
      levels[get_padded_idx(ci, bhl)] = AOMMIN(abs_qc_low, INT8_MAX);
    }
    if (qcoeff[ci]) {
      nz_ci[*nz_num] = ci;
      ++*nz_num;
    }
  }
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
static int get_tx_type_cost(const MACROBLOCK *x, const MACROBLOCKD *xd,
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
        int cost = x->mode_costs.intra_tx_type_costs[ext_tx_set][square_tx_size]
                                                    [intra_dir][tx_type];
        const LV_MAP_COEFF_COST *txb_costs = &x->coeff_costs.coeff_costs[0][0];
        if (txb_costs->ssim2_px > 0) {
          const int px = txb_costs->ssim2_px;
          const int bq = txb_costs->ssim2_base_q;
          const int is_portrait = txb_costs->ssim2_is_portrait;
          int ttc_opt = 0;
          if (px >= 2793000 && px < 3000000 && is_portrait && bq >= 75 && bq < 110) {
            ttc_opt = 1;
          } else if ((px >= 2700000 && px < 2793000 && bq >= 140 && bq < 165) ||
                     (px >= 500000 && px < 1000000 && bq <= 16) ||
                     (px < 1000 && bq >= 28 && bq < 50)) {
            ttc_opt = 2;
          } else if (px >= 3000000 && !is_portrait && bq >= 140 && bq < 165) {
            ttc_opt = 3;
          } else if (px >= 1500 && px < 15000 && bq >= 140 && bq < 165) {
            ttc_opt = 4;
          } else if (px >= 2700000 && px < 2793000 && bq >= 28 && bq < 50) {
            ttc_opt = 6;
          } else if ((px >= 3000000 && is_portrait && bq >= 75 && bq < 110) ||
                     (px >= 50000 && px < 100000 && bq <= 16) ||
                     (px >= 100000 && px < 200000 && bq >= 28 && bq < 50)) {
            ttc_opt = 7;
          } else if ((px >= 2793000 && px < 3000000 && !is_portrait && bq >= 75 && bq < 110) ||
                     (px >= 1000 && px < 1500 && bq <= 16)) {
            ttc_opt = 9;
          }
          if (ttc_opt == 1) {
            cost = (cost * 7) >> 3;
          } else if (ttc_opt == 2) {
            cost = (cost * 9) >> 3;
          } else if (ttc_opt == 3) {
            cost = (cost * 15) >> 4;
          } else if (ttc_opt == 4) {
            cost = (cost * 17) >> 4;
          } else if (ttc_opt == 6) {
            cost = (cost * 31) >> 5;
          } else if (ttc_opt == 7) {
            cost = (cost * 33) >> 5;
          } else if (ttc_opt == 9) {
            cost = (cost * 65) >> 6;
          }
        }
        return cost;
      }
    }
  }
  return 0;
}

static AOM_FORCE_INLINE void update_coeff_eob_facade(
    int *accu_rate, int64_t *accu_dist, int *eob, int *nz_num, int *nz_ci,
    int *si, TX_SIZE tx_size, TX_CLASS tx_class, int bhl, int width,
    int dc_sign_ctx, int64_t rdmult, int shift, const int16_t *dequant,
    const int16_t *scan, const LV_MAP_EOB_COST *txb_eob_costs,
    const LV_MAP_COEFF_COST *txb_costs, const tran_low_t *tcoeff,
    tran_low_t *qcoeff, tran_low_t *dqcoeff, uint8_t *levels, int sharpness,
    const qm_val_t *iqmatrix, const qm_val_t *qmatrix, int max_nz_num) {
  for (; *si >= 0 && *nz_num <= max_nz_num; --*si) {
    update_coeff_eob(accu_rate, accu_dist, eob, nz_num, nz_ci, *si, tx_size,
                     tx_class, bhl, width, dc_sign_ctx, rdmult, shift, dequant,
                     scan, txb_eob_costs, txb_costs, tcoeff, qcoeff, dqcoeff,
                     levels, sharpness, iqmatrix, qmatrix);
  }
}

static AOM_FORCE_INLINE void update_coeff_simple_facade(
    int *accu_rate, int *si, int eob, TX_SIZE tx_size, TX_CLASS tx_class,
    int bhl, int64_t rdmult, int shift, const int16_t *dequant,
    const int16_t *scan, const LV_MAP_COEFF_COST *txb_costs,
    const tran_low_t *tcoeff, tran_low_t *qcoeff, tran_low_t *dqcoeff,
    uint8_t *levels, int sharpness, const qm_val_t *iqmatrix,
    const qm_val_t *qmatrix) {
  for (; *si >= 1; --*si) {
    update_coeff_simple(accu_rate, *si, eob, tx_size, tx_class, bhl, rdmult,
                        shift, dequant, scan, txb_costs, tcoeff, qcoeff,
                        dqcoeff, levels, sharpness, iqmatrix, qmatrix);
  }
}

int av1_optimize_txb(const struct AV1_COMP *cpi, MACROBLOCK *x, int plane,
                     int block, TX_SIZE tx_size, TX_TYPE tx_type,
                     const TXB_CTX *const txb_ctx, int *rate_cost,
                     int sharpness) {
  MACROBLOCKD *xd = &x->e_mbd;
  const struct macroblock_plane *p = &x->plane[plane];
  const SCAN_ORDER *scan_order = get_scan(tx_size, tx_type);
  const int16_t *scan = scan_order->scan;
  const int shift = av1_get_tx_scale(tx_size);
  int eob = p->eobs[block];
  const int16_t *dequant = p->dequant_QTX;
  const qm_val_t *iqmatrix =
      av1_get_iqmatrix(&cpi->common.quant_params, xd, plane, tx_size, tx_type);
  const qm_val_t *qmatrix =
      cpi->oxcf.tune_cfg.dist_metric == AOM_DIST_METRIC_QM_PSNR
          ? av1_get_qmatrix(&cpi->common.quant_params, xd, plane, tx_size,
                            tx_type)
          : NULL;
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *qcoeff = p->qcoeff + block_offset;
  tran_low_t *dqcoeff = p->dqcoeff + block_offset;
  const tran_low_t *tcoeff = p->coeff + block_offset;
  const CoeffCosts *coeff_costs = &x->coeff_costs;

  // This function is not called if eob = 0.
  assert(eob > 0);

  const AV1_COMMON *cm = &cpi->common;
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  const TX_CLASS tx_class = tx_type_to_class[tx_type];
  const MB_MODE_INFO *mbmi = xd->mi[0];
  const int bhl = get_txb_bhl(tx_size);
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  assert(height == (1 << bhl));
  const int is_inter = is_inter_block(mbmi);
  const LV_MAP_COEFF_COST *txb_costs =
      &coeff_costs->coeff_costs[txs_ctx][plane_type];
  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const LV_MAP_EOB_COST *txb_eob_costs =
      &coeff_costs->eob_costs[eob_multi_size][plane_type];

  // For the IQ and SSIMULACRA 2 tunings, increase rshift from 2 to 4.
  // This biases trellis quantization towards keeping more coefficients, and
  // together with the IQ and SSIMULACRA2 rdmult adjustment in
  // av1_compute_rd_mult_based_on_qindex(), this helps preserve image
  // features (like repeating patterns and camera noise/film grain), which
  // improves SSIMULACRA 2 scores.
  const int rshift =
      (cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2)
          ? 6
          : ((cpi->oxcf.tune_cfg.tuning == AOM_TUNE_IQ) ? 7 : 5);

  const int(*trellis_rd_mult)[2] = cpi->sf.tx_sf.use_chroma_trellis_rd_mult
                                       ? plane_rd_mult_chroma
                                       : plane_rd_mult;
  int plane_mult = trellis_rd_mult[is_inter][plane_type];
  const int t_px = cpi->common.width * cpi->common.height;
  const int t_q = cpi->common.quant_params.base_qindex;
  const int t_portrait = (cpi->common.height > cpi->common.width);
  int64_t rdmult = ROUND_POWER_OF_TWO(
      (int64_t)x->rdmult * (8 - sharpness) *
          (plane_mult << (2 * (xd->bd - 8))),
      rshift);

  int eob_sharpness = sharpness;
  const int simple_sharpness = sharpness;
  bool allow_update_skip = (sharpness == 0);
  if (cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2) {
    if (t_px >= 15000 && t_px < 1000000) {
      allow_update_skip =
          (t_px >= 30000 && t_px < 500000 && t_q >= 140 && t_q < 165) ||
          (!(t_px >= 200000 && t_px < 500000) && t_q >= 28 && t_q < 50);
    } else if (t_px <= 1500) {
      allow_update_skip = (t_q >= 28 && t_q < 50);
    }
  }

  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, height);

  if (eob > 1) av1_txb_init_levels(qcoeff, width, height, levels);

  const int non_skip_cost =
      get_txb_non_skip_cost(txb_costs, txb_ctx->txb_skip_ctx);
  const int skip_cost = get_txb_skip_cost(txb_costs, txb_ctx->txb_skip_ctx);
  const int eob_cost = get_eob_cost(eob, txb_eob_costs, txb_costs, tx_class);
  int accu_rate = eob_cost;
  int64_t accu_dist = 0;
  int si = eob - 1;
  const int ci = scan[si];
  const tran_low_t qc = qcoeff[ci];
  const tran_low_t abs_qc = abs(qc);
  const int sign = qc < 0;
  int max_nz_num = 2;
  if (cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2) {
    if (t_px >= 200000 && t_px < 400000 && t_q >= 140 && t_q < 165) {
      max_nz_num = 1;
    } else if (((t_px >= 2700000 && t_px < 2793000) ||
                (t_px >= 50000 && t_px < 100000)) &&
               t_q >= 28 && t_q < 50) {
      max_nz_num = 3;
    }
  }
  int64_t eob_rdmult = rdmult;
  int64_t simple_rdmult = rdmult;
  int64_t dc_rdmult = rdmult;
  if (cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2) {
    if (((t_px >= 2000000 && t_px < 2700000) ||
         (t_px >= 100000 && t_px < 200000)) &&
        t_q <= 16) {
      eob_rdmult = (rdmult * 15) >> 4;
    } else if (t_px >= 100000 && t_px < 200000 &&
               ((t_q >= 140 && t_q < 165) || (t_q >= 75 && t_q < 110))) {
      eob_rdmult = (rdmult * 17) >> 4;
    } else if ((((t_px >= 2793000 && t_px < 3000000 && !t_portrait) ||
                 (t_px >= 200000 && t_px < 400000)) &&
                t_q >= 75 && t_q < 110) ||
               (t_px >= 500000 && t_px < 1000000 && t_q >= 28 && t_q < 50)) {
      simple_rdmult = (rdmult * 17) >> 4;
    } else if (t_px >= 50000 && t_px < 100000 && t_q >= 75 && t_q < 110) {
      eob_rdmult = (rdmult * 31) >> 5;
    } else if (t_px >= 2793000 && t_px < 3000000 && t_portrait &&
               t_q >= 140 && t_q < 165) {
      eob_rdmult = (rdmult * 33) >> 5;
    } else if (t_px >= 2000000 && t_px < 2700000 && t_q >= 75 && t_q < 110) {
      simple_rdmult = (rdmult * 31) >> 5;
    }

    int dcr_opt = 0;
    if ((t_px >= 2793000 && t_px < 3000000 && !t_portrait && t_q >= 140 &&
         t_q < 165) ||
        (t_px >= 400000 && t_px < 500000 && t_q >= 75 && t_q < 110)) {
      dcr_opt = 1;
    } else if (((t_px >= 2793000 && !t_portrait) && t_q >= 28 && t_q < 50) ||
               (t_px >= 2000000 && t_px < 2700000 && t_q >= 75 && t_q < 110)) {
      dcr_opt = 4;
    } else if (t_px >= 2700000 && t_px < 2793000 && t_q >= 28 && t_q < 50) {
      dcr_opt = 8;
    }
    if (dcr_opt == 1) {
      dc_rdmult = (rdmult * 7) >> 3;
    } else if (dcr_opt == 4) {
      dc_rdmult = (rdmult * 17) >> 4;
    } else if (dcr_opt == 8) {
      dc_rdmult = (rdmult * 63) >> 6;
    }
  }
  int nz_num = 1;
  int nz_ci[5] = { ci, 0, 0, 0, 0 };
  if (abs_qc >= 2) {
    int64_t last_rdmult = (si == 0) ? dc_rdmult : rdmult;
    if (si > 0 && cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2) {
      int lrm_opt = 0;
      if ((t_px >= 500000 && t_px < 1000000 && t_q >= 75 && t_q < 110) ||
          (t_px >= 200000 && t_px < 400000 && t_q <= 16)) {
        lrm_opt = 1;
      } else if ((t_px >= 2793000 && t_px < 3000000 && t_portrait &&
                  ((t_q >= 75 && t_q < 110) || (t_q >= 28 && t_q < 50))) ||
                 (t_px >= 2700000 && t_px < 2793000 && t_q >= 140 && t_q < 165) ||
                 (t_px >= 3000000 && !t_portrait && t_q >= 75 && t_q < 110)) {
        lrm_opt = 2;
      } else if (t_px >= 2700000 && t_px < 2793000 && t_q >= 28 && t_q < 50) {
        lrm_opt = 3;
      } else if ((t_px >= 3000000 && !t_portrait && t_q >= 28 && t_q < 50) ||
                 (t_px >= 2000000 && t_px < 2700000 && t_q >= 28 && t_q < 50)) {
        lrm_opt = 4;
      } else if (t_px >= 3000000 && t_portrait && t_q >= 75 && t_q < 110) {
        lrm_opt = 6;
      } else if (t_px >= 2793000 && t_px < 3000000 && !t_portrait && t_q >= 28 && t_q < 50) {
        lrm_opt = 7;
      } else if (t_px >= 2793000 && t_px < 3000000 && !t_portrait && t_q >= 75 && t_q < 110) {
        lrm_opt = 8;
      }
      if (lrm_opt == 1) {
        last_rdmult = (rdmult * 15) >> 4;
      } else if (lrm_opt == 2) {
        last_rdmult = (rdmult * 17) >> 4;
      } else if (lrm_opt == 3) {
        last_rdmult = (rdmult * 31) >> 5;
      } else if (lrm_opt == 4) {
        last_rdmult = (rdmult * 33) >> 5;
      } else if (lrm_opt == 6) {
        last_rdmult = (rdmult * 63) >> 6;
      } else if (lrm_opt == 7) {
        last_rdmult = (rdmult * 65) >> 6;
      } else if (lrm_opt == 8) {
        last_rdmult = (rdmult * 7) >> 3;
      }
    }
    update_coeff_general(&accu_rate, &accu_dist, si, eob, tx_size, tx_class,
                         bhl, width, last_rdmult, shift,
                         txb_ctx->dc_sign_ctx, dequant, scan, txb_costs, tcoeff,
                         qcoeff, dqcoeff, levels, iqmatrix, qmatrix);
    --si;
  } else {
    assert(abs_qc == 1);
    const int coeff_ctx = get_lower_levels_ctx_eob(bhl, width, si);
    accu_rate +=
        get_coeff_cost_eob(ci, abs_qc, sign, coeff_ctx, txb_ctx->dc_sign_ctx,
                           txb_costs, bhl, tx_class);
    const tran_low_t tqc = tcoeff[ci];
    const tran_low_t dqc = dqcoeff[ci];
    const int64_t dist = get_coeff_dist(tqc, dqc, shift, qmatrix, ci);
    const int64_t dist0 = get_coeff_dist(tqc, 0, shift, qmatrix, ci);
    accu_dist += dist - dist0;
    --si;
  }

#define UPDATE_COEFF_EOB_CASE(tx_class_literal)                            \
  case tx_class_literal:                                                   \
    update_coeff_eob_facade(                                               \
        &accu_rate, &accu_dist, &eob, &nz_num, nz_ci, &si, tx_size,        \
        tx_class_literal, bhl, width, txb_ctx->dc_sign_ctx, eob_rdmult,    \
        shift, dequant, scan, txb_eob_costs, txb_costs, tcoeff, qcoeff,    \
        dqcoeff, levels, eob_sharpness, iqmatrix, qmatrix, max_nz_num);    \
    break
  switch (tx_class) {
    UPDATE_COEFF_EOB_CASE(TX_CLASS_2D);
    UPDATE_COEFF_EOB_CASE(TX_CLASS_HORIZ);
    UPDATE_COEFF_EOB_CASE(TX_CLASS_VERT);
#undef UPDATE_COEFF_EOB_CASE
    default: assert(false);
  }

  if (si == -1 && nz_num <= max_nz_num && allow_update_skip) {
    int64_t skip_rdmult = rdmult;
    if (cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2 &&
        t_px >= 100000 && t_px < 200000 && t_q >= 140 && t_q < 165) {
      skip_rdmult = (rdmult * 9) >> 3;
    }
    update_skip(&accu_rate, accu_dist, &eob, nz_num, nz_ci, skip_rdmult,
                skip_cost, non_skip_cost, qcoeff, dqcoeff);
  }

#define UPDATE_COEFF_SIMPLE_CASE(tx_class_literal)                             \
  case tx_class_literal:                                                       \
    update_coeff_simple_facade(                                                \
        &accu_rate, &si, eob, tx_size, tx_class_literal, bhl, simple_rdmult,   \
        shift, dequant, scan, txb_costs, tcoeff, qcoeff, dqcoeff, levels,      \
        simple_sharpness, iqmatrix, qmatrix);                                  \
    break
  switch (tx_class) {
    UPDATE_COEFF_SIMPLE_CASE(TX_CLASS_2D);
    UPDATE_COEFF_SIMPLE_CASE(TX_CLASS_HORIZ);
    UPDATE_COEFF_SIMPLE_CASE(TX_CLASS_VERT);
#undef UPDATE_COEFF_SIMPLE_CASE
    default: assert(false);
  }

  // DC position
  if (si == 0) {
    // no need to update accu_dist because it's not used after this point
    int64_t dummy_dist = 0;
    update_coeff_general(&accu_rate, &dummy_dist, si, eob, tx_size, tx_class,
                         bhl, width, dc_rdmult, shift, txb_ctx->dc_sign_ctx,
                         dequant, scan, txb_costs, tcoeff, qcoeff, dqcoeff,
                         levels, iqmatrix, qmatrix);
  }

  const int tx_type_cost = get_tx_type_cost(x, xd, plane, tx_size, tx_type,
                                            cm->features.reduced_tx_set_used);
  if (eob == 0)
    accu_rate += skip_cost;
  else
    accu_rate += non_skip_cost + tx_type_cost;

  p->eobs[block] = eob;
  p->txb_entropy_ctx[block] =
      av1_get_txb_entropy_context(qcoeff, scan_order, p->eobs[block]);

  *rate_cost = accu_rate;
  return eob;
}

static AOM_FORCE_INLINE int warehouse_efficients_txb(
    const MACROBLOCK *x, const int plane, const int block,
    const TX_SIZE tx_size, const TXB_CTX *const txb_ctx,
    const struct macroblock_plane *p, const int eob,
    const PLANE_TYPE plane_type, const LV_MAP_COEFF_COST *const coeff_costs,
    const MACROBLOCKD *const xd, const TX_TYPE tx_type, const TX_CLASS tx_class,
    int reduced_tx_set_used) {
  const tran_low_t *const qcoeff = p->qcoeff + BLOCK_OFFSET(block);
  const int txb_skip_ctx = txb_ctx->txb_skip_ctx;
  const int bhl = get_txb_bhl(tx_size);
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
  const int16_t *const scan = scan_order->scan;
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, height);
  DECLARE_ALIGNED(16, int8_t, coeff_contexts[MAX_TX_SQUARE]);
  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const LV_MAP_EOB_COST *const eob_costs =
      &x->coeff_costs.eob_costs[eob_multi_size][plane_type];
  int cost = get_txb_non_skip_cost(coeff_costs, txb_skip_ctx);

  if (eob > 1) av1_txb_init_levels(qcoeff, width, height, levels);

  cost += get_tx_type_cost(x, xd, plane, tx_size, tx_type, reduced_tx_set_used);

  cost += get_eob_cost(eob, eob_costs, coeff_costs, tx_class);

  av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class, coeff_contexts);

  const int(*lps_cost)[COEFF_BASE_RANGE + 1 + COEFF_BASE_RANGE + 1] =
      coeff_costs->lps_cost;
  int c = eob - 1;
  {
    const int pos = scan[c];
    const tran_low_t v = qcoeff[pos];

    if (v) {
      const int sign = AOMSIGN(v);
      const int level = (v ^ sign) - sign;
      const int coeff_ctx = coeff_contexts[pos];
      cost += get_base_eob_cost(
          coeff_costs->base_eob_cost[coeff_ctx][AOMMIN(level, 3) - 1], coeff_costs);
      // sign bit cost
      if (level > NUM_BASE_LEVELS) {
        const int ctx = get_br_ctx_eob(pos, bhl, tx_class);
        cost += get_br_cost(level, lps_cost[ctx], coeff_costs);
      }
      if (c) {
        cost += get_ac_sign_cost(coeff_costs);
      } else {
        const int sign01 = (sign ^ sign) - sign;
        const int dc_sign_ctx = txb_ctx->dc_sign_ctx;
        cost += get_dc_sign_cost(coeff_costs->dc_sign_cost[dc_sign_ctx][sign01], coeff_costs);
        return cost;
      }
    }
  }
  const int(*base_cost)[8] = coeff_costs->base_cost;
  for (c = eob - 2; c >= 1; --c) {
    const int pos = scan[c];
    const int coeff_ctx = coeff_contexts[pos];
    const tran_low_t v = qcoeff[pos];
    if (!v) {
      cost += get_zero_coeff_cost(base_cost[coeff_ctx][0], coeff_costs);
      continue;
    }
    const int level = abs(v);
    cost += get_nz_base_cost(base_cost[coeff_ctx][AOMMIN(level, 3)],
                             AOMMIN(level, 3), coeff_costs);
    // sign bit cost
    cost += get_ac_sign_cost(coeff_costs);
    if (level > NUM_BASE_LEVELS) {
      const int ctx = get_br_ctx(levels, pos, bhl, tx_class);
      cost += get_br_cost(level, lps_cost[ctx], coeff_costs);
    }
  }
  // c == 0 after previous loop
  {
    const int pos = scan[c];
    const tran_low_t v = qcoeff[pos];
    const int coeff_ctx = coeff_contexts[pos];
    if (!v) {
      cost += get_zero_coeff_cost(base_cost[coeff_ctx][0], coeff_costs);
    } else {
      const int sign = AOMSIGN(v);
      const int level = (v ^ sign) - sign;
      cost += get_nz_base_cost(base_cost[coeff_ctx][AOMMIN(level, 3)],
                               AOMMIN(level, 3), coeff_costs);
      // sign bit cost
      const int sign01 = (sign ^ sign) - sign;
      const int dc_sign_ctx = txb_ctx->dc_sign_ctx;
      cost += get_dc_sign_cost(coeff_costs->dc_sign_cost[dc_sign_ctx][sign01], coeff_costs);
      if (level > NUM_BASE_LEVELS) {
        const int ctx = get_br_ctx(levels, pos, bhl, tx_class);
        cost += get_br_cost(level, lps_cost[ctx], coeff_costs);
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
  int cost = get_txb_non_skip_cost(coeff_costs, txb_skip_ctx);

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
    return get_txb_skip_cost(coeff_costs, txb_ctx->txb_skip_ctx);
  }

  const MACROBLOCKD *const xd = &x->e_mbd;
  const TX_CLASS tx_class = tx_type_to_class[tx_type];

  switch (tx_class) {
    case TX_CLASS_2D:
      return warehouse_efficients_txb(x, plane, block, tx_size, txb_ctx, p, eob,
                                      plane_type, coeff_costs, xd, tx_type,
                                      TX_CLASS_2D, reduced_tx_set_used);

    case TX_CLASS_VERT:
      return warehouse_efficients_txb(x, plane, block, tx_size, txb_ctx, p, eob,
                                      plane_type, coeff_costs, xd, tx_type,
                                      TX_CLASS_VERT, reduced_tx_set_used);

    case TX_CLASS_HORIZ:
      return warehouse_efficients_txb(x, plane, block, tx_size, txb_ctx, p, eob,
                                      plane_type, coeff_costs, xd, tx_type,
                                      TX_CLASS_HORIZ, reduced_tx_set_used);

    default: assert(0 && "Invalid TX_CLASS"); return 0;
  }
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
    return get_txb_skip_cost(coeff_costs, txb_ctx->txb_skip_ctx);
  }

  const MACROBLOCKD *const xd = &x->e_mbd;
  const TX_CLASS tx_class = tx_type_to_class[tx_type];

  return warehouse_efficients_txb_laplacian(
      x, plane, block, tx_size, txb_ctx, eob, plane_type, coeff_costs, xd,
      tx_type, tx_class, reduced_tx_set_used);
}    const int64_t d2 = (int64_t)dqv * dqv;
    const int64_t rd = RDCOST(simple_low_rdmult, rate, dist);
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/encoder/txb_rdopt.h"
#include "av1/encoder/txb_rdopt_utils.h"

#include "aom_ports/mem.h"
#include "av1/common/idct.h"

static inline void update_coeff_general(
    int *accu_rate, int64_t *accu_dist, int si, int eob, TX_SIZE tx_size,
    TX_CLASS tx_class, int bhl, int width, int64_t rdmult, int shift,
    int dc_sign_ctx, const int16_t *dequant, const int16_t *scan,
    const LV_MAP_COEFF_COST *txb_costs, const tran_low_t *tcoeff,
    tran_low_t *qcoeff, tran_low_t *dqcoeff, uint8_t *levels,
    const qm_val_t *iqmatrix, const qm_val_t *qmatrix) {
  const int dqv = get_dqv(dequant, scan[si], iqmatrix);
  const int ci = scan[si];
  const tran_low_t qc = qcoeff[ci];
  const int is_last = si == (eob - 1);
  const int coeff_ctx = get_lower_levels_ctx_general(
      is_last, si, bhl, width, levels, ci, tx_size, tx_class);
  if (qc == 0) {
    *accu_rate += get_zero_coeff_cost(txb_costs->base_cost[coeff_ctx][0], txb_costs);
  } else {
    const int sign = (qc < 0) ? 1 : 0;
    const tran_low_t abs_qc = abs(qc);
    const tran_low_t tqc = tcoeff[ci];
    const tran_low_t dqc = dqcoeff[ci];
    const int64_t dist = get_coeff_dist(tqc, dqc, shift, qmatrix, ci);
    const int64_t dist0 = get_coeff_dist(tqc, 0, shift, qmatrix, ci);
    const int rate =
        get_coeff_cost_general(is_last, ci, abs_qc, sign, coeff_ctx,
                               dc_sign_ctx, txb_costs, bhl, tx_class, levels);
    const int64_t rd = RDCOST(rdmult, rate, dist);

    tran_low_t qc_low, dqc_low;
    tran_low_t abs_qc_low;
    int64_t dist_low, rd_low;
    int rate_low;
    if (abs_qc == 1) {
      abs_qc_low = qc_low = dqc_low = 0;
      dist_low = dist0;
      rate_low = get_zero_coeff_cost(txb_costs->base_cost[coeff_ctx][0], txb_costs);
    } else {
      get_qc_dqc_low(abs_qc, sign, dqv, shift, &qc_low, &dqc_low);
      abs_qc_low = abs_qc - 1;
      dist_low = get_coeff_dist(tqc, dqc_low, shift, qmatrix, ci);
      rate_low =
          get_coeff_cost_general(is_last, ci, abs_qc_low, sign, coeff_ctx,
                                 dc_sign_ctx, txb_costs, bhl, tx_class, levels);
    }

    rd_low = RDCOST(rdmult, rate_low, dist_low);
    if (rd_low < rd) {
      qcoeff[ci] = qc_low;
      dqcoeff[ci] = dqc_low;
      levels[get_padded_idx(ci, bhl)] = AOMMIN(abs_qc_low, INT8_MAX);
      *accu_rate += rate_low;
      *accu_dist += dist_low - dist0;
    } else {
      *accu_rate += rate;
      *accu_dist += dist - dist0;
    }
  }
}

static AOM_FORCE_INLINE void update_coeff_simple(
    int *accu_rate, int si, int eob, TX_SIZE tx_size, TX_CLASS tx_class,
    int bhl, int64_t rdmult, int shift, const int16_t *dequant,
    const int16_t *scan, const LV_MAP_COEFF_COST *txb_costs,
    const tran_low_t *tcoeff, tran_low_t *qcoeff, tran_low_t *dqcoeff,
    uint8_t *levels, int sharpness, const qm_val_t *iqmatrix,
    const qm_val_t *qmatrix) {
  (void)eob;
  // this simple version assumes the coeff's scan_idx is not DC (scan_idx != 0)
  // and not the last (scan_idx != eob - 1)
  assert(si != eob - 1);
  assert(si > 0);
  const int ci = scan[si];
  const tran_low_t qc = qcoeff[ci];
  const int coeff_ctx =
      get_lower_levels_ctx(levels, ci, bhl, tx_size, tx_class);
  if (qc == 0) {
    *accu_rate += get_zero_coeff_cost(txb_costs->base_cost[coeff_ctx][0], txb_costs);
  } else {
    const tran_low_t abs_qc = abs(qc);
    const tran_low_t abs_tqc = abs(tcoeff[ci]);
    const tran_low_t abs_dqc = abs(dqcoeff[ci]);
    int rate_low = 0;
    const int rate = get_two_coeff_cost_simple(
        ci, abs_qc, coeff_ctx, txb_costs, bhl, tx_class, levels, &rate_low);
    if (abs_dqc < abs_tqc) {
      *accu_rate += rate;
      return;
    }

    const int dqv = get_dqv(dequant, scan[si], iqmatrix);
    const int64_t dist = get_coeff_dist(abs_tqc, abs_dqc, shift, qmatrix, ci);
    const int64_t rd = RDCOST(rdmult, rate, dist);

    const tran_low_t abs_qc_low = abs_qc - 1;
    const tran_low_t abs_dqc_low = (abs_qc_low * dqv) >> shift;
    const int64_t dist_low =
        get_coeff_dist(abs_tqc, abs_dqc_low, shift, qmatrix, ci);
    const int64_t rd_low = RDCOST(simple_low_rdmult, rate_low, dist_low);

    int allow_lower_qc = sharpness ? (abs_qc > 1) : 1;

    if (rd_low < rd && allow_lower_qc) {
      const int sign = (qc < 0) ? 1 : 0;
      qcoeff[ci] = (-sign ^ abs_qc_low) + sign;
      dqcoeff[ci] = (-sign ^ abs_dqc_low) + sign;
      levels[get_padded_idx(ci, bhl)] = AOMMIN(abs_qc_low, INT8_MAX);
      *accu_rate += rate_low;
    } else {
      *accu_rate += rate;
    }
  }
}

static AOM_FORCE_INLINE void update_coeff_eob(
    int *accu_rate, int64_t *accu_dist, int *eob, int *nz_num, int *nz_ci,
    int si, TX_SIZE tx_size, TX_CLASS tx_class, int bhl, int width,
    int dc_sign_ctx, int64_t rdmult, int shift, const int16_t *dequant,
    const int16_t *scan, const LV_MAP_EOB_COST *txb_eob_costs,
    const LV_MAP_COEFF_COST *txb_costs, const tran_low_t *tcoeff,
    tran_low_t *qcoeff, tran_low_t *dqcoeff, uint8_t *levels, int sharpness,
    const qm_val_t *iqmatrix, const qm_val_t *qmatrix) {
  assert(si != *eob - 1);
  const int ci = scan[si];
  const tran_low_t qc = qcoeff[ci];
  const int coeff_ctx =
      get_lower_levels_ctx(levels, ci, bhl, tx_size, tx_class);
  if (qc == 0) {
    *accu_rate += get_zero_coeff_cost(txb_costs->base_cost[coeff_ctx][0], txb_costs);
  } else {
    const int dqv = get_dqv(dequant, scan[si], iqmatrix);
    int lower_level = 0;
    const tran_low_t abs_qc = abs(qc);
    const tran_low_t tqc = tcoeff[ci];
    const tran_low_t dqc = dqcoeff[ci];
    const int sign = (qc < 0) ? 1 : 0;
    const int64_t dist0 = get_coeff_dist(tqc, 0, shift, qmatrix, ci);
    int64_t dist = get_coeff_dist(tqc, dqc, shift, qmatrix, ci) - dist0;
    int rate =
        get_coeff_cost_general(0, ci, abs_qc, sign, coeff_ctx, dc_sign_ctx,
                               txb_costs, bhl, tx_class, levels);
    int64_t rd = RDCOST(rdmult, *accu_rate + rate, *accu_dist + dist);

    tran_low_t qc_low, dqc_low;
    tran_low_t abs_qc_low;
    int64_t dist_low, rd_low;
    int rate_low;

    if (abs_qc == 1) {
      abs_qc_low = 0;
      dqc_low = qc_low = 0;
      dist_low = 0;
      rate_low = get_zero_coeff_cost(txb_costs->base_cost[coeff_ctx][0], txb_costs);
      rd_low = RDCOST(rdmult, *accu_rate + rate_low, *accu_dist);
    } else {
      get_qc_dqc_low(abs_qc, sign, dqv, shift, &qc_low, &dqc_low);
      abs_qc_low = abs_qc - 1;
      dist_low = get_coeff_dist(tqc, dqc_low, shift, qmatrix, ci) - dist0;
      rate_low =
          get_coeff_cost_general(0, ci, abs_qc_low, sign, coeff_ctx,
                                 dc_sign_ctx, txb_costs, bhl, tx_class, levels);
      rd_low = RDCOST(rdmult, *accu_rate + rate_low, *accu_dist + dist_low);
    }

    int lower_level_new_eob = 0;
    const int new_eob = si + 1;
    const int coeff_ctx_new_eob = get_lower_levels_ctx_eob(bhl, width, si);
    const int new_eob_cost =
        get_eob_cost(new_eob, txb_eob_costs, txb_costs, tx_class);
    int rate_coeff_eob =
        new_eob_cost + get_coeff_cost_eob(ci, abs_qc, sign, coeff_ctx_new_eob,
                                          dc_sign_ctx, txb_costs, bhl,
                                          tx_class);
    int64_t dist_new_eob = dist;
    int64_t rd_new_eob = RDCOST(rdmult, rate_coeff_eob, dist_new_eob);

    if (abs_qc_low > 0) {
      const int rate_coeff_eob_low =
          new_eob_cost + get_coeff_cost_eob(ci, abs_qc_low, sign,
                                            coeff_ctx_new_eob, dc_sign_ctx,
                                            txb_costs, bhl, tx_class);
      const int64_t dist_new_eob_low = dist_low;
      const int64_t rd_new_eob_low =
          RDCOST(rdmult, rate_coeff_eob_low, dist_new_eob_low);
      if (rd_new_eob_low < rd_new_eob) {
        lower_level_new_eob = 1;
        rd_new_eob = rd_new_eob_low;
        rate_coeff_eob = rate_coeff_eob_low;
        dist_new_eob = dist_new_eob_low;
      }
    }

    const int qc_threshold = (si <= 5) ? 2 : 1;
    const int allow_lower_qc = sharpness ? abs_qc > qc_threshold : 1;

    if (allow_lower_qc) {
      if (rd_low < rd) {
        lower_level = 1;
        rd = rd_low;
        rate = rate_low;
        dist = dist_low;
      }
    }

    if ((sharpness == 0 || new_eob >= 5) && rd_new_eob < rd) {
      for (int ni = 0; ni < *nz_num; ++ni) {
        int last_ci = nz_ci[ni];
        levels[get_padded_idx(last_ci, bhl)] = 0;
        qcoeff[last_ci] = 0;
        dqcoeff[last_ci] = 0;
      }
      *eob = new_eob;
      *nz_num = 0;
      *accu_rate = rate_coeff_eob;
      *accu_dist = dist_new_eob;
      lower_level = lower_level_new_eob;
    } else {
      *accu_rate += rate;
      *accu_dist += dist;
    }

    if (lower_level) {
      qcoeff[ci] = qc_low;
      dqcoeff[ci] = dqc_low;
      levels[get_padded_idx(ci, bhl)] = AOMMIN(abs_qc_low, INT8_MAX);
    }
    if (qcoeff[ci]) {
      nz_ci[*nz_num] = ci;
      ++*nz_num;
    }
  }
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
static int get_tx_type_cost(const MACROBLOCK *x, const MACROBLOCKD *xd,
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

static AOM_FORCE_INLINE void update_coeff_eob_facade(
    int *accu_rate, int64_t *accu_dist, int *eob, int *nz_num, int *nz_ci,
    int *si, TX_SIZE tx_size, TX_CLASS tx_class, int bhl, int width,
    int dc_sign_ctx, int64_t rdmult, int shift, const int16_t *dequant,
    const int16_t *scan, const LV_MAP_EOB_COST *txb_eob_costs,
    const LV_MAP_COEFF_COST *txb_costs, const tran_low_t *tcoeff,
    tran_low_t *qcoeff, tran_low_t *dqcoeff, uint8_t *levels, int sharpness,
    const qm_val_t *iqmatrix, const qm_val_t *qmatrix, int max_nz_num) {
  for (; *si >= 0 && *nz_num <= max_nz_num; --*si) {
    update_coeff_eob(accu_rate, accu_dist, eob, nz_num, nz_ci, *si, tx_size,
                     tx_class, bhl, width, dc_sign_ctx, rdmult, shift, dequant,
                     scan, txb_eob_costs, txb_costs, tcoeff, qcoeff, dqcoeff,
                     levels, sharpness, iqmatrix, qmatrix);
  }
}

static AOM_FORCE_INLINE void update_coeff_simple_facade(
    int *accu_rate, int *si, int eob, TX_SIZE tx_size, TX_CLASS tx_class,
    int bhl, int64_t rdmult, int shift, const int16_t *dequant,
    const int16_t *scan, const LV_MAP_COEFF_COST *txb_costs,
    const tran_low_t *tcoeff, tran_low_t *qcoeff, tran_low_t *dqcoeff,
    uint8_t *levels, int sharpness, const qm_val_t *iqmatrix,
    const qm_val_t *qmatrix) {
  for (; *si >= 1; --*si) {
    update_coeff_simple(accu_rate, *si, eob, tx_size, tx_class, bhl, rdmult,
                        shift, dequant, scan, txb_costs, tcoeff, qcoeff,
                        dqcoeff, levels, sharpness, iqmatrix, qmatrix);
  }
}

int av1_optimize_txb(const struct AV1_COMP *cpi, MACROBLOCK *x, int plane,
                     int block, TX_SIZE tx_size, TX_TYPE tx_type,
                     const TXB_CTX *const txb_ctx, int *rate_cost,
                     int sharpness) {
  MACROBLOCKD *xd = &x->e_mbd;
  const struct macroblock_plane *p = &x->plane[plane];
  const SCAN_ORDER *scan_order = get_scan(tx_size, tx_type);
  const int16_t *scan = scan_order->scan;
  const int shift = av1_get_tx_scale(tx_size);
  int eob = p->eobs[block];
  const int16_t *dequant = p->dequant_QTX;
  const qm_val_t *iqmatrix =
      av1_get_iqmatrix(&cpi->common.quant_params, xd, plane, tx_size, tx_type);
  const qm_val_t *qmatrix =
      cpi->oxcf.tune_cfg.dist_metric == AOM_DIST_METRIC_QM_PSNR
          ? av1_get_qmatrix(&cpi->common.quant_params, xd, plane, tx_size,
                            tx_type)
          : NULL;
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *qcoeff = p->qcoeff + block_offset;
  tran_low_t *dqcoeff = p->dqcoeff + block_offset;
  const tran_low_t *tcoeff = p->coeff + block_offset;
  const CoeffCosts *coeff_costs = &x->coeff_costs;

  // This function is not called if eob = 0.
  assert(eob > 0);

  const AV1_COMMON *cm = &cpi->common;
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  const TX_CLASS tx_class = tx_type_to_class[tx_type];
  const MB_MODE_INFO *mbmi = xd->mi[0];
  const int bhl = get_txb_bhl(tx_size);
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  assert(height == (1 << bhl));
  const int is_inter = is_inter_block(mbmi);
  const LV_MAP_COEFF_COST *txb_costs =
      &coeff_costs->coeff_costs[txs_ctx][plane_type];
  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const LV_MAP_EOB_COST *txb_eob_costs =
      &coeff_costs->eob_costs[eob_multi_size][plane_type];

  // For the IQ and SSIMULACRA 2 tunings, increase rshift from 2 to 4.
  // This biases trellis quantization towards keeping more coefficients, and
  // together with the IQ and SSIMULACRA2 rdmult adjustment in
  // av1_compute_rd_mult_based_on_qindex(), this helps preserve image
  // features (like repeating patterns and camera noise/film grain), which
  // improves SSIMULACRA 2 scores.
  const int rshift =
      (cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2)
          ? 6
          : ((cpi->oxcf.tune_cfg.tuning == AOM_TUNE_IQ) ? 7 : 5);

  const int(*trellis_rd_mult)[2] = cpi->sf.tx_sf.use_chroma_trellis_rd_mult
                                       ? plane_rd_mult_chroma
                                       : plane_rd_mult;
  const int64_t rdmult = ROUND_POWER_OF_TWO(
      (int64_t)x->rdmult * (8 - sharpness) *
          (trellis_rd_mult[is_inter][plane_type] << (2 * (xd->bd - 8))),
      rshift);

  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, height);

  if (eob > 1) av1_txb_init_levels(qcoeff, width, height, levels);

  // TODO(angirbird): check iqmatrix

  const int non_skip_cost = get_txb_non_skip_cost(txb_costs, txb_ctx->txb_skip_ctx);
  const int skip_cost = get_txb_skip_cost(txb_costs, txb_ctx->txb_skip_ctx);
  const int eob_cost = get_eob_cost(eob, txb_eob_costs, txb_costs, tx_class);
  int accu_rate = eob_cost;
  int64_t accu_dist = 0;
  int si = eob - 1;
  const int ci = scan[si];
  const tran_low_t qc = qcoeff[ci];
  const tran_low_t abs_qc = abs(qc);
  const int sign = qc < 0;
  const int max_nz_num = 2;
  int nz_num = 1;
  int nz_ci[3] = { ci, 0, 0 };
  if (abs_qc >= 2) {
    update_coeff_general(&accu_rate, &accu_dist, si, eob, tx_size, tx_class,
                         bhl, width, rdmult, shift, txb_ctx->dc_sign_ctx,
                         dequant, scan, txb_costs, tcoeff, qcoeff, dqcoeff,
                         levels, iqmatrix, qmatrix);
    --si;
  } else {
    assert(abs_qc == 1);
    const int coeff_ctx = get_lower_levels_ctx_eob(bhl, width, si);
    accu_rate +=
        get_coeff_cost_eob(ci, abs_qc, sign, coeff_ctx, txb_ctx->dc_sign_ctx,
                           txb_costs, bhl, tx_class);
    const tran_low_t tqc = tcoeff[ci];
    const tran_low_t dqc = dqcoeff[ci];
    const int64_t dist = get_coeff_dist(tqc, dqc, shift, qmatrix, ci);
    const int64_t dist0 = get_coeff_dist(tqc, 0, shift, qmatrix, ci);
    accu_dist += dist - dist0;
    --si;
  }

#define UPDATE_COEFF_EOB_CASE(tx_class_literal)                            \
  case tx_class_literal:                                                   \
    update_coeff_eob_facade(                                               \
        &accu_rate, &accu_dist, &eob, &nz_num, nz_ci, &si, tx_size,        \
        tx_class_literal, bhl, width, txb_ctx->dc_sign_ctx, rdmult, shift, \
        dequant, scan, txb_eob_costs, txb_costs, tcoeff, qcoeff, dqcoeff,  \
        levels, sharpness, iqmatrix, qmatrix, max_nz_num);                 \
    break
  switch (tx_class) {
    UPDATE_COEFF_EOB_CASE(TX_CLASS_2D);
    UPDATE_COEFF_EOB_CASE(TX_CLASS_HORIZ);
    UPDATE_COEFF_EOB_CASE(TX_CLASS_VERT);
#undef UPDATE_COEFF_EOB_CASE
    default: assert(false);
  }

  if (si == -1 && nz_num <= max_nz_num && sharpness == 0) {
    update_skip(&accu_rate, accu_dist, &eob, nz_num, nz_ci, rdmult, skip_cost,
                non_skip_cost, qcoeff, dqcoeff);
  }

#define UPDATE_COEFF_SIMPLE_CASE(tx_class_literal)                            \
  case tx_class_literal:                                                      \
    update_coeff_simple_facade(&accu_rate, &si, eob, tx_size,                 \
                               tx_class_literal, bhl, rdmult, shift, dequant, \
                               scan, txb_costs, tcoeff, qcoeff, dqcoeff,      \
                               levels, sharpness, iqmatrix, qmatrix);         \
    break
  switch (tx_class) {
    UPDATE_COEFF_SIMPLE_CASE(TX_CLASS_2D);
    UPDATE_COEFF_SIMPLE_CASE(TX_CLASS_HORIZ);
    UPDATE_COEFF_SIMPLE_CASE(TX_CLASS_VERT);
#undef UPDATE_COEFF_SIMPLE_CASE
    default: assert(false);
  }

  // DC position
  if (si == 0) {
    // no need to update accu_dist because it's not used after this point
    int64_t dummy_dist = 0;
    update_coeff_general(&accu_rate, &dummy_dist, si, eob, tx_size, tx_class,
                         bhl, width, rdmult, shift, txb_ctx->dc_sign_ctx,
                         dequant, scan, txb_costs, tcoeff, qcoeff, dqcoeff,
                         levels, iqmatrix, qmatrix);
  }

  const int tx_type_cost = get_tx_type_cost(x, xd, plane, tx_size, tx_type,
                                            cm->features.reduced_tx_set_used);
  if (eob == 0)
    accu_rate += skip_cost;
  else
    accu_rate += non_skip_cost + tx_type_cost;

  p->eobs[block] = eob;
  p->txb_entropy_ctx[block] =
      av1_get_txb_entropy_context(qcoeff, scan_order, p->eobs[block]);

  *rate_cost = accu_rate;
  return eob;
}

static AOM_FORCE_INLINE int warehouse_efficients_txb(
    const MACROBLOCK *x, const int plane, const int block,
    const TX_SIZE tx_size, const TXB_CTX *const txb_ctx,
    const struct macroblock_plane *p, const int eob,
    const PLANE_TYPE plane_type, const LV_MAP_COEFF_COST *const coeff_costs,
    const MACROBLOCKD *const xd, const TX_TYPE tx_type, const TX_CLASS tx_class,
    int reduced_tx_set_used) {
  const tran_low_t *const qcoeff = p->qcoeff + BLOCK_OFFSET(block);
  const int txb_skip_ctx = txb_ctx->txb_skip_ctx;
  const int bhl = get_txb_bhl(tx_size);
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
  const int16_t *const scan = scan_order->scan;
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, height);
  DECLARE_ALIGNED(16, int8_t, coeff_contexts[MAX_TX_SQUARE]);
  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const LV_MAP_EOB_COST *const eob_costs =
      &x->coeff_costs.eob_costs[eob_multi_size][plane_type];
  int cost = coeff_costs->txb_skip_cost[txb_skip_ctx][0];

  if (eob > 1) av1_txb_init_levels(qcoeff, width, height, levels);

  cost += get_tx_type_cost(x, xd, plane, tx_size, tx_type, reduced_tx_set_used);

  cost += get_eob_cost(eob, eob_costs, coeff_costs, tx_class);

  av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class, coeff_contexts);

  const int(*lps_cost)[COEFF_BASE_RANGE + 1 + COEFF_BASE_RANGE + 1] =
      coeff_costs->lps_cost;
  int c = eob - 1;
  {
    const int pos = scan[c];
    const tran_low_t v = qcoeff[pos];

    if (v) {
      const int sign = AOMSIGN(v);
      const int level = (v ^ sign) - sign;
      const int coeff_ctx = coeff_contexts[pos];
      cost += coeff_costs->base_eob_cost[coeff_ctx][AOMMIN(level, 3) - 1];
      // sign bit cost
      if (level > NUM_BASE_LEVELS) {
        const int ctx = get_br_ctx_eob(pos, bhl, tx_class);
        cost += get_br_cost(level, lps_cost[ctx]);
      }
      if (c) {
        cost += av1_cost_literal(1);
      } else {
        const int sign01 = (sign ^ sign) - sign;
        const int dc_sign_ctx = txb_ctx->dc_sign_ctx;
        cost += coeff_costs->dc_sign_cost[dc_sign_ctx][sign01];
        return cost;
      }
    }
  }
  const int(*base_cost)[8] = coeff_costs->base_cost;
  for (c = eob - 2; c >= 1; --c) {
    const int pos = scan[c];
    const int coeff_ctx = coeff_contexts[pos];
    const tran_low_t v = qcoeff[pos];
    if (!v) {
      cost += base_cost[coeff_ctx][0];
      continue;
    }
    const int level = abs(v);
    cost += base_cost[coeff_ctx][AOMMIN(level, 3)];
    // sign bit cost
    cost += av1_cost_literal(1);
    if (level > NUM_BASE_LEVELS) {
      const int ctx = get_br_ctx(levels, pos, bhl, tx_class);
      cost += get_br_cost(level, lps_cost[ctx]);
    }
  }
  // c == 0 after previous loop
  {
    const int pos = scan[c];
    const tran_low_t v = qcoeff[pos];
    const int coeff_ctx = coeff_contexts[pos];
    if (!v) {
      cost += base_cost[coeff_ctx][0];
    } else {
      const int sign = AOMSIGN(v);
      const int level = (v ^ sign) - sign;
      cost += base_cost[coeff_ctx][AOMMIN(level, 3)];
      // sign bit cost
      const int sign01 = (sign ^ sign) - sign;
      const int dc_sign_ctx = txb_ctx->dc_sign_ctx;
      cost += coeff_costs->dc_sign_cost[dc_sign_ctx][sign01];
      if (level > NUM_BASE_LEVELS) {
        const int ctx = get_br_ctx(levels, pos, bhl, tx_class);
        cost += get_br_cost(level, lps_cost[ctx]);
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

  switch (tx_class) {
    case TX_CLASS_2D:
      return warehouse_efficients_txb(x, plane, block, tx_size, txb_ctx, p, eob,
                                      plane_type, coeff_costs, xd, tx_type,
                                      TX_CLASS_2D, reduced_tx_set_used);

    case TX_CLASS_VERT:
      return warehouse_efficients_txb(x, plane, block, tx_size, txb_ctx, p, eob,
                                      plane_type, coeff_costs, xd, tx_type,
                                      TX_CLASS_VERT, reduced_tx_set_used);

    case TX_CLASS_HORIZ:
      return warehouse_efficients_txb(x, plane, block, tx_size, txb_ctx, p, eob,
                                      plane_type, coeff_costs, xd, tx_type,
                                      TX_CLASS_HORIZ, reduced_tx_set_used);

    default: assert(0 && "Invalid TX_CLASS"); return 0;
  }
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
