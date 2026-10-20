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

// High bit-depth intra predictors for WebAssembly SIMD128. Bit-exact with
// the C versions in aom_dsp/intrapred.c. Widths are processed four pixels at
// a time (bw >= 4 for every AV1 block size).

#include <wasm_simd128.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/intrapred_common.h"
#include "aom_dsp/wasm/simd128_utils.h"

static inline void store_u16x4(uint16_t *dst, v128_t v32) {
  wasm_v128_store64_lane(dst, wasm_u16x8_narrow_i32x4(v32, v32), 0);
}

static inline v128_t load_u16x4(const uint16_t *src) {
  return wasm_u32x4_load16x4(src);
}

// Weights smooth_weights[n - 4 + c .. c + 3] widened to i32x4.
static inline v128_t load_weights4(const uint8_t *w) {
  const v128_t b = wasm_v128_load32_zero(w);
  return wasm_u32x4_extend_low_u16x8(wasm_u16x8_extend_low_u8x16(b));
}

static inline void fill(uint16_t *dst, ptrdiff_t stride, int bw, int bh,
                        uint16_t value) {
  const v128_t v = wasm_i16x8_splat((int16_t)value);
  for (int r = 0; r < bh; ++r) {
    int c = 0;
    for (; c + 8 <= bw; c += 8) wasm_v128_store(dst + c, v);
    if (c < bw) wasm_v128_store64_lane(dst + c, v, 0);  // bw == 4
    dst += stride;
  }
}

static inline int sum_u16(const uint16_t *p, int n) {
  v128_t acc = wasm_i32x4_splat(0);
  for (int i = 0; i < n; i += 4) acc = wasm_i32x4_add(acc, load_u16x4(p + i));
  return aom_i32x4_hadd(acc);
}

static inline void highbd_v(uint16_t *dst, ptrdiff_t stride, int bw, int bh,
                            const uint16_t *above) {
  for (int r = 0; r < bh; ++r) {
    int c = 0;
    for (; c + 8 <= bw; c += 8)
      wasm_v128_store(dst + c, wasm_v128_load(above + c));
    if (c < bw)
      wasm_v128_store64_lane(dst + c, wasm_v128_load64_zero(above + c), 0);
    dst += stride;
  }
}

static inline void highbd_h(uint16_t *dst, ptrdiff_t stride, int bw, int bh,
                            const uint16_t *left) {
  for (int r = 0; r < bh; ++r) {
    fill(dst, stride, bw, 1, left[r]);
    dst += stride;
  }
}

static inline void highbd_paeth(uint16_t *dst, ptrdiff_t stride, int bw, int bh,
                                const uint16_t *above, const uint16_t *left) {
  const v128_t tl = wasm_i32x4_splat(above[-1]);
  for (int r = 0; r < bh; ++r) {
    const v128_t l = wasm_i32x4_splat(left[r]);
    for (int c = 0; c < bw; c += 4) {
      const v128_t t = load_u16x4(above + c);
      const v128_t base = wasm_i32x4_sub(wasm_i32x4_add(t, l), tl);
      const v128_t p_left = wasm_i32x4_abs(wasm_i32x4_sub(base, l));
      const v128_t p_top = wasm_i32x4_abs(wasm_i32x4_sub(base, t));
      const v128_t p_tl = wasm_i32x4_abs(wasm_i32x4_sub(base, tl));
      // (p_left <= p_top && p_left <= p_tl) ? left : (p_top <= p_tl) ? top : tl
      const v128_t use_left = wasm_v128_and(wasm_i32x4_le(p_left, p_top),
                                            wasm_i32x4_le(p_left, p_tl));
      const v128_t use_top = wasm_i32x4_le(p_top, p_tl);
      const v128_t v =
          wasm_v128_bitselect(l, wasm_v128_bitselect(t, tl, use_top), use_left);
      store_u16x4(dst + c, v);
    }
    dst += stride;
  }
}

