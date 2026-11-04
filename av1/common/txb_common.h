/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_COMMON_TXB_COMMON_H_
#define AOM_AV1_COMMON_TXB_COMMON_H_

#include "av1/common/av1_common_int.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const int16_t av1_eob_group_start[12];
extern const int16_t av1_eob_offset_bits[12];

extern const int8_t *const av1_nz_map_ctx_offset[TX_SIZES_ALL];

typedef struct txb_ctx {
  int txb_skip_ctx;
  int dc_sign_ctx;
} TXB_CTX;

static const int base_level_count_to_index[13] = {
  0, 0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
};

static const TX_CLASS tx_type_to_class[TX_TYPES] = {
  TX_CLASS_2D,     // DCT_DCT
  TX_CLASS_2D,     // ADST_DCT
  TX_CLASS_2D,     // DCT_ADST
  TX_CLASS_2D,     // ADST_ADST
  TX_CLASS_2D,     // FLIPADST_DCT
  TX_CLASS_2D,     // DCT_FLIPADST
  TX_CLASS_2D,     // FLIPADST_FLIPADST
  TX_CLASS_2D,     // ADST_FLIPADST
  TX_CLASS_2D,     // FLIPADST_ADST
  TX_CLASS_2D,     // IDTX
  TX_CLASS_VERT,   // V_DCT
  TX_CLASS_HORIZ,  // H_DCT
  TX_CLASS_VERT,   // V_ADST
  TX_CLASS_HORIZ,  // H_ADST
  TX_CLASS_VERT,   // V_FLIPADST
  TX_CLASS_HORIZ,  // H_FLIPADST
};

static inline int get_txb_bhl(TX_SIZE tx_size) {
  static const uint8_t txb_bhl_tab[TX_SIZES_ALL] = { 2, 3, 4, 5, 5, 3, 2,
                                                     4, 3, 5, 4, 5, 5, 4,
                                                     2, 5, 3, 5, 4 };
  return txb_bhl_tab[tx_size];
}

static inline int get_txb_wide(TX_SIZE tx_size) {
  static const uint8_t txb_wide_tab[TX_SIZES_ALL] = { 4,  8,  16, 32, 32, 4,  8,
                                                      8,  16, 16, 32, 32, 32, 4,
                                                      16, 8,  32, 16, 32 };
  return txb_wide_tab[tx_size];
}

static inline int get_txb_high(TX_SIZE tx_size) {
  static const uint8_t txb_high_tab[TX_SIZES_ALL] = {
    4, 8, 16, 32, 32, 8, 4, 16, 8, 32, 16, 32, 32, 16, 4, 32, 8, 32, 16
  };
  return txb_high_tab[tx_size];
}

static inline uint8_t *set_levels(uint8_t *const levels_buf, const int height) {
  return levels_buf + TX_PAD_TOP * (height + TX_PAD_HOR);
}

static inline int get_padded_idx(const int idx, const int bhl) {
  return idx + ((idx >> bhl) << TX_PAD_HOR_LOG2);
}

static inline int get_br_ctx_2d(const uint8_t *const levels,
                                const int c,  // raster order
                                const int bhl) {
  assert(c > 0);
  const int col = c >> bhl;
  const int row = c - (col << bhl);
  const intptr_t stride = (1 << bhl) + TX_PAD_HOR;
  const uint8_t *const lv = levels + c + (col << TX_PAD_HOR_LOG2);
  const uint8_t *const lvs = lv + stride;
  int mag = lv[1] + lvs[0] + lvs[1];
  mag = AOMMIN((mag + 1) >> 1, 6);
  //((row | col) < 2) is equivalent to ((row < 2) && (col < 2))
  if ((row | col) < 2) return mag + 7;
  return mag + 14;
}

