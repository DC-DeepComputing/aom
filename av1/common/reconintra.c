/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <math.h>
#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

// SSE2 is guaranteed by the 64-bit x86_64 ABI. Keeping these small neighbor
// copy/filter, DC, and block-fill helpers inline avoids per-block RTCD call
// overhead, while 32-bit x86 and non-x86 use portable C fallbacks and RTCD.
#if AOM_ARCH_X86_64 && HAVE_SSE2 && (defined(__SSE2__) || defined(_M_X64))
#define RECONINTRA_HAVE_SSE2 1
#include <emmintrin.h>
#else
#define RECONINTRA_HAVE_SSE2 0
#endif

#if defined(__GNUC__) || defined(__clang__)
#define RECONINTRA_NOINLINE __attribute__((noinline))
#else
#define RECONINTRA_NOINLINE
#endif

#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && \
    __GNUC__ >= 8
#define RECONINTRA_NO_UNROLL _Pragma("GCC unroll 1")
#else
#define RECONINTRA_NO_UNROLL
#endif

#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && \
    __GNUC__ >= 14
#define RECONINTRA_SCALAR_LOOP _Pragma("GCC novector") _Pragma("GCC unroll 1")
#else
#define RECONINTRA_SCALAR_LOOP RECONINTRA_NO_UNROLL
#endif

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/aom_once.h"
#include "aom_ports/mem.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/cfl.h"
#include "av1/common/reconintra.h"

enum {
  NEED_LEFT = 1 << 1,
  NEED_ABOVE = 1 << 2,
  NEED_ABOVERIGHT = 1 << 3,
  NEED_ABOVELEFT = 1 << 4,
  NEED_BOTTOMLEFT = 1 << 5,
};

#define INTRA_EDGE_FILT 3
#define INTRA_EDGE_TAPS 5
#define MAX_UPSAMPLE_SZ 16
#define NUM_INTRA_NEIGHBOUR_PIXELS (MAX_TX_SIZE * 2 + 32)

#if CONFIG_AV1_HIGHBITDEPTH
static const uint8_t extend_modes[INTRA_MODES] = {
  NEED_ABOVE | NEED_LEFT,                   // DC
  NEED_ABOVE,                               // V
  NEED_LEFT,                                // H
  NEED_ABOVE | NEED_ABOVERIGHT,             // D45
  NEED_LEFT | NEED_ABOVE | NEED_ABOVELEFT,  // D135
  NEED_LEFT | NEED_ABOVE | NEED_ABOVELEFT,  // D113
  NEED_LEFT | NEED_ABOVE | NEED_ABOVELEFT,  // D157
  NEED_LEFT | NEED_BOTTOMLEFT,              // D203
  NEED_ABOVE | NEED_ABOVERIGHT,             // D67
  NEED_LEFT | NEED_ABOVE,                   // SMOOTH
  NEED_LEFT | NEED_ABOVE,                   // SMOOTH_V
  NEED_LEFT | NEED_ABOVE,                   // SMOOTH_H
  NEED_LEFT | NEED_ABOVE | NEED_ABOVELEFT,  // PAETH
};
#endif  // CONFIG_AV1_HIGHBITDEPTH

// Tables to store if the top-right reference pixels are available. The flags
// are represented with bits, packed into 8-bit integers. E.g., for the 32x32
// blocks in a 128x128 superblock, the index of the "o" block is 10 (in raster
// order), so its flag is stored at the 3rd bit of the 2nd entry in the table,
// i.e. (table[10 / 8] >> (10 % 8)) & 1.
//       . . . .
//       . . . .
//       . . o .
//       . . . .
static uint8_t has_tr_4x4[128] = {
  255, 255, 255, 255, 85, 85, 85, 85, 119, 119, 119, 119, 85, 85, 85, 85,
  127, 127, 127, 127, 85, 85, 85, 85, 119, 119, 119, 119, 85, 85, 85, 85,
  255, 127, 255, 127, 85, 85, 85, 85, 119, 119, 119, 119, 85, 85, 85, 85,
  127, 127, 127, 127, 85, 85, 85, 85, 119, 119, 119, 119, 85, 85, 85, 85,
  255, 255, 255, 127, 85, 85, 85, 85, 119, 119, 119, 119, 85, 85, 85, 85,
  127, 127, 127, 127, 85, 85, 85, 85, 119, 119, 119, 119, 85, 85, 85, 85,
  255, 127, 255, 127, 85, 85, 85, 85, 119, 119, 119, 119, 85, 85, 85, 85,
  127, 127, 127, 127, 85, 85, 85, 85, 119, 119, 119, 119, 85, 85, 85, 85,
};
static uint8_t has_tr_4x8[64] = {
  255, 255, 255, 255, 119, 119, 119, 119, 127, 127, 127, 127, 119,
  119, 119, 119, 255, 127, 255, 127, 119, 119, 119, 119, 127, 127,
  127, 127, 119, 119, 119, 119, 255, 255, 255, 127, 119, 119, 119,
  119, 127, 127, 127, 127, 119, 119, 119, 119, 255, 127, 255, 127,
  119, 119, 119, 119, 127, 127, 127, 127, 119, 119, 119, 119,
};
static uint8_t has_tr_8x4[64] = {
  255, 255, 0, 0, 85, 85, 0, 0, 119, 119, 0, 0, 85, 85, 0, 0,
  127, 127, 0, 0, 85, 85, 0, 0, 119, 119, 0, 0, 85, 85, 0, 0,
  255, 127, 0, 0, 85, 85, 0, 0, 119, 119, 0, 0, 85, 85, 0, 0,
  127, 127, 0, 0, 85, 85, 0, 0, 119, 119, 0, 0, 85, 85, 0, 0,
};
static uint8_t has_tr_8x8[32] = {
  255, 255, 85, 85, 119, 119, 85, 85, 127, 127, 85, 85, 119, 119, 85, 85,
  255, 127, 85, 85, 119, 119, 85, 85, 127, 127, 85, 85, 119, 119, 85, 85,
};
static uint8_t has_tr_8x16[16] = {
  255, 255, 119, 119, 127, 127, 119, 119,
  255, 127, 119, 119, 127, 127, 119, 119,
};
static uint8_t has_tr_16x8[16] = {
  255, 0, 85, 0, 119, 0, 85, 0, 127, 0, 85, 0, 119, 0, 85, 0,
};
static uint8_t has_tr_16x16[8] = {
  255, 85, 119, 85, 127, 85, 119, 85,
};
static uint8_t has_tr_16x32[4] = { 255, 119, 127, 119 };
static uint8_t has_tr_32x16[4] = { 15, 5, 7, 5 };
static uint8_t has_tr_32x32[2] = { 95, 87 };
static uint8_t has_tr_32x64[1] = { 127 };
static uint8_t has_tr_64x32[1] = { 19 };
static uint8_t has_tr_64x64[1] = { 7 };
static uint8_t has_tr_64x128[1] = { 3 };
static uint8_t has_tr_128x64[1] = { 1 };
static uint8_t has_tr_128x128[1] = { 1 };
static uint8_t has_tr_4x16[32] = {
  255, 255, 255, 255, 127, 127, 127, 127, 255, 127, 255,
  127, 127, 127, 127, 127, 255, 255, 255, 127, 127, 127,
  127, 127, 255, 127, 255, 127, 127, 127, 127, 127,
};
static uint8_t has_tr_16x4[32] = {
  255, 0, 0, 0, 85, 0, 0, 0, 119, 0, 0, 0, 85, 0, 0, 0,
  127, 0, 0, 0, 85, 0, 0, 0, 119, 0, 0, 0, 85, 0, 0, 0,
};
static uint8_t has_tr_8x32[8] = {
  255, 255, 127, 127, 255, 127, 127, 127,
};
static uint8_t has_tr_32x8[8] = {
  15, 0, 5, 0, 7, 0, 5, 0,
};
static uint8_t has_tr_16x64[2] = { 255, 127 };
static uint8_t has_tr_64x16[2] = { 3, 1 };

static const uint8_t *const has_tr_tables[BLOCK_SIZES_ALL] = {
  // 4X4
  has_tr_4x4,
  // 4X8,       8X4,            8X8
  has_tr_4x8, has_tr_8x4, has_tr_8x8,
  // 8X16,      16X8,           16X16
  has_tr_8x16, has_tr_16x8, has_tr_16x16,
  // 16X32,     32X16,          32X32
  has_tr_16x32, has_tr_32x16, has_tr_32x32,
  // 32X64,     64X32,          64X64
  has_tr_32x64, has_tr_64x32, has_tr_64x64,
  // 64x128,    128x64,         128x128
  has_tr_64x128, has_tr_128x64, has_tr_128x128,
  // 4x16,      16x4,            8x32
  has_tr_4x16, has_tr_16x4, has_tr_8x32,
  // 32x8,      16x64,           64x16
  has_tr_32x8, has_tr_16x64, has_tr_64x16
};

static uint8_t has_tr_vert_8x8[32] = {
  255, 255, 0, 0, 119, 119, 0, 0, 127, 127, 0, 0, 119, 119, 0, 0,
  255, 127, 0, 0, 119, 119, 0, 0, 127, 127, 0, 0, 119, 119, 0, 0,
};
static uint8_t has_tr_vert_16x16[8] = {
  255, 0, 119, 0, 127, 0, 119, 0,
};
static uint8_t has_tr_vert_32x32[2] = { 15, 7 };
static uint8_t has_tr_vert_64x64[1] = { 3 };

// The _vert_* tables are like the ordinary tables above, but describe the
// order we visit square blocks when doing a PARTITION_VERT_A or
// PARTITION_VERT_B. This is the same order as normal except for on the last
// split where we go vertically (TL, BL, TR, BR). We treat the rectangular block
// as a pair of squares, which means that these tables work correctly for both
// mixed vertical partition types.
//
// There are tables for each of the square sizes. Vertical rectangles (like
// BLOCK_16X32) use their respective "non-vert" table
static const uint8_t *const has_tr_vert_tables[BLOCK_SIZES] = {
  // 4X4
  NULL,
  // 4X8,      8X4,         8X8
  has_tr_4x8, NULL, has_tr_vert_8x8,
  // 8X16,     16X8,        16X16
  has_tr_8x16, NULL, has_tr_vert_16x16,
  // 16X32,    32X16,       32X32
  has_tr_16x32, NULL, has_tr_vert_32x32,
  // 32X64,    64X32,       64X64
  has_tr_32x64, NULL, has_tr_vert_64x64,
  // 64x128,   128x64,      128x128
  has_tr_64x128, NULL, has_tr_128x128
};

static const uint8_t *get_has_tr_table(PARTITION_TYPE partition,
                                       BLOCK_SIZE bsize) {
  const uint8_t *ret = NULL;
  // If this is a mixed vertical partition, look up bsize in orders_vert.
  if (partition == PARTITION_VERT_A || partition == PARTITION_VERT_B) {
    assert(bsize < BLOCK_SIZES);
    ret = (bsize < BLOCK_SIZES) ? has_tr_vert_tables[bsize] : NULL;
  } else {
    ret = has_tr_tables[bsize];
  }
  assert(ret);
  return ret;
}

static int has_top_right(BLOCK_SIZE sb_size, BLOCK_SIZE bsize, int mi_row,
                         int mi_col, int top_available, int right_available,
                         PARTITION_TYPE partition, TX_SIZE txsz, int row_off,
                         int col_off, int ss_x, int ss_y) {
  if (!top_available || !right_available) return 0;

  const int bw_unit = mi_size_wide[bsize];
  const int plane_bw_unit = AOMMAX(bw_unit >> ss_x, 1);
  const int top_right_count_unit = tx_size_wide_unit[txsz];

  if (row_off > 0) {  // Just need to check if enough pixels on the right.
    if (UNLIKELY(block_size_wide[bsize] > block_size_wide[BLOCK_64X64])) {
      // Special case: For 128x128 blocks, the transform unit whose
      // top-right corner is at the center of the block does in fact have
      // pixels available at its top-right corner.
      const int plane_bw_unit_64 = mi_size_wide[BLOCK_64X64] >> ss_x;
      if (row_off == mi_size_high[BLOCK_64X64] >> ss_y &&
          col_off + top_right_count_unit == plane_bw_unit_64) {
        return 1;
      }
      const int col_off_64 = col_off & (plane_bw_unit_64 - 1);
      return col_off_64 + top_right_count_unit < plane_bw_unit_64;
    }
    return col_off + top_right_count_unit < plane_bw_unit;
  } else {
    // All top-right pixels are in the block above, which is already available.
    if (col_off + top_right_count_unit < plane_bw_unit) return 1;

    const int bw_in_mi_log2 = mi_size_wide_log2[bsize];
    const int bh_in_mi_log2 = mi_size_high_log2[bsize];
    const int sb_mi_size = mi_size_high[sb_size];
    const int blk_row_in_sb = (mi_row & (sb_mi_size - 1)) >> bh_in_mi_log2;
    if (blk_row_in_sb == 0) return 1;

    const int blk_col_in_sb = (mi_col & (sb_mi_size - 1)) >> bw_in_mi_log2;

    // Rightmost column of superblock (and not the top row): so top-right pixels
    // fall in the right superblock, which is not available yet.
    if (((blk_col_in_sb + 1) << bw_in_mi_log2) >= sb_mi_size) {
      return 0;
    }

    // General case (neither top row nor rightmost column): check if the
    // top-right block is coded before the current block.
    const uint32_t this_blk_index =
        (uint32_t)((blk_row_in_sb << (MAX_MIB_SIZE_LOG2 - bw_in_mi_log2)) +
                   blk_col_in_sb);
    const uint8_t *has_tr_table = get_has_tr_table(partition, bsize);
    return (has_tr_table[this_blk_index >> 3] >> (this_blk_index & 7u)) & 1;
  }
}

// Similar to the has_tr_* tables, but store if the bottom-left reference
// pixels are available.
static uint8_t has_bl_4x4[128] = {
  84, 85, 85, 85, 16, 17, 17, 17, 84, 85, 85, 85, 0,  1,  1,  1,  84, 85, 85,
  85, 16, 17, 17, 17, 84, 85, 85, 85, 0,  0,  1,  0,  84, 85, 85, 85, 16, 17,
  17, 17, 84, 85, 85, 85, 0,  1,  1,  1,  84, 85, 85, 85, 16, 17, 17, 17, 84,
  85, 85, 85, 0,  0,  0,  0,  84, 85, 85, 85, 16, 17, 17, 17, 84, 85, 85, 85,
  0,  1,  1,  1,  84, 85, 85, 85, 16, 17, 17, 17, 84, 85, 85, 85, 0,  0,  1,
  0,  84, 85, 85, 85, 16, 17, 17, 17, 84, 85, 85, 85, 0,  1,  1,  1,  84, 85,
  85, 85, 16, 17, 17, 17, 84, 85, 85, 85, 0,  0,  0,  0,
};
static uint8_t has_bl_4x8[64] = {
  16, 17, 17, 17, 0, 1, 1, 1, 16, 17, 17, 17, 0, 0, 1, 0,
  16, 17, 17, 17, 0, 1, 1, 1, 16, 17, 17, 17, 0, 0, 0, 0,
  16, 17, 17, 17, 0, 1, 1, 1, 16, 17, 17, 17, 0, 0, 1, 0,
  16, 17, 17, 17, 0, 1, 1, 1, 16, 17, 17, 17, 0, 0, 0, 0,
};
static uint8_t has_bl_8x4[64] = {
  254, 255, 84, 85, 254, 255, 16, 17, 254, 255, 84, 85, 254, 255, 0, 1,
  254, 255, 84, 85, 254, 255, 16, 17, 254, 255, 84, 85, 254, 255, 0, 0,
  254, 255, 84, 85, 254, 255, 16, 17, 254, 255, 84, 85, 254, 255, 0, 1,
  254, 255, 84, 85, 254, 255, 16, 17, 254, 255, 84, 85, 254, 255, 0, 0,
};
static uint8_t has_bl_8x8[32] = {
  84, 85, 16, 17, 84, 85, 0, 1, 84, 85, 16, 17, 84, 85, 0, 0,
  84, 85, 16, 17, 84, 85, 0, 1, 84, 85, 16, 17, 84, 85, 0, 0,
};
static uint8_t has_bl_8x16[16] = {
  16, 17, 0, 1, 16, 17, 0, 0, 16, 17, 0, 1, 16, 17, 0, 0,
};
static uint8_t has_bl_16x8[16] = {
  254, 84, 254, 16, 254, 84, 254, 0, 254, 84, 254, 16, 254, 84, 254, 0,
};
static uint8_t has_bl_16x16[8] = {
  84, 16, 84, 0, 84, 16, 84, 0,
};
static uint8_t has_bl_16x32[4] = { 16, 0, 16, 0 };
static uint8_t has_bl_32x16[4] = { 78, 14, 78, 14 };
static uint8_t has_bl_32x32[2] = { 4, 4 };
static uint8_t has_bl_32x64[1] = { 0 };
static uint8_t has_bl_64x32[1] = { 34 };
static uint8_t has_bl_64x64[1] = { 0 };
static uint8_t has_bl_64x128[1] = { 0 };
static uint8_t has_bl_128x64[1] = { 0 };
static uint8_t has_bl_128x128[1] = { 0 };
static uint8_t has_bl_4x16[32] = {
  0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0,
  0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0,
};
static uint8_t has_bl_16x4[32] = {
  254, 254, 254, 84, 254, 254, 254, 16, 254, 254, 254, 84, 254, 254, 254, 0,
  254, 254, 254, 84, 254, 254, 254, 16, 254, 254, 254, 84, 254, 254, 254, 0,
};
static uint8_t has_bl_8x32[8] = {
  0, 1, 0, 0, 0, 1, 0, 0,
};
static uint8_t has_bl_32x8[8] = {
  238, 78, 238, 14, 238, 78, 238, 14,
};
static uint8_t has_bl_16x64[2] = { 0, 0 };
static uint8_t has_bl_64x16[2] = { 42, 42 };

static const uint8_t *const has_bl_tables[BLOCK_SIZES_ALL] = {
  // 4X4
  has_bl_4x4,
  // 4X8,         8X4,         8X8
  has_bl_4x8, has_bl_8x4, has_bl_8x8,
  // 8X16,        16X8,        16X16
  has_bl_8x16, has_bl_16x8, has_bl_16x16,
  // 16X32,       32X16,       32X32
  has_bl_16x32, has_bl_32x16, has_bl_32x32,
  // 32X64,       64X32,       64X64
  has_bl_32x64, has_bl_64x32, has_bl_64x64,
  // 64x128,      128x64,      128x128
  has_bl_64x128, has_bl_128x64, has_bl_128x128,
  // 4x16,        16x4,        8x32
  has_bl_4x16, has_bl_16x4, has_bl_8x32,
  // 32x8,        16x64,       64x16
  has_bl_32x8, has_bl_16x64, has_bl_64x16
};

static uint8_t has_bl_vert_8x8[32] = {
  254, 255, 16, 17, 254, 255, 0, 1, 254, 255, 16, 17, 254, 255, 0, 0,
  254, 255, 16, 17, 254, 255, 0, 1, 254, 255, 16, 17, 254, 255, 0, 0,
};
static uint8_t has_bl_vert_16x16[8] = {
  254, 16, 254, 0, 254, 16, 254, 0,
};
static uint8_t has_bl_vert_32x32[2] = { 14, 14 };
static uint8_t has_bl_vert_64x64[1] = { 2 };

// The _vert_* tables are like the ordinary tables above, but describe the
// order we visit square blocks when doing a PARTITION_VERT_A or
// PARTITION_VERT_B. This is the same order as normal except for on the last
// split where we go vertically (TL, BL, TR, BR). We treat the rectangular block
// as a pair of squares, which means that these tables work correctly for both
// mixed vertical partition types.
//
// There are tables for each of the square sizes. Vertical rectangles (like
// BLOCK_16X32) use their respective "non-vert" table
static const uint8_t *const has_bl_vert_tables[BLOCK_SIZES] = {
  // 4X4
  NULL,
  // 4X8,     8X4,         8X8
  has_bl_4x8, NULL, has_bl_vert_8x8,
  // 8X16,    16X8,        16X16
  has_bl_8x16, NULL, has_bl_vert_16x16,
  // 16X32,   32X16,       32X32
  has_bl_16x32, NULL, has_bl_vert_32x32,
  // 32X64,   64X32,       64X64
  has_bl_32x64, NULL, has_bl_vert_64x64,
  // 64x128,  128x64,      128x128
  has_bl_64x128, NULL, has_bl_128x128
};

static const uint8_t *get_has_bl_table(PARTITION_TYPE partition,
                                       BLOCK_SIZE bsize) {
  const uint8_t *ret = NULL;
  // If this is a mixed vertical partition, look up bsize in orders_vert.
  if (partition == PARTITION_VERT_A || partition == PARTITION_VERT_B) {
    assert(bsize < BLOCK_SIZES);
    ret = (bsize < BLOCK_SIZES) ? has_bl_vert_tables[bsize] : NULL;
  } else {
    ret = has_bl_tables[bsize];
  }
  assert(ret);
  return ret;
}

static int has_bottom_left(BLOCK_SIZE sb_size, BLOCK_SIZE bsize, int mi_row,
                           int mi_col, int bottom_available, int left_available,
                           PARTITION_TYPE partition, TX_SIZE txsz, int row_off,
                           int col_off, int ss_x, int ss_y) {
  if (!bottom_available || !left_available) return 0;

  if (col_off > 0) {
    // Special case for 128x* blocks, when col_off is half the block width.
    // This is needed because 128x* superblocks are divided into 64x* blocks in
    // raster order
    if (UNLIKELY(block_size_wide[bsize] > block_size_wide[BLOCK_64X64])) {
      const int plane_bw_unit_64 = mi_size_wide[BLOCK_64X64] >> ss_x;
      if ((col_off & (plane_bw_unit_64 - 1)) == 0) {
        const int plane_bh_unit_64 = mi_size_high[BLOCK_64X64] >> ss_y;
        const int row_off_64 = row_off & (plane_bh_unit_64 - 1);
        const int plane_bh_unit =
            AOMMIN(mi_size_high[bsize] >> ss_y, plane_bh_unit_64);
        return row_off_64 + tx_size_high_unit[txsz] < plane_bh_unit;
      }
    }
    // Bottom-left pixels are in the bottom-left block, which is not available.
    return 0;
  } else {
    const int bh_unit = mi_size_high[bsize];
    const int plane_bh_unit = AOMMAX(bh_unit >> ss_y, 1);
    const int bottom_left_count_unit = tx_size_high_unit[txsz];

    // All bottom-left pixels are in the left block, which is already available.
    if (row_off + bottom_left_count_unit < plane_bh_unit) return 1;

    const int bw_in_mi_log2 = mi_size_wide_log2[bsize];
    const int bh_in_mi_log2 = mi_size_high_log2[bsize];
    const int sb_mi_size = mi_size_high[sb_size];
    const int blk_row_in_sb = (mi_row & (sb_mi_size - 1)) >> bh_in_mi_log2;
    const int blk_col_in_sb = (mi_col & (sb_mi_size - 1)) >> bw_in_mi_log2;

    // Leftmost column of superblock: so bottom-left pixels maybe in the left
    // and/or bottom-left superblocks. But only the left superblock is
    // available, so check if all required pixels fall in that superblock.
    if (blk_col_in_sb == 0) {
      const int blk_start_row_off = (blk_row_in_sb << bh_in_mi_log2) >> ss_y;
      const int row_off_in_sb = blk_start_row_off + row_off;
      const int sb_height_unit = sb_mi_size >> ss_y;
      return row_off_in_sb + bottom_left_count_unit < sb_height_unit;
    }

    // Bottom row of superblock (and not the leftmost column): so bottom-left
    // pixels fall in the bottom superblock, which is not available yet.
    if (((blk_row_in_sb + 1) << bh_in_mi_log2) >= sb_mi_size) return 0;

    // General case (neither leftmost column nor bottom row): check if the
    // bottom-left block is coded before the current block.
    const uint32_t this_blk_index =
        (uint32_t)((blk_row_in_sb << (MAX_MIB_SIZE_LOG2 - bw_in_mi_log2)) +
                   blk_col_in_sb);
    const uint8_t *has_bl_table = get_has_bl_table(partition, bsize);
    return (has_bl_table[this_blk_index >> 3] >> (this_blk_index & 7u)) & 1;
  }
}