static inline void highbd_smooth(uint16_t *dst, ptrdiff_t stride, int bw,
                                 int bh, const uint16_t *above,
                                 const uint16_t *left) {
  const v128_t below = wasm_i32x4_splat(left[bh - 1]);
  const v128_t right = wasm_i32x4_splat(above[bw - 1]);
  const uint8_t *const wts_w = smooth_weights + bw - 4;
  const uint8_t *const wts_h = smooth_weights + bh - 4;
  const int scale = 1 << SMOOTH_WEIGHT_LOG2_SCALE;
  const v128_t vscale = wasm_i32x4_splat(scale);
  const v128_t rnd = wasm_i32x4_splat(1 << SMOOTH_WEIGHT_LOG2_SCALE);
  for (int r = 0; r < bh; ++r) {
    const v128_t wh = wasm_i32x4_splat(wts_h[r]);
    const v128_t wh_inv = wasm_i32x4_splat(scale - wts_h[r]);
    const v128_t l = wasm_i32x4_splat(left[r]);
    // Row-constant terms: (scale - wh) * below.
    const v128_t row_term = wasm_i32x4_mul(wh_inv, below);
    for (int c = 0; c < bw; c += 4) {
      const v128_t ww = load_weights4(wts_w + c);
      const v128_t ww_inv = wasm_i32x4_sub(vscale, ww);
      v128_t p = wasm_i32x4_mul(wh, load_u16x4(above + c));
      p = wasm_i32x4_add(p, row_term);
      p = wasm_i32x4_add(p, wasm_i32x4_mul(ww, l));
      p = wasm_i32x4_add(p, wasm_i32x4_mul(ww_inv, right));
      // divide_round(p, 1 + SMOOTH_WEIGHT_LOG2_SCALE)
      p = wasm_u32x4_shr(wasm_i32x4_add(p, rnd), 1 + SMOOTH_WEIGHT_LOG2_SCALE);
      store_u16x4(dst + c, p);
    }
    dst += stride;
  }
}

static inline void highbd_smooth_v(uint16_t *dst, ptrdiff_t stride, int bw,
                                   int bh, const uint16_t *above,
                                   const uint16_t *left) {
  const v128_t below = wasm_i32x4_splat(left[bh - 1]);
  const uint8_t *const wts = smooth_weights + bh - 4;
  const int scale = 1 << SMOOTH_WEIGHT_LOG2_SCALE;
  const v128_t rnd = wasm_i32x4_splat(1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1));
  for (int r = 0; r < bh; ++r) {
    const v128_t wh = wasm_i32x4_splat(wts[r]);
    const v128_t row_term =
        wasm_i32x4_mul(wasm_i32x4_splat(scale - wts[r]), below);
    for (int c = 0; c < bw; c += 4) {
      v128_t p =
          wasm_i32x4_add(wasm_i32x4_mul(wh, load_u16x4(above + c)), row_term);
      p = wasm_u32x4_shr(wasm_i32x4_add(p, rnd), SMOOTH_WEIGHT_LOG2_SCALE);
      store_u16x4(dst + c, p);
    }
    dst += stride;
  }
}

static inline void highbd_smooth_h(uint16_t *dst, ptrdiff_t stride, int bw,
                                   int bh, const uint16_t *above,
                                   const uint16_t *left) {
  const v128_t right = wasm_i32x4_splat(above[bw - 1]);
  const uint8_t *const wts = smooth_weights + bw - 4;
  const v128_t vscale = wasm_i32x4_splat(1 << SMOOTH_WEIGHT_LOG2_SCALE);
  const v128_t rnd = wasm_i32x4_splat(1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1));
  for (int r = 0; r < bh; ++r) {
    const v128_t l = wasm_i32x4_splat(left[r]);
    for (int c = 0; c < bw; c += 4) {
      const v128_t ww = load_weights4(wts + c);
      v128_t p =
          wasm_i32x4_add(wasm_i32x4_mul(ww, l),
                         wasm_i32x4_mul(wasm_i32x4_sub(vscale, ww), right));
      p = wasm_u32x4_shr(wasm_i32x4_add(p, rnd), SMOOTH_WEIGHT_LOG2_SCALE);
      store_u16x4(dst + c, p);
    }
    dst += stride;
  }
}

