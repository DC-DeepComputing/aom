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

#include <string.h>
#include <wasm_simd128.h>

#include "config/av1_rtcd.h"

#include "av1/common/enums.h"

// levels[i * (height + TX_PAD_HOR) + j] = min(abs(coeff[i * height + j]), 127)
// followed by TX_PAD_HOR zeros per column, plus the bottom padding.
void av1_txb_init_levels_simd128(const tran_low_t *const coeff, const int width,
                                 const int height, uint8_t *const levels) {
  const int stride = height + TX_PAD_HOR;
  memset(levels + stride * width, 0,
         sizeof(*levels) * (TX_PAD_BOTTOM * stride + TX_PAD_END));
  const v128_t zero = wasm_i32x4_splat(0);
  uint8_t *ls = levels;
  const tran_low_t *cf = coeff;
  if (height == 4) {
    for (int i = 0; i < width; ++i) {
      const v128_t a = wasm_i32x4_abs(wasm_v128_load(cf));
      // Saturating narrows: |c| fits i32; i16 then i8 saturation clamps to
      // [0, 127] for non-negative inputs (abs(INT32_MIN) saturates too).
      const v128_t s16 = wasm_i16x8_narrow_i32x4(a, zero);
      const v128_t s8 = wasm_i8x16_narrow_i16x8(s16, zero);
      wasm_v128_store64_lane(ls, s8, 0);  // 4 levels + 4 zero pads.
      ls += stride;
      cf += 4;
    }
    return;
  }
  for (int i = 0; i < width; ++i) {
    int j = 0;
    for (; j + 16 <= height; j += 16) {
      const v128_t a0 = wasm_i32x4_abs(wasm_v128_load(cf + j));
      const v128_t a1 = wasm_i32x4_abs(wasm_v128_load(cf + j + 4));
      const v128_t a2 = wasm_i32x4_abs(wasm_v128_load(cf + j + 8));
      const v128_t a3 = wasm_i32x4_abs(wasm_v128_load(cf + j + 12));
      const v128_t s0 = wasm_i16x8_narrow_i32x4(a0, a1);
      const v128_t s1 = wasm_i16x8_narrow_i32x4(a2, a3);
      wasm_v128_store(ls + j, wasm_i8x16_narrow_i16x8(s0, s1));
    }
    for (; j + 8 <= height; j += 8) {
      const v128_t a0 = wasm_i32x4_abs(wasm_v128_load(cf + j));
      const v128_t a1 = wasm_i32x4_abs(wasm_v128_load(cf + j + 4));
      const v128_t s0 = wasm_i16x8_narrow_i32x4(a0, a1);
      wasm_v128_store64_lane(ls + j, wasm_i8x16_narrow_i16x8(s0, zero), 0);
    }
    memset(ls + height, 0, TX_PAD_HOR);
    ls += stride;
    cf += height;
  }
}
