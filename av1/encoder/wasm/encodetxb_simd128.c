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

#include <assert.h>
#include <string.h>
#include <wasm_simd128.h>

#include "config/av1_rtcd.h"

#include "aom_ports/mem.h"
#include "av1/common/enums.h"
#include "av1/common/txb_common.h"

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

// get_4_nz_map_contexts_2d coefficients:
static const DECLARE_ALIGNED(16, uint8_t, c_4_po_2d[2][16]) = {
  { 0, 1, 6, 6, 1, 6, 6, 21, 6, 6, 21, 21, 6, 21, 21, 21 },
  { 0, 16, 16, 16, 16, 16, 16, 16, 6, 6, 21, 21, 6, 21, 21, 21 }
};

// get_4_nz_map_contexts_ver coefficients:
static const DECLARE_ALIGNED(16, uint8_t, c_4_po_ver[16]) = {
  SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10
};

// get_4_nz_map_contexts_hor coefficients:
static const DECLARE_ALIGNED(16, uint8_t, c_4_po_hor[16]) = {
  SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 0,
  SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 0,
  SIG_COEF_CONTEXTS_2D + 5,  SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 5,  SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10
};

// get_8_coeff_contexts_2d coefficients:
// if (width == 8)
static const DECLARE_ALIGNED(16, uint8_t, c_8_po_2d_8[2][16]) = {
  { 0, 1, 6, 6, 21, 21, 21, 21, 1, 6, 6, 21, 21, 21, 21, 21 },
  { 6, 6, 21, 21, 21, 21, 21, 21, 6, 21, 21, 21, 21, 21, 21, 21 }
};
// if (width < 8)
static const DECLARE_ALIGNED(16, uint8_t, c_8_po_2d_l[2][16]) = {
  { 0, 11, 6, 6, 21, 21, 21, 21, 11, 11, 6, 21, 21, 21, 21, 21 },
  { 11, 11, 21, 21, 21, 21, 21, 21, 11, 11, 21, 21, 21, 21, 21, 21 }
};
// if (width > 8)
static const DECLARE_ALIGNED(16, uint8_t, c_8_po_2d_g[2][16]) = {
  { 0, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16 },
  { 6, 6, 21, 21, 21, 21, 21, 21, 6, 21, 21, 21, 21, 21, 21, 21 }
};

// get_8_coeff_contexts_ver coefficients:
static const DECLARE_ALIGNED(16, uint8_t, c_8_po_ver[16]) = {
  SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10
};

// get_8_coeff_contexts_hor coefficients:
static const DECLARE_ALIGNED(16, uint8_t, c_8_po_hor[16]) = {
  SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 0,
  SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 0,
  SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 5, SIG_COEF_CONTEXTS_2D + 5, SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 5, SIG_COEF_CONTEXTS_2D + 5, SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 5
};

// get_16n_coeff_contexts_2d coefficients:
// real_width == real_height
static const DECLARE_ALIGNED(16, uint8_t, c_16_po_2d_e[4][16]) = {
  { 0, 1, 6, 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 },
  { 1, 6, 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 },
  { 6, 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 },
  { 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 }
};
// real_width < real_height
static const DECLARE_ALIGNED(16, uint8_t, c_16_po_2d_g[3][16]) = {
  { 0, 11, 6, 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 },
  { 11, 11, 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 },
  { 11, 11, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 }
};
// real_width > real_height
static const DECLARE_ALIGNED(16, uint8_t, c_16_po_2d_l[3][16]) = {
  { 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16 },
  { 6, 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 },
  { 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 }
};

// get_16n_coeff_contexts_ver coefficients:
static const DECLARE_ALIGNED(16, uint8_t, c_16_po_ver[16]) = {
  SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 5,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
  SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10
};

// Gathers rows of 4, 8 or 16 levels spaced `stride` apart into one vector.
static inline v128_t load_levels_4x4(const uint8_t *src, int stride) {
  v128_t v = wasm_v128_load32_zero(src);
  v = wasm_v128_load32_lane(src + 1 * stride, v, 1);
  v = wasm_v128_load32_lane(src + 2 * stride, v, 2);
  return wasm_v128_load32_lane(src + 3 * stride, v, 3);
}

static inline v128_t load_levels_8x2(const uint8_t *src, int stride) {
  return wasm_v128_load64_lane(src + stride, wasm_v128_load64_zero(src), 1);
}

static inline v128_t load_levels_16x1(const uint8_t *src, int stride) {
  (void)stride;
  return wasm_v128_load(src);
}

typedef v128_t (*LoadLevelsFn)(const uint8_t *src, int stride);