typedef void (*intra_pred_fn)(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left);

static intra_pred_fn pred[INTRA_MODES][TX_SIZES_ALL];
static intra_pred_fn dc_pred[2][2][TX_SIZES_ALL];

#if CONFIG_AV1_HIGHBITDEPTH
typedef void (*intra_high_pred_fn)(uint16_t *dst, ptrdiff_t stride,
                                   const uint16_t *above, const uint16_t *left,
                                   int bd);
static intra_high_pred_fn pred_high[INTRA_MODES][TX_SIZES_ALL];
static intra_high_pred_fn dc_pred_high[2][2][TX_SIZES_ALL];
#endif

static void init_intra_predictors_internal(void) {
  assert(NELEMENTS(mode_to_angle_map) == INTRA_MODES);

#if CONFIG_REALTIME_ONLY && !CONFIG_AV1_DECODER
#define INIT_RECTANGULAR(p, type)             \
  p[TX_4X8] = aom_##type##_predictor_4x8;     \
  p[TX_8X4] = aom_##type##_predictor_8x4;     \
  p[TX_8X16] = aom_##type##_predictor_8x16;   \
  p[TX_16X8] = aom_##type##_predictor_16x8;   \
  p[TX_16X32] = aom_##type##_predictor_16x32; \
  p[TX_32X16] = aom_##type##_predictor_32x16; \
  p[TX_32X64] = aom_##type##_predictor_32x64; \
  p[TX_64X32] = aom_##type##_predictor_64x32;
#else
#define INIT_RECTANGULAR(p, type)             \
  p[TX_4X8] = aom_##type##_predictor_4x8;     \
  p[TX_8X4] = aom_##type##_predictor_8x4;     \
  p[TX_8X16] = aom_##type##_predictor_8x16;   \
  p[TX_16X8] = aom_##type##_predictor_16x8;   \
  p[TX_16X32] = aom_##type##_predictor_16x32; \
  p[TX_32X16] = aom_##type##_predictor_32x16; \
  p[TX_32X64] = aom_##type##_predictor_32x64; \
  p[TX_64X32] = aom_##type##_predictor_64x32; \
  p[TX_4X16] = aom_##type##_predictor_4x16;   \
  p[TX_16X4] = aom_##type##_predictor_16x4;   \
  p[TX_8X32] = aom_##type##_predictor_8x32;   \
  p[TX_32X8] = aom_##type##_predictor_32x8;   \
  p[TX_16X64] = aom_##type##_predictor_16x64; \
  p[TX_64X16] = aom_##type##_predictor_64x16;
#endif  // CONFIG_REALTIME_ONLY && !CONFIG_AV1_DECODER

#define INIT_NO_4X4(p, type)                  \
  p[TX_8X8] = aom_##type##_predictor_8x8;     \
  p[TX_16X16] = aom_##type##_predictor_16x16; \
  p[TX_32X32] = aom_##type##_predictor_32x32; \
  p[TX_64X64] = aom_##type##_predictor_64x64; \
  INIT_RECTANGULAR(p, type)

#define INIT_ALL_SIZES(p, type)           \
  p[TX_4X4] = aom_##type##_predictor_4x4; \
  INIT_NO_4X4(p, type)

  INIT_ALL_SIZES(pred[V_PRED], v)
  INIT_ALL_SIZES(pred[H_PRED], h)
  INIT_ALL_SIZES(pred[PAETH_PRED], paeth)
  INIT_ALL_SIZES(pred[SMOOTH_PRED], smooth)
  INIT_ALL_SIZES(pred[SMOOTH_V_PRED], smooth_v)
  INIT_ALL_SIZES(pred[SMOOTH_H_PRED], smooth_h)
  INIT_ALL_SIZES(dc_pred[0][0], dc_128)
  INIT_ALL_SIZES(dc_pred[0][1], dc_top)
  INIT_ALL_SIZES(dc_pred[1][0], dc_left)
  INIT_ALL_SIZES(dc_pred[1][1], dc)
#if CONFIG_AV1_HIGHBITDEPTH
  INIT_ALL_SIZES(pred_high[V_PRED], highbd_v)
  INIT_ALL_SIZES(pred_high[H_PRED], highbd_h)
  INIT_ALL_SIZES(pred_high[PAETH_PRED], highbd_paeth)
  INIT_ALL_SIZES(pred_high[SMOOTH_PRED], highbd_smooth)
  INIT_ALL_SIZES(pred_high[SMOOTH_V_PRED], highbd_smooth_v)
  INIT_ALL_SIZES(pred_high[SMOOTH_H_PRED], highbd_smooth_h)
  INIT_ALL_SIZES(dc_pred_high[0][0], highbd_dc_128)
  INIT_ALL_SIZES(dc_pred_high[0][1], highbd_dc_top)
  INIT_ALL_SIZES(dc_pred_high[1][0], highbd_dc_left)
  INIT_ALL_SIZES(dc_pred_high[1][1], highbd_dc)
#endif
#undef intra_pred_allsizes
}

// Directional prediction, zone 1: 0 < angle < 90
void av1_dr_prediction_z1_c(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                            const uint8_t *above, const uint8_t *left,
                            int upsample_above, int dx, int dy) {
  int r, c, x, base, shift, val;

  (void)left;
  (void)dy;
  assert(dy == 1);
  assert(dx > 0);

  const int max_base_x = ((bw + bh) - 1) << upsample_above;
  const int frac_bits = 6 - upsample_above;
  const int base_inc = 1 << upsample_above;
  x = dx;
  for (r = 0; r < bh; ++r, dst += stride, x += dx) {
    base = x >> frac_bits;
    shift = ((x << upsample_above) & 0x3F) >> 1;

    if (base >= max_base_x) {
      for (int i = r; i < bh; ++i) {
        memset(dst, above[max_base_x], bw * sizeof(dst[0]));
        dst += stride;
      }
      return;
    }

    for (c = 0; c < bw; ++c, base += base_inc) {
      if (base < max_base_x) {
        val = above[base] * (32 - shift) + above[base + 1] * shift;
        dst[c] = ROUND_POWER_OF_TWO(val, 5);
      } else {
        dst[c] = above[max_base_x];
      }
    }
  }
}

// Directional prediction, zone 2: 90 < angle < 180
void av1_dr_prediction_z2_c(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                            const uint8_t *above, const uint8_t *left,
                            int upsample_above, int upsample_left, int dx,
                            int dy) {
  assert(dx > 0);
  assert(dy > 0);

  const int min_base_x = -(1 << upsample_above);
  const int min_base_y = -(1 << upsample_left);
  (void)min_base_y;
  const int frac_bits_x = 6 - upsample_above;
  const int frac_bits_y = 6 - upsample_left;

  for (int r = 0; r < bh; ++r) {
    for (int c = 0; c < bw; ++c) {
      int val;
      int y = r + 1;
      int x = (c << 6) - y * dx;
      const int base_x = x >> frac_bits_x;
      if (base_x >= min_base_x) {
        const int shift = ((x * (1 << upsample_above)) & 0x3F) >> 1;
        val = above[base_x] * (32 - shift) + above[base_x + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 5);
      } else {
        x = c + 1;
        y = (r << 6) - x * dy;
        const int base_y = y >> frac_bits_y;
        assert(base_y >= min_base_y);
        const int shift = ((y * (1 << upsample_left)) & 0x3F) >> 1;
        val = left[base_y] * (32 - shift) + left[base_y + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 5);
      }
      dst[c] = val;
    }
    dst += stride;
  }
}

