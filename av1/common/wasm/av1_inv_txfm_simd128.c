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

// High bit-depth inverse transforms for WebAssembly SIMD128.
//
// Each 1D transform runs on four independent rows (or columns) at once, one
// per i32x4 lane, using the same 32-bit arithmetic as the C reference (which
// libaom documents as exact for conformant streams), so results are
// bit-exact with av1_highbd_inv_txfm_add_c().

#include <assert.h>
#include <wasm_simd128.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/wasm/simd128_utils.h"
#include "av1/common/av1_inv_txfm1d.h"
#include "av1/common/av1_inv_txfm1d_cfg.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/enums.h"
#include "av1/common/idct.h"

static inline v128_t half_btf_v(int32_t w0, v128_t in0, int32_t w1, v128_t in1,
                                int bit) {
  v128_t r = wasm_i32x4_add(wasm_i32x4_mul(in0, wasm_i32x4_splat(w0)),
                            wasm_i32x4_mul(in1, wasm_i32x4_splat(w1)));
  r = wasm_i32x4_add(r, wasm_i32x4_splat(1 << (bit - 1)));
  return wasm_i32x4_shr(r, bit);
}

static inline v128_t clamp_v(v128_t v, int8_t bit) {
  if (bit <= 0 || bit >= 32) return v;
  const v128_t hi = wasm_i32x4_splat((1 << (bit - 1)) - 1);
  const v128_t lo = wasm_i32x4_splat(-(1 << (bit - 1)));
  return wasm_i32x4_max(wasm_i32x4_min(v, hi), lo);
}

#include "av1/common/wasm/av1_inv_txfm1d_simd128.inc"

// round_shift((int64_t)m * x, 12) per lane, in 64-bit like the C code.
static inline v128_t mul_round_shift12_64(v128_t x, int32_t m) {
  const v128_t mm = wasm_i64x2_splat(m);
  const v128_t rnd = wasm_i64x2_splat(1 << (NewSqrt2Bits - 1));
  v128_t lo = wasm_i64x2_mul(wasm_i64x2_extend_low_i32x4(x), mm);
  v128_t hi = wasm_i64x2_mul(wasm_i64x2_extend_high_i32x4(x), mm);
  lo = wasm_i64x2_shr(wasm_i64x2_add(lo, rnd), NewSqrt2Bits);
  hi = wasm_i64x2_shr(wasm_i64x2_add(hi, rnd), NewSqrt2Bits);
  return wasm_i32x4_shuffle(lo, hi, 0, 2, 4, 6);  // (int32_t) truncation.
}

static void iidentity4_simd128(const v128_t *in, v128_t *out, int8_t cos_bit,
                               const int8_t *stage_range) {
  (void)cos_bit;
  (void)stage_range;
  for (int i = 0; i < 4; ++i) out[i] = mul_round_shift12_64(in[i], NewSqrt2);
}

static void iidentity8_simd128(const v128_t *in, v128_t *out, int8_t cos_bit,
                               const int8_t *stage_range) {
  (void)cos_bit;
  (void)stage_range;
  for (int i = 0; i < 8; ++i) out[i] = wasm_i32x4_shl(in[i], 1);
}

static void iidentity16_simd128(const v128_t *in, v128_t *out, int8_t cos_bit,
                                const int8_t *stage_range) {
  (void)cos_bit;
  (void)stage_range;
  for (int i = 0; i < 16; ++i)
    out[i] = mul_round_shift12_64(in[i], NewSqrt2 * 2);
}

static void iidentity32_simd128(const v128_t *in, v128_t *out, int8_t cos_bit,
                                const int8_t *stage_range) {
  (void)cos_bit;
  (void)stage_range;
  for (int i = 0; i < 32; ++i) out[i] = wasm_i32x4_shl(in[i], 2);
}