// Returns min((sum of min(level, 3) over the 5 neighbors + 1) >> 1, 4).
static inline v128_t get_coeff_contexts_kernel(const uint8_t *const src,
                                               const int stride,
                                               const ptrdiff_t *const offsets,
                                               LoadLevelsFn load) {
  const v128_t c3 = wasm_i8x16_splat(3);
  v128_t count = wasm_u8x16_min(load(src + 1, stride), c3);
  count = wasm_i8x16_add(count, wasm_u8x16_min(load(src + stride, stride), c3));
  for (int i = 0; i < 3; ++i) {
    count = wasm_i8x16_add(count,
                           wasm_u8x16_min(load(src + offsets[i], stride), c3));
  }
  // avgr(x, 0) = (x + 1) >> 1; count <= 15 so this cannot overflow.
  count = wasm_u8x16_avgr(count, wasm_i8x16_splat(0));
  return wasm_u8x16_min(count, wasm_i8x16_splat(4));
}

// Handles the height == 4 and height == 8 cases, where each 16-byte output
// vector spans `cols` (4 or 2) columns. `po[0..2]` are the position offsets
// for the first, second and remaining output vectors.
static inline void get_small_contexts(const uint8_t *levels, const int width,
                                      const int height,
                                      const ptrdiff_t *const offsets,
                                      const v128_t po[3], uint8_t *cc) {
  const int stride = height + TX_PAD_HOR;
  const int cols = 16 / height;
  const LoadLevelsFn load = height == 4 ? load_levels_4x4 : load_levels_8x2;
  assert(!(width % cols));
  for (int col = 0; col < width; col += cols) {
    const int i = col / cols;
    const v128_t count =
        get_coeff_contexts_kernel(levels, stride, offsets, load);
    wasm_v128_store(cc, wasm_i8x16_add(count, po[i < 2 ? i : 2]));
    levels += cols * stride;
    cc += 16;
  }
}

static inline void get_16n_coeff_contexts_2d(const uint8_t *levels,
                                             const int real_width,
                                             const int real_height,
                                             const int width, const int height,
                                             const ptrdiff_t *const offsets,
                                             uint8_t *cc) {
  const int stride = height + TX_PAD_HOR;
  v128_t pos_to_offset[5];
  v128_t pos_to_offset_large[3];

  assert(!(height % 16));

  pos_to_offset_large[2] = wasm_i8x16_splat(21);
  if (real_width == real_height) {
    pos_to_offset[0] = wasm_v128_load(c_16_po_2d_e[0]);
    pos_to_offset[1] = wasm_v128_load(c_16_po_2d_e[1]);
    pos_to_offset[2] = wasm_v128_load(c_16_po_2d_e[2]);
    pos_to_offset[3] = wasm_v128_load(c_16_po_2d_e[3]);
    pos_to_offset[4] = pos_to_offset_large[0] = pos_to_offset_large[1] =
        pos_to_offset_large[2];
  } else if (real_width < real_height) {
    pos_to_offset[0] = wasm_v128_load(c_16_po_2d_g[0]);
    pos_to_offset[1] = wasm_v128_load(c_16_po_2d_g[1]);
    pos_to_offset[2] = pos_to_offset[3] = pos_to_offset[4] =
        wasm_v128_load(c_16_po_2d_g[2]);
    pos_to_offset_large[0] = pos_to_offset_large[1] = pos_to_offset_large[2];
  } else {  // real_width > real_height
    pos_to_offset[0] = pos_to_offset[1] = wasm_v128_load(c_16_po_2d_l[0]);
    pos_to_offset[2] = wasm_v128_load(c_16_po_2d_l[1]);
    pos_to_offset[3] = wasm_v128_load(c_16_po_2d_l[2]);
    pos_to_offset[4] = pos_to_offset_large[2];
    pos_to_offset_large[0] = pos_to_offset_large[1] = wasm_i8x16_splat(16);
  }

  for (int col = 0; col < width; ++col) {
    v128_t po = pos_to_offset[0];
    for (int h = 0; h < height; h += 16) {
      const v128_t count =
          get_coeff_contexts_kernel(levels, stride, offsets, load_levels_16x1);
      wasm_v128_store(cc, wasm_i8x16_add(count, po));
      po = pos_to_offset_large[0];
      levels += 16;
      cc += 16;
    }
    pos_to_offset[0] = pos_to_offset[1];
    pos_to_offset[1] = pos_to_offset[2];
    pos_to_offset[2] = pos_to_offset[3];
    pos_to_offset[3] = pos_to_offset[4];
    pos_to_offset_large[0] = pos_to_offset_large[1];
    pos_to_offset_large[1] = pos_to_offset_large[2];
    levels += TX_PAD_HOR;
  }
}

