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

#ifndef AOM_AOM_DSP_WASM_SIMD128_UTILS_H_
#define AOM_AOM_DSP_WASM_SIMD128_UTILS_H_

#include <wasm_simd128.h>

#include "config/aom_config.h"

static inline int32_t aom_i32x4_hadd(v128_t v) {
  return wasm_i32x4_extract_lane(v, 0) + wasm_i32x4_extract_lane(v, 1) +
         wasm_i32x4_extract_lane(v, 2) + wasm_i32x4_extract_lane(v, 3);
}

// Equivalents of SSE2 unpacklo/hi.
#define aom_unpacklo_i16(a, b) \
  wasm_i16x8_shuffle(a, b, 0, 8, 1, 9, 2, 10, 3, 11)
#define aom_unpackhi_i16(a, b) \
  wasm_i16x8_shuffle(a, b, 4, 12, 5, 13, 6, 14, 7, 15)
#define aom_unpacklo_i32(a, b) wasm_i32x4_shuffle(a, b, 0, 4, 1, 5)
#define aom_unpackhi_i32(a, b) wasm_i32x4_shuffle(a, b, 2, 6, 3, 7)
#define aom_unpacklo_i64(a, b) wasm_i64x2_shuffle(a, b, 0, 2)
#define aom_unpackhi_i64(a, b) wasm_i64x2_shuffle(a, b, 1, 3)

// In-place transpose of an 8x8 matrix of int16 (one row per vector).
static inline void aom_transpose_i16_8x8(v128_t *r) {
  const v128_t a0 = aom_unpacklo_i16(r[0], r[1]);
  const v128_t a1 = aom_unpacklo_i16(r[2], r[3]);
  const v128_t a2 = aom_unpacklo_i16(r[4], r[5]);
  const v128_t a3 = aom_unpacklo_i16(r[6], r[7]);
  const v128_t a4 = aom_unpackhi_i16(r[0], r[1]);
  const v128_t a5 = aom_unpackhi_i16(r[2], r[3]);
  const v128_t a6 = aom_unpackhi_i16(r[4], r[5]);
  const v128_t a7 = aom_unpackhi_i16(r[6], r[7]);
  const v128_t b0 = aom_unpacklo_i32(a0, a1);
  const v128_t b1 = aom_unpacklo_i32(a2, a3);
  const v128_t b2 = aom_unpackhi_i32(a0, a1);
  const v128_t b3 = aom_unpackhi_i32(a2, a3);
  const v128_t b4 = aom_unpacklo_i32(a4, a5);
  const v128_t b5 = aom_unpacklo_i32(a6, a7);
  const v128_t b6 = aom_unpackhi_i32(a4, a5);
  const v128_t b7 = aom_unpackhi_i32(a6, a7);
  r[0] = aom_unpacklo_i64(b0, b1);
  r[1] = aom_unpackhi_i64(b0, b1);
  r[2] = aom_unpacklo_i64(b2, b3);
  r[3] = aom_unpackhi_i64(b2, b3);
  r[4] = aom_unpacklo_i64(b4, b5);
  r[5] = aom_unpackhi_i64(b4, b5);
  r[6] = aom_unpacklo_i64(b6, b7);
  r[7] = aom_unpackhi_i64(b6, b7);
}

// Transpose of a 4x4 matrix of int32 (one row per vector).
static inline void aom_transpose_i32_4x4(const v128_t *in, v128_t *out) {
  const v128_t a0 = aom_unpacklo_i32(in[0], in[1]);
  const v128_t a1 = aom_unpacklo_i32(in[2], in[3]);
  const v128_t a2 = aom_unpackhi_i32(in[0], in[1]);
  const v128_t a3 = aom_unpackhi_i32(in[2], in[3]);
  out[0] = aom_unpacklo_i64(a0, a1);
  out[1] = aom_unpackhi_i64(a0, a1);
  out[2] = aom_unpacklo_i64(a2, a3);
  out[3] = aom_unpackhi_i64(a2, a3);
}

#endif  // AOM_AOM_DSP_WASM_SIMD128_UTILS_H_