// av1_iadst4 uses 64-bit intermediates; mirror it in i64x2 halves.
static void iadst4_half(const int64_t *x, int64_t *o, int bit) {
  const int32_t *sinpi = sinpi_arr(bit);
  int64_t x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3];
  int64_t s0, s1, s2, s3, s4, s5, s6, s7;
  if (!(x0 | x1 | x2 | x3)) {
    o[0] = o[1] = o[2] = o[3] = 0;
    return;
  }
  s0 = sinpi[1] * x0;
  s1 = sinpi[2] * x0;
  s2 = sinpi[3] * x1;
  s3 = sinpi[4] * x2;
  s4 = sinpi[1] * x2;
  s5 = sinpi[2] * x3;
  s6 = sinpi[4] * x3;
  s7 = (x0 - x2) + x3;
  s0 = s0 + s3;
  s1 = s1 - s4;
  s3 = s2;
  s2 = sinpi[3] * s7;
  s0 = s0 + s5;
  s1 = s1 - s6;
  x0 = s0 + s3;
  x1 = s1 + s3;
  x2 = s2;
  x3 = s0 + s1;
  x3 = x3 - s3;
  o[0] = round_shift(x0, bit);
  o[1] = round_shift(x1, bit);
  o[2] = round_shift(x2, bit);
  o[3] = round_shift(x3, bit);
}

static void iadst4_simd128(const v128_t *in, v128_t *out, int8_t cos_bit,
                           const int8_t *stage_range) {
  (void)stage_range;
  // Rare in practice (4-point ADST); per-lane scalar keeps exact semantics,
  // including the all-zero early exit, which is evaluated per lane in C.
  int32_t lanes[4][4];
  for (int i = 0; i < 4; ++i) wasm_v128_store(lanes[i], in[i]);
  int32_t res[4][4];
  for (int l = 0; l < 4; ++l) {
    int64_t x[4] = { lanes[0][l], lanes[1][l], lanes[2][l], lanes[3][l] };
    int64_t o[4];
    iadst4_half(x, o, cos_bit);
    for (int i = 0; i < 4; ++i) res[i][l] = (int32_t)o[i];
  }
  for (int i = 0; i < 4; ++i) out[i] = wasm_v128_load(res[i]);
}

typedef void (*TxfmFuncV)(const v128_t *input, v128_t *output, int8_t cos_bit,
                          const int8_t *stage_range);

static TxfmFuncV txfm_func_v(TXFM_TYPE t) {
  switch (t) {
    case TXFM_TYPE_DCT4: return idct4_simd128;
    case TXFM_TYPE_DCT8: return idct8_simd128;
    case TXFM_TYPE_DCT16: return idct16_simd128;
    case TXFM_TYPE_DCT32: return idct32_simd128;
    case TXFM_TYPE_ADST4: return iadst4_simd128;
    case TXFM_TYPE_ADST8: return iadst8_simd128;
    case TXFM_TYPE_ADST16: return iadst16_simd128;
    case TXFM_TYPE_IDENTITY4: return iidentity4_simd128;
    case TXFM_TYPE_IDENTITY8: return iidentity8_simd128;
    case TXFM_TYPE_IDENTITY16: return iidentity16_simd128;
    case TXFM_TYPE_IDENTITY32: return iidentity32_simd128;
    default: assert(0); return NULL;
  }
}

static inline v128_t round_shift_v(v128_t v, int bit) {
  // av1_round_shift_array(): bit > 0 rounds right; bit < 0 is a saturating
  // left shift (never used by inverse transforms with these configs).
  if (bit == 0) return v;
  assert(bit > 0);
  return wasm_i32x4_shr(wasm_i32x4_add(v, wasm_i32x4_splat(1 << (bit - 1))),
                        bit);
}

