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

#include <stdlib.h>
#include <wasm_simd128.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/wasm/simd128_utils.h"

int aom_satd_simd128(const tran_low_t *coeff, int length) {
  v128_t acc = wasm_i32x4_splat(0);
  int i = 0;
  for (; i + 8 <= length; i += 8) {
    const v128_t a = wasm_v128_load(coeff + i);
    const v128_t b = wasm_v128_load(coeff + i + 4);
    acc = wasm_i32x4_add(acc, wasm_i32x4_abs(a));
    acc = wasm_i32x4_add(acc, wasm_i32x4_abs(b));
  }
  int satd = aom_i32x4_hadd(acc);
  for (; i < length; ++i) satd += abs(coeff[i]);
  return satd;
}

#if CONFIG_AV1_HIGHBITDEPTH
// One 8-point Hadamard pass, applied to 8 independent lanes. Inputs/outputs
// follow the ordering of hadamard_highbd_col8_*_pass() in avg.c.
#define HADAMARD8(ADD, SUB, in, out)     \
  do {                                   \
    const v128_t b0 = ADD(in[0], in[1]); \
    const v128_t b1 = SUB(in[0], in[1]); \
    const v128_t b2 = ADD(in[2], in[3]); \
    const v128_t b3 = SUB(in[2], in[3]); \
    const v128_t b4 = ADD(in[4], in[5]); \
    const v128_t b5 = SUB(in[4], in[5]); \
    const v128_t b6 = ADD(in[6], in[7]); \
    const v128_t b7 = SUB(in[6], in[7]); \
    const v128_t c0 = ADD(b0, b2);       \
    const v128_t c1 = ADD(b1, b3);       \
    const v128_t c2 = SUB(b0, b2);       \
    const v128_t c3 = SUB(b1, b3);       \
    const v128_t c4 = ADD(b4, b6);       \
    const v128_t c5 = ADD(b5, b7);       \
    const v128_t c6 = SUB(b4, b6);       \
    const v128_t c7 = SUB(b5, b7);       \
    out[0] = ADD(c0, c4);                \
    out[7] = ADD(c1, c5);                \
    out[3] = ADD(c2, c6);                \
    out[4] = ADD(c3, c7);                \
    out[2] = SUB(c0, c4);                \
    out[6] = SUB(c1, c5);                \
    out[1] = SUB(c2, c6);                \
    out[5] = SUB(c3, c7);                \
  } while (0)

void aom_highbd_hadamard_8x8_simd128(const int16_t *src_diff,
                                     ptrdiff_t src_stride, tran_low_t *coeff) {
  v128_t r[8], t[8];
  for (int i = 0; i < 8; ++i) r[i] = wasm_v128_load(src_diff + i * src_stride);
  // First pass (int16, wrapping like the C code): lanes are columns.
  HADAMARD8(wasm_i16x8_add, wasm_i16x8_sub, r, t);
  // The C code stores column-pass outputs column-major; transposing makes
  // each vector hold one input "row" of the second pass across 8 lanes.
  aom_transpose_i16_8x8(t);
  // Second pass in int32: low and high halves.
  v128_t lo[8], hi[8], olo[8], ohi[8];
  for (int i = 0; i < 8; ++i) {
    lo[i] = wasm_i32x4_extend_low_i16x8(t[i]);
    hi[i] = wasm_i32x4_extend_high_i16x8(t[i]);
  }
  HADAMARD8(wasm_i32x4_add, wasm_i32x4_sub, lo, olo);
  HADAMARD8(wasm_i32x4_add, wasm_i32x4_sub, hi, ohi);
  // olo[k] lane j = output k of second-pass column j (j < 4); C layout is
  // coeff[8 * j + k], i.e. a transpose.
  v128_t blk[4], tr[4];
  for (int kb = 0; kb < 8; kb += 4) {
    for (int half = 0; half < 2; ++half) {
      const v128_t *src = half ? ohi : olo;
      for (int i = 0; i < 4; ++i) blk[i] = src[kb + i];
      aom_transpose_i32_4x4(blk, tr);
      for (int i = 0; i < 4; ++i)
        wasm_v128_store(coeff + 8 * (half * 4 + i) + kb, tr[i]);
    }
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