// `first_row` is the position offset for the first 16 rows of each column and
// `col_po[min(col, 2)]` the offset for the remaining rows (TX_CLASS_VERT) or
// for the whole column (TX_CLASS_HORIZ, where first_row is unused).
static inline void get_16n_coeff_contexts_1d(
    const uint8_t *levels, const int width, const int height,
    const ptrdiff_t *const offsets, const int is_vert, const v128_t first_row,
    const v128_t col_po[3], uint8_t *cc) {
  const int stride = height + TX_PAD_HOR;
  assert(!(height % 16));
  for (int col = 0; col < width; ++col) {
    const v128_t rest = col_po[is_vert ? 2 : (col < 2 ? col : 2)];
    v128_t po = is_vert ? first_row : rest;
    for (int h = 0; h < height; h += 16) {
      const v128_t count =
          get_coeff_contexts_kernel(levels, stride, offsets, load_levels_16x1);
      wasm_v128_store(cc, wasm_i8x16_add(count, po));
      po = rest;
      levels += 16;
      cc += 16;
    }
    levels += TX_PAD_HOR;
  }
}

// Note: levels[] must be in the range [0, 127], inclusive.
void av1_get_nz_map_contexts_simd128(const uint8_t *const levels,
                                     const int16_t *const scan, const int eob,
                                     const TX_SIZE tx_size,
                                     const TX_CLASS tx_class,
                                     int8_t *const coeff_contexts) {
  const int last_idx = eob - 1;
  if (!last_idx) {
    coeff_contexts[0] = 0;
    return;
  }

  uint8_t *const cc = (uint8_t *)coeff_contexts;
  const int real_width = tx_size_wide[tx_size];
  const int real_height = tx_size_high[tx_size];
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  const int stride = height + TX_PAD_HOR;
  ptrdiff_t offsets[3];
  v128_t po[3];

  /* coeff_contexts must be 16 byte aligned. */
  assert(!((intptr_t)coeff_contexts & 0xf));

  if (tx_class == TX_CLASS_2D) {
    offsets[0] = 0 * stride + 2;
    offsets[1] = 1 * stride + 1;
    offsets[2] = 2 * stride + 0;
    if (height == 16 || height == 32) {
      get_16n_coeff_contexts_2d(levels, real_width, real_height, width, height,
                                offsets, cc);
    } else {
      if (height == 4) {
        po[0] = wasm_v128_load(c_4_po_2d[width == 4 ? 0 : 1]);
        po[1] = po[2] = wasm_i8x16_splat(21);
      } else {
        const uint8_t (*t)[16] =
            width == 8 ? c_8_po_2d_8 : (width < 8 ? c_8_po_2d_l : c_8_po_2d_g);
        po[0] = wasm_v128_load(t[0]);
        po[1] = wasm_v128_load(t[1]);
        po[2] = wasm_i8x16_splat(21);
      }
      get_small_contexts(levels, width, height, offsets, po, cc);
    }
    cc[0] = 0;
  } else if (tx_class == TX_CLASS_HORIZ) {
    offsets[0] = 2 * stride;
    offsets[1] = 3 * stride;
    offsets[2] = 4 * stride;
    if (height == 4 || height == 8) {
      po[0] = wasm_v128_load(height == 4 ? c_4_po_hor : c_8_po_hor);
      po[1] = po[2] = wasm_i8x16_splat(SIG_COEF_CONTEXTS_2D + 10);
      get_small_contexts(levels, width, height, offsets, po, cc);
    } else {
      po[0] = wasm_i8x16_splat(SIG_COEF_CONTEXTS_2D + 0);
      po[1] = wasm_i8x16_splat(SIG_COEF_CONTEXTS_2D + 5);
      po[2] = wasm_i8x16_splat(SIG_COEF_CONTEXTS_2D + 10);
      get_16n_coeff_contexts_1d(levels, width, height, offsets, 0, po[0], po,
                                cc);
    }
  } else {  // TX_CLASS_VERT
    offsets[0] = 2;
    offsets[1] = 3;
    offsets[2] = 4;
    if (height == 4 || height == 8) {
      po[0] = po[1] = po[2] =
          wasm_v128_load(height == 4 ? c_4_po_ver : c_8_po_ver);
      get_small_contexts(levels, width, height, offsets, po, cc);
    } else {
      po[0] = po[1] = po[2] = wasm_i8x16_splat(SIG_COEF_CONTEXTS_2D + 10);
      get_16n_coeff_contexts_1d(levels, width, height, offsets, 1,
                                wasm_v128_load(c_16_po_ver), po, cc);
    }
  }

  const int bhl = get_txb_bhl(tx_size);
  const int pos = scan[last_idx];
  if (last_idx <= (width << bhl) / 8)
    coeff_contexts[pos] = 1;
  else if (last_idx <= (width << bhl) / 4)
    coeff_contexts[pos] = 2;
  else
    coeff_contexts[pos] = 3;
}
