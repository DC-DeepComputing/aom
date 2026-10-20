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

// Lane-parallel transform helpers for WebAssembly SIMD128: one i32x4 holds the
// same coefficient position of four independent rows/columns. Arithmetic
// matches av1_txfm.h (32-bit wrapping where the C code relies on it, 64-bit
// where the C code needs it).

#ifndef AOM_AV1_COMMON_WASM_AV1_TXFM_SIMD128_H_
#define AOM_AV1_COMMON_WASM_AV1_TXFM_SIMD128_H_

#include <assert.h>
#include <wasm_simd128.h>

#include "av1/common/av1_txfm.h"

// half_btf() with 32-bit wrapping arithmetic, which av1_txfm.h documents as
// exact for conformant streams.
static inline v128_t half_btf_v(int32_t w0, v128_t in0, int32_t w1, v128_t in1,
                                int bit) {
  v128_t r = wasm_i32x4_add(wasm_i32x4_mul(in0, wasm_i32x4_splat(w0)),
                            wasm_i32x4_mul(in1, wasm_i32x4_splat(w1)));
  r = wasm_i32x4_add(r, wasm_i32x4_splat(1 << (bit - 1)));
  return wasm_i32x4_shr(r, bit);
}

// clamp_value() from av1_inv_txfm1d.h.
static inline v128_t clamp_v(v128_t v, int8_t bit) {
  if (bit <= 0 || bit >= 32) return v;
  const v128_t hi = wasm_i32x4_splat((1 << (bit - 1)) - 1);
  const v128_t lo = wasm_i32x4_splat(-(1 << (bit - 1)));
  return wasm_i32x4_max(wasm_i32x4_min(v, hi), lo);
}

// round_shift((int64_t)m * x, NewSqrt2Bits) per lane, in 64-bit.
static inline v128_t mul_round_shift12_64(v128_t x, int32_t m) {
  const v128_t mm = wasm_i64x2_splat(m);
  const v128_t rnd = wasm_i64x2_splat(1 << (NewSqrt2Bits - 1));
  v128_t lo = wasm_i64x2_mul(wasm_i64x2_extend_low_i32x4(x), mm);
  v128_t hi = wasm_i64x2_mul(wasm_i64x2_extend_high_i32x4(x), mm);
  lo = wasm_i64x2_shr(wasm_i64x2_add(lo, rnd), NewSqrt2Bits);
  hi = wasm_i64x2_shr(wasm_i64x2_add(hi, rnd), NewSqrt2Bits);
  return wasm_i32x4_shuffle(lo, hi, 0, 2, 4, 6);  // (int32_t) truncation.
}

// av1_round_shift_array() for bit >= 0 (rounding right shift).
static inline v128_t round_shift_right_v(v128_t v, int bit) {
  assert(bit >= 0);
  if (bit == 0) return v;
  return wasm_i32x4_shr(wasm_i32x4_add(v, wasm_i32x4_splat(1 << (bit - 1))),
                        bit);
}

#endif  // AOM_AV1_COMMON_WASM_AV1_TXFM_SIMD128_H_