#define HIGHBD_PRED_SIZE(W, H)                                  \
  void aom_highbd_v_predictor_##W##x##H##_simd128(              \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,   \
      const uint16_t *left, int bd) {                           \
    (void)left;                                                 \
    (void)bd;                                                   \
    highbd_v(dst, stride, W, H, above);                         \
  }                                                             \
  void aom_highbd_h_predictor_##W##x##H##_simd128(              \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,   \
      const uint16_t *left, int bd) {                           \
    (void)above;                                                \
    (void)bd;                                                   \
    highbd_h(dst, stride, W, H, left);                          \
  }                                                             \
  void aom_highbd_dc_128_predictor_##W##x##H##_simd128(         \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,   \
      const uint16_t *left, int bd) {                           \
    (void)above;                                                \
    (void)left;                                                 \
    fill(dst, stride, W, H, (uint16_t)(128 << (bd - 8)));       \
  }                                                             \
  void aom_highbd_dc_left_predictor_##W##x##H##_simd128(        \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,   \
      const uint16_t *left, int bd) {                           \
    (void)above;                                                \
    (void)bd;                                                   \
    fill(dst, stride, W, H,                                     \
         (uint16_t)((sum_u16(left, H) + ((H) >> 1)) / (H)));    \
  }                                                             \
  void aom_highbd_dc_top_predictor_##W##x##H##_simd128(         \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,   \
      const uint16_t *left, int bd) {                           \
    (void)left;                                                 \
    (void)bd;                                                   \
    fill(dst, stride, W, H,                                     \
         (uint16_t)((sum_u16(above, W) + ((W) >> 1)) / (W)));   \
  }                                                             \
  void aom_highbd_dc_predictor_##W##x##H##_simd128(             \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,   \
      const uint16_t *left, int bd) {                           \
    (void)bd;                                                   \
    const int sum = sum_u16(above, W) + sum_u16(left, H);       \
    fill(dst, stride, W, H,                                     \
         (uint16_t)((sum + (((W) + (H)) >> 1)) / ((W) + (H)))); \
  }                                                             \
  void aom_highbd_paeth_predictor_##W##x##H##_simd128(          \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,   \
      const uint16_t *left, int bd) {                           \
    (void)bd;                                                   \
    highbd_paeth(dst, stride, W, H, above, left);               \
  }                                                             \
  void aom_highbd_smooth_predictor_##W##x##H##_simd128(         \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,   \
      const uint16_t *left, int bd) {                           \
    (void)bd;                                                   \
    highbd_smooth(dst, stride, W, H, above, left);              \
  }                                                             \
  void aom_highbd_smooth_v_predictor_##W##x##H##_simd128(       \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,   \
      const uint16_t *left, int bd) {                           \
    (void)bd;                                                   \
    highbd_smooth_v(dst, stride, W, H, above, left);            \
  }                                                             \
  void aom_highbd_smooth_h_predictor_##W##x##H##_simd128(       \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,   \
      const uint16_t *left, int bd) {                           \
    (void)bd;                                                   \
    highbd_smooth_h(dst, stride, W, H, above, left);            \
  }

HIGHBD_PRED_SIZE(4, 4)
HIGHBD_PRED_SIZE(4, 8)
HIGHBD_PRED_SIZE(8, 4)
HIGHBD_PRED_SIZE(8, 8)
HIGHBD_PRED_SIZE(8, 16)
HIGHBD_PRED_SIZE(16, 8)
HIGHBD_PRED_SIZE(16, 16)
HIGHBD_PRED_SIZE(16, 32)
HIGHBD_PRED_SIZE(32, 16)
HIGHBD_PRED_SIZE(32, 32)
HIGHBD_PRED_SIZE(32, 64)
HIGHBD_PRED_SIZE(64, 32)
HIGHBD_PRED_SIZE(64, 64)
#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
HIGHBD_PRED_SIZE(4, 16)
HIGHBD_PRED_SIZE(16, 4)
HIGHBD_PRED_SIZE(8, 32)
HIGHBD_PRED_SIZE(32, 8)
HIGHBD_PRED_SIZE(16, 64)
HIGHBD_PRED_SIZE(64, 16)
#endif