static AOM_FORCE_INLINE int get_br_ctx_eob(const int c,  // raster order
                                           const int bhl,
                                           const TX_CLASS tx_class) {
  if (c == 0) return 0;
  const int col = c >> bhl;
  const int row = c & ((1 << bhl) - 1);
  if ((tx_class == TX_CLASS_2D && ((row | col) < 2)) ||
      (tx_class == TX_CLASS_HORIZ && col == 0) ||
      (tx_class == TX_CLASS_VERT && row == 0))
    return 7;
  return 14;
}

static AOM_FORCE_INLINE int get_br_ctx_stride(const uint8_t *const levels,
                                              const int c,  // raster order
                                              const int bhl,
                                              const intptr_t stride,
                                              const TX_CLASS tx_class) {
  const intptr_t col = (intptr_t)c >> bhl;
  const uint8_t *const lv = levels + (intptr_t)c + (col << TX_PAD_HOR_LOG2);
  const uint8_t *const lvs = lv + stride;
  int mag = lv[1] + lvs[0];
  switch (tx_class) {
    case TX_CLASS_2D: {
      mag += lvs[1];
      mag = AOMMIN((mag + 1) >> 1, 6);
      if (c == 0) return mag;
      if ((c & ~((int)stride - (TX_PAD_HOR - 1))) == 0) return mag + 7;
      break;
    }
    case TX_CLASS_HORIZ:
      mag += lvs[stride];
      mag = AOMMIN((mag + 1) >> 1, 6);
      if (c == 0) return mag;
      if (c < ((int)stride - TX_PAD_HOR)) return mag + 7;
      break;
    case TX_CLASS_VERT: {
      mag += lv[2];
      mag = AOMMIN((mag + 1) >> 1, 6);
      if (c == 0) return mag;
      if ((c & ((int)stride - (TX_PAD_HOR + 1))) == 0) return mag + 7;
      break;
    }
    default: break;
  }

  return mag + 14;
}

static AOM_FORCE_INLINE int get_br_ctx(const uint8_t *const levels,
                                       const int c,  // raster order
                                       const int bhl, const TX_CLASS tx_class) {
  const intptr_t stride = (1 << bhl) + TX_PAD_HOR;
  return get_br_ctx_stride(levels, c, bhl, stride, tx_class);
}

static AOM_FORCE_INLINE int get_br_ctx_simple_stride(
    const uint8_t *const levels,
    const int c,  // raster order > 0
    const int bhl, const intptr_t stride, const TX_CLASS tx_class) {
  assert(c > 0);
  const intptr_t col = (intptr_t)c >> bhl;
  const uint8_t *const lv = levels + (intptr_t)c + (col << TX_PAD_HOR_LOG2);
  const uint8_t *const lvs = lv + stride;
  int mag = lv[1] + lvs[0];
  switch (tx_class) {
    case TX_CLASS_2D: {
      mag += lvs[1];
      mag = AOMMIN((mag + 1) >> 1, 6);
      return mag + (((c & ~((int)stride - (TX_PAD_HOR - 1))) == 0) ? 7 : 14);
    }
    case TX_CLASS_HORIZ:
      mag += lvs[stride];
      mag = AOMMIN((mag + 1) >> 1, 6);
      return mag + ((c < ((int)stride - TX_PAD_HOR)) ? 7 : 14);
    case TX_CLASS_VERT: {
      mag += lv[2];
      mag = AOMMIN((mag + 1) >> 1, 6);
      return mag + (((c & ((int)stride - (TX_PAD_HOR + 1))) == 0) ? 7 : 14);
    }
    default: return mag + 14;
  }
}

static AOM_FORCE_INLINE int get_br_ctx_simple(const uint8_t *const levels,
                                              const int c,  // raster order > 0
                                              const int bhl,
                                              const TX_CLASS tx_class) {
  const intptr_t stride = (1 << bhl) + TX_PAD_HOR;
  return get_br_ctx_simple_stride(levels, c, bhl, stride, tx_class);
}

