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

// High bit-depth forward transforms for WebAssembly SIMD128. Mirrors
// fwd_txfm2d_c() in av1_fwd_txfm2d.c with lane-parallel 1D transforms (four
// columns, then four rows, per call), bit-exact with the C code.

#include <assert.h>
#include <wasm_simd128.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/wasm/simd128_utils.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/enums.h"
#include "av1/common/wasm/av1_txfm_simd128.h"
#include "av1/encoder/av1_fwd_txfm1d_cfg.h"

#include "av1/encoder/wasm/av1_fwd_txfm1d_simd128.inc"

static void fadst4_simd128(const v128_t *input, v128_t *output, int8_t cos_bit,
                           const int8_t *stage_range) {
  (void)stage_range;
  // av1_fadst4() in 32-bit arithmetic. Its all-zero early exit produces the
  // same zeros the arithmetic below does, so it needs no lane masking.
  const int32_t *sinpi = sinpi_arr(cos_bit);
  const v128_t x0 = input[0], x1 = input[1], x2 = input[2], x3 = input[3];
#define MULC(k, x) wasm_i32x4_mul(wasm_i32x4_splat(sinpi[k]), (x))
  const v128_t s0 = MULC(1, x0);
  const v128_t s1 = MULC(4, x0);
  const v128_t s2 = MULC(2, x1);
  const v128_t s3 = MULC(1, x1);
  const v128_t s4 = MULC(3, x2);
  const v128_t s5 = MULC(4, x3);
  const v128_t s6 = MULC(2, x3);
  const v128_t s7 = wasm_i32x4_sub(wasm_i32x4_add(x0, x1), x3);
  v128_t y0 = wasm_i32x4_add(s0, s2);
  const v128_t y1 = MULC(3, s7);
  v128_t y2 = wasm_i32x4_sub(s1, s3);
  const v128_t y3 = s4;
#undef MULC
  y0 = wasm_i32x4_add(y0, s5);
  y2 = wasm_i32x4_add(y2, s6);
  const v128_t o0 = wasm_i32x4_add(y0, y3);
  const v128_t o1 = y1;
  const v128_t o2 = wasm_i32x4_sub(y2, y3);
  const v128_t o3 = wasm_i32x4_add(wasm_i32x4_sub(y2, y0), y3);
  output[0] = round_shift_right_v(o0, cos_bit);
  output[1] = round_shift_right_v(o1, cos_bit);
  output[2] = round_shift_right_v(o2, cos_bit);
  output[3] = round_shift_right_v(o3, cos_bit);
}

static void fidentity4_simd128(const v128_t *in, v128_t *out, int8_t cos_bit,
                               const int8_t *stage_range) {
  (void)cos_bit;
  (void)stage_range;
  for (int i = 0; i < 4; ++i) out[i] = mul_round_shift12_64(in[i], NewSqrt2);
}

static void fidentity8_simd128(const v128_t *in, v128_t *out, int8_t cos_bit,
                               const int8_t *stage_range) {
  (void)cos_bit;
  (void)stage_range;
  for (int i = 0; i < 8; ++i) out[i] = wasm_i32x4_shl(in[i], 1);
}

static void fidentity16_simd128(const v128_t *in, v128_t *out, int8_t cos_bit,
                                const int8_t *stage_range) {
  (void)cos_bit;
  (void)stage_range;
  for (int i = 0; i < 16; ++i)
    out[i] = mul_round_shift12_64(in[i], 2 * NewSqrt2);
}

static void fidentity32_simd128(const v128_t *in, v128_t *out, int8_t cos_bit,
                                const int8_t *stage_range) {
  (void)cos_bit;
  (void)stage_range;
  for (int i = 0; i < 32; ++i) out[i] = wasm_i32x4_shl(in[i], 2);
}

typedef void (*FwdTxfmFuncV)(const v128_t *input, v128_t *output,
                             int8_t cos_bit, const int8_t *stage_range);

static FwdTxfmFuncV fwd_txfm_func_v(TXFM_TYPE t) {
  switch (t) {
    case TXFM_TYPE_DCT4: return fdct4_simd128;
    case TXFM_TYPE_DCT8: return fdct8_simd128;
    case TXFM_TYPE_DCT16: return fdct16_simd128;
    case TXFM_TYPE_DCT32: return fdct32_simd128;
    case TXFM_TYPE_ADST4: return fadst4_simd128;
    case TXFM_TYPE_ADST8: return fadst8_simd128;
    case TXFM_TYPE_ADST16: return fadst16_simd128;
    case TXFM_TYPE_IDENTITY4: return fidentity4_simd128;
    case TXFM_TYPE_IDENTITY8: return fidentity8_simd128;
    case TXFM_TYPE_IDENTITY16: return fidentity16_simd128;
    case TXFM_TYPE_IDENTITY32: return fidentity32_simd128;
    default: assert(0); return NULL;
  }
}