// Mirrors inv_txfm2d_add_c() in av1_inv_txfm2d.c for sizes up to 32x32.
static void inv_txfm2d_add_simd128(const int32_t *input, uint16_t *output,
                                   int stride, TX_TYPE tx_type, TX_SIZE tx_size,
                                   int bd) {
  TXFM_2D_FLIP_CFG cfg;
  av1_get_inv_txfm_cfg(tx_type, tx_size, &cfg);
  const int w = tx_size_wide[cfg.tx_size];
  const int h = tx_size_high[cfg.tx_size];
  const int8_t *shift = cfg.shift;
  const int rect_type = get_rect_tx_log_ratio(w, h);
  int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
  int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
  av1_gen_inv_stage_range(stage_range_col, stage_range_row, &cfg, tx_size, bd);
  const TxfmFuncV row_fn = txfm_func_v(cfg.txfm_type_row);
  const TxfmFuncV col_fn = txfm_func_v(cfg.txfm_type_col);

  DECLARE_ALIGNED(16, int32_t, buf[32 * 32]);  // Row-pass output, row-major.
  v128_t tin[32], tout[32];

  // Rows, four at a time: lane l holds row r + l.
  for (int r = 0; r < h; r += 4) {
    for (int c = 0; c < w; ++c) {
      v128_t v = wasm_v128_load(input + c * h + r);
      if (abs(rect_type) == 1) v = mul_round_shift12_64(v, NewInvSqrt2);
      tin[c] = clamp_v(v, bd + 8);
    }
    row_fn(tin, tout, cfg.cos_bit_row, stage_range_row);
    for (int c = 0; c < w; c += 4) {
      v128_t blk[4], tr[4];
      for (int i = 0; i < 4; ++i)
        blk[i] = round_shift_v(tout[c + i], -shift[0]);
      aom_transpose_i32_4x4(blk, tr);  // tr[l] = row r + l, cols c..c+3.
      for (int l = 0; l < 4; ++l) wasm_v128_store(buf + (r + l) * w + c, tr[l]);
    }
  }

  // Columns, four at a time: lane l holds column c + l.
  const int max_val = (1 << bd) - 1;
  const v128_t vmax = wasm_i32x4_splat(max_val);
  const v128_t vzero = wasm_i32x4_splat(0);
  const int8_t col_clamp = AOMMAX(bd + 6, 16);
  for (int c = 0; c < w; c += 4) {
    for (int r = 0; r < h; ++r) {
      v128_t v;
      if (cfg.lr_flip == 0) {
        v = wasm_v128_load(buf + r * w + c);
      } else {
        v = wasm_v128_load(buf + r * w + (w - c - 4));
        v = wasm_i32x4_shuffle(v, v, 3, 2, 1, 0);
      }
      tin[r] = clamp_v(v, col_clamp);
    }
    col_fn(tin, tout, cfg.cos_bit_col, stage_range_col);
    for (int r = 0; r < h; ++r) {
      const v128_t res =
          round_shift_v(tout[cfg.ud_flip ? h - r - 1 : r], -shift[1]);
      uint16_t *dst = output + r * stride + c;
      const v128_t d = wasm_u32x4_load16x4(dst);
      v128_t s = wasm_i32x4_add(d, res);
      s = wasm_i32x4_max(wasm_i32x4_min(s, vmax), vzero);
      wasm_v128_store64_lane(dst, wasm_u16x8_narrow_i32x4(s, s), 0);
    }
  }
}

void av1_highbd_inv_txfm_add_simd128(const tran_low_t *input, uint8_t *dest,
                                     int stride, const TxfmParam *txfm_param) {
  const TX_SIZE tx_size = txfm_param->tx_size;
  // 64-point sizes (which zero-pad a 32x32 input) and lossless WHT use C.
  if (txfm_param->lossless || tx_size_wide[tx_size] == 64 ||
      tx_size_high[tx_size] == 64) {
    av1_highbd_inv_txfm_add_c(input, dest, stride, txfm_param);
    return;
  }
  inv_txfm2d_add_simd128(input, CONVERT_TO_SHORTPTR(dest), stride,
                         txfm_param->tx_type, tx_size, txfm_param->bd);
}