static const uint8_t clip_max3[256] = {
  0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
};

static AOM_FORCE_INLINE int get_nz_mag_stride(const uint8_t *const levels,
                                              const intptr_t stride,
                                              const TX_CLASS tx_class) {
  int mag;
  const uint8_t *const lvs = levels + stride;

  // Note: AOMMIN(level, 3) is useless for decoder since level < 3.
  mag = clip_max3[lvs[0]];      // { 0, 1 }
  mag += clip_max3[levels[1]];  // { 1, 0 }

  if (tx_class == TX_CLASS_2D) {
    mag += clip_max3[lvs[1]];       // { 1, 1 }
    mag += clip_max3[lvs[stride]];  // { 0, 2 }
    mag += clip_max3[levels[2]];    // { 2, 0 }
  } else if (tx_class == TX_CLASS_VERT) {
    mag += clip_max3[levels[2]];  // { 2, 0 }
    mag += clip_max3[levels[3]];  // { 3, 0 }
    mag += clip_max3[levels[4]];  // { 4, 0 }
  } else {
    const uint8_t *const lvs2 = lvs + stride;
    mag += clip_max3[lvs2[0]];           // { 0, 2 }
    mag += clip_max3[lvs2[stride]];      // { 0, 3 }
    mag += clip_max3[lvs2[stride * 2]];  // { 0, 4 }
  }

  return mag;
}

static AOM_FORCE_INLINE int get_nz_mag(const uint8_t *const levels,
                                       const int bhl, const TX_CLASS tx_class) {
  const intptr_t stride = (1 << bhl) + TX_PAD_HOR;
  return get_nz_mag_stride(levels, stride, tx_class);
}

#define NZ_MAP_CTX_0 SIG_COEF_CONTEXTS_2D
#define NZ_MAP_CTX_5 (NZ_MAP_CTX_0 + 5)
#define NZ_MAP_CTX_10 (NZ_MAP_CTX_0 + 10)

static const int nz_map_ctx_offset_1d[32] = {
  NZ_MAP_CTX_0,  NZ_MAP_CTX_5,  NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10,
  NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10,
  NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10,
  NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10,
  NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10,
  NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10, NZ_MAP_CTX_10,
  NZ_MAP_CTX_10, NZ_MAP_CTX_10,
};

static AOM_FORCE_INLINE int get_nz_map_ctx_from_stats(
    const int stats,
    const int coeff_idx,  // raster order
    const int bhl, const TX_SIZE tx_size, const TX_CLASS tx_class) {
  // tx_class == 0(TX_CLASS_2D)
  if ((tx_class | coeff_idx) == 0) return 0;
  int ctx = (stats + 1) >> 1;
  ctx = AOMMIN(ctx, 4);
  switch (tx_class) {
    case TX_CLASS_2D: {
      // This is the algorithm to generate av1_nz_map_ctx_offset[][]
      //   const int width = tx_size_wide[tx_size];
      //   const int height = tx_size_high[tx_size];
      //   if (width < height) {
      //     if (row < 2) return 11 + ctx;
      //   } else if (width > height) {
      //     if (col < 2) return 16 + ctx;
      //   }
      //   if (row + col < 2) return ctx + 1;
      //   if (row + col < 4) return 5 + ctx + 1;
      //   return 21 + ctx;
      return ctx + av1_nz_map_ctx_offset[tx_size][coeff_idx];
    }
    case TX_CLASS_HORIZ: {
      const int col = coeff_idx >> bhl;
      return ctx + nz_map_ctx_offset_1d[col];
    }
    case TX_CLASS_VERT: {
      const int col = coeff_idx >> bhl;
      const int row = coeff_idx - (col << bhl);
      return ctx + nz_map_ctx_offset_1d[row];
    }
    default: break;
  }
  return 0;
}