// Sizes up to 32x32 (64-point transforms keep using C).
static void fwd_txfm2d_simd128(const int16_t *input, int32_t *output,
                               int stride, TX_TYPE tx_type, TX_SIZE tx_size,
                               int bd) {
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, tx_size, &cfg);
  const int w = tx_size_wide[cfg.tx_size];
  const int h = tx_size_high[cfg.tx_size];
  assert(w <= 32 && h <= 32);
  const int8_t *shift = cfg.shift;
  // shift[0] >= 0 is a left shift of the int16 residual (cannot saturate);
  // shift[1], shift[2] <= 0 are rounding right shifts.
  assert(shift[0] >= 0 && shift[1] <= 0 && shift[2] <= 0);
  const int rect_type = get_rect_tx_log_ratio(w, h);
  // The SIMD128 1D transforms do no range checking, so bd is unused.
  (void)bd;
  const FwdTxfmFuncV col_fn = fwd_txfm_func_v(cfg.txfm_type_col);
  const FwdTxfmFuncV row_fn = fwd_txfm_func_v(cfg.txfm_type_row);

  DECLARE_ALIGNED(16, int32_t, buf[32 * 32]);  // Column-pass output, row-major.
  v128_t tin[32], tout[32];

  // Columns, four at a time: lane l holds column c + l.
  for (int c = 0; c < w; c += 4) {
    for (int r = 0; r < h; ++r) {
      const int rr = cfg.ud_flip ? h - r - 1 : r;
      tin[r] = wasm_i32x4_shl(wasm_i32x4_load16x4(input + rr * stride + c),
                              shift[0]);
    }
    col_fn(tin, tout, cfg.cos_bit_col, NULL);
    for (int r = 0; r < h; ++r) {
      v128_t v = round_shift_right_v(tout[r], -shift[1]);
      if (cfg.lr_flip == 0) {
        wasm_v128_store(buf + r * w + c, v);
      } else {
        v = wasm_i32x4_shuffle(v, v, 3, 2, 1, 0);
        wasm_v128_store(buf + r * w + (w - c - 4), v);
      }
    }
  }

  // Rows, four at a time: lane l holds row r + l.
  for (int r = 0; r < h; r += 4) {
    for (int c = 0; c < w; c += 4) {
      v128_t blk[4];
      for (int l = 0; l < 4; ++l)
        blk[l] = wasm_v128_load(buf + (r + l) * w + c);
      aom_transpose_i32_4x4(blk, tin + c);  // tin[c + i] = column c + i.
    }
    row_fn(tin, tout, cfg.cos_bit_row, NULL);
    for (int c = 0; c < w; ++c) {
      v128_t v = round_shift_right_v(tout[c], -shift[2]);
      if (abs(rect_type) == 1) v = mul_round_shift12_64(v, NewSqrt2);
      wasm_v128_store(output + c * h + r, v);  // output[c * h + r + l].
    }
  }
}

#define FWD_TXFM2D_SIMD128(W, H)                                          \
  void av1_fwd_txfm2d_##W##x##H##_simd128(const int16_t *input,           \
                                          int32_t *output, int stride,    \
                                          TX_TYPE tx_type, int bd) {      \
    fwd_txfm2d_simd128(input, output, stride, tx_type, TX_##W##X##H, bd); \
  }

FWD_TXFM2D_SIMD128(4, 4)
FWD_TXFM2D_SIMD128(8, 8)
FWD_TXFM2D_SIMD128(16, 16)
FWD_TXFM2D_SIMD128(32, 32)
FWD_TXFM2D_SIMD128(4, 8)
FWD_TXFM2D_SIMD128(8, 4)
FWD_TXFM2D_SIMD128(8, 16)
FWD_TXFM2D_SIMD128(16, 8)
FWD_TXFM2D_SIMD128(16, 32)
FWD_TXFM2D_SIMD128(32, 16)
FWD_TXFM2D_SIMD128(16, 4)
#if !CONFIG_REALTIME_ONLY
FWD_TXFM2D_SIMD128(4, 16)
FWD_TXFM2D_SIMD128(8, 32)
FWD_TXFM2D_SIMD128(32, 8)
#endif