// Directional prediction, zone 3: 180 < angle < 270
void av1_dr_prediction_z3_c(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                            const uint8_t *above, const uint8_t *left,
                            int upsample_left, int dx, int dy) {
  int r, c, y, base, shift, val;

  (void)above;
  (void)dx;

  assert(dx == 1);
  assert(dy > 0);

  const int max_base_y = (bw + bh - 1) << upsample_left;
  const int frac_bits = 6 - upsample_left;
  const int base_inc = 1 << upsample_left;
  y = dy;
  for (c = 0; c < bw; ++c, y += dy) {
    base = y >> frac_bits;
    shift = ((y << upsample_left) & 0x3F) >> 1;

    for (r = 0; r < bh; ++r, base += base_inc) {
      if (base < max_base_y) {
        val = left[base] * (32 - shift) + left[base + 1] * shift;
        dst[r * stride + c] = ROUND_POWER_OF_TWO(val, 5);
      } else {
        for (; r < bh; ++r) dst[r * stride + c] = left[max_base_y];
        break;
      }
    }
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
// Directional prediction, zone 1: 0 < angle < 90
void av1_highbd_dr_prediction_z1_c(uint16_t *dst, ptrdiff_t stride, int bw,
                                   int bh, const uint16_t *above,
                                   const uint16_t *left, int upsample_above,
                                   int dx, int dy, int bd) {
  int r, c, x, base, shift, val;

  (void)left;
  (void)dy;
  (void)bd;
  assert(dy == 1);
  assert(dx > 0);

  const int max_base_x = ((bw + bh) - 1) << upsample_above;
  const int frac_bits = 6 - upsample_above;
  const int base_inc = 1 << upsample_above;
  x = dx;
  for (r = 0; r < bh; ++r, dst += stride, x += dx) {
    base = x >> frac_bits;
    shift = ((x << upsample_above) & 0x3F) >> 1;

    if (base >= max_base_x) {
      for (int i = r; i < bh; ++i) {
        aom_memset16(dst, above[max_base_x], bw);
        dst += stride;
      }
      return;
    }

    for (c = 0; c < bw; ++c, base += base_inc) {
      if (base < max_base_x) {
        val = above[base] * (32 - shift) + above[base + 1] * shift;
        dst[c] = ROUND_POWER_OF_TWO(val, 5);
      } else {
        dst[c] = above[max_base_x];
      }
    }
  }
}

// Directional prediction, zone 2: 90 < angle < 180
void av1_highbd_dr_prediction_z2_c(uint16_t *dst, ptrdiff_t stride, int bw,
                                   int bh, const uint16_t *above,
                                   const uint16_t *left, int upsample_above,
                                   int upsample_left, int dx, int dy, int bd) {
  (void)bd;
  assert(dx > 0);
  assert(dy > 0);

  const int min_base_x = -(1 << upsample_above);
  const int min_base_y = -(1 << upsample_left);
  (void)min_base_y;
  const int frac_bits_x = 6 - upsample_above;
  const int frac_bits_y = 6 - upsample_left;

  for (int r = 0; r < bh; ++r) {
    for (int c = 0; c < bw; ++c) {
      int val;
      int y = r + 1;
      int x = (c << 6) - y * dx;
      const int base_x = x >> frac_bits_x;
      if (base_x >= min_base_x) {
        const int shift = ((x * (1 << upsample_above)) & 0x3F) >> 1;
        val = above[base_x] * (32 - shift) + above[base_x + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 5);
      } else {
        x = c + 1;
        y = (r << 6) - x * dy;
        const int base_y = y >> frac_bits_y;
        assert(base_y >= min_base_y);
        const int shift = ((y * (1 << upsample_left)) & 0x3F) >> 1;
        val = left[base_y] * (32 - shift) + left[base_y + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 5);
      }
      dst[c] = val;
    }
    dst += stride;
  }
}

// Directional prediction, zone 3: 180 < angle < 270
void av1_highbd_dr_prediction_z3_c(uint16_t *dst, ptrdiff_t stride, int bw,
                                   int bh, const uint16_t *above,
                                   const uint16_t *left, int upsample_left,
                                   int dx, int dy, int bd) {
  int r, c, y, base, shift, val;

  (void)above;
  (void)dx;
  (void)bd;
  assert(dx == 1);
  assert(dy > 0);

  const int max_base_y = (bw + bh - 1) << upsample_left;
  const int frac_bits = 6 - upsample_left;
  const int base_inc = 1 << upsample_left;
  y = dy;
  for (c = 0; c < bw; ++c, y += dy) {
    base = y >> frac_bits;
    shift = ((y << upsample_left) & 0x3F) >> 1;

    for (r = 0; r < bh; ++r, base += base_inc) {
      if (base < max_base_y) {
        val = left[base] * (32 - shift) + left[base + 1] * shift;
        dst[r * stride + c] = ROUND_POWER_OF_TWO(val, 5);
      } else {
        for (; r < bh; ++r) dst[r * stride + c] = left[max_base_y];
        break;
      }
    }
  }
}

static AOM_FORCE_INLINE void highbd_dr_predictor(
    uint16_t *dst, ptrdiff_t stride, TX_SIZE tx_size, const uint16_t *above,
    const uint16_t *left, int upsample_above, int upsample_left, int angle,
    int bd) {
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];
  assert(angle > 0 && angle < 270);

  if (angle < 90) {
    av1_highbd_dr_prediction_z1(dst, stride, bw, bh, above, left,
                                upsample_above, dr_intra_derivative[angle], 1,
                                bd);
  } else if (angle < 180) {
    assert(angle > 90);
    av1_highbd_dr_prediction_z2(
        dst, stride, bw, bh, above, left, upsample_above, upsample_left,
        dr_intra_derivative[180 - angle], dr_intra_derivative[angle - 90], bd);
  } else {
    assert(angle > 180 && angle < 270);
    av1_highbd_dr_prediction_z3(dst, stride, bw, bh, above, left, upsample_left,
                                1, dr_intra_derivative[270 - angle], bd);
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

DECLARE_ALIGNED(16, const int8_t,
                av1_filter_intra_taps[FILTER_INTRA_MODES][8][8]) = {
  {
      { -6, 10, 0, 0, 0, 12, 0, 0 },
      { -5, 2, 10, 0, 0, 9, 0, 0 },
      { -3, 1, 1, 10, 0, 7, 0, 0 },
      { -3, 1, 1, 2, 10, 5, 0, 0 },
      { -4, 6, 0, 0, 0, 2, 12, 0 },
      { -3, 2, 6, 0, 0, 2, 9, 0 },
      { -3, 2, 2, 6, 0, 2, 7, 0 },
      { -3, 1, 2, 2, 6, 3, 5, 0 },
  },
  {
      { -10, 16, 0, 0, 0, 10, 0, 0 },
      { -6, 0, 16, 0, 0, 6, 0, 0 },
      { -4, 0, 0, 16, 0, 4, 0, 0 },
      { -2, 0, 0, 0, 16, 2, 0, 0 },
      { -10, 16, 0, 0, 0, 0, 10, 0 },
      { -6, 0, 16, 0, 0, 0, 6, 0 },
      { -4, 0, 0, 16, 0, 0, 4, 0 },
      { -2, 0, 0, 0, 16, 0, 2, 0 },
  },
  {
      { -8, 8, 0, 0, 0, 16, 0, 0 },
      { -8, 0, 8, 0, 0, 16, 0, 0 },
      { -8, 0, 0, 8, 0, 16, 0, 0 },
      { -8, 0, 0, 0, 8, 16, 0, 0 },
      { -4, 4, 0, 0, 0, 0, 16, 0 },
      { -4, 0, 4, 0, 0, 0, 16, 0 },
      { -4, 0, 0, 4, 0, 0, 16, 0 },
      { -4, 0, 0, 0, 4, 0, 16, 0 },
  },
  {
      { -2, 8, 0, 0, 0, 10, 0, 0 },
      { -1, 3, 8, 0, 0, 6, 0, 0 },
      { -1, 2, 3, 8, 0, 4, 0, 0 },
      { 0, 1, 2, 3, 8, 2, 0, 0 },
      { -1, 4, 0, 0, 0, 3, 10, 0 },
      { -1, 3, 4, 0, 0, 4, 6, 0 },
      { -1, 2, 3, 4, 0, 4, 4, 0 },
      { -1, 2, 2, 3, 4, 3, 3, 0 },
  },
  {
      { -12, 14, 0, 0, 0, 14, 0, 0 },
      { -10, 0, 14, 0, 0, 12, 0, 0 },
      { -9, 0, 0, 14, 0, 11, 0, 0 },
      { -8, 0, 0, 0, 14, 10, 0, 0 },
      { -10, 12, 0, 0, 0, 0, 14, 0 },
      { -9, 1, 12, 0, 0, 0, 12, 0 },
      { -8, 0, 0, 12, 0, 1, 11, 0 },
      { -7, 0, 0, 1, 12, 1, 9, 0 },
  },
};

void av1_filter_intra_predictor_c(uint8_t *dst, ptrdiff_t stride,
                                  TX_SIZE tx_size, const uint8_t *above,
                                  const uint8_t *left, int mode) {
  int r, c;
  uint8_t buffer[33][33];
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];

  assert(bw <= 32 && bh <= 32);

  for (r = 0; r < bh; ++r) buffer[r + 1][0] = left[r];
  memcpy(buffer[0], &above[-1], (bw + 1) * sizeof(uint8_t));

  for (r = 1; r < bh + 1; r += 2)
    for (c = 1; c < bw + 1; c += 4) {
      const uint8_t p0 = buffer[r - 1][c - 1];
      const uint8_t p1 = buffer[r - 1][c];
      const uint8_t p2 = buffer[r - 1][c + 1];
      const uint8_t p3 = buffer[r - 1][c + 2];
      const uint8_t p4 = buffer[r - 1][c + 3];
      const uint8_t p5 = buffer[r][c - 1];
      const uint8_t p6 = buffer[r + 1][c - 1];
      for (int k = 0; k < 8; ++k) {
        int r_offset = k >> 2;
        int c_offset = k & 0x03;
        int pr = av1_filter_intra_taps[mode][k][0] * p0 +
                 av1_filter_intra_taps[mode][k][1] * p1 +
                 av1_filter_intra_taps[mode][k][2] * p2 +
                 av1_filter_intra_taps[mode][k][3] * p3 +
                 av1_filter_intra_taps[mode][k][4] * p4 +
                 av1_filter_intra_taps[mode][k][5] * p5 +
                 av1_filter_intra_taps[mode][k][6] * p6;
        // Section 7.11.2.3 specifies the right-hand side of the assignment as
        //   Clip1( Round2Signed( pr, INTRA_FILTER_SCALE_BITS ) ).
        // Since Clip1() clips a negative value to 0, it is safe to replace
        // Round2Signed() with Round2().
        buffer[r + r_offset][c + c_offset] =
            clip_pixel(ROUND_POWER_OF_TWO(pr, FILTER_INTRA_SCALE_BITS));
      }
    }

  for (r = 0; r < bh; ++r) {
    memcpy(dst, &buffer[r + 1][1], bw * sizeof(uint8_t));
    dst += stride;
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
static void highbd_filter_intra_predictor(uint16_t *dst, ptrdiff_t stride,
                                          TX_SIZE tx_size,
                                          const uint16_t *above,
                                          const uint16_t *left, int mode,
                                          int bd) {
  int r, c;
  uint16_t buffer[33][33];
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];

  assert(bw <= 32 && bh <= 32);

  for (r = 0; r < bh; ++r) buffer[r + 1][0] = left[r];
  memcpy(buffer[0], &above[-1], (bw + 1) * sizeof(buffer[0][0]));

  for (r = 1; r < bh + 1; r += 2)
    for (c = 1; c < bw + 1; c += 4) {
      const uint16_t p0 = buffer[r - 1][c - 1];
      const uint16_t p1 = buffer[r - 1][c];
      const uint16_t p2 = buffer[r - 1][c + 1];
      const uint16_t p3 = buffer[r - 1][c + 2];
      const uint16_t p4 = buffer[r - 1][c + 3];
      const uint16_t p5 = buffer[r][c - 1];
      const uint16_t p6 = buffer[r + 1][c - 1];
      for (int k = 0; k < 8; ++k) {
        int r_offset = k >> 2;
        int c_offset = k & 0x03;
        int pr = av1_filter_intra_taps[mode][k][0] * p0 +
                 av1_filter_intra_taps[mode][k][1] * p1 +
                 av1_filter_intra_taps[mode][k][2] * p2 +
                 av1_filter_intra_taps[mode][k][3] * p3 +
                 av1_filter_intra_taps[mode][k][4] * p4 +
                 av1_filter_intra_taps[mode][k][5] * p5 +
                 av1_filter_intra_taps[mode][k][6] * p6;
        // Section 7.11.2.3 specifies the right-hand side of the assignment as
        //   Clip1( Round2Signed( pr, INTRA_FILTER_SCALE_BITS ) ).
        // Since Clip1() clips a negative value to 0, it is safe to replace
        // Round2Signed() with Round2().
        buffer[r + r_offset][c + c_offset] = clip_pixel_highbd(
            ROUND_POWER_OF_TWO(pr, FILTER_INTRA_SCALE_BITS), bd);
      }
    }

  for (r = 0; r < bh; ++r) {
    memcpy(dst, &buffer[r + 1][1], bw * sizeof(dst[0]));
    dst += stride;
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

static AOM_FORCE_INLINE int is_smooth_luma(const MB_MODE_INFO *mbmi) {
  return (uint32_t)(mbmi->mode - SMOOTH_PRED) <= 2u;
}

static AOM_FORCE_INLINE int is_smooth_chroma(const MB_MODE_INFO *mbmi) {
  return !is_inter_block(mbmi) &&
         (uint32_t)(mbmi->uv_mode - UV_SMOOTH_PRED) <= 2u;
}

static AOM_FORCE_INLINE int get_intra_edge_filter_type(const MACROBLOCKD *xd,
                                                       int plane) {
  if (plane == 0) {
    const MB_MODE_INFO *const above = xd->above_mbmi;
    const MB_MODE_INFO *const left = xd->left_mbmi;
    return (above && is_smooth_luma(above)) || (left && is_smooth_luma(left));
  } else {
    const MB_MODE_INFO *const above = xd->chroma_above_mbmi;
    const MB_MODE_INFO *const left = xd->chroma_left_mbmi;
    return (above && is_smooth_chroma(above)) ||
           (left && is_smooth_chroma(left));
  }
}

// Packed intra edge-filter/upsample lookup tables indexed by block perimeter
// bucket: bits [15:0] = (0, 8], [31:16] = (8, 16], [47:32] = (16, 24], and
// [63:48] = (24, 32]. Within each 16-bit entry, bits [4:0], [9:5], [14:10]
// store angle-delta thresholds t0, t1, t2 divided by 4 (31 stands for 124 > 90,
// i.e. unreachable for valid |delta| < 90), and bit 15 stores the upsample
// eligibility flag. Returning 0u for blk_wh > 32 encodes (0, 0, 0, 0), giving
// strength 3 and upsample 0.
#define EDGE_FILT_CFG(t0, t1, t2, up)                                \
  ((uint64_t)(t0) | ((uint64_t)(t1) << 5) | ((uint64_t)(t2) << 10) | \
   ((uint64_t)(up) << 15))
#define EDGE_FILT_LUT0                                                   \
  (EDGE_FILT_CFG(14, 31, 31, 1) | (EDGE_FILT_CFG(10, 31, 31, 1) << 16) | \
   (EDGE_FILT_CFG(2, 4, 8, 0) << 32) | (EDGE_FILT_CFG(0, 1, 8, 0) << 48))
#define EDGE_FILT_LUT1                                                  \
  (EDGE_FILT_CFG(10, 16, 31, 1) | (EDGE_FILT_CFG(5, 12, 31, 0) << 16) | \
   (EDGE_FILT_CFG(1, 1, 1, 0) << 32) | (EDGE_FILT_CFG(0, 0, 0, 0) << 48))

static AOM_FORCE_INLINE uint32_t intra_edge_filter_cfg_lbd(int blk_wh,
                                                           int type) {
  assert(blk_wh > 0);
  if (blk_wh > 32) return 0u;
  const uint64_t lut = type ? EDGE_FILT_LUT1 : EDGE_FILT_LUT0;
  const unsigned int sh = ((unsigned int)(blk_wh - 1) & 24u) << 1;
  return (uint32_t)((lut >> sh) & 0xffffu);
}

static AOM_FORCE_INLINE int eval_edge_filt_s_lbd(uint32_t cfg, int d) {
  assert(d >= 0 && d < 124);
  const int dq = d >> 2;
  return (dq >= (int)(cfg & 31u)) + (dq >= (int)((cfg >> 5) & 31u)) +
         (dq >= (int)((cfg >> 10) & 31u));
}

#if CONFIG_AV1_HIGHBITDEPTH
static AOM_FORCE_INLINE int intra_edge_filter_strength_pos(int blk_wh, int d,
                                                           int type) {
  return eval_edge_filt_s_lbd(intra_edge_filter_cfg_lbd(blk_wh, type), d);
}

static AOM_FORCE_INLINE int intra_edge_filter_strength(int bs0, int bs1,
                                                       int delta, int type) {
  const int d = abs(delta);
  if (d == 0) return 0;
  return intra_edge_filter_strength_pos(bs0 + bs1, d, type);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

void av1_filter_intra_edge_c(uint8_t *p, int sz, int strength) {
  if (!strength) return;

  const int kernel[INTRA_EDGE_FILT][INTRA_EDGE_TAPS] = { { 0, 4, 8, 4, 0 },
                                                         { 0, 5, 6, 5, 0 },
                                                         { 2, 4, 4, 4, 2 } };
  const int filt = strength - 1;
  uint8_t edge[129];

  memcpy(edge, p, sz * sizeof(*p));
  for (int i = 1; i < sz; i++) {
    int s = 0;
    for (int j = 0; j < INTRA_EDGE_TAPS; j++) {
      int k = i - 2 + j;
      k = (k < 0) ? 0 : k;
      k = (k > sz - 1) ? sz - 1 : k;
      s += edge[k] * kernel[filt][j];
    }
    s = (s + 8) >> 4;
    p[i] = s;
  }
}

void av1_upsample_intra_edge_c(uint8_t *p, int sz) {
  // interpolate half-sample positions
  assert(sz <= MAX_UPSAMPLE_SZ);

  uint8_t in[MAX_UPSAMPLE_SZ + 3];
  // copy p[-1..(sz-1)] and extend first and last samples
  in[0] = p[-1];
  in[1] = p[-1];
  for (int i = 0; i < sz; i++) {
    in[i + 2] = p[i];
  }
  in[sz + 2] = p[sz - 1];

  // interpolate half-sample edge positions
  p[-2] = in[0];
  for (int i = 0; i < sz; i++) {
    int s = -in[i] + (9 * in[i + 1]) + (9 * in[i + 2]) - in[i + 3];
    s = clip_pixel((s + 8) >> 4);
    p[2 * i - 1] = s;
    p[2 * i] = in[i + 2];
  }
}

// Scratch-buffer neighbor copy/fill helpers: under RECONINTRA_HAVE_SSE2, 4-,
// 8-, and 12-byte copies/remainders and 16-byte pad chunks issue full 16-byte
// unaligned stores, so destination buffers must have at least 16 writable bytes
// of tail room per chunk (provided by above_data, left_data, z2_buf,
// edge_data).
RECONINTRA_NOINLINE static void copy_above_row_tail_lbd(
    uint8_t *above_row, const uint8_t *above_ref, int n_copy) {
  int i = 0;
#if RECONINTRA_HAVE_SSE2
  RECONINTRA_NO_UNROLL
  for (; i + 16 <= n_copy; i += 16) {
    _mm_storeu_si128((__m128i *)(above_row + i),
                     _mm_loadu_si128((const __m128i *)(above_ref + i)));
  }
  const int rem = n_copy - i;
  if (rem == 8) {
    _mm_storeu_si128((__m128i *)(above_row + i),
                     _mm_loadl_epi64((const __m128i *)(above_ref + i)));
    return;
  }
  if (rem == 4) {
    uint32_t u0;
    memcpy(&u0, above_ref + i, 4);
    _mm_storeu_si128((__m128i *)(above_row + i), _mm_cvtsi32_si128((int)u0));
    return;
  }
#endif
  if (n_copy - i >= 8) {
    memcpy(above_row + i, above_ref + i, 8);
    i += 8;
  }
  if (n_copy - i >= 4) {
    memcpy(above_row + i, above_ref + i, 4);
    i += 4;
  }
  RECONINTRA_SCALAR_LOOP
  for (; i < n_copy; ++i) above_row[i] = above_ref[i];
}

// Destination above_row must live in an edge scratch buffer with >= 16 writable
// bytes past each 16-byte chunk start when RECONINTRA_HAVE_SSE2 is enabled.
static AOM_FORCE_INLINE void copy_above_row_lbd(uint8_t *above_row,
                                                const uint8_t *above_ref,
                                                int n_copy) {
#if RECONINTRA_HAVE_SSE2
  if (n_copy == 4) {
    uint32_t u0;
    memcpy(&u0, above_ref, 4);
    _mm_storeu_si128((__m128i *)above_row, _mm_cvtsi32_si128((int)u0));
  } else if (n_copy == 8) {
    _mm_storeu_si128((__m128i *)above_row,
                     _mm_loadl_epi64((const __m128i *)above_ref));
  } else if (n_copy == 16) {
    _mm_storeu_si128((__m128i *)above_row,
                     _mm_loadu_si128((const __m128i *)above_ref));
  } else if (n_copy == 32) {
    const __m128i v0 = _mm_loadu_si128((const __m128i *)above_ref);
    const __m128i v1 = _mm_loadu_si128((const __m128i *)(above_ref + 16));
    _mm_storeu_si128((__m128i *)above_row, v0);
    _mm_storeu_si128((__m128i *)(above_row + 16), v1);
  } else if (n_copy == 64) {
    const __m128i v0 = _mm_loadu_si128((const __m128i *)above_ref);
    const __m128i v1 = _mm_loadu_si128((const __m128i *)(above_ref + 16));
    const __m128i v2 = _mm_loadu_si128((const __m128i *)(above_ref + 32));
    const __m128i v3 = _mm_loadu_si128((const __m128i *)(above_ref + 48));
    _mm_storeu_si128((__m128i *)above_row, v0);
    _mm_storeu_si128((__m128i *)(above_row + 16), v1);
    _mm_storeu_si128((__m128i *)(above_row + 32), v2);
    _mm_storeu_si128((__m128i *)(above_row + 48), v3);
  } else {
    copy_above_row_tail_lbd(above_row, above_ref, n_copy);
  }
#else
  if (n_copy == 4) {
    memcpy(above_row, above_ref, 4);
  } else if (n_copy == 8) {
    memcpy(above_row, above_ref, 8);
  } else if (n_copy == 16) {
    memcpy(above_row, above_ref, 16);
  } else if (n_copy == 32) {
    memcpy(above_row, above_ref, 32);
  } else if (n_copy == 64) {
    memcpy(above_row, above_ref, 64);
  } else {
    copy_above_row_tail_lbd(above_row, above_ref, n_copy);
  }
#endif
}

#if RECONINTRA_HAVE_SSE2
static AOM_FORCE_INLINE __m128i sse2_bcast8_lbd(uint8_t val) {
  uint32_t v4 = (uint32_t)val * 0x01010101u;
  return _mm_shuffle_epi32(_mm_cvtsi32_si128((int)v4), 0);
}
#endif

static AOM_FORCE_INLINE void fill_intra_edge_lbd(uint8_t *p, uint8_t val,
                                                 int n) {
#if RECONINTRA_HAVE_SSE2
  const __m128i v = sse2_bcast8_lbd(val);
  _mm_storeu_si128((__m128i *)p, v);
  if (UNLIKELY(n > 16)) {
    RECONINTRA_SCALAR_LOOP
    for (int i = 16; i < n; i += 16) {
      _mm_storeu_si128((__m128i *)(p + i), v);
    }
  }
#else
  memset(p, val, n);
#endif
}

#if RECONINTRA_HAVE_SSE2
static AOM_FORCE_INLINE uint32_t gather_left4_lbd(const uint8_t *r0,
                                                  ptrdiff_t s) {
  const uint8_t *r2 = r0 + s * 2;
  return (uint32_t)r0[0] | ((uint32_t)r0[s] << 8) | ((uint32_t)r2[0] << 16) |
         ((uint32_t)r2[s] << 24);
}

static AOM_FORCE_INLINE __m128i gather_left4_xmm_lbd(const uint8_t *r0,
                                                     ptrdiff_t s) {
  return _mm_cvtsi32_si128((int)gather_left4_lbd(r0, s));
}
#endif

// Destination left_col must live in an edge scratch buffer with >= 16 writable
// bytes past each 16-byte chunk start when RECONINTRA_HAVE_SSE2 is enabled.
RECONINTRA_NOINLINE static void copy_left_col_tail_lbd(uint8_t *left_col,
                                                       const uint8_t *left_ref,
                                                       ptrdiff_t ref_stride,
                                                       int n_copy) {
  assert(n_copy > 0);
  const ptrdiff_t s = ref_stride;
  const ptrdiff_t s4 = s * 4;
#if RECONINTRA_HAVE_SSE2
  if (LIKELY(n_copy >= 16)) {
    RECONINTRA_SCALAR_LOOP
    do {
      const __m128i v0 = gather_left4_xmm_lbd(left_ref, s);
      left_ref += s4;
      const __m128i v1 = gather_left4_xmm_lbd(left_ref, s);
      left_ref += s4;
      const __m128i v2 = gather_left4_xmm_lbd(left_ref, s);
      left_ref += s4;
      const __m128i v3 = gather_left4_xmm_lbd(left_ref, s);
      left_ref += s4;
      const __m128i lo = _mm_unpacklo_epi32(v0, v1);
      const __m128i hi = _mm_unpacklo_epi32(v2, v3);
      _mm_storeu_si128((__m128i *)left_col, _mm_unpacklo_epi64(lo, hi));
      left_col += 16;
      n_copy -= 16;
    } while (n_copy >= 16);
  }
  if (n_copy <= 0) return;
  if ((n_copy & 3) == 0) {
    const __m128i v0 = gather_left4_xmm_lbd(left_ref, s);
    if (n_copy == 4) {
      _mm_storeu_si128((__m128i *)left_col, v0);
      return;
    }
    left_ref += s4;
    const __m128i v1 = gather_left4_xmm_lbd(left_ref, s);
    const __m128i lo = _mm_unpacklo_epi32(v0, v1);
    if (n_copy == 8) {
      _mm_storeu_si128((__m128i *)left_col, lo);
      return;
    }
    left_ref += s4;
    const __m128i v2 = gather_left4_xmm_lbd(left_ref, s);
    _mm_storeu_si128((__m128i *)left_col, _mm_unpacklo_epi64(lo, v2));
    return;
  }
#else
  (void)s4;
#endif
  RECONINTRA_SCALAR_LOOP
  do {
    *left_col++ = *left_ref;
    left_ref += s;
    --n_copy;
  } while (n_copy > 0);
}

#if RECONINTRA_HAVE_SSE2
static AOM_FORCE_INLINE void copy_left_col16_lbd(uint8_t *dst,
                                                 const uint8_t *left_ref,
                                                 ptrdiff_t s, ptrdiff_t s4) {
  const __m128i v0 = gather_left4_xmm_lbd(left_ref, s);
  const __m128i v1 = gather_left4_xmm_lbd(left_ref + s4, s);
  left_ref += s4 * 2;
  const __m128i v2 = gather_left4_xmm_lbd(left_ref, s);
  const __m128i v3 = gather_left4_xmm_lbd(left_ref + s4, s);
  const __m128i lo = _mm_unpacklo_epi32(v0, v1);
  const __m128i hi = _mm_unpacklo_epi32(v2, v3);
  _mm_storeu_si128((__m128i *)dst, _mm_unpacklo_epi64(lo, hi));
}
#endif

static AOM_FORCE_INLINE void copy_left_col_lbd(uint8_t *left_col,
                                               const uint8_t *left_ref,
                                               ptrdiff_t ref_stride,
                                               int n_copy) {
  const ptrdiff_t s = ref_stride;
#if RECONINTRA_HAVE_SSE2
  if (n_copy == 4) {
    const uint32_t v0 = gather_left4_lbd(left_ref, s);
    _mm_storeu_si128((__m128i *)left_col, _mm_cvtsi32_si128((int)v0));
  } else if (n_copy == 8) {
    const uint32_t v0 = gather_left4_lbd(left_ref, s);
    const uint32_t v1 = gather_left4_lbd(left_ref + s * 4, s);
    _mm_storeu_si128((__m128i *)left_col,
                     _mm_unpacklo_epi32(_mm_cvtsi32_si128((int)v0),
                                        _mm_cvtsi32_si128((int)v1)));
  } else if (n_copy == 16) {
    copy_left_col16_lbd(left_col, left_ref, s, s * 4);
  } else if (n_copy == 32) {
    const ptrdiff_t s4 = s * 4;
    copy_left_col16_lbd(left_col, left_ref, s, s4);
    copy_left_col16_lbd(left_col + 16, left_ref + s4 * 4, s, s4);
  } else {
    copy_left_col_tail_lbd(left_col, left_ref, s, n_copy);
  }
#else
  if (n_copy == 4) {
    const uint8_t *r2 = left_ref + s * 2;
    left_col[0] = left_ref[0];
    left_col[1] = left_ref[s];
    left_col[2] = r2[0];
    left_col[3] = r2[s];
  } else if (n_copy == 8) {
    const uint8_t *r2 = left_ref + s * 2;
    const uint8_t *r4 = left_ref + s * 4;
    const uint8_t *r6 = r4 + s * 2;
    left_col[0] = left_ref[0];
    left_col[1] = left_ref[s];
    left_col[2] = r2[0];
    left_col[3] = r2[s];
    left_col[4] = r4[0];
    left_col[5] = r4[s];
    left_col[6] = r6[0];
    left_col[7] = r6[s];
  } else {
    copy_left_col_tail_lbd(left_col, left_ref, s, n_copy);
  }
#endif
}

RECONINTRA_NOINLINE static void copy_and_pad_above_lbd(uint8_t *above_row,
                                                       const uint8_t *above_ref,
                                                       int n_copy,
                                                       int num_needed) {
#if RECONINTRA_HAVE_SSE2
  if (n_copy == 4 && num_needed >= 8) {
    uint32_t u0;
    memcpy(&u0, above_ref, 4);
    const uint8_t last = (uint8_t)(u0 >> 24);
    const __m128i vp = sse2_bcast8_lbd(last);
    _mm_storeu_si128(
        (__m128i *)above_row,
        _mm_unpacklo_epi64(_mm_unpacklo_epi32(_mm_cvtsi32_si128((int)u0), vp),
                           vp));
    if (num_needed > 16) {
      fill_intra_edge_lbd(above_row + 16, last, num_needed - 16);
    }
    return;
  }
  if (n_copy == 8 && num_needed >= 12) {
    const uint8_t last = above_ref[7];
    const __m128i vp = sse2_bcast8_lbd(last);
    _mm_storeu_si128(
        (__m128i *)above_row,
        _mm_unpacklo_epi64(_mm_loadl_epi64((const __m128i *)above_ref), vp));
    if (num_needed > 16) {
      fill_intra_edge_lbd(above_row + 16, last, num_needed - 16);
    }
    return;
  }
#endif
  copy_above_row_lbd(above_row, above_ref, n_copy);
  fill_intra_edge_lbd(above_row + n_copy, above_ref[n_copy - 1],
                      num_needed - n_copy);
}

RECONINTRA_NOINLINE static void copy_and_pad_left_lbd(uint8_t *left_col,
                                                      const uint8_t *left_ref,
                                                      ptrdiff_t ref_stride,
                                                      int n_copy,
                                                      int num_needed) {
#if RECONINTRA_HAVE_SSE2
  const ptrdiff_t s = ref_stride;
  if (n_copy == 4 && num_needed >= 8) {
    const uint32_t u0 = gather_left4_lbd(left_ref, s);
    const uint8_t last = (uint8_t)(u0 >> 24);
    const __m128i vp = sse2_bcast8_lbd(last);
    _mm_storeu_si128(
        (__m128i *)left_col,
        _mm_unpacklo_epi64(_mm_unpacklo_epi32(_mm_cvtsi32_si128((int)u0), vp),
                           vp));
    if (num_needed > 16) {
      fill_intra_edge_lbd(left_col + 16, last, num_needed - 16);
    }
    return;
  }
  if (n_copy == 8 && num_needed >= 12) {
    const uint32_t v0 = gather_left4_lbd(left_ref, s);
    const uint32_t v1 = gather_left4_lbd(left_ref + s * 4, s);
    const uint8_t last = (uint8_t)(v1 >> 24);
    const __m128i lo = _mm_unpacklo_epi32(_mm_cvtsi32_si128((int)v0),
                                          _mm_cvtsi32_si128((int)v1));
    const __m128i vp = sse2_bcast8_lbd(last);
    _mm_storeu_si128((__m128i *)left_col, _mm_unpacklo_epi64(lo, vp));
    if (num_needed > 16) {
      fill_intra_edge_lbd(left_col + 16, last, num_needed - 16);
    }
    return;
  }
#endif
  copy_left_col_lbd(left_col, left_ref, ref_stride, n_copy);
  fill_intra_edge_lbd(left_col + n_copy, left_ref[(n_copy - 1) * ref_stride],
                      num_needed - n_copy);
}

#if RECONINTRA_HAVE_SSE2
static AOM_FORCE_INLINE __m128i sse2_all_ones_lbd(void) {
  const __m128i z = _mm_setzero_si128();
  return _mm_cmpeq_epi32(z, z);
}

static AOM_FORCE_INLINE __m128i shl1_ins0_lbd(__m128i v, int b0) {
  return _mm_or_si128(_mm_slli_si128(v, 1), _mm_cvtsi32_si128(b0));
}

static AOM_FORCE_INLINE __m128i shr1_merge_lbd(__m128i lo, __m128i hi) {
  return _mm_or_si128(_mm_srli_si128(lo, 1), _mm_slli_si128(hi, 15));
}

static AOM_FORCE_INLINE __m128i shr2_merge_lbd(__m128i lo, __m128i hi) {
  return _mm_or_si128(_mm_srli_si128(lo, 2), _mm_slli_si128(hi, 14));
}

static AOM_FORCE_INLINE __m128i load_tail_padded_lbd(const uint8_t *src,
                                                     int rem, __m128i v_last) {
  if (rem >= 16) return _mm_loadu_si128((const __m128i *)src);
  if (rem == 4) {
    uint32_t u0;
    memcpy(&u0, src, 4);
    return _mm_unpacklo_epi64(
        _mm_unpacklo_epi32(_mm_cvtsi32_si128((int)u0), v_last), v_last);
  }
  if (rem == 8) {
    return _mm_unpacklo_epi64(_mm_loadl_epi64((const __m128i *)src), v_last);
  }
  uint32_t u2;
  memcpy(&u2, src + 8, 4);
  return _mm_unpacklo_epi64(
      _mm_loadl_epi64((const __m128i *)src),
      _mm_unpacklo_epi32(_mm_cvtsi32_si128((int)u2), v_last));
}

static AOM_FORCE_INLINE __m128i filt1_vec16_lbd(__m128i a, __m128i b, __m128i c,
                                                __m128i one8) {
  const __m128i odd = _mm_and_si128(_mm_xor_si128(a, c), one8);
  return _mm_avg_epu8(_mm_sub_epi8(_mm_avg_epu8(a, c), odd), b);
}

static AOM_FORCE_INLINE __m128i filt2_half_lbd(__m128i a16, __m128i b16,
                                               __m128i c16, __m128i eight) {
  const __m128i ac = _mm_add_epi16(a16, c16);
  const __m128i abc = _mm_add_epi16(ac, b16);
  const __m128i ac2b = _mm_add_epi16(ac, _mm_add_epi16(b16, b16));
  return _mm_srli_epi16(
      _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(abc, 2), ac2b), eight), 4);
}

static AOM_FORCE_INLINE __m128i filt3_half_lbd(__m128i aa16, __m128i a16,
                                               __m128i b16, __m128i c16,
                                               __m128i d16, __m128i four) {
  const __m128i ae = _mm_add_epi16(aa16, d16);
  const __m128i bcd = _mm_add_epi16(_mm_add_epi16(a16, b16), c16);
  return _mm_srli_epi16(
      _mm_add_epi16(_mm_add_epi16(ae, four), _mm_add_epi16(bcd, bcd)), 3);
}

RECONINTRA_NOINLINE static void filter_intra_edge_odd_lbd(
    uint8_t *dst, const uint8_t *src, int corner_flag, int len, int strength) {
  if (dst != src) copy_above_row_lbd(dst, src, len);
  dst[-1] = (uint8_t)corner_flag;
  av1_filter_intra_edge_c(dst - 1, len + 1, strength);
}

static AOM_FORCE_INLINE __m128i pad_tail16_lbd(__m128i out, int len,
                                               __m128i v_last) {
  if (len == 4) {
    return _mm_unpacklo_epi64(_mm_unpacklo_epi32(out, v_last), v_last);
  }
  if (len == 8) return _mm_unpacklo_epi64(out, v_last);
  return _mm_unpacklo_epi64(out,
                            _mm_unpacklo_epi32(_mm_srli_si128(out, 8), v_last));
}

// corner_flag: bits [7:0] = corner sample (src[-1]), bit 8 = Z2 shifted-output
// flag (0 writes out[0..len-1] at dst; 1 writes [corner, out[0..len-1]] at
// dst - 1). For odd/non-multiple-of-4 len, falls back to scalar filtering at
// dst - 1.
RECONINTRA_NOINLINE static void filter_intra_edge_from_lbd(
    uint8_t *dst, const uint8_t *src, int corner_flag, int len, int strength) {
  assert((corner_flag & ~0x1ff) == 0);
  assert(len > 0 && len <= MAX_TX_SIZE * 2);
  assert(strength >= 1 && strength <= 3);
  if (UNLIKELY((len & 3) != 0)) {
    filter_intra_edge_odd_lbd(dst, src, corner_flag, len, strength);
    return;
  }
  const int corner = corner_flag & 0xff;
  const int z2 = corner_flag >> 8;
  const __m128i z = _mm_setzero_si128();
  const __m128i ones = sse2_all_ones_lbd();

  if (len <= 8) {
    __m128i v_last, b;
    if (LIKELY(len == 8)) {
      const __m128i b0 = _mm_loadl_epi64((const __m128i *)src);
      v_last = _mm_shuffle_epi32(
          _mm_shufflehi_epi16(_mm_unpacklo_epi8(b0, b0), 0xff), 0xff);
      b = _mm_unpacklo_epi64(b0, v_last);
    } else {
      uint32_t u0;
      memcpy(&u0, src, 4);
      const __m128i b0 = _mm_cvtsi32_si128((int)u0);
      v_last = _mm_shuffle_epi32(
          _mm_shufflelo_epi16(_mm_unpacklo_epi8(b0, b0), 0xff), 0);
      b = _mm_unpacklo_epi64(_mm_unpacklo_epi32(b0, v_last), v_last);
    }
    const __m128i a = shl1_ins0_lbd(b, corner);
    const __m128i c = shr1_merge_lbd(b, v_last);
    __m128i out;
    if (strength == 1) {
      out = filt1_vec16_lbd(a, b, c, _mm_sub_epi8(z, ones));
    } else if (strength == 2) {
      const __m128i eight = _mm_slli_epi16(_mm_srli_epi16(ones, 15), 3);
      const __m128i r =
          filt2_half_lbd(_mm_unpacklo_epi8(a, z), _mm_unpacklo_epi8(b, z),
                         _mm_unpacklo_epi8(c, z), eight);
      out = _mm_unpacklo_epi64(_mm_packus_epi16(r, r), v_last);
    } else {
      const __m128i four = _mm_slli_epi16(_mm_srli_epi16(ones, 15), 2);
      const __m128i aa = shl1_ins0_lbd(a, corner);
      const __m128i d = shr2_merge_lbd(b, v_last);
      const __m128i r =
          filt3_half_lbd(_mm_unpacklo_epi8(aa, z), _mm_unpacklo_epi8(a, z),
                         _mm_unpacklo_epi8(b, z), _mm_unpacklo_epi8(c, z),
                         _mm_unpacklo_epi8(d, z), four);
      const __m128i r8 = _mm_packus_epi16(r, r);
      out = UNLIKELY(len < 8)
                ? _mm_unpacklo_epi64(_mm_unpacklo_epi32(r8, v_last), v_last)
                : _mm_unpacklo_epi64(r8, v_last);
    }
    if (z2) {
      _mm_storeu_si128((__m128i *)(dst - 1), shl1_ins0_lbd(out, corner));
    } else {
      _mm_storeu_si128((__m128i *)dst, out);
    }
    return;
  }

  if (len <= 16) {
    __m128i v_last, b;
    if (LIKELY(len == 16)) {
      b = _mm_loadu_si128((const __m128i *)src);
      v_last = _mm_shuffle_epi32(
          _mm_shufflehi_epi16(_mm_unpackhi_epi8(b, b), 0xff), 0xff);
    } else {
      uint32_t u2;
      memcpy(&u2, src + 8, 4);
      const __m128i b2 = _mm_cvtsi32_si128((int)u2);
      v_last = _mm_shuffle_epi32(
          _mm_shufflelo_epi16(_mm_unpacklo_epi8(b2, b2), 0xff), 0);
      b = _mm_unpacklo_epi64(_mm_loadl_epi64((const __m128i *)src),
                             _mm_unpacklo_epi32(b2, v_last));
    }
    const __m128i a = shl1_ins0_lbd(b, corner);
    const __m128i c = shr1_merge_lbd(b, v_last);
    __m128i out;
    if (strength == 1) {
      out = filt1_vec16_lbd(a, b, c, _mm_sub_epi8(z, ones));
    } else if (strength == 2) {
      const __m128i eight = _mm_slli_epi16(_mm_srli_epi16(ones, 15), 3);
      const __m128i rl =
          filt2_half_lbd(_mm_unpacklo_epi8(a, z), _mm_unpacklo_epi8(b, z),
                         _mm_unpacklo_epi8(c, z), eight);
      const __m128i rh =
          filt2_half_lbd(_mm_unpackhi_epi8(a, z), _mm_unpackhi_epi8(b, z),
                         _mm_unpackhi_epi8(c, z), eight);
      out = _mm_packus_epi16(rl, rh);
    } else {
      const __m128i four = _mm_slli_epi16(_mm_srli_epi16(ones, 15), 2);
      const __m128i aa = shl1_ins0_lbd(a, corner);
      const __m128i d = shr2_merge_lbd(b, v_last);
      const __m128i rl =
          filt3_half_lbd(_mm_unpacklo_epi8(aa, z), _mm_unpacklo_epi8(a, z),
                         _mm_unpacklo_epi8(b, z), _mm_unpacklo_epi8(c, z),
                         _mm_unpacklo_epi8(d, z), four);
      const __m128i rh =
          filt3_half_lbd(_mm_unpackhi_epi8(aa, z), _mm_unpackhi_epi8(a, z),
                         _mm_unpackhi_epi8(b, z), _mm_unpackhi_epi8(c, z),
                         _mm_unpackhi_epi8(d, z), four);
      out = _mm_packus_epi16(rl, rh);
      if (UNLIKELY(len < 16)) {
        out = _mm_unpacklo_epi64(
            out, _mm_unpacklo_epi32(_mm_srli_si128(out, 8), v_last));
      }
    }
    if (z2) {
      if (LIKELY(len == 16)) _mm_storeu_si128((__m128i *)dst, out);
      _mm_storeu_si128((__m128i *)(dst - 1), shl1_ins0_lbd(out, corner));
    } else {
      _mm_storeu_si128((__m128i *)dst, out);
    }
    return;
  }

  const __m128i v_last = sse2_bcast8_lbd(src[len - 1]);
  __m128i cur = _mm_loadu_si128((const __m128i *)src);
  __m128i prev_v =
      _mm_slli_si128(_mm_cvtsi32_si128((int)((uint32_t)corner * 0x0101u)), 14);
  if (!z2) {
    if (strength == 1) {
      const __m128i one8 = _mm_sub_epi8(z, ones);
      for (;;) {
        const __m128i next =
            (len <= 16) ? v_last
                        : load_tail_padded_lbd(src + 16, len - 16, v_last);
        const __m128i a =
            _mm_or_si128(_mm_srli_si128(prev_v, 15), _mm_slli_si128(cur, 1));
        _mm_store_si128(
            (__m128i *)dst,
            filt1_vec16_lbd(a, cur, shr1_merge_lbd(cur, next), one8));
        if (len <= 16) return;
        prev_v = cur;
        cur = next;
        src += 16;
        dst += 16;
        len -= 16;
      }
    } else if (strength == 2) {
      const __m128i eight = _mm_slli_epi16(_mm_srli_epi16(ones, 15), 3);
      for (;;) {
        const __m128i next =
            (len <= 16) ? v_last
                        : load_tail_padded_lbd(src + 16, len - 16, v_last);
        const __m128i a =
            _mm_or_si128(_mm_srli_si128(prev_v, 15), _mm_slli_si128(cur, 1));
        const __m128i c = shr1_merge_lbd(cur, next);
        const __m128i rl =
            filt2_half_lbd(_mm_unpacklo_epi8(a, z), _mm_unpacklo_epi8(cur, z),
                           _mm_unpacklo_epi8(c, z), eight);
        const __m128i rh =
            filt2_half_lbd(_mm_unpackhi_epi8(a, z), _mm_unpackhi_epi8(cur, z),
                           _mm_unpackhi_epi8(c, z), eight);
        _mm_store_si128((__m128i *)dst, _mm_packus_epi16(rl, rh));
        if (len <= 16) return;
        prev_v = cur;
        cur = next;
        src += 16;
        dst += 16;
        len -= 16;
      }
    } else {
      const __m128i four = _mm_slli_epi16(_mm_srli_epi16(ones, 15), 2);
      for (;;) {
        const __m128i next =
            (len <= 16) ? v_last
                        : load_tail_padded_lbd(src + 16, len - 16, v_last);
        const __m128i aa =
            _mm_or_si128(_mm_srli_si128(prev_v, 14), _mm_slli_si128(cur, 2));
        const __m128i a =
            _mm_or_si128(_mm_srli_si128(prev_v, 15), _mm_slli_si128(cur, 1));
        const __m128i c = shr1_merge_lbd(cur, next);
        const __m128i d = shr2_merge_lbd(cur, next);
        const __m128i rl =
            filt3_half_lbd(_mm_unpacklo_epi8(aa, z), _mm_unpacklo_epi8(a, z),
                           _mm_unpacklo_epi8(cur, z), _mm_unpacklo_epi8(c, z),
                           _mm_unpacklo_epi8(d, z), four);
        const __m128i rh =
            filt3_half_lbd(_mm_unpackhi_epi8(aa, z), _mm_unpackhi_epi8(a, z),
                           _mm_unpackhi_epi8(cur, z), _mm_unpackhi_epi8(c, z),
                           _mm_unpackhi_epi8(d, z), four);
        __m128i out = _mm_packus_epi16(rl, rh);
        if (UNLIKELY(len < 16)) out = pad_tail16_lbd(out, len, v_last);
        _mm_store_si128((__m128i *)dst, out);
        if (len <= 16) return;
        prev_v = cur;
        cur = next;
        src += 16;
        dst += 16;
        len -= 16;
      }
    }
  }

  __m128i out_prev = prev_v;
  --dst;
  if (strength == 1) {
    const __m128i one8 = _mm_sub_epi8(z, ones);
    for (;;) {
      const __m128i next =
          (len <= 16) ? v_last
                      : load_tail_padded_lbd(src + 16, len - 16, v_last);
      const __m128i a =
          _mm_or_si128(_mm_srli_si128(prev_v, 15), _mm_slli_si128(cur, 1));
      const __m128i out =
          filt1_vec16_lbd(a, cur, shr1_merge_lbd(cur, next), one8);
      prev_v = cur;
      _mm_storeu_si128(
          (__m128i *)dst,
          _mm_or_si128(_mm_srli_si128(out_prev, 15), _mm_slli_si128(out, 1)));
      out_prev = out;
      if (len <= 16) break;
      cur = next;
      src += 16;
      dst += 16;
      len -= 16;
    }
  } else if (strength == 2) {
    const __m128i eight = _mm_slli_epi16(_mm_srli_epi16(ones, 15), 3);
    for (;;) {
      const __m128i next =
          (len <= 16) ? v_last
                      : load_tail_padded_lbd(src + 16, len - 16, v_last);
      const __m128i a =
          _mm_or_si128(_mm_srli_si128(prev_v, 15), _mm_slli_si128(cur, 1));
      const __m128i c = shr1_merge_lbd(cur, next);
      const __m128i rl =
          filt2_half_lbd(_mm_unpacklo_epi8(a, z), _mm_unpacklo_epi8(cur, z),
                         _mm_unpacklo_epi8(c, z), eight);
      const __m128i rh =
          filt2_half_lbd(_mm_unpackhi_epi8(a, z), _mm_unpackhi_epi8(cur, z),
                         _mm_unpackhi_epi8(c, z), eight);
      prev_v = cur;
      const __m128i out = _mm_packus_epi16(rl, rh);
      _mm_storeu_si128(
          (__m128i *)dst,
          _mm_or_si128(_mm_srli_si128(out_prev, 15), _mm_slli_si128(out, 1)));
      out_prev = out;
      if (len <= 16) break;
      cur = next;
      src += 16;
      dst += 16;
      len -= 16;
    }
  } else {
    const __m128i four = _mm_slli_epi16(_mm_srli_epi16(ones, 15), 2);
    for (;;) {
      const __m128i next =
          (len <= 16) ? v_last
                      : load_tail_padded_lbd(src + 16, len - 16, v_last);
      const __m128i aa =
          _mm_or_si128(_mm_srli_si128(prev_v, 14), _mm_slli_si128(cur, 2));
      const __m128i a =
          _mm_or_si128(_mm_srli_si128(prev_v, 15), _mm_slli_si128(cur, 1));
      const __m128i c = shr1_merge_lbd(cur, next);
      const __m128i d = shr2_merge_lbd(cur, next);
      const __m128i rl =
          filt3_half_lbd(_mm_unpacklo_epi8(aa, z), _mm_unpacklo_epi8(a, z),
                         _mm_unpacklo_epi8(cur, z), _mm_unpacklo_epi8(c, z),
                         _mm_unpacklo_epi8(d, z), four);
      const __m128i rh =
          filt3_half_lbd(_mm_unpackhi_epi8(aa, z), _mm_unpackhi_epi8(a, z),
                         _mm_unpackhi_epi8(cur, z), _mm_unpackhi_epi8(c, z),
                         _mm_unpackhi_epi8(d, z), four);
      prev_v = cur;
      __m128i out = _mm_packus_epi16(rl, rh);
      if (UNLIKELY(len < 16)) out = pad_tail16_lbd(out, len, v_last);
      _mm_storeu_si128(
          (__m128i *)dst,
          _mm_or_si128(_mm_srli_si128(out_prev, 15), _mm_slli_si128(out, 1)));
      out_prev = out;
      if (len <= 16) break;
      cur = next;
      src += 16;
      dst += 16;
      len -= 16;
    }
  }
  if (LIKELY(len == 16)) {
    _mm_storeu_si128(
        (__m128i *)(dst + 16),
        _mm_or_si128(_mm_srli_si128(out_prev, 15), _mm_slli_si128(v_last, 1)));
  }
}

RECONINTRA_NOINLINE static void upsample_intra_edge_from_lbd(uint8_t *dst,
                                                             const uint8_t *src,
                                                             uint8_t corner,
                                                             int sz) {
  assert(sz > 0 && sz <= MAX_UPSAMPLE_SZ && (sz & 3) == 0);
  const __m128i z = _mm_setzero_si128();
  const __m128i ones = sse2_all_ones_lbd();
  const __m128i eight = _mm_slli_epi16(_mm_srli_epi16(ones, 15), 3);
  const int tl2 = (int)((uint32_t)corner * 0x0101u);
  if (sz <= 8) {
    __m128i v;
    int last_b;
    if (sz == 4) {
      uint32_t u;
      memcpy(&u, src, 4);
      last_b = (int)(u >> 24);
      const uint32_t pad = (uint32_t)last_b * 0x01010101u;
      v = _mm_unpacklo_epi32(_mm_cvtsi32_si128((int)u),
                             _mm_cvtsi32_si128((int)pad));
    } else {
      v = _mm_loadl_epi64((const __m128i *)src);
      last_b = src[7];
    }
    __m128i ext = _mm_or_si128(_mm_cvtsi32_si128(tl2), _mm_slli_si128(v, 2));
    ext = _mm_insert_epi16(ext, (int16_t)((uint32_t)last_b * 0x0101u), 5);
    const __m128i a16 = _mm_unpacklo_epi8(ext, z);
    const __m128i b = _mm_srli_si128(ext, 1);
    const __m128i b16 = _mm_unpacklo_epi8(b, z);
    const __m128i c16 = _mm_unpacklo_epi8(_mm_srli_si128(ext, 2), z);
    const __m128i d16 = _mm_unpacklo_epi8(_mm_srli_si128(ext, 3), z);
    const __m128i bc = _mm_add_epi16(b16, c16);
    const __m128i bc9 = _mm_add_epi16(bc, _mm_slli_epi16(bc, 3));
    const __m128i ad = _mm_sub_epi16(eight, _mm_add_epi16(a16, d16));
    const __m128i s16 = _mm_srai_epi16(_mm_add_epi16(bc9, ad), 4);
    const __m128i s8 = _mm_packus_epi16(s16, s16);
    _mm_storeu_si128((__m128i *)(dst - 2), _mm_unpacklo_epi8(b, s8));
    _mm_storeu_si128((__m128i *)(dst + 14), _mm_cvtsi32_si128(last_b));
    return;
  }

  __m128i c;
  int last_b;
  if (sz == 12) {
    uint32_t u2;
    memcpy(&u2, src + 8, 4);
    last_b = (int)(u2 >> 24);
    const uint32_t pad = (uint32_t)last_b * 0x01010101u;
    const __m128i lo = _mm_loadl_epi64((const __m128i *)src);
    const __m128i hi = _mm_unpacklo_epi32(_mm_cvtsi32_si128((int)u2),
                                          _mm_cvtsi32_si128((int)pad));
    c = _mm_unpacklo_epi64(lo, hi);
  } else {
    c = _mm_loadu_si128((const __m128i *)src);
    last_b = (_mm_extract_epi16(c, 7) >> 8) & 0xff;
  }
  const int last_pair = last_b | (last_b << 8);
  const __m128i b = shl1_ins0_lbd(c, corner);
  const __m128i a = shl1_ins0_lbd(b, corner);
  const __m128i d =
      _mm_insert_epi16(_mm_srli_si128(c, 1), (int16_t)last_pair, 7);

  const __m128i bc_lo =
      _mm_add_epi16(_mm_unpacklo_epi8(b, z), _mm_unpacklo_epi8(c, z));
  const __m128i bc_hi =
      _mm_add_epi16(_mm_unpackhi_epi8(b, z), _mm_unpackhi_epi8(c, z));
  const __m128i bc9_lo = _mm_add_epi16(bc_lo, _mm_slli_epi16(bc_lo, 3));
  const __m128i bc9_hi = _mm_add_epi16(bc_hi, _mm_slli_epi16(bc_hi, 3));
  const __m128i ad_lo = _mm_sub_epi16(
      eight, _mm_add_epi16(_mm_unpacklo_epi8(a, z), _mm_unpacklo_epi8(d, z)));
  const __m128i ad_hi = _mm_sub_epi16(
      eight, _mm_add_epi16(_mm_unpackhi_epi8(a, z), _mm_unpackhi_epi8(d, z)));
  const __m128i s_lo = _mm_srai_epi16(_mm_add_epi16(bc9_lo, ad_lo), 4);
  const __m128i s_hi = _mm_srai_epi16(_mm_add_epi16(bc9_hi, ad_hi), 4);
  const __m128i s8 = _mm_packus_epi16(s_lo, s_hi);

  _mm_storeu_si128((__m128i *)(dst - 2), _mm_unpacklo_epi8(b, s8));
  _mm_storeu_si128((__m128i *)(dst + 14), _mm_unpackhi_epi8(b, s8));
  if (sz == 16) {
    _mm_storeu_si128((__m128i *)(dst + 30), _mm_cvtsi32_si128(last_b));
  }
}

static AOM_FORCE_INLINE void store_z2_corner_lbd(uint8_t *edge,
                                                 uint8_t corner) {
  const __m128i v = _mm_loadu_si128((const __m128i *)edge);
  _mm_storeu_si128((__m128i *)(edge - 1), shl1_ins0_lbd(v, corner));
}

static AOM_FORCE_INLINE void fill_z2_edge_lbd(uint8_t *edge, uint8_t corner,
                                              uint8_t val, int n) {
  const __m128i v = sse2_bcast8_lbd(val);
  _mm_storeu_si128((__m128i *)(edge - 1), shl1_ins0_lbd(v, corner));
  if (UNLIKELY(n >= 16)) {
    RECONINTRA_SCALAR_LOOP
    for (int i = 15; i < n; i += 16) {
      _mm_storeu_si128((__m128i *)(edge + i), v);
    }
  }
}

static AOM_FORCE_INLINE void copy_above_z2_lbd(uint8_t *edge,
                                               const uint8_t *above_ref,
                                               uint8_t corner, int n) {
  if (n == 4) {
    uint32_t u0;
    memcpy(&u0, above_ref, 4);
    _mm_storeu_si128((__m128i *)(edge - 1),
                     shl1_ins0_lbd(_mm_cvtsi32_si128((int)u0), corner));
  } else if (n == 8) {
    const __m128i v0 = _mm_loadl_epi64((const __m128i *)above_ref);
    _mm_storeu_si128((__m128i *)(edge - 1), shl1_ins0_lbd(v0, corner));
  } else if (n == 16) {
    const __m128i v0 = _mm_loadu_si128((const __m128i *)above_ref);
    _mm_storeu_si128((__m128i *)(edge - 1), shl1_ins0_lbd(v0, corner));
    _mm_storeu_si128((__m128i *)(edge + 15),
                     _mm_cvtsi32_si128((_mm_extract_epi16(v0, 7) >> 8) & 0xff));
  } else if (n == 32) {
    const __m128i v0 = _mm_loadu_si128((const __m128i *)above_ref);
    const __m128i v1 = _mm_loadu_si128((const __m128i *)(above_ref + 15));
    _mm_storeu_si128((__m128i *)(edge - 1), shl1_ins0_lbd(v0, corner));
    _mm_storeu_si128((__m128i *)(edge + 15), v1);
    _mm_storeu_si128((__m128i *)(edge + 31),
                     _mm_cvtsi32_si128((int)above_ref[31]));
  } else {
    const __m128i v0 = _mm_loadu_si128((const __m128i *)above_ref);
    const __m128i v1 = _mm_loadu_si128((const __m128i *)(above_ref + 15));
    const __m128i v2 = _mm_loadu_si128((const __m128i *)(above_ref + 31));
    const __m128i v3 = _mm_loadu_si128((const __m128i *)(above_ref + 47));
    _mm_storeu_si128((__m128i *)(edge - 1), shl1_ins0_lbd(v0, corner));
    _mm_storeu_si128((__m128i *)(edge + 15), v1);
    _mm_storeu_si128((__m128i *)(edge + 31), v2);
    _mm_storeu_si128((__m128i *)(edge + 47), v3);
    _mm_storeu_si128((__m128i *)(edge + 63),
                     _mm_cvtsi32_si128((int)above_ref[63]));
  }
}
#else
// corner_flag: bits [7:0] = corner sample (dst[-1]), bit 8 = Z2 flag (ignored
// here because av1_filter_intra_edge already keeps dst[-1] = corner and updates
// dst[0..len-1] in place).
static AOM_FORCE_INLINE void filter_intra_edge_from_lbd(uint8_t *dst,
                                                        const uint8_t *src,
                                                        int corner_flag,
                                                        int len, int strength) {
  assert((corner_flag & ~0x1ff) == 0);
  assert(len > 0 && len <= MAX_TX_SIZE * 2);
  assert(strength >= 1 && strength <= 3);
  if (dst != src) copy_above_row_lbd(dst, src, len);
  dst[-1] = (uint8_t)corner_flag;
  av1_filter_intra_edge(dst - 1, len + 1, strength);
}

static AOM_FORCE_INLINE void upsample_intra_edge_from_lbd(uint8_t *dst,
                                                          const uint8_t *src,
                                                          uint8_t corner,
                                                          int sz) {
  assert(sz > 0 && sz <= MAX_UPSAMPLE_SZ && (sz & 3) == 0);
  if (dst != src) copy_above_row_lbd(dst, src, sz);
  dst[-1] = corner;
  av1_upsample_intra_edge(dst, sz);
}

static AOM_FORCE_INLINE void store_z2_corner_lbd(uint8_t *edge,
                                                 uint8_t corner) {
  edge[-1] = corner;
}

static AOM_FORCE_INLINE void fill_z2_edge_lbd(uint8_t *edge, uint8_t corner,
                                              uint8_t val, int n) {
  memset(edge, val, n);
  edge[-1] = corner;
}

static AOM_FORCE_INLINE void copy_above_z2_lbd(uint8_t *edge,
                                               const uint8_t *above_ref,
                                               uint8_t corner, int n) {
  copy_above_row_lbd(edge, above_ref, n);
  edge[-1] = corner;
}
#endif

static AOM_FORCE_INLINE void fill_block_lbd(uint8_t *dst, ptrdiff_t dst_stride,
                                            uint8_t val, int txwpx, int txhpx) {
  const ptrdiff_t s2 = dst_stride * 2;
  if (txwpx == 4) {
    const uint32_t v = (uint32_t)val * 0x01010101u;
    RECONINTRA_SCALAR_LOOP
    do {
      memcpy(dst, &v, 4);
      memcpy(dst + dst_stride, &v, 4);
      dst += s2;
      memcpy(dst, &v, 4);
      memcpy(dst + dst_stride, &v, 4);
      dst += s2;
      txhpx -= 4;
    } while (txhpx > 0);
  } else if (txwpx == 8) {
    const uint64_t v = (uint64_t)val * 0x0101010101010101ULL;
    RECONINTRA_SCALAR_LOOP
    do {
      memcpy(dst, &v, 8);
      memcpy(dst + dst_stride, &v, 8);
      dst += s2;
      memcpy(dst, &v, 8);
      memcpy(dst + dst_stride, &v, 8);
      dst += s2;
      txhpx -= 4;
    } while (txhpx > 0);
  } else {
#if RECONINTRA_HAVE_SSE2
    const __m128i v = sse2_bcast8_lbd(val);
    if (LIKELY(txwpx == 16)) {
      RECONINTRA_SCALAR_LOOP
      do {
        _mm_storeu_si128((__m128i *)dst, v);
        _mm_storeu_si128((__m128i *)(dst + dst_stride), v);
        dst += s2;
        _mm_storeu_si128((__m128i *)dst, v);
        _mm_storeu_si128((__m128i *)(dst + dst_stride), v);
        dst += s2;
        txhpx -= 4;
      } while (txhpx > 0);
    } else if (LIKELY(txwpx == 32)) {
      RECONINTRA_SCALAR_LOOP
      do {
        _mm_storeu_si128((__m128i *)dst, v);
        _mm_storeu_si128((__m128i *)(dst + 16), v);
        _mm_storeu_si128((__m128i *)(dst + dst_stride), v);
        _mm_storeu_si128((__m128i *)(dst + dst_stride + 16), v);
        dst += s2;
        txhpx -= 2;
      } while (txhpx > 0);
    } else {
      RECONINTRA_SCALAR_LOOP
      do {
        _mm_storeu_si128((__m128i *)dst, v);
        _mm_storeu_si128((__m128i *)(dst + 16), v);
        _mm_storeu_si128((__m128i *)(dst + 32), v);
        _mm_storeu_si128((__m128i *)(dst + 48), v);
        dst += dst_stride;
        --txhpx;
      } while (txhpx > 0);
    }
#else
    do {
      memset(dst, val, txwpx);
      dst += dst_stride;
      --txhpx;
    } while (txhpx > 0);
#endif
  }
}

// mode_tx: bits [7:0] = filter_intra_mode, bits [31:8] = tx_size.
// top_left_px: bits [15:0] = n_top_px, bits [31:16] = n_left_px.
RECONINTRA_NOINLINE static void build_filter_intra_predictors(
    const uint8_t *ref, ptrdiff_t ref_stride, uint8_t *dst,
    ptrdiff_t dst_stride, uint32_t mode_tx, uint32_t top_left_px) {
  const FILTER_INTRA_MODE filter_intra_mode =
      (FILTER_INTRA_MODE)(mode_tx & 0xff);
  const TX_SIZE tx_size = (TX_SIZE)(mode_tx >> 8);
  const int n_top_px = (int)(top_left_px & 0xffff);
  const int n_left_px = (int)(top_left_px >> 16);
  assert(filter_intra_mode < FILTER_INTRA_MODES);
  assert(tx_size < TX_SIZES_ALL);
  const uint8_t *above_ref = ref - ref_stride;
  const uint8_t *left_ref = ref - 1;
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  assert(n_top_px <= txwpx && n_left_px <= txhpx);

  DECLARE_ALIGNED(16, uint8_t, left_data[NUM_INTRA_NEIGHBOUR_PIXELS]);
  DECLARE_ALIGNED(16, uint8_t, above_data[NUM_INTRA_NEIGHBOUR_PIXELS]);
  // Align above_fi - 1 to 16 bytes (above_data + 16) so [corner, above0..14]
  // writes land on a single 16-byte-aligned vector boundary.
  uint8_t *const above_fi = above_data + 17;
  uint8_t *const left_col = left_data + 16;

  if (LIKELY(n_left_px == txhpx)) {
    copy_left_col_lbd(left_col, left_ref, ref_stride, txhpx);
  } else if (n_left_px > 0) {
    copy_and_pad_left_lbd(left_col, left_ref, ref_stride, n_left_px, txhpx);
  } else {
    const uint8_t val = (n_top_px > 0) ? above_ref[0] : 129;
    fill_intra_edge_lbd(left_col, val, txhpx);
  }

  uint8_t corner;
  if (LIKELY(n_top_px > 0 && n_left_px > 0)) {
    corner = above_ref[-1];
  } else if (n_top_px > 0) {
    corner = above_ref[0];
  } else if (n_left_px > 0) {
    corner = left_ref[0];
  } else {
    corner = 128;
  }

  if (LIKELY(n_top_px == txwpx)) {
    copy_above_z2_lbd(above_fi, above_ref, corner, txwpx);
  } else if (n_top_px > 0) {
    copy_and_pad_above_lbd(above_fi, above_ref, n_top_px, txwpx);
    store_z2_corner_lbd(above_fi, corner);
  } else {
    const uint8_t val = (n_left_px > 0) ? left_ref[0] : 127;
    fill_z2_edge_lbd(above_fi, corner, val, txwpx);
  }

  av1_filter_intra_predictor(dst, dst_stride, tx_size, above_fi, left_col,
                             filter_intra_mode);
}

// angle_tx_flags: bits [8:0] = p_angle, bits [13:9] = tx_size,
//   bit 14 = disable_edge_filter, bit 15 = intra_edge_filter_type.
// top_left_px: bits [15:0] = n_top_px, bits [31:16] = n_left_px.
RECONINTRA_NOINLINE static void build_dr_z2_lbd(
    const uint8_t *ref, ptrdiff_t ref_stride, uint8_t *dst,
    ptrdiff_t dst_stride, uint32_t angle_tx_flags, uint32_t top_left_px) {
  const int p_angle = (int)(angle_tx_flags & 0x1ff);
  const TX_SIZE tx_size = (TX_SIZE)((angle_tx_flags >> 9) & 0x1f);
  const int disable_edge_filter = (int)((angle_tx_flags >> 14) & 1);
  const int intra_edge_filter_type = (int)(angle_tx_flags >> 15);
  const int n_top_px = (int)(top_left_px & 0xffff);
  const int n_left_px = (int)(top_left_px >> 16);
  assert((angle_tx_flags >> 16) == 0);
  assert(tx_size < TX_SIZES_ALL);
  assert(p_angle > 90 && p_angle < 180);
  const uint8_t *above_ref = ref - ref_stride;
  const uint8_t *left_ref = ref - 1;
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  assert(n_top_px <= txwpx);
  assert(n_left_px <= txhpx);
  const int blk_wh = txwpx + txhpx;
  const int d_left = 180 - p_angle;
  const int d_above = p_angle - 90;

  uint8_t z2_buf[2 * 192 + 64];
  uint8_t *const left_base =
      (uint8_t *)(((uintptr_t)z2_buf + 63u) & ~(uintptr_t)63u);
  uint8_t *const left_z2 = left_base + 16;
  uint8_t *const above_z2 = left_base + (192 + 16);
  memset(left_base, 0, 16);
  memset(left_base + 192, 0, 16);
  int upsample_left = 0;
  int upsample_above = 0;

  if (LIKELY(n_top_px == txwpx && n_left_px == txhpx)) {
    uint8_t corner = above_ref[-1];
    int s_left = 0;
    int s_above = 0;
    if (LIKELY(!disable_edge_filter)) {
      if (blk_wh >= 24) {
        corner =
            (uint8_t)((left_ref[0] * 5 + corner * 6 + above_ref[0] * 5 + 8) >>
                      4);
      }
      if (blk_wh > 32) {
        s_left = 3;
        s_above = 3;
      } else if (LIKELY((blk_wh <= 16) & !intra_edge_filter_type)) {
        const int s1_thr = (blk_wh == 8) ? 56 : 40;
        upsample_left = (d_left < 40);
        s_left = (d_left >= s1_thr);
        upsample_above = (d_above < 40);
        s_above = (d_above >= s1_thr);
      } else {
        const uint32_t cfg =
            intra_edge_filter_cfg_lbd(blk_wh, intra_edge_filter_type);
        const int can_up = (int)(cfg >> 15);
        upsample_left = can_up & (d_left < 40);
        upsample_above = can_up & (d_above < 40);
        s_left = eval_edge_filt_s_lbd(cfg, d_left);
        s_above = eval_edge_filt_s_lbd(cfg, d_above);
      }
    }
#if RECONINTRA_HAVE_SSE2
    if (UNLIKELY(!(upsample_left | s_left) && txhpx <= 8)) {
      const ptrdiff_t s = ref_stride;
      const uint32_t v0 = gather_left4_lbd(left_ref, s);
      __m128i lo = _mm_cvtsi32_si128((int)v0);
      if (txhpx == 8) {
        const uint32_t v1 = gather_left4_lbd(left_ref + s * 4, s);
        lo = _mm_unpacklo_epi32(lo, _mm_cvtsi32_si128((int)v1));
      }
      _mm_storeu_si128((__m128i *)(left_z2 - 1), shl1_ins0_lbd(lo, corner));
    } else
#endif
    {
      copy_left_col_lbd(left_z2, left_ref, ref_stride, txhpx);
      if (upsample_left) {
        upsample_intra_edge_from_lbd(left_z2, left_z2, corner, txhpx);
      } else if (LIKELY(s_left)) {
        filter_intra_edge_from_lbd(left_z2, left_z2, (int)corner | 0x100, txhpx,
                                   s_left);
      } else {
        store_z2_corner_lbd(left_z2, corner);
      }
    }
    if (upsample_above) {
      upsample_intra_edge_from_lbd(above_z2, above_ref, corner, txwpx);
    } else if (LIKELY(s_above)) {
      filter_intra_edge_from_lbd(above_z2, above_ref, (int)corner | 0x100,
                                 txwpx, s_above);
    } else {
      copy_above_z2_lbd(above_z2, above_ref, corner, txwpx);
    }
  } else {
    uint8_t corner;
    if (n_top_px > 0 && n_left_px > 0) {
      corner = above_ref[-1];
      if (blk_wh >= 24 && !disable_edge_filter) {
        corner =
            (uint8_t)((left_ref[0] * 5 + corner * 6 + above_ref[0] * 5 + 8) >>
                      4);
      }
    } else if (n_top_px > 0) {
      corner = above_ref[0];
    } else if (n_left_px > 0) {
      corner = left_ref[0];
    } else {
      corner = 128;
    }

    int s_left = 0;
    int s_above = 0;
    if (!disable_edge_filter) {
      const uint32_t cfg =
          intra_edge_filter_cfg_lbd(blk_wh, intra_edge_filter_type);
      const int can_up = (int)(cfg >> 15);
      upsample_left = can_up & (d_left < 40);
      upsample_above = can_up & (d_above < 40);
      if (n_left_px > 0) s_left = eval_edge_filt_s_lbd(cfg, d_left);
      if (n_top_px > 0) s_above = eval_edge_filt_s_lbd(cfg, d_above);
    }

    if (n_left_px == 0) {
      const uint8_t val = (n_top_px > 0) ? above_ref[0] : 129;
      if (upsample_left) {
        fill_intra_edge_lbd(left_z2, val, txhpx);
        upsample_intra_edge_from_lbd(left_z2, left_z2, corner, txhpx);
      } else {
        fill_z2_edge_lbd(left_z2, corner, val, txhpx);
      }
    } else {
      if (n_left_px == txhpx) {
        copy_left_col_lbd(left_z2, left_ref, ref_stride, txhpx);
      } else {
        copy_and_pad_left_lbd(left_z2, left_ref, ref_stride, n_left_px, txhpx);
      }
      if (upsample_left) {
        upsample_intra_edge_from_lbd(left_z2, left_z2, corner, txhpx);
      } else if (s_left) {
        filter_intra_edge_from_lbd(left_z2, left_z2, (int)corner | 0x100,
                                   n_left_px, s_left);
      } else {
        store_z2_corner_lbd(left_z2, corner);
      }
    }

    if (n_top_px == 0) {
      const uint8_t val = (n_left_px > 0) ? left_ref[0] : 127;
      if (upsample_above) {
        fill_intra_edge_lbd(above_z2, val, txwpx);
        upsample_intra_edge_from_lbd(above_z2, above_z2, corner, txwpx);
      } else {
        fill_z2_edge_lbd(above_z2, corner, val, txwpx);
      }
    } else {
      const uint8_t *above_src = above_ref;
      if (n_top_px < txwpx) {
        copy_and_pad_above_lbd(above_z2, above_ref, n_top_px, txwpx);
        above_src = above_z2;
      }
      if (upsample_above) {
        upsample_intra_edge_from_lbd(above_z2, above_src, corner, txwpx);
      } else if (s_above) {
        filter_intra_edge_from_lbd(above_z2, above_src, (int)corner | 0x100,
                                   n_top_px, s_above);
      } else if (above_src == above_ref) {
        copy_above_z2_lbd(above_z2, above_ref, corner, txwpx);
      } else {
        store_z2_corner_lbd(above_z2, corner);
      }
    }
  }

  memset(above_z2 + (txwpx << upsample_above), 0, 16);
  memset(left_z2 + (txhpx << upsample_left), 0, 16);
  av1_dr_prediction_z2(
      dst, dst_stride, txwpx, txhpx, above_z2, left_z2, upsample_above,
      upsample_left, dr_intra_derivative[d_left], dr_intra_derivative[d_above]);
}

// angle_tx_flags: bits [8:0] = p_angle, bits [13:9] = tx_size,
//   bit 14 = disable_edge_filter, bit 15 = intra_edge_filter_type.
// avail_px: bits [7:0] = n_top_px, bits [15:8] = n_topright_px,
//   bits [23:16] = n_left_px, bits [31:24] = n_bottomleft_px.
RECONINTRA_NOINLINE static void build_directional_intra_predictors(
    const uint8_t *ref, ptrdiff_t ref_stride, uint8_t *dst,
    ptrdiff_t dst_stride, uint32_t angle_tx_flags, uint32_t avail_px) {
  const int p_angle = (int)(angle_tx_flags & 0x1ff);
  const TX_SIZE tx_size = (TX_SIZE)((angle_tx_flags >> 9) & 0x1f);
  const int disable_edge_filter = (int)((angle_tx_flags >> 14) & 1);
  const int intra_edge_filter_type = (int)(angle_tx_flags >> 15);
  const int n_top_px = (int)(avail_px & 0xff);
  const int n_topright_px = (int)((avail_px >> 8) & 0xff);
  const int n_left_px = (int)((avail_px >> 16) & 0xff);
  const int n_bottomleft_px = (int)(avail_px >> 24);
  assert((angle_tx_flags >> 16) == 0);
  assert(tx_size < TX_SIZES_ALL);
  assert(p_angle > 0 && p_angle < 270);
  const uint8_t *above_ref = ref - ref_stride;
  const uint8_t *left_ref = ref - 1;
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  assert(n_top_px <= txwpx);
  assert(n_left_px <= txhpx);
  assert(n_topright_px <= txwpx);
  assert(n_bottomleft_px <= txhpx);
  uint8_t fill_val;

  uint8_t edge_data[NUM_INTRA_NEIGHBOUR_PIXELS + 80];
  uint8_t *const edge =
      (uint8_t *)(((uintptr_t)(edge_data + 16) + 63u) & ~(uintptr_t)63u);

  if (p_angle < 90) {
    if (UNLIKELY(n_top_px == 0)) {
      fill_val = (n_left_px > 0) ? left_ref[0] : 127;
      goto fill_solid;
    }
    assert(n_topright_px >= 0);
    const int blk_wh = txwpx + txhpx;
    const uint8_t corner = (n_left_px > 0) ? above_ref[-1] : above_ref[0];
    int upsample_above = 0;
    int strength = 0;
    if (LIKELY(!disable_edge_filter)) {
      const int d = 90 - p_angle;
      if (blk_wh > 32) {
        strength = 3;
      } else if (LIKELY((blk_wh <= 16) & !intra_edge_filter_type)) {
        upsample_above = (d < 40);
        strength = (d >= ((blk_wh == 8) ? 56 : 40));
      } else {
        const uint32_t cfg =
            intra_edge_filter_cfg_lbd(blk_wh, intra_edge_filter_type);
        upsample_above = (int)(cfg >> 15) & (d < 40);
        if (!upsample_above) strength = eval_edge_filt_s_lbd(cfg, d);
      }
    }
    const uint8_t *src = edge;
    if (n_top_px == txwpx && n_topright_px >= txhpx) {
      if (upsample_above | strength) {
        src = above_ref;
      } else {
        copy_above_row_lbd(edge, above_ref, blk_wh);
      }
    } else {
      const int n_top_copy = AOMMIN(n_top_px + n_topright_px, blk_wh);
      if (n_top_copy < blk_wh) {
        copy_and_pad_above_lbd(edge, above_ref, n_top_copy, blk_wh);
      } else {
        copy_above_row_lbd(edge, above_ref, blk_wh);
      }
    }
    if (upsample_above) {
      upsample_intra_edge_from_lbd(edge, src, corner, blk_wh);
    } else if (strength) {
      filter_intra_edge_from_lbd(edge, src, corner, n_top_px + txhpx, strength);
    }
    memset(edge + (blk_wh << upsample_above), 0, (blk_wh == 128) ? 16 : 32);
    av1_dr_prediction_z1(dst, dst_stride, txwpx, txhpx, edge, NULL,
                         upsample_above, dr_intra_derivative[p_angle], 1);
  } else if (LIKELY(p_angle > 180)) {
    if (UNLIKELY(n_left_px == 0)) {
      fill_val = (n_top_px > 0) ? above_ref[0] : 129;
      goto fill_solid;
    }
    assert(n_bottomleft_px >= 0);
    const int blk_wh = txhpx + txwpx;
    const uint8_t corner = (n_top_px > 0) ? above_ref[-1] : left_ref[0];
    const int n_left_copy = AOMMIN(n_left_px + n_bottomleft_px, blk_wh);
    if (n_left_copy < blk_wh) {
      copy_and_pad_left_lbd(edge, left_ref, ref_stride, n_left_copy, blk_wh);
    } else {
      copy_left_col_lbd(edge, left_ref, ref_stride, blk_wh);
    }
    int upsample_left = 0;
    if (LIKELY(!disable_edge_filter)) {
      const int d = p_angle - 180;
      int strength = 0;
      if (blk_wh > 32) {
        strength = 3;
      } else if (LIKELY((blk_wh <= 16) & !intra_edge_filter_type)) {
        upsample_left = (d < 40);
        strength = (d >= ((blk_wh == 8) ? 56 : 40));
      } else {
        const uint32_t cfg =
            intra_edge_filter_cfg_lbd(blk_wh, intra_edge_filter_type);
        upsample_left = (int)(cfg >> 15) & (d < 40);
        if (!upsample_left) strength = eval_edge_filt_s_lbd(cfg, d);
      }
      if (upsample_left) {
        upsample_intra_edge_from_lbd(edge, edge, corner, blk_wh);
      } else if (strength) {
        filter_intra_edge_from_lbd(edge, edge, corner, n_left_px + txwpx,
                                   strength);
      }
    }
    memset(edge + (blk_wh << upsample_left), 0, (blk_wh == 128) ? 16 : 32);
    av1_dr_prediction_z3(dst, dst_stride, txwpx, txhpx, NULL, edge,
                         upsample_left, 1, dr_intra_derivative[270 - p_angle]);
  } else if (p_angle == 90) {
    if (n_top_px == 0) {
      fill_val = (n_left_px > 0) ? left_ref[0] : 127;
      goto fill_solid;
    }
    assert(n_top_px < txwpx);
    copy_and_pad_above_lbd(edge, above_ref, n_top_px, txwpx);
    pred[V_PRED][tx_size](dst, dst_stride, edge, NULL);
  } else if (p_angle == 180) {
    if (n_left_px == 0) {
      fill_val = (n_top_px > 0) ? above_ref[0] : 129;
      goto fill_solid;
    }
    assert(n_left_px < txhpx);
    copy_and_pad_left_lbd(edge, left_ref, ref_stride, n_left_px, txhpx);
    pred[H_PRED][tx_size](dst, dst_stride, NULL, edge);
  } else {
    build_dr_z2_lbd(ref, ref_stride, dst, dst_stride, angle_tx_flags,
                    (uint32_t)n_top_px | ((uint32_t)n_left_px << 16));
  }
  return;

fill_solid:
  fill_block_lbd(dst, dst_stride, fill_val, txwpx, txhpx);
}

#if RECONINTRA_HAVE_SSE2
static AOM_FORCE_INLINE uint32_t sum_sad64_pair_lbd(__m128i s) {
  return (uint32_t)_mm_cvtsi128_si32(_mm_add_epi32(s, _mm_shuffle_epi32(s, 2)));
}

static AOM_FORCE_INLINE uint32_t
sum_above_row_le32_lbd(const uint8_t *above_ref, int txwpx) {
  const __m128i zero = _mm_setzero_si128();
  if (txwpx == 4) {
    uint32_t v;
    memcpy(&v, above_ref, 4);
    return (uint32_t)_mm_cvtsi128_si32(
        _mm_sad_epu8(_mm_cvtsi32_si128((int)v), zero));
  }
  if (txwpx == 8) {
    return (uint32_t)_mm_cvtsi128_si32(
        _mm_sad_epu8(_mm_loadl_epi64((const __m128i *)above_ref), zero));
  }
  if (LIKELY(txwpx == 16)) {
    const __m128i v = _mm_loadu_si128((const __m128i *)above_ref);
    return sum_sad64_pair_lbd(_mm_sad_epu8(v, zero));
  }
  const __m128i v0 = _mm_loadu_si128((const __m128i *)above_ref);
  const __m128i v1 = _mm_loadu_si128((const __m128i *)(above_ref + 16));
  return sum_sad64_pair_lbd(
      _mm_add_epi64(_mm_sad_epu8(v0, zero), _mm_sad_epu8(v1, zero)));
}
#endif

static AOM_FORCE_INLINE uint32_t sum_above_row_lbd(const uint8_t *above_ref,
                                                   int txwpx) {
#if RECONINTRA_HAVE_SSE2
  if (LIKELY(txwpx <= 32)) return sum_above_row_le32_lbd(above_ref, txwpx);
  const __m128i zero = _mm_setzero_si128();
  const __m128i v0 = _mm_loadu_si128((const __m128i *)above_ref);
  const __m128i v1 = _mm_loadu_si128((const __m128i *)(above_ref + 16));
  const __m128i v2 = _mm_loadu_si128((const __m128i *)(above_ref + 32));
  const __m128i v3 = _mm_loadu_si128((const __m128i *)(above_ref + 48));
  const __m128i s01 =
      _mm_add_epi64(_mm_sad_epu8(v0, zero), _mm_sad_epu8(v1, zero));
  const __m128i s23 =
      _mm_add_epi64(_mm_sad_epu8(v2, zero), _mm_sad_epu8(v3, zero));
  return sum_sad64_pair_lbd(_mm_add_epi64(s01, s23));
#else
  uint32_t sum = 0;
  for (int i = 0; i < txwpx; ++i) sum += above_ref[i];
  return sum;
#endif
}

static AOM_FORCE_INLINE uint32_t sum_left_col_lbd(const uint8_t *r0,
                                                  ptrdiff_t s, int txhpx) {
  const uint8_t *r2 = r0 + s * 2;
  uint32_t sum0 = (uint32_t)r0[0] + (uint32_t)r0[s];
  uint32_t sum1 = (uint32_t)r2[0] + (uint32_t)r2[s];
  if (txhpx > 4) {
    const ptrdiff_t s4 = s * 4;
    RECONINTRA_SCALAR_LOOP
    do {
      r0 += s4;
      r2 += s4;
      sum0 += (uint32_t)r0[0] + (uint32_t)r0[s];
      sum1 += (uint32_t)r2[0] + (uint32_t)r2[s];
      txhpx -= 4;
    } while (txhpx > 4);
  }
  return sum0 + sum1;
}

static const uint8_t dc_shift1_tab[TX_SIZES_ALL] = {
  2, 3, 4, 5, 6,           // 4x4, 8x8, 16x16, 32x32, 64x64
  2, 2, 3, 3, 4, 4, 5, 5,  // 4x8..64x32
  2, 2, 3, 3, 4, 4         // 4x16..64x16
};
static const uint16_t dc_mult_tab[TX_SIZES_ALL] = {
  0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x5556, 0x5556,
  0x5556, 0x5556, 0x5556, 0x5556, 0x5556, 0x5556, 0x3334,
  0x3334, 0x3334, 0x3334, 0x3334, 0x3334
};

RECONINTRA_NOINLINE static uint8_t compute_dc_border_val_lbd(
    const uint8_t *ref, ptrdiff_t ref_stride, TX_SIZE tx_size, int txwpx,
    int txhpx, int n_top_px, int n_left_px) {
  assert(tx_size < TX_SIZES_ALL);
  assert(n_top_px >= 0 && n_top_px <= txwpx);
  assert(n_left_px >= 0 && n_left_px <= txhpx);
  uint32_t sum = 0;
  if (n_top_px > 0) {
    const uint8_t *above_ref = ref - ref_stride;
    if (LIKELY(n_top_px == txwpx)) {
      sum = sum_above_row_lbd(above_ref, txwpx);
    } else {
      RECONINTRA_SCALAR_LOOP
      for (int i = 0; i < n_top_px; ++i) sum += above_ref[i];
      sum += (uint32_t)above_ref[n_top_px - 1] * (uint32_t)(txwpx - n_top_px);
    }
  }
  if (n_left_px > 0) {
    const uint8_t *left_ref = ref - 1;
    if (LIKELY(n_left_px == txhpx)) {
      sum += sum_left_col_lbd(left_ref, ref_stride, txhpx);
    } else {
      const uint8_t *p = left_ref;
      RECONINTRA_SCALAR_LOOP
      for (int i = 0; i < n_left_px; ++i) {
        sum += *p;
        p += ref_stride;
      }
      sum += (uint32_t)p[-ref_stride] * (uint32_t)(txhpx - n_left_px);
    }
    if (n_top_px > 0) {
      const uint32_t total = sum + (uint32_t)((txwpx + txhpx) >> 1);
      sum = ((total >> dc_shift1_tab[tx_size]) * dc_mult_tab[tx_size]) >> 16;
    } else {
      sum = (sum + (uint32_t)(txhpx >> 1)) >> tx_size_high_log2[tx_size];
    }
  } else if (n_top_px > 0) {
    sum = (sum + (uint32_t)(txwpx >> 1)) >> tx_size_wide_log2[tx_size];
  } else {
    sum = 128;
  }
  return (uint8_t)sum;
}

// tx_top_left: bits [7:0] = tx_size, bits [15:8] = n_top_px,
//   bits [31:16] = n_left_px.
RECONINTRA_NOINLINE static void predict_dc_block_edge_lbd(
    const uint8_t *ref, ptrdiff_t ref_stride, uint8_t *dst,
    ptrdiff_t dst_stride, uint32_t tx_top_left) {
  assert(dst != NULL);
  const TX_SIZE tx_size = (TX_SIZE)(tx_top_left & 0xff);
  const int n_top_px = (int)((tx_top_left >> 8) & 0xff);
  const int n_left_px = (int)(tx_top_left >> 16);
  assert(tx_size < TX_SIZES_ALL);
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  const uint8_t dc_val = compute_dc_border_val_lbd(
      ref, ref_stride, tx_size, txwpx, txhpx, n_top_px, n_left_px);
  fill_block_lbd(dst, dst_stride, dc_val, txwpx, txhpx);
}

#if (!CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER) && HAVE_SSSE3 && \
    RECONINTRA_HAVE_SSE2
static AOM_FORCE_INLINE uint8_t compute_dc_val_cfl_lbd(
    const uint8_t *ref, ptrdiff_t ref_stride, TX_SIZE tx_size, int txwpx,
    int txhpx, int n_top_px, int n_left_px) {
  assert(tx_size < TX_SIZES_ALL);
  assert(txwpx <= 32 && txhpx <= 32);
  if (LIKELY(n_top_px == txwpx && n_left_px == txhpx)) {
    const uint32_t sum_top = sum_above_row_le32_lbd(ref - ref_stride, txwpx);
    const uint32_t sum_left = sum_left_col_lbd(ref - 1, ref_stride, txhpx);
    const uint32_t total =
        sum_top + sum_left + (uint32_t)((txwpx + txhpx) >> 1);
    return (
        uint8_t)(((total >> dc_shift1_tab[tx_size]) * dc_mult_tab[tx_size]) >>
                 16);
  }
  return compute_dc_border_val_lbd(ref, ref_stride, tx_size, txwpx, txhpx,
                                   n_top_px, n_left_px);
}
#endif

static AOM_FORCE_INLINE uint8_t
compute_dc_val_interior_lbd(const uint8_t *ref, ptrdiff_t ref_stride,
                            TX_SIZE tx_size, int txwpx, int txhpx) {
  assert(tx_size < TX_SIZES_ALL);
  const uint32_t sum_top = sum_above_row_lbd(ref - ref_stride, txwpx);
  const uint32_t sum_left = sum_left_col_lbd(ref - 1, ref_stride, txhpx);
  const uint32_t total = sum_top + sum_left + (uint32_t)((txwpx + txhpx) >> 1);
  return (uint8_t)(((total >> dc_shift1_tab[tx_size]) * dc_mult_tab[tx_size]) >>
                   16);
}

static AOM_FORCE_INLINE void predict_dc_block_lbd(
    const uint8_t *ref, ptrdiff_t ref_stride, uint8_t *dst,
    ptrdiff_t dst_stride, TX_SIZE tx_size, int txwpx, int txhpx, int n_top_px,
    int n_left_px) {
  if (LIKELY(n_top_px == txwpx && n_left_px == txhpx)) {
    const uint8_t dc_val =
        compute_dc_val_interior_lbd(ref, ref_stride, tx_size, txwpx, txhpx);
    fill_block_lbd(dst, dst_stride, dc_val, txwpx, txhpx);
    return;
  }
  assert(tx_size < TX_SIZES_ALL);
  assert(n_top_px >= 0 && n_top_px <= 255);
  assert(n_left_px >= 0 && n_left_px <= 255);
  predict_dc_block_edge_lbd(ref, ref_stride, dst, dst_stride,
                            (uint32_t)tx_size | ((uint32_t)n_top_px << 8) |
                                ((uint32_t)n_left_px << 16));
}

void av1_predict_dc_block_lbd_c(const uint8_t *ref, int ref_stride,
                                uint8_t *dst, int dst_stride, TX_SIZE tx_size,
                                int txwpx, int txhpx, int n_top_px,
                                int n_left_px) {
  predict_dc_block_lbd(ref, ref_stride, dst, dst_stride, tx_size, txwpx, txhpx,
                       n_top_px, n_left_px);
}

static AOM_FORCE_INLINE void v_pred_direct_lbd(uint8_t *dst,
                                               ptrdiff_t dst_stride,
                                               const uint8_t *above_ref,
                                               int txwpx, int txhpx) {
  const ptrdiff_t s2 = dst_stride * 2;
  if (txwpx == 4) {
    uint32_t v;
    memcpy(&v, above_ref, 4);
    RECONINTRA_SCALAR_LOOP
    do {
      memcpy(dst, &v, 4);
      memcpy(dst + dst_stride, &v, 4);
      dst += s2;
      memcpy(dst, &v, 4);
      memcpy(dst + dst_stride, &v, 4);
      dst += s2;
      txhpx -= 4;
    } while (txhpx > 0);
  } else if (txwpx == 8) {
    uint64_t v;
    memcpy(&v, above_ref, 8);
    RECONINTRA_SCALAR_LOOP
    do {
      memcpy(dst, &v, 8);
      memcpy(dst + dst_stride, &v, 8);
      dst += s2;
      memcpy(dst, &v, 8);
      memcpy(dst + dst_stride, &v, 8);
      dst += s2;
      txhpx -= 4;
    } while (txhpx > 0);
  } else {
#if RECONINTRA_HAVE_SSE2
    if (LIKELY(txwpx == 16)) {
      const __m128i v = _mm_loadu_si128((const __m128i *)above_ref);
      RECONINTRA_SCALAR_LOOP
      do {
        _mm_storeu_si128((__m128i *)dst, v);
        _mm_storeu_si128((__m128i *)(dst + dst_stride), v);
        dst += s2;
        _mm_storeu_si128((__m128i *)dst, v);
        _mm_storeu_si128((__m128i *)(dst + dst_stride), v);
        dst += s2;
        txhpx -= 4;
      } while (txhpx > 0);
    } else if (LIKELY(txwpx == 32)) {
      const __m128i v0 = _mm_loadu_si128((const __m128i *)above_ref);
      const __m128i v1 = _mm_loadu_si128((const __m128i *)(above_ref + 16));
      RECONINTRA_SCALAR_LOOP
      do {
        _mm_storeu_si128((__m128i *)dst, v0);
        _mm_storeu_si128((__m128i *)(dst + 16), v1);
        _mm_storeu_si128((__m128i *)(dst + dst_stride), v0);
        _mm_storeu_si128((__m128i *)(dst + dst_stride + 16), v1);
        dst += s2;
        txhpx -= 2;
      } while (txhpx > 0);
    } else {
      const __m128i v0 = _mm_loadu_si128((const __m128i *)above_ref);
      const __m128i v1 = _mm_loadu_si128((const __m128i *)(above_ref + 16));
      const __m128i v2 = _mm_loadu_si128((const __m128i *)(above_ref + 32));
      const __m128i v3 = _mm_loadu_si128((const __m128i *)(above_ref + 48));
      RECONINTRA_SCALAR_LOOP
      do {
        _mm_storeu_si128((__m128i *)dst, v0);
        _mm_storeu_si128((__m128i *)(dst + 16), v1);
        _mm_storeu_si128((__m128i *)(dst + 32), v2);
        _mm_storeu_si128((__m128i *)(dst + 48), v3);
        dst += dst_stride;
        --txhpx;
      } while (txhpx > 0);
    }
#else
    do {
      memcpy(dst, above_ref, txwpx);
      dst += dst_stride;
      --txhpx;
    } while (txhpx > 0);
#endif
  }
}

static AOM_FORCE_INLINE void h_pred_direct_lbd(uint8_t *dst,
                                               ptrdiff_t dst_stride,
                                               const uint8_t *r0, ptrdiff_t rs,
                                               int txwpx, int txhpx) {
  const ptrdiff_t ds2 = dst_stride * 2;
  const ptrdiff_t rs2 = rs * 2;
  if (txwpx == 4) {
    RECONINTRA_SCALAR_LOOP
    do {
      const uint32_t v0 = (uint32_t)r0[0] * 0x01010101u;
      const uint32_t v1 = (uint32_t)r0[rs] * 0x01010101u;
      r0 += rs2;
      const uint32_t v2 = (uint32_t)r0[0] * 0x01010101u;
      const uint32_t v3 = (uint32_t)r0[rs] * 0x01010101u;
      r0 += rs2;
      memcpy(dst, &v0, 4);
      memcpy(dst + dst_stride, &v1, 4);
      dst += ds2;
      memcpy(dst, &v2, 4);
      memcpy(dst + dst_stride, &v3, 4);
      dst += ds2;
      txhpx -= 4;
    } while (txhpx > 0);
  } else if (txwpx == 8) {
    RECONINTRA_SCALAR_LOOP
    do {
      const uint64_t v0 = (uint64_t)r0[0] * 0x0101010101010101ULL;
      const uint64_t v1 = (uint64_t)r0[rs] * 0x0101010101010101ULL;
      r0 += rs2;
      const uint64_t v2 = (uint64_t)r0[0] * 0x0101010101010101ULL;
      const uint64_t v3 = (uint64_t)r0[rs] * 0x0101010101010101ULL;
      r0 += rs2;
      memcpy(dst, &v0, 8);
      memcpy(dst + dst_stride, &v1, 8);
      dst += ds2;
      memcpy(dst, &v2, 8);
      memcpy(dst + dst_stride, &v3, 8);
      dst += ds2;
      txhpx -= 4;
    } while (txhpx > 0);
  } else {
#if RECONINTRA_HAVE_SSE2
    if (LIKELY(txwpx == 16)) {
      RECONINTRA_SCALAR_LOOP
      do {
        const __m128i v0 = sse2_bcast8_lbd(r0[0]);
        const __m128i v1 = sse2_bcast8_lbd(r0[rs]);
        r0 += rs2;
        _mm_storeu_si128((__m128i *)dst, v0);
        _mm_storeu_si128((__m128i *)(dst + dst_stride), v1);
        dst += ds2;
        txhpx -= 2;
      } while (txhpx > 0);
    } else if (LIKELY(txwpx == 32)) {
      RECONINTRA_SCALAR_LOOP
      do {
        const uint32_t p01 = (uint32_t)r0[0] | ((uint32_t)r0[rs] << 8);
        r0 += rs2;
        __m128i x = _mm_cvtsi32_si128((int)p01);
        x = _mm_unpacklo_epi8(x, x);
        x = _mm_unpacklo_epi16(x, x);
        const __m128i v0 = _mm_shuffle_epi32(x, 0x00);
        const __m128i v1 = _mm_shuffle_epi32(x, 0x55);
        _mm_storeu_si128((__m128i *)dst, v0);
        _mm_storeu_si128((__m128i *)(dst + 16), v0);
        _mm_storeu_si128((__m128i *)(dst + dst_stride), v1);
        _mm_storeu_si128((__m128i *)(dst + dst_stride + 16), v1);
        dst += ds2;
        txhpx -= 2;
      } while (txhpx > 0);
    } else {
      RECONINTRA_SCALAR_LOOP
      do {
        const uint32_t p01 = (uint32_t)r0[0] | ((uint32_t)r0[rs] << 8);
        r0 += rs2;
        __m128i x = _mm_cvtsi32_si128((int)p01);
        x = _mm_unpacklo_epi8(x, x);
        x = _mm_unpacklo_epi16(x, x);
        const __m128i v0 = _mm_shuffle_epi32(x, 0x00);
        const __m128i v1 = _mm_shuffle_epi32(x, 0x55);
        _mm_storeu_si128((__m128i *)dst, v0);
        _mm_storeu_si128((__m128i *)(dst + 16), v0);
        _mm_storeu_si128((__m128i *)(dst + 32), v0);
        _mm_storeu_si128((__m128i *)(dst + 48), v0);
        uint8_t *d1 = dst + dst_stride;
        _mm_storeu_si128((__m128i *)d1, v1);
        _mm_storeu_si128((__m128i *)(d1 + 16), v1);
        _mm_storeu_si128((__m128i *)(d1 + 32), v1);
        _mm_storeu_si128((__m128i *)(d1 + 48), v1);
        dst += ds2;
        txhpx -= 2;
      } while (txhpx > 0);
    }
#else
    do {
      memset(dst, *r0, txwpx);
      dst += dst_stride;
      r0 += rs;
      --txhpx;
    } while (txhpx > 0);
#endif
  }
}

// This function generates the pred data of a given block for non-directional
// intra prediction modes (i.e., DC, SMOOTH, SMOOTH_H, SMOOTH_V and PAETH).
static void build_non_directional_intra_predictors(const uint8_t *ref,
                                                   int ref_stride, uint8_t *dst,
                                                   int dst_stride,
                                                   uint32_t mode_tx,
                                                   uint32_t top_left_px) {
  const PREDICTION_MODE mode = (PREDICTION_MODE)(mode_tx & 0xff);
  const TX_SIZE tx_size = (TX_SIZE)(mode_tx >> 8);
  const int n_top_px = (int)(top_left_px & 0xffff);
  const int n_left_px = (int)(top_left_px >> 16);
  const uint8_t *above_ref = ref - ref_stride;
  const uint8_t *left_ref = ref - 1;
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  assert(n_top_px >= 0);
  assert(n_left_px >= 0);
  assert(mode == DC_PRED || mode == SMOOTH_PRED || mode == SMOOTH_V_PRED ||
         mode == SMOOTH_H_PRED || mode == PAETH_PRED);

  DECLARE_ALIGNED(16, uint8_t, left_data[NUM_INTRA_NEIGHBOUR_PIXELS]);
  DECLARE_ALIGNED(16, uint8_t, above_data[NUM_INTRA_NEIGHBOUR_PIXELS]);
  uint8_t *const above_row = above_data + 16;
  uint8_t *const left_col = left_data + 16;

  if (mode == SMOOTH_V_PRED) {
    left_col[txhpx - 1] = (n_left_px > 0)
                              ? left_ref[(n_left_px - 1) * ref_stride]
                              : ((n_top_px > 0) ? above_ref[0] : 129);
    if (LIKELY(n_top_px == txwpx)) {
      pred[SMOOTH_V_PRED][tx_size](dst, dst_stride, above_ref, left_col);
      return;
    }
    if (n_top_px > 0) {
      copy_and_pad_above_lbd(above_row, above_ref, n_top_px, txwpx);
    } else {
      const uint8_t val = (n_left_px > 0) ? left_ref[0] : 127;
      fill_intra_edge_lbd(above_row, val, txwpx);
    }
    pred[SMOOTH_V_PRED][tx_size](dst, dst_stride, above_row, left_col);
    return;
  }

  if (LIKELY(n_left_px == txhpx)) {
    copy_left_col_lbd(left_col, left_ref, ref_stride, txhpx);
  } else if (n_left_px > 0) {
    copy_and_pad_left_lbd(left_col, left_ref, ref_stride, n_left_px, txhpx);
  } else {
    const uint8_t val = (n_top_px > 0) ? above_ref[0] : 129;
    fill_intra_edge_lbd(left_col, val, txhpx);
  }

  if (mode == SMOOTH_H_PRED) {
    if (LIKELY(n_top_px == txwpx)) {
      pred[SMOOTH_H_PRED][tx_size](dst, dst_stride, above_ref, left_col);
      return;
    }
    above_row[txwpx - 1] = (n_top_px > 0)
                               ? above_ref[n_top_px - 1]
                               : ((n_left_px > 0) ? left_ref[0] : 127);
    pred[SMOOTH_H_PRED][tx_size](dst, dst_stride, above_row, left_col);
    return;
  }
  if (mode == SMOOTH_PRED && LIKELY(n_top_px == txwpx)) {
    pred[SMOOTH_PRED][tx_size](dst, dst_stride, above_ref, left_col);
    return;
  }
  if (mode == PAETH_PRED && LIKELY(n_top_px == txwpx && n_left_px > 0) &&
      ((((uintptr_t)above_ref & 15) == 0) || txwpx == 8 ||
       (txwpx == 4 && (((uintptr_t)above_ref & 3) == 0))
#if HAVE_AVX2
       ||
       (txwpx >= 16 && pred[PAETH_PRED][TX_8X8] == aom_paeth_predictor_8x8_avx2)
#endif
           )) {
    pred[PAETH_PRED][tx_size](dst, dst_stride, above_ref, left_col);
    return;
  }

  if (LIKELY(n_top_px == txwpx)) {
    copy_above_row_lbd(above_row, above_ref, txwpx);
  } else if (n_top_px > 0) {
    copy_and_pad_above_lbd(above_row, above_ref, n_top_px, txwpx);
  } else {
    const uint8_t val = (n_left_px > 0) ? left_ref[0] : 127;
    fill_intra_edge_lbd(above_row, val, txwpx);
  }

  if (mode == PAETH_PRED) {
    if (txhpx == 4) memset(left_col + 4, 0, 4);
    if (txwpx == 4) memset(above_row + 4, 0, 4);
    if (n_top_px > 0 && n_left_px > 0) {
      above_row[-1] = above_ref[-1];
    } else if (n_top_px > 0) {
      above_row[-1] = above_ref[0];
    } else if (n_left_px > 0) {
      above_row[-1] = left_ref[0];
    } else {
      above_row[-1] = 128;
    }
    left_col[-1] = above_row[-1];
  }

  if (mode == DC_PRED) {
    dc_pred[n_left_px > 0][n_top_px > 0][tx_size](dst, dst_stride, above_row,
                                                  left_col);
  } else {
    pred[mode][tx_size](dst, dst_stride, above_row, left_col);
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
void av1_highbd_filter_intra_edge_c(uint16_t *p, int sz, int strength) {
  if (!strength) return;

  const int kernel[INTRA_EDGE_FILT][INTRA_EDGE_TAPS] = { { 0, 4, 8, 4, 0 },
                                                         { 0, 5, 6, 5, 0 },
                                                         { 2, 4, 4, 4, 2 } };
  const int filt = strength - 1;
  uint16_t edge[129];

  memcpy(edge, p, sz * sizeof(*p));
  for (int i = 1; i < sz; i++) {
    int s = 0;
    for (int j = 0; j < INTRA_EDGE_TAPS; j++) {
      int k = i - 2 + j;
      k = (k < 0) ? 0 : k;
      k = (k > sz - 1) ? sz - 1 : k;
      s += edge[k] * kernel[filt][j];
    }
    s = (s + 8) >> 4;
    p[i] = s;
  }
}

static AOM_FORCE_INLINE void highbd_filter_intra_edge_corner(uint16_t *p_above,
                                                             uint16_t *p_left) {
  const int kernel[3] = { 5, 6, 5 };

  int s = (p_left[0] * kernel[0]) + (p_above[-1] * kernel[1]) +
          (p_above[0] * kernel[2]);
  s = (s + 8) >> 4;
  p_above[-1] = s;
  p_left[-1] = s;
}

void av1_highbd_upsample_intra_edge_c(uint16_t *p, int sz, int bd) {
  // interpolate half-sample positions
  assert(sz <= MAX_UPSAMPLE_SZ);

  uint16_t in[MAX_UPSAMPLE_SZ + 3];
  // copy p[-1..(sz-1)] and extend first and last samples
  in[0] = p[-1];
  in[1] = p[-1];
  for (int i = 0; i < sz; i++) {
    in[i + 2] = p[i];
  }
  in[sz + 2] = p[sz - 1];

  // interpolate half-sample edge positions
  p[-2] = in[0];
  for (int i = 0; i < sz; i++) {
    int s = -in[i] + (9 * in[i + 1]) + (9 * in[i + 2]) - in[i + 3];
    s = (s + 8) >> 4;
    s = clip_pixel_highbd(s, bd);
    p[2 * i - 1] = s;
    p[2 * i] = in[i + 2];
  }
}

static AOM_FORCE_INLINE void copy_above_row_hbd(uint16_t *above_row,
                                                const uint16_t *above_ref,
                                                int n_copy) {
  if (n_copy == 4) {
    memcpy(above_row, above_ref, 4 * sizeof(uint16_t));
  } else if (n_copy == 8) {
    memcpy(above_row, above_ref, 8 * sizeof(uint16_t));
  } else if (n_copy == 16) {
    memcpy(above_row, above_ref, 16 * sizeof(uint16_t));
  } else if (n_copy == 32) {
    memcpy(above_row, above_ref, 32 * sizeof(uint16_t));
  } else if (n_copy == 64) {
    memcpy(above_row, above_ref, 64 * sizeof(uint16_t));
  } else if (n_copy == 128) {
    memcpy(above_row, above_ref, 128 * sizeof(uint16_t));
  } else {
    memcpy(above_row, above_ref, n_copy * sizeof(uint16_t));
  }
}

static void highbd_build_directional_and_filter_intra_predictors(
    const uint8_t *ref8, int ref_stride, uint8_t *dst8, int dst_stride,
    PREDICTION_MODE mode, int p_angle, FILTER_INTRA_MODE filter_intra_mode,
    TX_SIZE tx_size, int disable_edge_filter, int n_top_px, int n_topright_px,
    int n_left_px, int n_bottomleft_px, int intra_edge_filter_type,
    int bit_depth) {
  int i;
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  const uint16_t *const ref = CONVERT_TO_SHORTPTR(ref8);
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  const uint16_t *above_ref = ref - ref_stride;
  const uint16_t *left_ref = ref - 1;
  const int is_dr_mode = av1_is_directional_mode(mode);
  const int use_filter_intra = filter_intra_mode != FILTER_INTRA_MODES;
  assert(use_filter_intra || is_dr_mode);
  (void)is_dr_mode;
  const int base = 128 << (bit_depth - 8);
  const int need_above = use_filter_intra || (p_angle < 180);
  const int need_left = use_filter_intra || (p_angle > 90);
  const int need_above_left = 1;

  assert(n_top_px >= 0);
  assert(n_topright_px >= -1);
  assert(n_left_px >= 0);
  assert(n_bottomleft_px >= -1);

  if ((!need_above && n_left_px == 0) || (!need_left && n_top_px == 0)) {
    int val;
    if (need_left) {
      val = (n_top_px > 0) ? above_ref[0] : base + 1;
    } else {
      val = (n_left_px > 0) ? left_ref[0] : base - 1;
    }
    for (i = 0; i < txhpx; ++i) {
      aom_memset16(dst, val, txwpx);
      dst += dst_stride;
    }
    return;
  }

  DECLARE_ALIGNED(16, uint16_t, left_data[NUM_INTRA_NEIGHBOUR_PIXELS]);
  DECLARE_ALIGNED(16, uint16_t, above_data[NUM_INTRA_NEIGHBOUR_PIXELS]);
  uint16_t *const above_row = above_data + 16;
  uint16_t *const left_col = left_data + 16;

  // NEED_LEFT
  if (need_left) {
    const int num_left_pixels_needed =
        txhpx + (n_bottomleft_px >= 0 ? txwpx : 0);
    if (n_left_px > 0) {
      const int n_left_copy =
          n_left_px + (n_bottomleft_px > 0 ? n_bottomleft_px : 0);
      assert(n_left_copy > 0);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC novector
#endif
      for (i = 0; i < n_left_copy; i++) left_col[i] = left_ref[i * ref_stride];
      if (i < num_left_pixels_needed)
        aom_memset16(&left_col[i], left_col[i - 1], num_left_pixels_needed - i);
    } else {
      const int val = (n_top_px > 0) ? above_ref[0] : base + 1;
      aom_memset16(left_col, val, num_left_pixels_needed);
    }
  }

  // NEED_ABOVE
  if (need_above) {
    const int num_top_pixels_needed = txwpx + (n_topright_px >= 0 ? txhpx : 0);
    if (n_top_px > 0) {
      const int n_top_copy = n_top_px + (n_topright_px > 0 ? n_topright_px : 0);
      copy_above_row_hbd(above_row, above_ref, n_top_copy);
      if (n_top_copy < num_top_pixels_needed)
        aom_memset16(&above_row[n_top_copy], above_row[n_top_copy - 1],
                     num_top_pixels_needed - n_top_copy);
    } else {
      const int val = (n_left_px > 0) ? left_ref[0] : base - 1;
      aom_memset16(above_row, val, num_top_pixels_needed);
    }
  }

  if (!use_filter_intra) {
    if (p_angle == 90) {
      pred_high[V_PRED][tx_size](dst, dst_stride, above_row, left_col,
                                 bit_depth);
      return;
    }
    if (p_angle == 180) {
      pred_high[H_PRED][tx_size](dst, dst_stride, above_row, left_col,
                                 bit_depth);
      return;
    }
  }

  memset(above_data, 0, 16 * sizeof(above_data[0]));
  memset(left_data, 0, 16 * sizeof(left_data[0]));
  memset(above_row + txwpx + txhpx, 0, 16 * sizeof(above_row[0]));
  memset(left_col + txwpx + txhpx, 0, 16 * sizeof(left_col[0]));

  if (need_above_left) {
    if (n_top_px > 0 && n_left_px > 0) {
      above_row[-1] = above_ref[-1];
    } else if (n_top_px > 0) {
      above_row[-1] = above_ref[0];
    } else if (n_left_px > 0) {
      above_row[-1] = left_ref[0];
    } else {
      above_row[-1] = base;
    }
    left_col[-1] = above_row[-1];
  }

  if (use_filter_intra) {
    highbd_filter_intra_predictor(dst, dst_stride, tx_size, above_row, left_col,
                                  filter_intra_mode, bit_depth);
    return;
  }

  assert(is_dr_mode);
  int upsample_above = 0;
  int upsample_left = 0;
  if (!disable_edge_filter) {
    const int need_right = p_angle < 90;
    const int need_bottom = p_angle > 180;
    const int ab_le = 1;
    if (need_above && need_left && (txwpx + txhpx >= 24)) {
      highbd_filter_intra_edge_corner(above_row, left_col);
    }
    if (need_above) {
      if (n_top_px > 0) {
        const int strength = intra_edge_filter_strength(
            txwpx, txhpx, p_angle - 90, intra_edge_filter_type);
        if (strength) {
          const int n_px = n_top_px + ab_le + (need_right ? txhpx : 0);
          av1_highbd_filter_intra_edge(above_row - ab_le, n_px, strength);
        }
      }
      if (av1_use_intra_edge_upsample(txwpx, txhpx, p_angle - 90,
                                      intra_edge_filter_type)) {
        upsample_above = 1;
        const int n_px = txwpx + (need_right ? txhpx : 0);
        av1_highbd_upsample_intra_edge(above_row, n_px, bit_depth);
      }
    }
    if (need_left) {
      if (n_left_px > 0) {
        const int strength = intra_edge_filter_strength(
            txhpx, txwpx, p_angle - 180, intra_edge_filter_type);
        if (strength) {
          const int n_px = n_left_px + ab_le + (need_bottom ? txwpx : 0);
          av1_highbd_filter_intra_edge(left_col - ab_le, n_px, strength);
        }
      }
      if (av1_use_intra_edge_upsample(txhpx, txwpx, p_angle - 180,
                                      intra_edge_filter_type)) {
        upsample_left = 1;
        const int n_px = txhpx + (need_bottom ? txwpx : 0);
        av1_highbd_upsample_intra_edge(left_col, n_px, bit_depth);
      }
    }
  }
  highbd_dr_predictor(dst, dst_stride, tx_size, above_row, left_col,
                      upsample_above, upsample_left, p_angle, bit_depth);
}

// For HBD encode/decode, this function generates the pred data of a given
// block for non-directional intra prediction modes (i.e., DC, SMOOTH, SMOOTH_H,
// SMOOTH_V and PAETH).
static void highbd_build_non_directional_intra_predictors(
    const uint8_t *ref8, int ref_stride, uint8_t *dst8, int dst_stride,
    PREDICTION_MODE mode, TX_SIZE tx_size, int n_top_px, int n_left_px,
    int bit_depth) {
  int i = 0;
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  const uint16_t *const ref = CONVERT_TO_SHORTPTR(ref8);
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  int need_left = extend_modes[mode] & NEED_LEFT;
  int need_above = extend_modes[mode] & NEED_ABOVE;
  int need_above_left = extend_modes[mode] & NEED_ABOVELEFT;
  const uint16_t *above_ref = ref - ref_stride;
  const uint16_t *left_ref = ref - 1;
  const int base = 128 << (bit_depth - 8);

  assert(n_top_px >= 0);
  assert(n_left_px >= 0);
  assert(mode == DC_PRED || mode == SMOOTH_PRED || mode == SMOOTH_V_PRED ||
         mode == SMOOTH_H_PRED || mode == PAETH_PRED);

  if ((!need_above && n_left_px == 0) || (!need_left && n_top_px == 0)) {
    int val = 0;
    if (need_left) {
      val = (n_top_px > 0) ? above_ref[0] : base + 1;
    } else {
      val = (n_left_px > 0) ? left_ref[0] : base - 1;
    }
    for (i = 0; i < txhpx; ++i) {
      aom_memset16(dst, val, txwpx);
      dst += dst_stride;
    }
    return;
  }

  DECLARE_ALIGNED(16, uint16_t, left_data[NUM_INTRA_NEIGHBOUR_PIXELS]);
  DECLARE_ALIGNED(16, uint16_t, above_data[NUM_INTRA_NEIGHBOUR_PIXELS]);
  uint16_t *const above_row = above_data + 16;
  uint16_t *const left_col = left_data + 16;

  if (need_left) {
    if (n_left_px > 0) {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC novector
#endif
      for (i = 0; i < n_left_px; i++) left_col[i] = left_ref[i * ref_stride];
      if (i < txhpx) aom_memset16(&left_col[i], left_col[i - 1], txhpx - i);
    } else {
      const int val = (n_top_px > 0) ? above_ref[0] : base + 1;
      aom_memset16(left_col, val, txhpx);
    }
  }

  if (need_above) {
    if (LIKELY(n_top_px == txwpx)) {
      copy_above_row_hbd(above_row, above_ref, txwpx);
    } else if (n_top_px > 0) {
      copy_above_row_hbd(above_row, above_ref, n_top_px);
      aom_memset16(&above_row[n_top_px], above_row[n_top_px - 1],
                   txwpx - n_top_px);
    } else {
      const int val = (n_left_px > 0) ? left_ref[0] : base - 1;
      aom_memset16(above_row, val, txwpx);
    }
  }

  if (need_above_left) {
    if (n_top_px > 0 && n_left_px > 0) {
      above_row[-1] = above_ref[-1];
    } else if (n_top_px > 0) {
      above_row[-1] = above_ref[0];
    } else if (n_left_px > 0) {
      above_row[-1] = left_ref[0];
    } else {
      above_row[-1] = base;
    }
    left_col[-1] = above_row[-1];
  }

  if (mode == DC_PRED) {
    dc_pred_high[n_left_px > 0][n_top_px > 0][tx_size](
        dst, dst_stride, above_row, left_col, bit_depth);
  } else {
    pred_high[mode][tx_size](dst, dst_stride, above_row, left_col, bit_depth);
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

static AOM_FORCE_INLINE BLOCK_SIZE scale_chroma_bsize(BLOCK_SIZE bsize,
                                                      int subsampling_x,
                                                      int subsampling_y) {
  assert(subsampling_x >= 0 && subsampling_x < 2);
  assert(subsampling_y >= 0 && subsampling_y < 2);
  BLOCK_SIZE bs = bsize;
  switch (bsize) {
    case BLOCK_4X4:
      if (subsampling_x == 1 && subsampling_y == 1)
        bs = BLOCK_8X8;
      else if (subsampling_x == 1)
        bs = BLOCK_8X4;
      else if (subsampling_y == 1)
        bs = BLOCK_4X8;
      break;
    case BLOCK_4X8:
      if (subsampling_x == 1 && subsampling_y == 1)
        bs = BLOCK_8X8;
      else if (subsampling_x == 1)
        bs = BLOCK_8X8;
      else if (subsampling_y == 1)
        bs = BLOCK_4X8;
      break;
    case BLOCK_8X4:
      if (subsampling_x == 1 && subsampling_y == 1)
        bs = BLOCK_8X8;
      else if (subsampling_x == 1)
        bs = BLOCK_8X4;
      else if (subsampling_y == 1)
        bs = BLOCK_8X8;
      break;
    case BLOCK_4X16:
      if (subsampling_x == 1 && subsampling_y == 1)
        bs = BLOCK_8X16;
      else if (subsampling_x == 1)
        bs = BLOCK_8X16;
      else if (subsampling_y == 1)
        bs = BLOCK_4X16;
      break;
    case BLOCK_16X4:
      if (subsampling_x == 1 && subsampling_y == 1)
        bs = BLOCK_16X8;
      else if (subsampling_x == 1)
        bs = BLOCK_16X4;
      else if (subsampling_y == 1)
        bs = BLOCK_16X8;
      break;
    default: break;
  }
  return bs;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void predict_palette_block(const MACROBLOCKD *xd, int wpx, int txwpx,
                                  int txhpx, int x, int y, uint8_t *dst,
                                  int dst_stride, int plane, int is_hbd) {
  int r, c;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const uint8_t *const map = xd->plane[plane != 0].color_index_map +
                             xd->color_index_map_offset[plane != 0];
  const uint16_t *const palette =
      mbmi->palette_mode_info.palette_colors + plane * PALETTE_MAX_SIZE;
  if (is_hbd) {
    uint16_t *dst16 = CONVERT_TO_SHORTPTR(dst);
    for (r = 0; r < txhpx; ++r) {
      for (c = 0; c < txwpx; ++c) {
        dst16[r * dst_stride + c] = palette[map[(r + y) * wpx + c + x]];
      }
    }
  } else {
    for (r = 0; r < txhpx; ++r) {
      for (c = 0; c < txwpx; ++c) {
        dst[r * dst_stride + c] = (uint8_t)palette[map[(r + y) * wpx + c + x]];
      }
    }
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void highbd_predict_intra_block_internal(
    const MACROBLOCKD *xd, BLOCK_SIZE sb_size, int enable_intra_edge_filter,
    int wpx, int hpx, TX_SIZE tx_size, PREDICTION_MODE mode, int angle_delta,
    FILTER_INTRA_MODE filter_intra_mode, const uint8_t *ref, int ref_stride,
    uint8_t *dst, int dst_stride, int col_off, int row_off, int plane) {
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  const int x = col_off << MI_SIZE_LOG2;
  const int y = row_off << MI_SIZE_LOG2;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const int ss_x = plane ? pd->subsampling_x : 0;
  const int ss_y = plane ? pd->subsampling_y : 0;
  const int have_top =
      row_off || (ss_y ? xd->chroma_up_available : xd->up_available);
  const int have_left =
      col_off || (ss_x ? xd->chroma_left_available : xd->left_available);
  const int xr = (xd->mb_to_right_edge >> (3 + ss_x)) + wpx - x - txwpx;
  const int yd = (xd->mb_to_bottom_edge >> (3 + ss_y)) + hpx - y - txhpx;
  const int use_filter_intra = filter_intra_mode != FILTER_INTRA_MODES;
  const int is_dr_mode = av1_is_directional_mode(mode);
  const int n_top_px = have_top ? clamp(xr + txwpx, 0, txwpx) : 0;
  const int n_left_px = have_left ? clamp(yd + txhpx, 0, txhpx) : 0;
  if (!use_filter_intra && !is_dr_mode) {
    highbd_build_non_directional_intra_predictors(ref, ref_stride, dst,
                                                  dst_stride, mode, tx_size,
                                                  n_top_px, n_left_px, xd->bd);
    return;
  }
  int p_angle = 0;
  int n_topright_px = -1;
  int n_bottomleft_px = -1;
  if (is_dr_mode) {
    p_angle = mode_to_angle_map[mode] + angle_delta;
    if (p_angle < 90) {
      n_topright_px = 0;
      if (have_top && xr > 0) {
        const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
        const int txw = tx_size_wide_unit[tx_size];
        if (mi_col + ((col_off + txw) << ss_x) < xd->tile.mi_col_end) {
          const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
          BLOCK_SIZE bsize = mbmi->bsize;
          if (ss_x || ss_y) bsize = scale_chroma_bsize(bsize, ss_x, ss_y);
          if (has_top_right(sb_size, bsize, mi_row, mi_col, 1, 1,
                            mbmi->partition, tx_size, row_off, col_off, ss_x,
                            ss_y)) {
            n_topright_px = AOMMIN(xr, txwpx);
          }
        }
      }
    } else if (p_angle > 180) {
      n_bottomleft_px = 0;
      if (have_left && yd > 0) {
        const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
        const int txh = tx_size_high_unit[tx_size];
        if (mi_row + ((row_off + txh) << ss_y) < xd->tile.mi_row_end) {
          const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
          BLOCK_SIZE bsize = mbmi->bsize;
          if (ss_x || ss_y) bsize = scale_chroma_bsize(bsize, ss_x, ss_y);
          if (has_bottom_left(sb_size, bsize, mi_row, mi_col, 1, 1,
                              mbmi->partition, tx_size, row_off, col_off, ss_x,
                              ss_y)) {
            n_bottomleft_px = AOMMIN(yd, txhpx);
          }
        }
      }
    }
  }
  const int disable_edge_filter = !enable_intra_edge_filter;
  const int intra_edge_filter_type =
      (!disable_edge_filter && !use_filter_intra && p_angle != 90 &&
       p_angle != 180)
          ? get_intra_edge_filter_type(xd, plane)
          : 0;
  highbd_build_directional_and_filter_intra_predictors(
      ref, ref_stride, dst, dst_stride, mode, p_angle, filter_intra_mode,
      tx_size, disable_edge_filter, n_top_px, n_topright_px, n_left_px,
      n_bottomleft_px, intra_edge_filter_type, xd->bd);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

static AOM_FORCE_INLINE void predict_intra_block_internal(
    const MACROBLOCKD *xd, BLOCK_SIZE sb_size, int enable_intra_edge_filter,
    int wpx, int hpx, TX_SIZE tx_size, PREDICTION_MODE mode, int angle_delta,
    int use_palette, FILTER_INTRA_MODE filter_intra_mode, const uint8_t *ref,
    int ref_stride, uint8_t *dst, int dst_stride, int col_off, int row_off,
    int plane) {
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  const int x = col_off << MI_SIZE_LOG2;
  const int y = row_off << MI_SIZE_LOG2;
  const int is_hbd = is_cur_buf_hbd(xd);

  assert(mode < INTRA_MODES);

  if (UNLIKELY(use_palette)) {
    predict_palette_block(xd, wpx, txwpx, txhpx, x, y, dst, dst_stride, plane,
                          is_hbd);
    return;
  }
#if CONFIG_AV1_HIGHBITDEPTH
  if (UNLIKELY(is_hbd)) {
    highbd_predict_intra_block_internal(xd, sb_size, enable_intra_edge_filter,
                                        wpx, hpx, tx_size, mode, angle_delta,
                                        filter_intra_mode, ref, ref_stride, dst,
                                        dst_stride, col_off, row_off, plane);
    return;
  }
#endif  // CONFIG_AV1_HIGHBITDEPTH

  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const int ss_x = plane ? pd->subsampling_x : 0;
  const int ss_y = plane ? pd->subsampling_y : 0;
  const int have_top =
      (row_off | (ss_y ? xd->chroma_up_available : xd->up_available)) != 0;
  const int have_left =
      (col_off | (ss_x ? xd->chroma_left_available : xd->left_available)) != 0;

  int n_top_px;
  int n_left_px;
  if (LIKELY((xd->mb_to_right_edge | xd->mb_to_bottom_edge) >= 0)) {
    n_top_px = have_top ? txwpx : 0;
    n_left_px = have_left ? txhpx : 0;
  } else {
    const int right_px = (xd->mb_to_right_edge >> (3 + ss_x)) + wpx - x;
    const int bottom_px = (xd->mb_to_bottom_edge >> (3 + ss_y)) + hpx - y;
    n_top_px = AOMMIN(AOMMAX(right_px, 0), txwpx) & -have_top;
    n_left_px = AOMMIN(AOMMAX(bottom_px, 0), txhpx) & -have_left;
  }
  if (UNLIKELY(filter_intra_mode != FILTER_INTRA_MODES)) {
    build_filter_intra_predictors(
        ref, ref_stride, dst, dst_stride,
        (uint32_t)filter_intra_mode | ((uint32_t)tx_size << 8),
        (uint32_t)n_top_px | ((uint32_t)n_left_px << 16));
    return;
  }
  if (!av1_is_directional_mode(mode)) {
    if (mode == DC_PRED) {
#if AOM_ARCH_X86 && !AOM_ARCH_X86_64
      av1_predict_dc_block_lbd(ref, ref_stride, dst, dst_stride, tx_size, txwpx,
                               txhpx, n_top_px, n_left_px);
#else
      predict_dc_block_lbd(ref, ref_stride, dst, dst_stride, tx_size, txwpx,
                           txhpx, n_top_px, n_left_px);
#endif
      return;
    }
    build_non_directional_intra_predictors(
        ref, ref_stride, dst, dst_stride,
        (uint32_t)mode | ((uint32_t)tx_size << 8),
        (uint32_t)n_top_px | ((uint32_t)n_left_px << 16));
    return;
  }

  const int p_angle = mode_to_angle_map[mode] + angle_delta;
  if (p_angle == 90) {
    if (LIKELY(n_top_px == txwpx)) {
      v_pred_direct_lbd(dst, dst_stride, ref - ref_stride, txwpx, txhpx);
      return;
    }
  } else if (p_angle == 180) {
    if (LIKELY(n_left_px == txhpx)) {
      h_pred_direct_lbd(dst, dst_stride, ref - 1, ref_stride, txwpx, txhpx);
      return;
    }
  }

  const MB_MODE_INFO *const mbmi = xd->mi[0];
  int n_topright_px = 0;
  int n_bottomleft_px = 0;
  if (p_angle < 90) {
    if (n_top_px > 0) {
      const int xr = (xd->mb_to_right_edge >> (3 + ss_x)) + wpx - x - txwpx;
      if (xr > 0) {
        const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
        const int txw = tx_size_wide_unit[tx_size];
        if (mi_col + ((col_off + txw) << ss_x) < xd->tile.mi_col_end) {
          const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
          BLOCK_SIZE bsize = mbmi->bsize;
          if (ss_x || ss_y) bsize = scale_chroma_bsize(bsize, ss_x, ss_y);
          if (has_top_right(sb_size, bsize, mi_row, mi_col, 1, 1,
                            mbmi->partition, tx_size, row_off, col_off, ss_x,
                            ss_y)) {
            n_topright_px = AOMMIN(xr, txwpx);
          }
        }
      }
    }
  } else if (p_angle > 180) {
    if (n_left_px > 0) {
      const int yd = (xd->mb_to_bottom_edge >> (3 + ss_y)) + hpx - y - txhpx;
      if (yd > 0) {
        const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
        const int txh = tx_size_high_unit[tx_size];
        if (mi_row + ((row_off + txh) << ss_y) < xd->tile.mi_row_end) {
          const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
          BLOCK_SIZE bsize = mbmi->bsize;
          if (ss_x || ss_y) bsize = scale_chroma_bsize(bsize, ss_x, ss_y);
          if (has_bottom_left(sb_size, bsize, mi_row, mi_col, 1, 1,
                              mbmi->partition, tx_size, row_off, col_off, ss_x,
                              ss_y)) {
            n_bottomleft_px = AOMMIN(yd, txhpx);
          }
        }
      }
    }
  }

  const int disable_edge_filter = !enable_intra_edge_filter;
  const int intra_edge_filter_type =
      (!disable_edge_filter && txwpx + txhpx <= 32 && p_angle != 90 &&
       p_angle != 180)
          ? get_intra_edge_filter_type(xd, plane)
          : 0;
  assert(n_top_px >= 0 && n_top_px <= 255);
  assert(n_topright_px >= 0 && n_topright_px <= 255);
  assert(n_left_px >= 0 && n_left_px <= 255);
  assert(n_bottomleft_px >= 0 && n_bottomleft_px <= 255);
  build_directional_intra_predictors(
      ref, ref_stride, dst, dst_stride,
      (uint32_t)p_angle | ((uint32_t)tx_size << 9) |
          ((uint32_t)disable_edge_filter << 14) |
          ((uint32_t)intra_edge_filter_type << 15),
      (uint32_t)n_top_px | ((uint32_t)n_topright_px << 8) |
          ((uint32_t)n_left_px << 16) | ((uint32_t)n_bottomleft_px << 24));
}

void av1_predict_intra_block(const MACROBLOCKD *xd, BLOCK_SIZE sb_size,
                             int enable_intra_edge_filter, int wpx, int hpx,
                             TX_SIZE tx_size, PREDICTION_MODE mode,
                             int angle_delta, int use_palette,
                             FILTER_INTRA_MODE filter_intra_mode,
                             const uint8_t *ref, int ref_stride, uint8_t *dst,
                             int dst_stride, int col_off, int row_off,
                             int plane) {
  predict_intra_block_internal(xd, sb_size, enable_intra_edge_filter, wpx, hpx,
                               tx_size, mode, angle_delta, use_palette,
                               filter_intra_mode, ref, ref_stride, dst,
                               dst_stride, col_off, row_off, plane);
}

static AOM_FORCE_INLINE uint32_t cur_buf_hbd_flag(const MACROBLOCKD *xd) {
#if CONFIG_AV1_HIGHBITDEPTH
  return (uint32_t)(xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH);
#else
  (void)xd;
  return 0;
#endif
}

static void predict_intra_block_slow_facade(const AV1_COMMON *cm,
                                            MACROBLOCKD *xd, int plane_flags,
                                            int blk_col, int blk_row,
                                            TX_SIZE tx_size);

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
// All SIMD specializations of cfl_get_predict_lbd_fn (SSSE3 and AVX2 on x86,
// even when alpha_q3 == 0) broadcast dst[0] across every pixel and overwrite
// the full block, whereas cfl_predict_lbd_c reads every dst[r * stride + c].
// Matching intra_mode_search.c, when SIMD CfL is active at runtime we only need
// to compute/cache the scalar DC byte in dst[0] and dc_pred_cache[plane][0].
#if HAVE_SSSE3 && CONFIG_RUNTIME_CPU_DETECT && RECONINTRA_HAVE_SSE2
static AOM_FORCE_INLINE int cfl_uses_simd_dc0_lbd(void) {
  cfl_predict_lbd_fn (*p)(TX_SIZE) = cfl_get_predict_lbd_fn;
  return p != cfl_get_predict_lbd_fn_c;
}
#endif

RECONINTRA_NOINLINE static void predict_cfl_block_facade(const AV1_COMMON *cm,
                                                         MACROBLOCKD *xd,
                                                         int plane, int blk_col,
                                                         int blk_row,
                                                         TX_SIZE tx_size) {
#if CONFIG_DEBUG
  const MB_MODE_INFO *const dbg_mbmi = xd->mi[0];
  struct macroblockd_plane *const dbg_pd = &xd->plane[plane];
  assert(is_cfl_allowed(xd));
  const BLOCK_SIZE plane_bsize = get_plane_block_size(
      dbg_mbmi->bsize, dbg_pd->subsampling_x, dbg_pd->subsampling_y);
  (void)plane_bsize;
  assert(plane_bsize < BLOCK_SIZES_ALL);
  if (!xd->lossless[dbg_mbmi->segment_id]) {
    assert(blk_col == 0);
    assert(blk_row == 0);
    assert(block_size_wide[plane_bsize] == tx_size_wide[tx_size]);
    assert(block_size_high[plane_bsize] == tx_size_high[tx_size]);
  }
#endif
#if HAVE_SSSE3 && RECONINTRA_HAVE_SSE2
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  if (UNLIKELY((cur_buf_hbd_flag(xd) |
                mbmi->palette_mode_info.palette_size[plane != 0]) != 0)) {
    predict_intra_block_slow_facade(cm, xd, plane | 4, blk_col, blk_row,
                                    tx_size);
    return;
  }
#if CONFIG_RUNTIME_CPU_DETECT
  if (UNLIKELY(!cfl_uses_simd_dc0_lbd())) {
    predict_intra_block_slow_facade(cm, xd, plane | 4, blk_col, blk_row,
                                    tx_size);
    return;
  }
#endif
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const int dst_stride = pd->dst.stride;
  uint8_t *dst =
      &pd->dst.buf[((ptrdiff_t)blk_row * dst_stride + blk_col) * MI_SIZE];
  CFL_CTX *const cfl = &xd->cfl;
  const CFL_PRED_TYPE pred_plane = get_cfl_pred_type(plane);
  if (cfl->dc_pred_is_cached[pred_plane]) {
    dst[0] = ((const uint8_t *)cfl->dc_pred_cache[pred_plane])[0];
    av1_cfl_predict_block(xd, dst, dst_stride, tx_size, plane);
    return;
  }
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  const int have_top = (blk_row | xd->chroma_up_available) != 0;
  const int have_left = (blk_col | xd->chroma_left_available) != 0;
  int n_top_px;
  int n_left_px;
  if (LIKELY((xd->mb_to_right_edge | xd->mb_to_bottom_edge) >= 0)) {
    n_top_px = have_top ? txwpx : 0;
    n_left_px = have_left ? txhpx : 0;
  } else {
    const int ss_x = pd->subsampling_x;
    const int ss_y = pd->subsampling_y;
    const int right_px = (xd->mb_to_right_edge >> (3 + ss_x)) + pd->width -
                         (blk_col << MI_SIZE_LOG2);
    const int bottom_px = (xd->mb_to_bottom_edge >> (3 + ss_y)) + pd->height -
                          (blk_row << MI_SIZE_LOG2);
    n_top_px = AOMMIN(AOMMAX(right_px, 0), txwpx) & -have_top;
    n_left_px = AOMMIN(AOMMAX(bottom_px, 0), txhpx) & -have_left;
  }
  const uint8_t dc_val = compute_dc_val_cfl_lbd(dst, dst_stride, tx_size, txwpx,
                                                txhpx, n_top_px, n_left_px);
  dst[0] = dc_val;
  if (cfl->use_dc_pred_cache) {
    ((uint8_t *)cfl->dc_pred_cache[pred_plane])[0] = dc_val;
    cfl->dc_pred_is_cached[pred_plane] = true;
  }
  av1_cfl_predict_block(xd, dst, dst_stride, tx_size, plane);
  return;
#else
  predict_intra_block_slow_facade(cm, xd, plane | 4, blk_col, blk_row, tx_size);
#endif
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

// plane_flags: bits [1:0] = plane, bit 2 = UV_CFL_PRED flag.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void predict_intra_block_slow_facade(const AV1_COMMON *cm,
                                            MACROBLOCKD *xd, int plane_flags,
                                            int blk_col, int blk_row,
                                            TX_SIZE tx_size) {
  assert((plane_flags & ~7) == 0);
  const int plane = plane_flags & 3;
  assert(plane < MAX_MB_PLANE);
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const int dst_stride = pd->dst.stride;
  uint8_t *dst =
      &pd->dst.buf[((ptrdiff_t)blk_row * dst_stride + blk_col) * MI_SIZE];
  const int is_chroma = plane != 0;
  const int use_palette = mbmi->palette_mode_info.palette_size[is_chroma] > 0;
#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
  if (plane_flags & 4) {
    CFL_CTX *const cfl = &xd->cfl;
    const CFL_PRED_TYPE pred_plane = get_cfl_pred_type(plane);
    if (!cfl->dc_pred_is_cached[pred_plane]) {
      const SequenceHeader *const seq_params = cm->seq_params;
      av1_predict_intra_block(xd, seq_params->sb_size,
                              seq_params->enable_intra_edge_filter, pd->width,
                              pd->height, tx_size, DC_PRED, 0, use_palette,
                              FILTER_INTRA_MODES, dst, dst_stride, dst,
                              dst_stride, blk_col, blk_row, plane);
      if (cfl->use_dc_pred_cache) {
        cfl_store_dc_pred(xd, dst, pred_plane, tx_size_wide[tx_size]);
        cfl->dc_pred_is_cached[pred_plane] = true;
      }
    } else {
      cfl_load_dc_pred(xd, dst, dst_stride, tx_size, pred_plane);
    }
    av1_cfl_predict_block(xd, dst, dst_stride, tx_size, plane);
    return;
  }
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
  if (use_palette) {
    predict_palette_block(xd, pd->width, tx_size_wide[tx_size],
                          tx_size_high[tx_size], blk_col << MI_SIZE_LOG2,
                          blk_row << MI_SIZE_LOG2, dst, dst_stride, plane,
                          is_cur_buf_hbd(xd));
    return;
  }
#if CONFIG_AV1_HIGHBITDEPTH
  const SequenceHeader *const seq_params = cm->seq_params;
  const PREDICTION_MODE mode =
      is_chroma ? get_uv_mode(mbmi->uv_mode) : mbmi->mode;
  const FILTER_INTRA_MODE filter_intra_mode =
      (!is_chroma && mbmi->filter_intra_mode_info.use_filter_intra)
          ? mbmi->filter_intra_mode_info.filter_intra_mode
          : FILTER_INTRA_MODES;
  highbd_predict_intra_block_internal(
      xd, seq_params->sb_size, seq_params->enable_intra_edge_filter, pd->width,
      pd->height, tx_size, mode, mbmi->angle_delta[is_chroma] * ANGLE_STEP,
      filter_intra_mode, dst, dst_stride, dst, dst_stride, blk_col, blk_row,
      plane);
#else
  (void)cm;
#endif  // CONFIG_AV1_HIGHBITDEPTH
}

// blk_plane_tx_angle: bits [5:0] = blk_col, bits [11:6] = blk_row,
//   bits [13:12] = plane, bits [19:14] = tx_size, bits [31:20] = p_angle.
// top_left_px: bits [15:0] = n_top_px, bits [31:16] = n_left_px.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void predict_directional_chroma_facade(const AV1_COMMON *cm,
                                              const MACROBLOCKD *xd,
                                              uint8_t *dst, int dst_stride,
                                              uint32_t blk_plane_tx_angle,
                                              uint32_t top_left_px) {
  const int blk_col = (int)(blk_plane_tx_angle & 0x3f);
  const int blk_row = (int)((blk_plane_tx_angle >> 6) & 0x3f);
  const int plane = (int)((blk_plane_tx_angle >> 12) & 0x3);
  const TX_SIZE tx_size = (TX_SIZE)((blk_plane_tx_angle >> 14) & 0x3f);
  const int p_angle = (int)(blk_plane_tx_angle >> 20);
  const int n_top_px = (int)(top_left_px & 0xffff);
  const int n_left_px = (int)(top_left_px >> 16);
  const SequenceHeader *const seq_params = cm->seq_params;
  int n_topright_px = 0;
  int n_bottomleft_px = 0;
  if (p_angle < 90) {
    if (n_top_px > 0) {
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      const int ss_x = pd->subsampling_x;
      const int txwpx = tx_size_wide[tx_size];
      const int xr = (xd->mb_to_right_edge >> (3 + ss_x)) + pd->width -
                     (blk_col << MI_SIZE_LOG2) - txwpx;
      if (xr > 0) {
        const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
        const int txw = tx_size_wide_unit[tx_size];
        if (mi_col + ((blk_col + txw) << ss_x) < xd->tile.mi_col_end) {
          const int ss_y = pd->subsampling_y;
          const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
          const MB_MODE_INFO *const mbmi = xd->mi[0];
          BLOCK_SIZE bsize = mbmi->bsize;
          if (ss_x || ss_y) bsize = scale_chroma_bsize(bsize, ss_x, ss_y);
          if (has_top_right(seq_params->sb_size, bsize, mi_row, mi_col, 1, 1,
                            mbmi->partition, tx_size, blk_row, blk_col, ss_x,
                            ss_y)) {
            n_topright_px = AOMMIN(xr, txwpx);
          }
        }
      }
    }
  } else if (p_angle > 180) {
    if (n_left_px > 0) {
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      const int ss_x = pd->subsampling_x;
      if (((blk_col << ss_x) & 15) == 0) {
        const int ss_y = pd->subsampling_y;
        const int txhpx = tx_size_high[tx_size];
        const int yd = (xd->mb_to_bottom_edge >> (3 + ss_y)) + pd->height -
                       (blk_row << MI_SIZE_LOG2) - txhpx;
        if (yd > 0) {
          const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
          const int txh = tx_size_high_unit[tx_size];
          if (mi_row + ((blk_row + txh) << ss_y) < xd->tile.mi_row_end) {
            const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
            const MB_MODE_INFO *const mbmi = xd->mi[0];
            BLOCK_SIZE bsize = mbmi->bsize;
            if (ss_x || ss_y) bsize = scale_chroma_bsize(bsize, ss_x, ss_y);
            if (has_bottom_left(seq_params->sb_size, bsize, mi_row, mi_col, 1,
                                1, mbmi->partition, tx_size, blk_row, blk_col,
                                ss_x, ss_y)) {
              n_bottomleft_px = AOMMIN(yd, txhpx);
            }
          }
        }
      }
    }
  }

  const int disable_edge_filter = !seq_params->enable_intra_edge_filter;
  const int intra_edge_filter_type =
      (!disable_edge_filter &&
       tx_size_wide[tx_size] + tx_size_high[tx_size] <= 32 && p_angle != 90 &&
       p_angle != 180)
          ? get_intra_edge_filter_type(xd, plane)
          : 0;
  assert(n_top_px >= 0 && n_top_px <= 255);
  assert(n_topright_px >= 0 && n_topright_px <= 255);
  assert(n_left_px >= 0 && n_left_px <= 255);
  assert(n_bottomleft_px >= 0 && n_bottomleft_px <= 255);
  build_directional_intra_predictors(
      dst, dst_stride, dst, dst_stride,
      (uint32_t)p_angle | ((uint32_t)tx_size << 9) |
          ((uint32_t)disable_edge_filter << 14) |
          ((uint32_t)intra_edge_filter_type << 15),
      (uint32_t)n_top_px | ((uint32_t)n_topright_px << 8) |
          ((uint32_t)n_left_px << 16) | ((uint32_t)n_bottomleft_px << 24));
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void predict_intra_block_chroma_facade(const AV1_COMMON *cm,
                                              MACROBLOCKD *xd, int plane,
                                              int blk_col, int blk_row,
                                              TX_SIZE tx_size) {
  const MB_MODE_INFO *const mbmi = xd->mi[0];
#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
  if (mbmi->uv_mode == UV_CFL_PRED) {
    predict_cfl_block_facade(cm, xd, plane, blk_col, blk_row, tx_size);
    return;
  }
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
  if (UNLIKELY((cur_buf_hbd_flag(xd) |
                mbmi->palette_mode_info.palette_size[1]) != 0)) {
    predict_intra_block_slow_facade(cm, xd, plane, blk_col, blk_row, tx_size);
    return;
  }
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const int dst_stride = pd->dst.stride;
  uint8_t *dst =
      &pd->dst.buf[((ptrdiff_t)blk_row * dst_stride + blk_col) * MI_SIZE];
  const PREDICTION_MODE mode = (PREDICTION_MODE)mbmi->uv_mode;
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  const int have_top = (blk_row | xd->chroma_up_available) != 0;
  const int have_left = (blk_col | xd->chroma_left_available) != 0;
  int n_top_px;
  int n_left_px;
  if (LIKELY((xd->mb_to_right_edge | xd->mb_to_bottom_edge) >= 0)) {
    n_top_px = have_top ? txwpx : 0;
    n_left_px = have_left ? txhpx : 0;
  } else {
    const int ss_x = pd->subsampling_x;
    const int ss_y = pd->subsampling_y;
    const int right_px = (xd->mb_to_right_edge >> (3 + ss_x)) + pd->width -
                         (blk_col << MI_SIZE_LOG2);
    const int bottom_px = (xd->mb_to_bottom_edge >> (3 + ss_y)) + pd->height -
                          (blk_row << MI_SIZE_LOG2);
    n_top_px = AOMMIN(AOMMAX(right_px, 0), txwpx) & -have_top;
    n_left_px = AOMMIN(AOMMAX(bottom_px, 0), txhpx) & -have_left;
  }

  if (!av1_is_directional_mode(mode)) {
    if (mode == DC_PRED) {
#if AOM_ARCH_X86 && !AOM_ARCH_X86_64
      av1_predict_dc_block_lbd(dst, dst_stride, dst, dst_stride, tx_size, txwpx,
                               txhpx, n_top_px, n_left_px);
#else
      predict_dc_block_lbd(dst, dst_stride, dst, dst_stride, tx_size, txwpx,
                           txhpx, n_top_px, n_left_px);
#endif
      return;
    }
    build_non_directional_intra_predictors(
        dst, dst_stride, dst, dst_stride,
        (uint32_t)mode | ((uint32_t)tx_size << 8),
        (uint32_t)n_top_px | ((uint32_t)n_left_px << 16));
    return;
  }

  const int p_angle =
      mode_to_angle_map[mode] + mbmi->angle_delta[1] * ANGLE_STEP;
  if (p_angle == 90) {
    if (LIKELY(n_top_px == txwpx)) {
      v_pred_direct_lbd(dst, dst_stride, dst - dst_stride, txwpx, txhpx);
      return;
    }
  } else if (p_angle == 180) {
    if (LIKELY(n_left_px == txhpx)) {
      h_pred_direct_lbd(dst, dst_stride, dst - 1, dst_stride, txwpx, txhpx);
      return;
    }
  }

  if ((uint32_t)(p_angle - 91) < 89u) {
    const int disable_edge_filter = !cm->seq_params->enable_intra_edge_filter;
    const int ftype = (!disable_edge_filter && txwpx + txhpx <= 32)
                          ? get_intra_edge_filter_type(xd, plane)
                          : 0;
    build_dr_z2_lbd(dst, dst_stride, dst, dst_stride,
                    (uint32_t)p_angle | ((uint32_t)tx_size << 9) |
                        ((uint32_t)disable_edge_filter << 14) |
                        ((uint32_t)ftype << 15),
                    (uint32_t)n_top_px | ((uint32_t)n_left_px << 16));
    return;
  }

  assert((blk_col & ~0x3f) == 0 && (blk_row & ~0x3f) == 0);
  assert((plane & ~0x3) == 0 && tx_size < TX_SIZES_ALL);
  assert(p_angle > 0 && p_angle < 270);
  predict_directional_chroma_facade(
      cm, xd, dst, dst_stride,
      (uint32_t)blk_col | ((uint32_t)blk_row << 6) | ((uint32_t)plane << 12) |
          ((uint32_t)tx_size << 14) | ((uint32_t)p_angle << 20),
      (uint32_t)n_top_px | ((uint32_t)n_left_px << 16));
}

// blk_tx_angle: bits [6:0] = blk_col, bits [13:7] = blk_row,
//   bits [19:14] = tx_size, bits [31:20] = p_angle.
// top_left_px: bits [15:0] = n_top_px, bits [31:16] = n_left_px.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void predict_directional_luma_facade(const AV1_COMMON *cm,
                                            const MACROBLOCKD *xd, uint8_t *dst,
                                            int dst_stride,
                                            uint32_t blk_tx_angle,
                                            uint32_t top_left_px) {
  const int blk_col = (int)(blk_tx_angle & 0x7f);
  const int blk_row = (int)((blk_tx_angle >> 7) & 0x7f);
  const TX_SIZE tx_size = (TX_SIZE)((blk_tx_angle >> 14) & 0x3f);
  const int p_angle = (int)(blk_tx_angle >> 20);
  const int n_top_px = (int)(top_left_px & 0xffff);
  const int n_left_px = (int)(top_left_px >> 16);
  const SequenceHeader *const seq_params = cm->seq_params;
  int n_topright_px = 0;
  int n_bottomleft_px = 0;
  if (p_angle < 90) {
    if (n_top_px > 0) {
      const int txwpx = tx_size_wide[tx_size];
      const int xr = (xd->mb_to_right_edge >> 3) + xd->plane[0].width -
                     (blk_col << MI_SIZE_LOG2) - txwpx;
      if (xr > 0) {
        const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
        const int txw = tx_size_wide_unit[tx_size];
        if (mi_col + blk_col + txw < xd->tile.mi_col_end) {
          const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
          const MB_MODE_INFO *const mbmi = xd->mi[0];
          if (has_top_right(seq_params->sb_size, mbmi->bsize, mi_row, mi_col, 1,
                            1, mbmi->partition, tx_size, blk_row, blk_col, 0,
                            0)) {
            n_topright_px = AOMMIN(xr, txwpx);
          }
        }
      }
    }
  } else if (p_angle > 180) {
    if (n_left_px > 0 && (blk_col & 15) == 0) {
      const int txhpx = tx_size_high[tx_size];
      const int yd = (xd->mb_to_bottom_edge >> 3) + xd->plane[0].height -
                     (blk_row << MI_SIZE_LOG2) - txhpx;
      if (yd > 0) {
        const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
        const int txh = tx_size_high_unit[tx_size];
        if (mi_row + blk_row + txh < xd->tile.mi_row_end) {
          const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
          const MB_MODE_INFO *const mbmi = xd->mi[0];
          if (has_bottom_left(seq_params->sb_size, mbmi->bsize, mi_row, mi_col,
                              1, 1, mbmi->partition, tx_size, blk_row, blk_col,
                              0, 0)) {
            n_bottomleft_px = AOMMIN(yd, txhpx);
          }
        }
      }
    }
  }

  const int disable_edge_filter = !seq_params->enable_intra_edge_filter;
  const int intra_edge_filter_type =
      (!disable_edge_filter &&
       tx_size_wide[tx_size] + tx_size_high[tx_size] <= 32 && p_angle != 90 &&
       p_angle != 180)
          ? get_intra_edge_filter_type(xd, 0)
          : 0;
  assert(n_top_px >= 0 && n_top_px <= 255);
  assert(n_topright_px >= 0 && n_topright_px <= 255);
  assert(n_left_px >= 0 && n_left_px <= 255);
  assert(n_bottomleft_px >= 0 && n_bottomleft_px <= 255);
  build_directional_intra_predictors(
      dst, dst_stride, dst, dst_stride,
      (uint32_t)p_angle | ((uint32_t)tx_size << 9) |
          ((uint32_t)disable_edge_filter << 14) |
          ((uint32_t)intra_edge_filter_type << 15),
      (uint32_t)n_top_px | ((uint32_t)n_topright_px << 8) |
          ((uint32_t)n_left_px << 16) | ((uint32_t)n_bottomleft_px << 24));
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void predict_intra_block_luma_facade(const AV1_COMMON *cm,
                                            MACROBLOCKD *xd, int blk_col,
                                            int blk_row, TX_SIZE tx_size) {
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  if (UNLIKELY((cur_buf_hbd_flag(xd) |
                mbmi->palette_mode_info.palette_size[0]) != 0)) {
    predict_intra_block_slow_facade(cm, xd, 0, blk_col, blk_row, tx_size);
    return;
  }
  struct macroblockd_plane *const pd = &xd->plane[0];
  const int dst_stride = pd->dst.stride;
  uint8_t *dst =
      &pd->dst.buf[((ptrdiff_t)blk_row * dst_stride + blk_col) * MI_SIZE];
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  const int have_top = (blk_row | xd->up_available) != 0;
  const int have_left = (blk_col | xd->left_available) != 0;
  int n_top_px;
  int n_left_px;
  if (LIKELY((xd->mb_to_right_edge | xd->mb_to_bottom_edge) >= 0)) {
    n_top_px = have_top ? txwpx : 0;
    n_left_px = have_left ? txhpx : 0;
  } else {
    const int right_px =
        (xd->mb_to_right_edge >> 3) + pd->width - (blk_col << MI_SIZE_LOG2);
    const int bottom_px =
        (xd->mb_to_bottom_edge >> 3) + pd->height - (blk_row << MI_SIZE_LOG2);
    n_top_px = AOMMIN(AOMMAX(right_px, 0), txwpx) & -have_top;
    n_left_px = AOMMIN(AOMMAX(bottom_px, 0), txhpx) & -have_left;
  }

  if (UNLIKELY(mbmi->filter_intra_mode_info.use_filter_intra)) {
    build_filter_intra_predictors(
        dst, dst_stride, dst, dst_stride,
        (uint32_t)mbmi->filter_intra_mode_info.filter_intra_mode |
            ((uint32_t)tx_size << 8),
        (uint32_t)n_top_px | ((uint32_t)n_left_px << 16));
    return;
  }
  const PREDICTION_MODE mode = mbmi->mode;
  if (!av1_is_directional_mode(mode)) {
    if (mode == DC_PRED) {
#if AOM_ARCH_X86 && !AOM_ARCH_X86_64
      av1_predict_dc_block_lbd(dst, dst_stride, dst, dst_stride, tx_size, txwpx,
                               txhpx, n_top_px, n_left_px);
#else
      predict_dc_block_lbd(dst, dst_stride, dst, dst_stride, tx_size, txwpx,
                           txhpx, n_top_px, n_left_px);
#endif
      return;
    }
    build_non_directional_intra_predictors(
        dst, dst_stride, dst, dst_stride,
        (uint32_t)mode | ((uint32_t)tx_size << 8),
        (uint32_t)n_top_px | ((uint32_t)n_left_px << 16));
    return;
  }

  const int p_angle =
      mode_to_angle_map[mode] + mbmi->angle_delta[0] * ANGLE_STEP;
  if (p_angle == 90) {
    if (LIKELY(n_top_px == txwpx)) {
      v_pred_direct_lbd(dst, dst_stride, dst - dst_stride, txwpx, txhpx);
      return;
    }
  } else if (p_angle == 180) {
    if (LIKELY(n_left_px == txhpx)) {
      h_pred_direct_lbd(dst, dst_stride, dst - 1, dst_stride, txwpx, txhpx);
      return;
    }
  }

  if ((uint32_t)(p_angle - 91) < 89u) {
    const int disable_edge_filter = !cm->seq_params->enable_intra_edge_filter;
    const int ftype = (!disable_edge_filter && txwpx + txhpx <= 32)
                          ? get_intra_edge_filter_type(xd, 0)
                          : 0;
    build_dr_z2_lbd(dst, dst_stride, dst, dst_stride,
                    (uint32_t)p_angle | ((uint32_t)tx_size << 9) |
                        ((uint32_t)disable_edge_filter << 14) |
                        ((uint32_t)ftype << 15),
                    (uint32_t)n_top_px | ((uint32_t)n_left_px << 16));
    return;
  }

  assert((blk_col & ~0x7f) == 0 && (blk_row & ~0x7f) == 0);
  assert(tx_size < TX_SIZES_ALL && p_angle > 0 && p_angle < 270);
  predict_directional_luma_facade(
      cm, xd, dst, dst_stride,
      (uint32_t)blk_col | ((uint32_t)blk_row << 7) | ((uint32_t)tx_size << 14) |
          ((uint32_t)p_angle << 20),
      (uint32_t)n_top_px | ((uint32_t)n_left_px << 16));
}

void av1_predict_intra_block_facade(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                    int plane, int blk_col, int blk_row,
                                    TX_SIZE tx_size) {
  if (plane == AOM_PLANE_Y) {
    predict_intra_block_luma_facade(cm, xd, blk_col, blk_row, tx_size);
  } else {
    predict_intra_block_chroma_facade(cm, xd, plane, blk_col, blk_row, tx_size);
  }
}

void av1_init_intra_predictors(void) {
  aom_once(init_intra_predictors_internal);
}

#undef EDGE_FILT_LUT1
#undef EDGE_FILT_LUT0
#undef EDGE_FILT_CFG
#undef RECONINTRA_SCALAR_LOOP
#undef RECONINTRA_NO_UNROLL
#undef RECONINTRA_NOINLINE
#undef RECONINTRA_HAVE_SSE2