typedef aom_cdf_prob (*base_cdf_arr)[CDF_SIZE(4)];
typedef aom_cdf_prob (*br_cdf_arr)[CDF_SIZE(BR_CDF_SIZE)];

static inline int get_lower_levels_ctx_eob(int bhl, int width, int scan_idx) {
  if (scan_idx == 0) return 0;
  if (scan_idx <= ((width << bhl) >> 3)) return 1;
  if (scan_idx <= ((width << bhl) >> 2)) return 2;
  return 3;
}

static inline int get_lower_levels_ctx_2d(const uint8_t *levels, int coeff_idx,
                                          int bhl, TX_SIZE tx_size) {
  assert(coeff_idx > 0);
  int mag;
  // Note: AOMMIN(level, 3) is useless for decoder since level < 3.
  levels = levels + get_padded_idx(coeff_idx, bhl);
  mag = AOMMIN(levels[(1 << bhl) + TX_PAD_HOR], 3);               // { 0, 1 }
  mag += AOMMIN(levels[1], 3);                                    // { 1, 0 }
  mag += AOMMIN(levels[(1 << bhl) + TX_PAD_HOR + 1], 3);          // { 1, 1 }
  mag += AOMMIN(levels[(2 << bhl) + (2 << TX_PAD_HOR_LOG2)], 3);  // { 0, 2 }
  mag += AOMMIN(levels[2], 3);                                    // { 2, 0 }

  const int ctx = AOMMIN((mag + 1) >> 1, 4);
  return ctx + av1_nz_map_ctx_offset[tx_size][coeff_idx];
}
static AOM_FORCE_INLINE int get_lower_levels_ctx(const uint8_t *levels,
                                                 int coeff_idx, int bhl,
                                                 TX_SIZE tx_size,
                                                 TX_CLASS tx_class) {
  if ((tx_class | coeff_idx) == 0) return 0;
  const int stats =
      get_nz_mag(levels + get_padded_idx(coeff_idx, bhl), bhl, tx_class);
  return get_nz_map_ctx_from_stats(stats, coeff_idx, bhl, tx_size, tx_class);
}

static AOM_FORCE_INLINE int get_lower_levels_ctx_fast(
    const uint8_t *levels, int coeff_idx, int bhl, intptr_t stride,
    const int8_t *nz_map_ctx_offset, TX_CLASS tx_class) {
  if ((tx_class | coeff_idx) == 0) return 0;
  const intptr_t col = (intptr_t)coeff_idx >> bhl;
  const int stats =
      get_nz_mag_stride(levels + (intptr_t)coeff_idx + (col << TX_PAD_HOR_LOG2),
                        stride, tx_class);
  int ctx = (stats + 1) >> 1;
  ctx = AOMMIN(ctx, 4);
  switch (tx_class) {
    case TX_CLASS_2D: return ctx + nz_map_ctx_offset[coeff_idx];
    case TX_CLASS_HORIZ: return ctx + nz_map_ctx_offset_1d[col];
    case TX_CLASS_VERT:
      return ctx + nz_map_ctx_offset_1d[coeff_idx & ((int)stride - 5)];
    default: return 0;
  }
}

static AOM_FORCE_INLINE int get_lower_levels_ctx_simple_fast(
    const uint8_t *levels, int coeff_idx, int bhl, intptr_t stride,
    const int8_t *nz_map_ctx_offset, TX_CLASS tx_class) {
  const intptr_t col = (intptr_t)coeff_idx >> bhl;
  const int stats =
      get_nz_mag_stride(levels + (intptr_t)coeff_idx + (col << TX_PAD_HOR_LOG2),
                        stride, tx_class);
  int ctx = (stats + 1) >> 1;
  ctx = AOMMIN(ctx, 4);
  switch (tx_class) {
    case TX_CLASS_2D: return ctx + nz_map_ctx_offset[coeff_idx];
    case TX_CLASS_HORIZ: return ctx + nz_map_ctx_offset_1d[col];
    case TX_CLASS_VERT:
      return ctx + nz_map_ctx_offset_1d[coeff_idx & ((int)stride - 5)];
    default: return 0;
  }
}

static inline int get_lower_levels_ctx_general(int is_last, int scan_idx,
                                               int bhl, int width,
                                               const uint8_t *levels,
                                               int coeff_idx, TX_SIZE tx_size,
                                               TX_CLASS tx_class) {
  if (is_last) {
    if (scan_idx == 0) return 0;
    if (scan_idx <= (width << bhl) >> 3) return 1;
    if (scan_idx <= (width << bhl) >> 2) return 2;
    return 3;
  }
  return get_lower_levels_ctx(levels, coeff_idx, bhl, tx_size, tx_class);
}

static inline void set_dc_sign(int *cul_level, int dc_val) {
  *cul_level += ((dc_val < 0) + ((dc_val > 0) << 1)) << COEFF_CONTEXT_BITS;
}

static inline uint8_t av1_get_txb_entropy_context(const tran_low_t *qcoeff,
                                                  const SCAN_ORDER *scan_order,
                                                  int eob) {
  if (eob == 0) return 0;
  const tran_low_t dc_val = qcoeff[0];
  const int dc_sign = ((dc_val < 0) + ((dc_val > 0) << 1))
                      << COEFF_CONTEXT_BITS;
  int cul_level = abs(dc_val);
  if (cul_level >= COEFF_CONTEXT_MASK) {
    return (uint8_t)(COEFF_CONTEXT_MASK | dc_sign);
  }
  if (eob == 1) {
    return (uint8_t)(cul_level | dc_sign);
  }
  const int16_t *const scan = scan_order->scan;
  for (int c = 1; c < eob; ++c) {
    cul_level += abs(qcoeff[scan[c]]);
    if (cul_level >= COEFF_CONTEXT_MASK) {
      return (uint8_t)(COEFF_CONTEXT_MASK | dc_sign);
    }
  }
  return (uint8_t)(cul_level | dc_sign);
}

#define MAX_TX_SIZE_UNIT 16
static const uint8_t kTxbCtxSkipCat[3][3] = {
  { 1, 2, 3 },
  { 2, 4, 5 },
  { 3, 5, 6 },
};

static inline uint64_t load_entropy_ctx_u64(const ENTROPY_CONTEXT *p,
                                            int unit) {
  uint64_t v;
  if (unit == 1) {
    v = (uint8_t)p[0];
  } else if (unit == 2) {
    uint16_t v2;
    memcpy(&v2, p, 2);
    v = v2;
  } else if (unit == 4) {
    uint32_t v4;
    memcpy(&v4, p, 4);
    v = v4;
  } else {
    memcpy(&v, p, 8);
  }
  return v;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void get_txb_ctx_general(const BLOCK_SIZE plane_bsize,
                                const TX_SIZE tx_size, const int plane,
                                const ENTROPY_CONTEXT *const a,
                                const ENTROPY_CONTEXT *const l,
                                TXB_CTX *const txb_ctx) {
  const int txb_w_unit = tx_size_wide_unit[tx_size];
  const int txb_h_unit = tx_size_high_unit[tx_size];
  const uint64_t va0 = (txb_w_unit == 16) ? load_entropy_ctx_u64(a, 8)
                                          : load_entropy_ctx_u64(a, txb_w_unit);
  const uint64_t va1 = (txb_w_unit == 16) ? load_entropy_ctx_u64(a + 8, 8) : 0;
  const uint64_t vl0 = (txb_h_unit == 16) ? load_entropy_ctx_u64(l, 8)
                                          : load_entropy_ctx_u64(l, txb_h_unit);
  const uint64_t vl1 = (txb_h_unit == 16) ? load_entropy_ctx_u64(l + 8, 8) : 0;

  const uint64_t pos = ((va0 >> 4) & 0x0101010101010101ULL) +
                       ((va1 >> 4) & 0x0101010101010101ULL) +
                       ((vl0 >> 4) & 0x0101010101010101ULL) +
                       ((vl1 >> 4) & 0x0101010101010101ULL);
  const uint64_t neg = ((va0 >> 3) & 0x0101010101010101ULL) +
                       ((va1 >> 3) & 0x0101010101010101ULL) +
                       ((vl0 >> 3) & 0x0101010101010101ULL) +
                       ((vl1 >> 3) & 0x0101010101010101ULL);
  const int64_t sh = (int64_t)((pos - neg) * 0x0101010101010101ULL) >> 55;
  txb_ctx->dc_sign_ctx = (sh < -1) + ((sh > 0) << 1);

  const uint64_t va_or = va0 | va1;
  const uint64_t vl_or = vl0 | vl1;
  if (plane == 0) {
    if (plane_bsize == txsize_to_bsize[tx_size]) {
      txb_ctx->txb_skip_ctx = 0;
    } else {
      const int top_cat =
          (va_or != 0) + (((va_or >> 2) & 0x0101010101010101ULL) != 0);
      const int left_cat =
          (vl_or != 0) + (((vl_or >> 2) & 0x0101010101010101ULL) != 0);
      txb_ctx->txb_skip_ctx = kTxbCtxSkipCat[top_cat][left_cat];
    }
  } else {
    const int ctx_base = (va_or != 0) + (vl_or != 0);
    const int ctx_offset = (num_pels_log2_lookup[plane_bsize] >
                            num_pels_log2_lookup[txsize_to_bsize[tx_size]])
                               ? 10
                               : 7;
    txb_ctx->txb_skip_ctx = ctx_base + ctx_offset;
  }
}

static inline void get_txb_ctx(const BLOCK_SIZE plane_bsize,
                               const TX_SIZE tx_size, const int plane,
                               const ENTROPY_CONTEXT *const a,
                               const ENTROPY_CONTEXT *const l,
                               TXB_CTX *const txb_ctx) {
  uint64_t va, vl;
  if (tx_size == TX_4X4) {
    va = (uint8_t)a[0];
    vl = (uint8_t)l[0];
  } else if (tx_size == TX_8X8) {
    uint16_t va2, vl2;
    memcpy(&va2, a, 2);
    memcpy(&vl2, l, 2);
    va = va2;
    vl = vl2;
  } else if (tx_size == TX_16X16) {
    uint32_t va4, vl4;
    memcpy(&va4, a, 4);
    memcpy(&vl4, l, 4);
    va = va4;
    vl = vl4;
  } else if (LIKELY(tx_size == TX_32X32)) {
    memcpy(&va, a, 8);
    memcpy(&vl, l, 8);
  } else {
    get_txb_ctx_general(plane_bsize, tx_size, plane, a, l, txb_ctx);
    return;
  }

  const uint64_t pos =
      ((va >> 4) & 0x0101010101010101ULL) + ((vl >> 4) & 0x0101010101010101ULL);
  const uint64_t neg =
      ((va >> 3) & 0x0101010101010101ULL) + ((vl >> 3) & 0x0101010101010101ULL);
  const int64_t sh = (int64_t)((pos - neg) * 0x0101010101010101ULL) >> 55;
  txb_ctx->dc_sign_ctx = (sh < -1) + ((sh > 0) << 1);

  if (plane == 0) {
    if (plane_bsize == txsize_to_bsize[tx_size]) {
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
    const int ctx_offset = (num_pels_log2_lookup[plane_bsize] >
                            num_pels_log2_lookup[txsize_to_bsize[tx_size]])
                               ? 10
                               : 7;
    txb_ctx->txb_skip_ctx = ctx_base + ctx_offset;
  }
}
#undef MAX_TX_SIZE_UNIT

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_COMMON_TXB_COMMON_H_
