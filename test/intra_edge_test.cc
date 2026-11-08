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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gtest/gtest.h"
#include "test/register_state_check.h"
#include "test/function_equivalence_test.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"
#include "av1/common/enums.h"
#if !CONFIG_SHARED
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "av1/common/av1_common_int.h"
#include "av1/common/reconintra.h"
#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
extern "C" {
#include "av1/common/cfl.h"
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
#endif  // !CONFIG_SHARED

using libaom_test::FunctionEquivalenceTest;

namespace {

template <typename F, typename T>
class UpsampleTest : public FunctionEquivalenceTest<F> {
 protected:
  static const int kIterations = 1000000;
  static const int kMinEdge = 4;
  static const int kMaxEdge = 24;
  static const int kBufSize = 2 * 64 + 32;
  static const int kOffset = 16;

  ~UpsampleTest() override = default;

  virtual void Execute(T *edge_tst) = 0;

  void Common() {
    edge_ref_ = &edge_ref_data_[kOffset];
    edge_tst_ = &edge_tst_data_[kOffset];

    Execute(edge_tst_);

    const int max_idx = (size_ - 1) * 2;
    for (int r = -2; r <= max_idx; ++r) {
      ASSERT_EQ(edge_ref_[r], edge_tst_[r]);
    }
  }

  T edge_ref_data_[kBufSize];
  T edge_tst_data_[kBufSize];

  T *edge_ref_;
  T *edge_tst_;

  int size_;
};

using UP8B = void (*)(uint8_t *p, int size);
using TestFuncs = libaom_test::FuncParam<UP8B>;

class UpsampleTest8B : public UpsampleTest<UP8B, uint8_t> {
 protected:
  void Execute(uint8_t *edge_tst) override {
    params_.ref_func(edge_ref_, size_);
    API_REGISTER_STATE_CHECK(params_.tst_func(edge_tst, size_));
  }
};

TEST_P(UpsampleTest8B, RandomValues) {
  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    size_ = 4 * (this->rng_(4) + 1);

    int i, pix = 0;
    for (i = 0; i < kOffset + size_; ++i) {
      pix = rng_.Rand8();
      edge_ref_data_[i] = pix;
      edge_tst_data_[i] = edge_ref_data_[i];
    }

    // Extend final sample
    while (i < kBufSize) {
      edge_ref_data_[i] = pix;
      edge_tst_data_[i] = pix;
      i++;
    }

    Common();
  }
}

TEST_P(UpsampleTest8B, DISABLED_Speed) {
  const int test_count = 10000000;
  size_ = kMaxEdge;
  for (int i = 0; i < kOffset + size_; ++i) {
    edge_tst_data_[i] = rng_.Rand8();
  }
  edge_tst_ = &edge_tst_data_[kOffset];
  for (int iter = 0; iter < test_count; ++iter) {
    API_REGISTER_STATE_CHECK(params_.tst_func(edge_tst_, size_));
  }
}

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, UpsampleTest8B,
    ::testing::Values(TestFuncs(av1_upsample_intra_edge_c,
                                av1_upsample_intra_edge_sse4_1)));
#endif  // HAVE_SSE4_1

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, UpsampleTest8B,
    ::testing::Values(TestFuncs(av1_upsample_intra_edge_c,
                                av1_upsample_intra_edge_neon)));
#endif  // HAVE_NEON

template <typename F, typename T>
class FilterEdgeTest : public FunctionEquivalenceTest<F> {
 protected:
  static const int kIterations = 1000000;
  static const int kMaxEdge = 2 * 64;
  static const int kBufSize = kMaxEdge + 32;
  static const int kOffset = 15;

  ~FilterEdgeTest() override = default;

  virtual void Execute(T *edge_tst) = 0;

  void Common() {
    edge_ref_ = &edge_ref_data_[kOffset];
    edge_tst_ = &edge_tst_data_[kOffset];

    Execute(edge_tst_);

    for (int r = 0; r < size_; ++r) {
      ASSERT_EQ(edge_ref_[r], edge_tst_[r]);
    }
  }

  T edge_ref_data_[kBufSize];
  T edge_tst_data_[kBufSize];

  T *edge_ref_;
  T *edge_tst_;

  int size_;
  int strength_;
};

using FE8B = void (*)(uint8_t *p, int size, int strength);
using FilterEdgeTestFuncs = libaom_test::FuncParam<FE8B>;

class FilterEdgeTest8B : public FilterEdgeTest<FE8B, uint8_t> {
 protected:
  void Execute(uint8_t *edge_tst) override {
    params_.ref_func(edge_ref_, size_, strength_);
    API_REGISTER_STATE_CHECK(params_.tst_func(edge_tst, size_, strength_));
  }
};

TEST_P(FilterEdgeTest8B, RandomValues) {
  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    strength_ = this->rng_(4);
    size_ = 4 * (this->rng_(128 / 4) + 1) + 1;

    int i, pix = 0;
    for (i = 0; i < kOffset + size_; ++i) {
      pix = rng_.Rand8();
      edge_ref_data_[i] = pix;
      edge_tst_data_[i] = pix;
    }

    Common();
  }
}

TEST_P(FilterEdgeTest8B, DISABLED_Speed) {
  const int test_count = 10000000;
  size_ = kMaxEdge;
  strength_ = 1;
  for (int i = 0; i < kOffset + size_; ++i) {
    edge_tst_data_[i] = rng_.Rand8();
  }
  edge_tst_ = &edge_tst_data_[kOffset];
  for (int iter = 0; iter < test_count; ++iter) {
    API_REGISTER_STATE_CHECK(params_.tst_func(edge_tst_, size_, strength_));
    // iterate over filter strengths (1,2,3)
    strength_ = strength_ == 3 ? 1 : strength_ + 1;
  }
}

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, FilterEdgeTest8B,
    ::testing::Values(FilterEdgeTestFuncs(av1_filter_intra_edge_c,
                                          av1_filter_intra_edge_sse4_1)));
#endif  // HAVE_SSE4_1

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, FilterEdgeTest8B,
    ::testing::Values(FilterEdgeTestFuncs(av1_filter_intra_edge_c,
                                          av1_filter_intra_edge_neon)));
#endif  // HAVE_NEON

#if CONFIG_AV1_HIGHBITDEPTH

using UPHB = void (*)(uint16_t *p, int size, int bd);
using TestFuncsHBD = libaom_test::FuncParam<UPHB>;

class UpsampleTestHB : public UpsampleTest<UPHB, uint16_t> {
 protected:
  void Execute(uint16_t *edge_tst) override {
    params_.ref_func(edge_ref_, size_, bit_depth_);
    API_REGISTER_STATE_CHECK(params_.tst_func(edge_tst, size_, bit_depth_));
  }
  int bit_depth_;
};

TEST_P(UpsampleTestHB, RandomValues) {
  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    switch (rng_(3)) {
      case 0: bit_depth_ = 8; break;
      case 1: bit_depth_ = 10; break;
      default: bit_depth_ = 12; break;
    }
    const int hi = 1 << bit_depth_;

    size_ = 4 * (this->rng_(4) + 1);

    int i, pix = 0;
    for (i = 0; i < kOffset + size_; ++i) {
      pix = rng_(hi);
      edge_ref_data_[i] = pix;
      edge_tst_data_[i] = pix;
    }

    // Extend final sample
    while (i < kBufSize) {
      edge_ref_data_[i] = pix;
      edge_tst_data_[i] = pix;
      i++;
    }

    Common();
  }
}

TEST_P(UpsampleTestHB, DISABLED_Speed) {
  const int test_count = 10000000;
  size_ = kMaxEdge;
  bit_depth_ = 12;
  const int hi = 1 << bit_depth_;
  for (int i = 0; i < kOffset + size_; ++i) {
    edge_tst_data_[i] = rng_(hi);
  }
  edge_tst_ = &edge_tst_data_[kOffset];
  for (int iter = 0; iter < test_count; ++iter) {
    API_REGISTER_STATE_CHECK(params_.tst_func(edge_tst_, size_, bit_depth_));
  }
}

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, UpsampleTestHB,
    ::testing::Values(TestFuncsHBD(av1_highbd_upsample_intra_edge_c,
                                   av1_highbd_upsample_intra_edge_sse4_1)));
#endif  // HAVE_SSE4_1

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, UpsampleTestHB,
    ::testing::Values(TestFuncsHBD(av1_highbd_upsample_intra_edge_c,
                                   av1_highbd_upsample_intra_edge_neon)));
#endif  // HAVE_NEON

using FEHB = void (*)(uint16_t *p, int size, int strength);
using FilterEdgeTestFuncsHBD = libaom_test::FuncParam<FEHB>;

class FilterEdgeTestHB : public FilterEdgeTest<FEHB, uint16_t> {
 protected:
  void Execute(uint16_t *edge_tst) override {
    params_.ref_func(edge_ref_, size_, strength_);
    API_REGISTER_STATE_CHECK(params_.tst_func(edge_tst, size_, strength_));
  }
  int bit_depth_;
};

TEST_P(FilterEdgeTestHB, RandomValues) {
  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    switch (rng_(3)) {
      case 0: bit_depth_ = 8; break;
      case 1: bit_depth_ = 10; break;
      default: bit_depth_ = 12; break;
    }
    const int hi = 1 << bit_depth_;
    strength_ = this->rng_(4);
    size_ = 4 * (this->rng_(128 / 4) + 1) + 1;

    int i, pix = 0;
    for (i = 0; i < kOffset + size_; ++i) {
      pix = rng_(hi);
      edge_ref_data_[i] = pix;
      edge_tst_data_[i] = pix;
    }

    Common();
  }
}

TEST_P(FilterEdgeTestHB, DISABLED_Speed) {
  const int test_count = 10000000;
  size_ = kMaxEdge;
  strength_ = 1;
  bit_depth_ = 12;
  const int hi = 1 << bit_depth_;
  for (int i = 0; i < kOffset + size_; ++i) {
    edge_tst_data_[i] = rng_(hi);
  }
  edge_tst_ = &edge_tst_data_[kOffset];
  for (int iter = 0; iter < test_count; ++iter) {
    API_REGISTER_STATE_CHECK(params_.tst_func(edge_tst_, size_, strength_));
    // iterate over filter strengths (1,2,3)
    strength_ = strength_ == 3 ? 1 : strength_ + 1;
  }
}

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE4_1, FilterEdgeTestHB,
                         ::testing::Values(FilterEdgeTestFuncsHBD(
                             av1_highbd_filter_intra_edge_c,
                             av1_highbd_filter_intra_edge_sse4_1)));
#endif  // HAVE_SSE4_1

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, FilterEdgeTestHB,
                         ::testing::Values(FilterEdgeTestFuncsHBD(
                             av1_highbd_filter_intra_edge_c,
                             av1_highbd_filter_intra_edge_neon)));
#endif  // HAVE_NEON

#endif  // CONFIG_AV1_HIGHBITDEPTH

#if !CONFIG_SHARED

using IntraPredRefFn = void (*)(uint8_t *dst, ptrdiff_t stride,
                                const uint8_t *above, const uint8_t *left);

struct RefIntraPredTables {
  IntraPredRefFn pred[INTRA_MODES][TX_SIZES_ALL];
  IntraPredRefFn dc_pred[2][2][TX_SIZES_ALL];

  RefIntraPredTables() : pred{}, dc_pred{} {
#if CONFIG_REALTIME_ONLY && !CONFIG_AV1_DECODER
#define INIT_REF_RECT(p, type)                  \
  p[TX_4X8] = aom_##type##_predictor_4x8_c;     \
  p[TX_8X4] = aom_##type##_predictor_8x4_c;     \
  p[TX_8X16] = aom_##type##_predictor_8x16_c;   \
  p[TX_16X8] = aom_##type##_predictor_16x8_c;   \
  p[TX_16X32] = aom_##type##_predictor_16x32_c; \
  p[TX_32X16] = aom_##type##_predictor_32x16_c; \
  p[TX_32X64] = aom_##type##_predictor_32x64_c; \
  p[TX_64X32] = aom_##type##_predictor_64x32_c
#else
#define INIT_REF_RECT(p, type)                  \
  p[TX_4X8] = aom_##type##_predictor_4x8_c;     \
  p[TX_8X4] = aom_##type##_predictor_8x4_c;     \
  p[TX_8X16] = aom_##type##_predictor_8x16_c;   \
  p[TX_16X8] = aom_##type##_predictor_16x8_c;   \
  p[TX_16X32] = aom_##type##_predictor_16x32_c; \
  p[TX_32X16] = aom_##type##_predictor_32x16_c; \
  p[TX_32X64] = aom_##type##_predictor_32x64_c; \
  p[TX_64X32] = aom_##type##_predictor_64x32_c; \
  p[TX_4X16] = aom_##type##_predictor_4x16_c;   \
  p[TX_16X4] = aom_##type##_predictor_16x4_c;   \
  p[TX_8X32] = aom_##type##_predictor_8x32_c;   \
  p[TX_32X8] = aom_##type##_predictor_32x8_c;   \
  p[TX_16X64] = aom_##type##_predictor_16x64_c; \
  p[TX_64X16] = aom_##type##_predictor_64x16_c
#endif  // CONFIG_REALTIME_ONLY && !CONFIG_AV1_DECODER

#define INIT_REF_ALL(p, type)                     \
  do {                                            \
    p[TX_4X4] = aom_##type##_predictor_4x4_c;     \
    p[TX_8X8] = aom_##type##_predictor_8x8_c;     \
    p[TX_16X16] = aom_##type##_predictor_16x16_c; \
    p[TX_32X32] = aom_##type##_predictor_32x32_c; \
    p[TX_64X64] = aom_##type##_predictor_64x64_c; \
    INIT_REF_RECT(p, type);                       \
  } while (0)

    INIT_REF_ALL(pred[V_PRED], v);
    INIT_REF_ALL(pred[H_PRED], h);
    INIT_REF_ALL(pred[PAETH_PRED], paeth);
    INIT_REF_ALL(pred[SMOOTH_PRED], smooth);
    INIT_REF_ALL(pred[SMOOTH_V_PRED], smooth_v);
    INIT_REF_ALL(pred[SMOOTH_H_PRED], smooth_h);
    INIT_REF_ALL(dc_pred[0][0], dc_128);
    INIT_REF_ALL(dc_pred[0][1], dc_top);
    INIT_REF_ALL(dc_pred[1][0], dc_left);
    INIT_REF_ALL(dc_pred[1][1], dc);

#undef INIT_REF_ALL
#undef INIT_REF_RECT
  }
};

constexpr int kRefModeToAngle[INTRA_MODES] = {
  0, 90, 180, 45, 135, 113, 157, 203, 67, 0, 0, 0, 0,
};

int RefEdgeFilterStrength(int blk_wh, int d, int type) {
  if (d == 0) return 0;
  if (type == 0) {
    if (blk_wh <= 8) return d >= 56 ? 1 : 0;
    if (blk_wh <= 16) return d >= 40 ? 1 : 0;
    if (blk_wh <= 24) return (d >= 8) + (d >= 16) + (d >= 32);
    if (blk_wh <= 32) return 1 + (d >= 4) + (d >= 32);
    return 3;
  }
  if (blk_wh <= 8) return (d >= 40) + (d >= 64);
  if (blk_wh <= 16) return (d >= 20) + (d >= 48);
  if (blk_wh <= 24) return d >= 4 ? 3 : 0;
  return 3;
}

bool RefUseEdgeUpsample(int blk_wh, int d, int type) {
  return d > 0 && d < 40 && blk_wh <= (16 >> type);
}

void RefCopyAndPadRow(uint8_t *dst, const uint8_t *src, ptrdiff_t src_step,
                      int n_avail, int n_need, uint8_t fallback) {
  if (n_avail <= 0) {
    memset(dst, fallback, n_need);
    return;
  }
  const int n_copy = std::min(n_avail, n_need);
  for (int i = 0; i < n_copy; ++i) {
    dst[i] = src[static_cast<ptrdiff_t>(i) * src_step];
  }
  if (n_copy < n_need) {
    memset(dst + n_copy, dst[n_copy - 1], n_need - n_copy);
  }
}

void RefFillBlock(uint8_t *dst, ptrdiff_t dst_stride, int w, int h,
                  uint8_t val) {
  for (int r = 0; r < h; ++r) {
    memset(dst + static_cast<ptrdiff_t>(r) * dst_stride, val, w);
  }
}

// Self-contained AV1 8-bit intra prediction reference built strictly from
// scalar RTCD _c primitives.
void RefPredictIntraBlock8B(const RefIntraPredTables &tabs, const uint8_t *ref,
                            ptrdiff_t ref_stride, uint8_t *dst,
                            ptrdiff_t dst_stride, TX_SIZE tx_size,
                            PREDICTION_MODE mode, int angle_delta_deg,
                            FILTER_INTRA_MODE filter_intra_mode,
                            int enable_edge_filter, int edge_filter_type,
                            int n_top_px, int n_topright_px, int n_left_px,
                            int n_bottomleft_px) {
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  const uint8_t *const above_ref = ref - ref_stride;
  const uint8_t *const left_ref = ref - 1;

  DECLARE_ALIGNED(16, uint8_t, above_buf[MAX_TX_SIZE * 4 + 64]);
  DECLARE_ALIGNED(16, uint8_t, left_buf[MAX_TX_SIZE * 4 + 64]);
  memset(above_buf, 0, sizeof(above_buf));
  memset(left_buf, 0, sizeof(left_buf));
  uint8_t *const above_row = above_buf + 32;
  uint8_t *const left_col = left_buf + 32;

  const auto compute_corner = [&]() -> uint8_t {
    if (n_top_px > 0 && n_left_px > 0) return above_ref[-1];
    if (n_top_px > 0) return above_ref[0];
    if (n_left_px > 0) return left_ref[0];
    return 128;
  };
  const uint8_t missing_above = (n_left_px > 0) ? left_ref[0] : 127;
  const uint8_t missing_left = (n_top_px > 0) ? above_ref[0] : 129;

  if (filter_intra_mode != FILTER_INTRA_MODES) {
    RefCopyAndPadRow(above_row, above_ref, 1, n_top_px, txwpx, missing_above);
    RefCopyAndPadRow(left_col, left_ref, ref_stride, n_left_px, txhpx,
                     missing_left);
    above_row[-1] = compute_corner();
    left_col[-1] = above_row[-1];
    av1_filter_intra_predictor_c(dst, dst_stride, tx_size, above_row, left_col,
                                 filter_intra_mode);
    return;
  }

  if (!av1_is_directional_mode(mode)) {
    RefCopyAndPadRow(above_row, above_ref, 1, n_top_px, txwpx, missing_above);
    RefCopyAndPadRow(left_col, left_ref, ref_stride, n_left_px, txhpx,
                     missing_left);
    if (mode == PAETH_PRED) {
      above_row[-1] = compute_corner();
      left_col[-1] = above_row[-1];
    }
    if (mode == DC_PRED) {
      tabs.dc_pred[n_left_px > 0][n_top_px > 0][tx_size](dst, dst_stride,
                                                         above_row, left_col);
    } else {
      tabs.pred[mode][tx_size](dst, dst_stride, above_row, left_col);
    }
    return;
  }

  const int p_angle = kRefModeToAngle[mode] + angle_delta_deg;
  const int blk_wh = txwpx + txhpx;
  if (p_angle == 90) {
    if (n_top_px == 0) {
      RefFillBlock(dst, dst_stride, txwpx, txhpx, missing_above);
      return;
    }
    RefCopyAndPadRow(above_row, above_ref, 1, n_top_px, txwpx, missing_above);
    tabs.pred[V_PRED][tx_size](dst, dst_stride, above_row, left_col);
    return;
  }
  if (p_angle == 180) {
    if (n_left_px == 0) {
      RefFillBlock(dst, dst_stride, txwpx, txhpx, missing_left);
      return;
    }
    RefCopyAndPadRow(left_col, left_ref, ref_stride, n_left_px, txhpx,
                     missing_left);
    tabs.pred[H_PRED][tx_size](dst, dst_stride, above_row, left_col);
    return;
  }
  if (p_angle < 90) {
    if (n_top_px == 0) {
      RefFillBlock(dst, dst_stride, txwpx, txhpx, missing_above);
      return;
    }
    RefCopyAndPadRow(above_row, above_ref, 1, n_top_px + n_topright_px, blk_wh,
                     missing_above);
    above_row[-1] = (n_left_px > 0) ? above_ref[-1] : above_ref[0];
    int upsample_above = 0;
    if (enable_edge_filter) {
      const int d = 90 - p_angle;
      if (RefUseEdgeUpsample(blk_wh, d, edge_filter_type)) {
        upsample_above = 1;
        av1_upsample_intra_edge_c(above_row, blk_wh);
      } else {
        const int strength = RefEdgeFilterStrength(blk_wh, d, edge_filter_type);
        if (strength > 0) {
          av1_filter_intra_edge_c(above_row - 1, n_top_px + txhpx + 1,
                                  strength);
        }
      }
    }
    av1_dr_prediction_z1_c(dst, dst_stride, txwpx, txhpx, above_row, nullptr,
                           upsample_above, dr_intra_derivative[p_angle], 1);
    return;
  }
  if (p_angle > 180) {
    if (n_left_px == 0) {
      RefFillBlock(dst, dst_stride, txwpx, txhpx, missing_left);
      return;
    }
    RefCopyAndPadRow(left_col, left_ref, ref_stride,
                     n_left_px + n_bottomleft_px, blk_wh, missing_left);
    left_col[-1] = (n_top_px > 0) ? above_ref[-1] : left_ref[0];
    int upsample_left = 0;
    if (enable_edge_filter) {
      const int d = p_angle - 180;
      if (RefUseEdgeUpsample(blk_wh, d, edge_filter_type)) {
        upsample_left = 1;
        av1_upsample_intra_edge_c(left_col, blk_wh);
      } else {
        const int strength = RefEdgeFilterStrength(blk_wh, d, edge_filter_type);
        if (strength > 0) {
          av1_filter_intra_edge_c(left_col - 1, n_left_px + txwpx + 1,
                                  strength);
        }
      }
    }
    av1_dr_prediction_z3_c(dst, dst_stride, txwpx, txhpx, nullptr, left_col,
                           upsample_left, 1,
                           dr_intra_derivative[270 - p_angle]);
    return;
  }

  // Zone 2: 90 < p_angle < 180.
  RefCopyAndPadRow(above_row, above_ref, 1, n_top_px, txwpx, missing_above);
  RefCopyAndPadRow(left_col, left_ref, ref_stride, n_left_px, txhpx,
                   missing_left);
  uint8_t corner = compute_corner();
  if (enable_edge_filter && blk_wh >= 24 && n_top_px > 0 && n_left_px > 0) {
    corner = static_cast<uint8_t>(
        (left_ref[0] * 5 + corner * 6 + above_ref[0] * 5 + 8) >> 4);
  }
  above_row[-1] = corner;
  left_col[-1] = corner;

  int upsample_above = 0;
  int upsample_left = 0;
  if (enable_edge_filter) {
    const int d_above = p_angle - 90;
    if (RefUseEdgeUpsample(blk_wh, d_above, edge_filter_type)) {
      upsample_above = 1;
      av1_upsample_intra_edge_c(above_row, txwpx);
    } else if (n_top_px > 0) {
      const int s = RefEdgeFilterStrength(blk_wh, d_above, edge_filter_type);
      if (s > 0) av1_filter_intra_edge_c(above_row - 1, n_top_px + 1, s);
    }
    const int d_left = 180 - p_angle;
    if (RefUseEdgeUpsample(blk_wh, d_left, edge_filter_type)) {
      upsample_left = 1;
      av1_upsample_intra_edge_c(left_col, txhpx);
    } else if (n_left_px > 0) {
      const int s = RefEdgeFilterStrength(blk_wh, d_left, edge_filter_type);
      if (s > 0) av1_filter_intra_edge_c(left_col - 1, n_left_px + 1, s);
    }
  }
  av1_dr_prediction_z2_c(dst, dst_stride, txwpx, txhpx, above_row, left_col,
                         upsample_above, upsample_left,
                         dr_intra_derivative[180 - p_angle],
                         dr_intra_derivative[p_angle - 90]);
}

void CheckBlockEq(const uint8_t *exp, ptrdiff_t exp_stride, const uint8_t *act,
                  ptrdiff_t act_stride, int w, int h, const char *ctx,
                  int tx_size, int mode, int border_idx, int extra) {
  for (int r = 0; r < h; ++r) {
    const uint8_t *er = exp + static_cast<ptrdiff_t>(r) * exp_stride;
    const uint8_t *ar = act + static_cast<ptrdiff_t>(r) * act_stride;
    for (int c = 0; c < w; ++c) {
      ASSERT_EQ(er[c], ar[c])
          << ctx << " tx=" << tx_size << " (" << w << "x" << h
          << ") mode=" << mode << " border=" << border_idx << " extra=" << extra
          << " at (" << r << "," << c << ")";
    }
  }
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
int RefCflIdxToAlpha(uint8_t alpha_idx, int8_t joint_sign,
                     CFL_PRED_TYPE pred_type) {
  const int alpha_sign = (pred_type == CFL_PRED_U) ? CFL_SIGN_U(joint_sign)
                                                   : CFL_SIGN_V(joint_sign);
  if (alpha_sign == CFL_SIGN_ZERO) return 0;
  const int abs_alpha_q3 =
      (pred_type == CFL_PRED_U) ? CFL_IDX_U(alpha_idx) : CFL_IDX_V(alpha_idx);
  return (alpha_sign == CFL_SIGN_POS) ? abs_alpha_q3 + 1 : -abs_alpha_q3 - 1;
}

void RefCflPredict8B(const RefIntraPredTables &tabs, const uint8_t *dc_ref,
                     ptrdiff_t dc_ref_stride, int n_top_px, int n_left_px,
                     const uint16_t *luma_src_q3, int buf_w, int buf_h,
                     TX_SIZE tx_size, uint8_t alpha_idx, int8_t alpha_signs,
                     int plane, uint8_t *out, ptrdiff_t out_stride) {
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
  RefPredictIntraBlock8B(tabs, dc_ref, dc_ref_stride, out, out_stride, tx_size,
                         DC_PRED, 0, FILTER_INTRA_MODES, 0, 0, n_top_px, 0,
                         n_left_px, 0);

  DECLARE_ALIGNED(32, uint16_t, padded_q3[CFL_BUF_SQUARE]);
  DECLARE_ALIGNED(32, int16_t, ac_q3[CFL_BUF_SQUARE]);
  memcpy(padded_q3, luma_src_q3, sizeof(padded_q3));
  for (int j = 0; j < buf_h; ++j) {
    for (int i = buf_w; i < txwpx; ++i) {
      padded_q3[j * CFL_BUF_LINE + i] = padded_q3[j * CFL_BUF_LINE + buf_w - 1];
    }
  }
  for (int j = buf_h; j < txhpx; ++j) {
    memcpy(&padded_q3[j * CFL_BUF_LINE], &padded_q3[(buf_h - 1) * CFL_BUF_LINE],
           txwpx * sizeof(padded_q3[0]));
  }
  cfl_get_subtract_average_fn_c(tx_size)(padded_q3, ac_q3);
  const int alpha_q3 =
      RefCflIdxToAlpha(alpha_idx, alpha_signs, get_cfl_pred_type(plane));
  cfl_get_predict_lbd_fn_c(tx_size)(ac_q3, out, static_cast<int>(out_stride),
                                    alpha_q3);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

struct BorderCase {
  int up_avail;
  int left_avail;
  // Positive >= 128 means full interior; >= 0 and < txwpx+txhpx constrains
  // visible frame extent from block origin (x, y) to test partial borders and
  // partial top-right / bottom-left extensions.
  int right_avail_px;
  int bottom_avail_px;
  bool block_tr_tile;
  bool block_bl_tile;
  bool neg_stride;
};

TEST(ReconIntraTest, PredictIntraBlockAllModesAndBorders) {
  av1_init_intra_predictors();
  const RefIntraPredTables tabs;
  libaom_test::ACMRandom rng(libaom_test::ACMRandom::DeterministicSeed());

  auto cm = std::make_unique<AV1_COMMON>();
  SequenceHeader seq_params = {};
  seq_params.sb_size = BLOCK_128X128;
  seq_params.enable_intra_edge_filter = 1;
  seq_params.enable_filter_intra = 1;
  cm->seq_params = &seq_params;

  MACROBLOCKD xd = {};
  MB_MODE_INFO mbmi = {};
  MB_MODE_INFO neighbor_smooth = {};
  MB_MODE_INFO neighbor_plain = {};
  neighbor_smooth.mode = SMOOTH_PRED;
  neighbor_smooth.uv_mode = UV_SMOOTH_PRED;
  neighbor_plain.mode = DC_PRED;
  neighbor_plain.uv_mode = UV_DC_PRED;
  MB_MODE_INFO *mi_ptr = &mbmi;
  xd.mi = &mi_ptr;
  xd.bd = 8;
#if CONFIG_AV1_HIGHBITDEPTH
  YV12_BUFFER_CONFIG cur_buf = {};
  xd.cur_buf = &cur_buf;
#endif  // CONFIG_AV1_HIGHBITDEPTH

  // Unaligned prime ref_stride exercises arbitrary neighbor byte alignments,
  // while dst uses AV1's 16-byte-multiple frame stride + natural TX alignment.
  constexpr int kRefStride = 211;
  constexpr int kDstStride = 176;
  constexpr int kPadRows = 4;
  constexpr int kBufRows = MAX_TX_SIZE * 2 + 16;
  std::vector<uint8_t> ref_storage(kBufRows * kRefStride + 64);
  std::vector<uint8_t> act_storage(kBufRows * kDstStride + 128);
  std::vector<uint8_t> exp_storage(MAX_TX_SIZE * MAX_TX_SIZE);
  uint8_t *const act_aligned_base = reinterpret_cast<uint8_t *>(
      (reinterpret_cast<uintptr_t>(act_storage.data()) + 31u) &
      ~static_cast<uintptr_t>(31u));
  for (size_t i = 0; i < ref_storage.size(); ++i) {
    ref_storage[i] = rng.Rand8();
  }

  for (int tx = 0; tx < TX_SIZES_ALL && !HasFatalFailure(); ++tx) {
    const TX_SIZE tx_size = static_cast<TX_SIZE>(tx);
    if (tabs.pred[V_PRED][tx_size] == nullptr) continue;
    const int txwpx = tx_size_wide[tx_size];
    const int txhpx = tx_size_high[tx_size];

    const BorderCase kBorders[] = {
      // Full interior with and without top-right / bottom-left availability.
      { 1, 1, 256, 256, false, false, false },
      { 1, 1, 256, 256, true, true, false },
      // Partial top-right (xr=3) and partial bottom-left (yd=5).
      { 1, 1, txwpx + 3, txhpx + 5, false, false, false },
      // Missing top, missing left, missing both.
      { 0, 1, 256, 256, false, false, false },
      { 1, 0, 256, 256, false, false, false },
      { 0, 0, 256, 256, false, false, false },
      // Right/bottom frame-edge cropping: odd (1, 3, tx-1), half, and zero.
      { 1, 1, 1, txhpx - 1, false, false, false },
      { 1, 1, std::min(3, txwpx - 1), 1, false, false, false },
      { 1, 1, std::max(1, txwpx / 2 + 1), std::max(1, txhpx / 2 - 1), false,
        false, false },
      { 1, 1, txwpx - 1, 0, false, false, false },
      { 1, 1, 0, std::min(5, txhpx - 1), false, false, false },
      // Negative ref_stride and negative dst_stride with partial border.
      { 1, 1, 256, 256, false, false, true },
      { 1, 1, txwpx - 1, txhpx + 3, false, false, true },
    };

    for (size_t b = 0;
         b < sizeof(kBorders) / sizeof(kBorders[0]) && !HasFatalFailure();
         ++b) {
      const BorderCase &bc = kBorders[b];
      // Perturb a few neighbor samples per case while avoiding full refills.
      ref_storage[(b * 37 + tx * 13) % (ref_storage.size() - 16)] ^=
          static_cast<uint8_t>((0x5au + b + static_cast<size_t>(tx)) & 0xffu);

      // Negative ref_stride across all modes (plus negative dst_stride tested
      // separately below on reconintra.c's direct DC/V/H/fill paths, since
      // pre-existing intrapred_ssse3.c shifts dst_stride << 1).
      const ptrdiff_t r_stride = bc.neg_stride ? -kRefStride : kRefStride;
      const ptrdiff_t d_stride = kDstStride;
      // Offset ref by (b & 7) to test unaligned (odd, mod-4, mod-8, mod-16)
      // above_ref/left_ref pointers; offset dst by multiples of min(txwpx, 16).
      const size_t ref_off = 16 + (b & 7);
      const size_t dst_off =
          (txwpx == 4) ? (b & 3) * 4 : ((txwpx == 8) ? (b & 1) * 8 : 0);
      const uint8_t *const ref =
          bc.neg_stride
              ? &ref_storage[(kBufRows - kPadRows) * kRefStride + ref_off]
              : &ref_storage[kPadRows * kRefStride + ref_off];
      uint8_t *const act = &act_aligned_base[kPadRows * kDstStride + dst_off];

      mbmi = {};
      mbmi.bsize = txsize_to_bsize[tx_size];
      mbmi.partition = PARTITION_NONE;
      xd.up_available = bc.up_avail;
      xd.left_available = bc.left_avail;
      xd.plane[0].subsampling_x = 0;
      xd.plane[0].subsampling_y = 0;
      xd.mb_to_left_edge = 0;
      xd.mb_to_top_edge = 0;
      if (bc.right_avail_px >= 256 && bc.bottom_avail_px >= 256) {
        xd.mb_to_right_edge = 256 * 8;
        xd.mb_to_bottom_edge = 256 * 8;
      } else {
        xd.mb_to_right_edge = (bc.right_avail_px - txwpx) * 8;
        xd.mb_to_bottom_edge = (bc.bottom_avail_px - txhpx) * 8;
      }
      xd.tile.mi_col_end = bc.block_tr_tile ? tx_size_wide_unit[tx_size] : 64;
      xd.tile.mi_row_end = bc.block_bl_tile ? tx_size_high_unit[tx_size] : 64;

      const int n_top_px =
          bc.up_avail ? std::clamp(bc.right_avail_px, 0, txwpx) : 0;
      const int n_left_px =
          bc.left_avail ? std::clamp(bc.bottom_avail_px, 0, txhpx) : 0;
      const int xr = bc.right_avail_px - txwpx;
      const int yd = bc.bottom_avail_px - txhpx;
      const int n_topright_px = (n_top_px > 0 && xr > 0 && !bc.block_tr_tile)
                                    ? std::min(xr, txwpx)
                                    : 0;
      const int n_bottomleft_px = (n_left_px > 0 && yd > 0 && !bc.block_bl_tile)
                                      ? std::min(yd, txhpx)
                                      : 0;

      // 1. Non-directional modes.
      const PREDICTION_MODE kNonDirModes[] = {
        DC_PRED, SMOOTH_PRED, SMOOTH_V_PRED, SMOOTH_H_PRED, PAETH_PRED,
      };
      for (PREDICTION_MODE mode : kNonDirModes) {
        av1_predict_intra_block(&xd, BLOCK_128X128, 1, txwpx, txhpx, tx_size,
                                mode, 0, /*use_palette=*/0, FILTER_INTRA_MODES,
                                ref, static_cast<int>(r_stride), act,
                                static_cast<int>(d_stride), 0, 0, 0);
        RefPredictIntraBlock8B(tabs, ref, r_stride, exp_storage.data(), txwpx,
                               tx_size, mode, 0, FILTER_INTRA_MODES, 1, 0,
                               n_top_px, 0, n_left_px, 0);
        CheckBlockEq(exp_storage.data(), txwpx, act, d_stride, txwpx, txhpx,
                     "predict_nondir", tx, mode, static_cast<int>(b), 0);
        if (HasFatalFailure()) return;
      }

      if (bc.neg_stride) {
        // Exercise negative dst_stride directly on reconintra.c's own
        // DC / V_PRED / H_PRED / fill_block_lbd writers.
        uint8_t *const act_neg =
            &act_aligned_base[(kBufRows - kPadRows) * kDstStride + dst_off];
        for (PREDICTION_MODE mode : { DC_PRED, V_PRED, H_PRED }) {
          av1_predict_intra_block(&xd, BLOCK_128X128, 1, txwpx, txhpx, tx_size,
                                  mode, 0, 0, FILTER_INTRA_MODES, ref,
                                  static_cast<int>(r_stride), act_neg,
                                  -kDstStride, 0, 0, 0);
          RefPredictIntraBlock8B(tabs, ref, r_stride, exp_storage.data(), txwpx,
                                 tx_size, mode, 0, FILTER_INTRA_MODES, 1, 0,
                                 n_top_px, 0, n_left_px, 0);
          CheckBlockEq(exp_storage.data(), txwpx, act_neg, -kDstStride, txwpx,
                       txhpx, "predict_neg_dst", tx, mode, static_cast<int>(b),
                       0);
          if (HasFatalFailure()) return;
        }
      }

      // 2. FILTER_INTRA modes (valid for blocks with max dimension <= 32).
      if (std::max(txwpx, txhpx) <= 32) {
        for (int fm = 0; fm < FILTER_INTRA_MODES; ++fm) {
          const FILTER_INTRA_MODE fmode = static_cast<FILTER_INTRA_MODE>(fm);
          av1_predict_intra_block(&xd, BLOCK_128X128, 1, txwpx, txhpx, tx_size,
                                  DC_PRED, 0, 0, fmode, ref,
                                  static_cast<int>(r_stride), act,
                                  static_cast<int>(d_stride), 0, 0, 0);
          RefPredictIntraBlock8B(tabs, ref, r_stride, exp_storage.data(), txwpx,
                                 tx_size, DC_PRED, 0, fmode, 1, 0, n_top_px, 0,
                                 n_left_px, 0);
          CheckBlockEq(exp_storage.data(), txwpx, act, d_stride, txwpx, txhpx,
                       "predict_fintra", tx, DC_PRED, static_cast<int>(b), fm);
          if (HasFatalFailure()) return;
        }
      }

      // 3. Directional modes (Z1, 90, Z2, 180, Z3) across angle_delta -3..+3,
      // edge-filter disabled + enabled (types 0 and 1 -> strengths 0..3 &
      // upsampling).
      const bool full_ef_sweep = (b == 0 || b == 2 || b == 6);
      for (int m = V_PRED; m <= D67_PRED; ++m) {
        const PREDICTION_MODE mode = static_cast<PREDICTION_MODE>(m);
        for (int delta = -3; delta <= 3; ++delta) {
          const int deg = delta * ANGLE_STEP;
          const int ef_end = full_ef_sweep ? 3 : 1;
          for (int ef_cfg = 0; ef_cfg < ef_end; ++ef_cfg) {
            const int chosen =
                full_ef_sweep ? ef_cfg
                              : (static_cast<int>(b) + m + (delta + 3)) % 3;
            const int enable_ef = (chosen > 0) ? 1 : 0;
            const int ef_type = (chosen == 2) ? 1 : 0;
            xd.above_mbmi = ef_type ? &neighbor_smooth : &neighbor_plain;
            xd.left_mbmi = &neighbor_plain;

            av1_predict_intra_block(&xd, BLOCK_128X128, enable_ef, txwpx, txhpx,
                                    tx_size, mode, deg, 0, FILTER_INTRA_MODES,
                                    ref, static_cast<int>(r_stride), act,
                                    static_cast<int>(d_stride), 0, 0, 0);
            RefPredictIntraBlock8B(
                tabs, ref, r_stride, exp_storage.data(), txwpx, tx_size, mode,
                deg, FILTER_INTRA_MODES, enable_ef, ef_type, n_top_px,
                n_topright_px, n_left_px, n_bottomleft_px);
            CheckBlockEq(exp_storage.data(), txwpx, act, d_stride, txwpx, txhpx,
                         "predict_dir", tx, mode, static_cast<int>(b),
                         delta * 10 + chosen);
            if (HasFatalFailure()) return;
          }
        }
      }
    }
  }

  // 4. Test av1_predict_intra_block_facade (in-place pd->dst.buf) across luma
  // (plane=0) and chroma (plane=1,2; 4:4:4 & 4:2:0), blk_col/blk_row == 0 and
  // non-zero, partial borders, guard-byte checks, and CfL (uncached + cached).
  constexpr int kFacadeStride = 208;
  std::vector<uint8_t> canvas_init((MAX_TX_SIZE * 2 + 24) * kFacadeStride +
                                   128);
  std::vector<uint8_t> canvas(canvas_init.size());
  const size_t canvas_align_pad =
      (32u - (reinterpret_cast<uintptr_t>(canvas.data()) & 31u)) & 31u;
  for (size_t i = 0; i < canvas_init.size(); ++i) canvas_init[i] = rng.Rand8();

  struct FacadeBorder {
    int up_avail;
    int left_avail;
    int blk_col;
    int blk_row;
    int crop_right;  // >= 0: visible px inside/beyond txwpx; < 0: full interior
    int crop_bottom;  // >= 0: visible px inside/beyond txhpx; < 0: full
                      // interior
  };

  for (int plane_cfg = 0; plane_cfg < 3 && !HasFatalFailure(); ++plane_cfg) {
    // 0: luma (plane 0), 1: chroma U 4:4:4, 2: chroma V 4:2:0.
    const int plane = (plane_cfg == 0) ? 0 : plane_cfg;
    const int ss_x = (plane_cfg == 2) ? 1 : 0;
    const int ss_y = (plane_cfg == 2) ? 1 : 0;

    for (int tx = 0; tx < TX_SIZES_ALL && !HasFatalFailure(); ++tx) {
      const TX_SIZE tx_size = static_cast<TX_SIZE>(tx);
      if (tabs.pred[V_PRED][tx_size] == nullptr) continue;
      const int txwpx = tx_size_wide[tx_size];
      const int txhpx = tx_size_high[tx_size];
      if (ss_x && (txwpx == 64 || txhpx == 64)) continue;

      const int nz_col = tx_size_wide_unit[tx_size];
      const int nz_row = tx_size_high_unit[tx_size];
      const FacadeBorder kFBorders[] = {
        { 1, 1, 0, 0, -1, -1 },
        { 0, 0, nz_col, nz_row, -1, -1 },  // Non-zero blk_col/row -> avail!
        { 0, 1, 0, 0, -1, -1 },
        { 1, 0, 0, 0, -1, -1 },
        { 0, 0, 0, 0, -1, -1 },
        { 1, 1, 0, 0, std::min(3, txwpx - 1), txhpx + 2 },
        { 1, 1, 0, 0, txwpx - 1, std::min(5, txhpx - 1) },
        { 1, 1, nz_col, nz_row, 1, 0 },
      };

      for (size_t fb_idx = 0;
           fb_idx < sizeof(kFBorders) / sizeof(kFBorders[0]) &&
           !HasFatalFailure();
           ++fb_idx) {
        const FacadeBorder &fb = kFBorders[fb_idx];
        const int x = fb.blk_col * MI_SIZE;
        const int y = fb.blk_row * MI_SIZE;
        // Use BLOCK_128X128 when blk_col/row > 0 so tx fits inside the block.
        if (fb.blk_col > 0 &&
            (x + txwpx > (128 >> ss_x) || y + txhpx > (128 >> ss_y))) {
          continue;
        }

        mbmi = {};
        mbmi.bsize =
            (fb.blk_col > 0 || ss_x) ? BLOCK_128X128 : txsize_to_bsize[tx_size];
        mbmi.partition = PARTITION_NONE;
        xd.up_available = fb.up_avail;
        xd.left_available = fb.left_avail;
        xd.chroma_up_available = fb.up_avail;
        xd.chroma_left_available = fb.left_avail;
        xd.mb_to_left_edge = 0;
        xd.mb_to_top_edge = 0;
        xd.tile.mi_col_end = 64;
        xd.tile.mi_row_end = 64;

        auto &pd = xd.plane[plane];
        pd.subsampling_x = ss_x;
        pd.subsampling_y = ss_y;
        pd.width = std::max(x + txwpx, block_size_wide[mbmi.bsize] >> ss_x);
        pd.height = std::max(y + txhpx, block_size_high[mbmi.bsize] >> ss_y);
        pd.dst.stride = kFacadeStride;

        const bool have_top = (fb.blk_row | fb.up_avail) != 0;
        const bool have_left = (fb.blk_col | fb.left_avail) != 0;
        int n_top_px = have_top ? txwpx : 0;
        int n_left_px = have_left ? txhpx : 0;
        int xr = 256;
        int yd = 256;
        if (fb.crop_right < 0 && fb.crop_bottom < 0) {
          xd.mb_to_right_edge = 256 * 8;
          xd.mb_to_bottom_edge = 256 * 8;
        } else {
          const int r_px = fb.crop_right;
          const int b_px = fb.crop_bottom;
          xd.mb_to_right_edge = (r_px + x - pd.width) * (8 << ss_x);
          xd.mb_to_bottom_edge = (b_px + y - pd.height) * (8 << ss_y);
          n_top_px = have_top ? std::clamp(r_px, 0, txwpx) : 0;
          n_left_px = have_left ? std::clamp(b_px, 0, txhpx) : 0;
          xr = r_px - txwpx;
          yd = b_px - txhpx;
        }

        // Evaluate top-right / bottom-left availability matching BLOCK_128X128
        // / txsize_to_bsize at (mi_row=0, mi_col=0).
        const int plane_bw64 = mi_size_wide[BLOCK_64X64] >> ss_x;
        const int plane_bh64 = mi_size_high[BLOCK_64X64] >> ss_y;
        bool tr_ok = true;
        if (fb.blk_row > 0) {
          if (fb.blk_row == plane_bh64 &&
              fb.blk_col + tx_size_wide_unit[tx_size] == plane_bw64) {
            tr_ok = true;
          } else {
            const int col64 = fb.blk_col & (plane_bw64 - 1);
            tr_ok = (col64 + tx_size_wide_unit[tx_size] < plane_bw64);
          }
        }
        bool bl_ok = (fb.blk_col == 0);
        if (fb.blk_col > 0 && (fb.blk_col & (plane_bw64 - 1)) == 0) {
          const int row64 = fb.blk_row & (plane_bh64 - 1);
          bl_ok = (row64 + tx_size_high_unit[tx_size] < plane_bh64);
        }
        const int n_topright_px =
            (n_top_px > 0 && xr > 0 && tr_ok) ? std::min(xr, txwpx) : 0;
        const int n_bottomleft_px =
            (n_left_px > 0 && yd > 0 && bl_ok) ? std::min(yd, txhpx) : 0;

        const size_t base_off = canvas_align_pad + 4 * kFacadeStride + 32;
        const ptrdiff_t blk_off =
            (static_cast<ptrdiff_t>(fb.blk_row) * kFacadeStride + fb.blk_col) *
            MI_SIZE;
        const uint8_t *const ref_blk = &canvas_init[base_off] + blk_off;

        auto run_facade_case = [&](PREDICTION_MODE mode, int delta,
                                   FILTER_INTRA_MODE fmode, int enable_ef,
                                   int ef_type) {
          memcpy(canvas.data(), canvas_init.data(), canvas.size());
          pd.dst.buf = &canvas[base_off];
          seq_params.enable_intra_edge_filter = enable_ef;
          xd.above_mbmi = ef_type ? &neighbor_smooth : &neighbor_plain;
          xd.left_mbmi = &neighbor_plain;
          xd.chroma_above_mbmi = xd.above_mbmi;
          xd.chroma_left_mbmi = xd.left_mbmi;
          mbmi.filter_intra_mode_info.use_filter_intra =
              (plane == 0 && fmode != FILTER_INTRA_MODES) ? 1 : 0;
          mbmi.filter_intra_mode_info.filter_intra_mode = fmode;
          if (plane == 0) {
            mbmi.mode = mode;
            mbmi.angle_delta[PLANE_TYPE_Y] = delta;
          } else {
            mbmi.uv_mode = static_cast<UV_PREDICTION_MODE>(mode);
            mbmi.angle_delta[PLANE_TYPE_UV] = delta;
          }

          av1_predict_intra_block_facade(cm.get(), &xd, plane, fb.blk_col,
                                         fb.blk_row, tx_size);
          RefPredictIntraBlock8B(
              tabs, ref_blk, kFacadeStride, exp_storage.data(), txwpx, tx_size,
              mode, delta * ANGLE_STEP, fmode, enable_ef, ef_type, n_top_px,
              n_topright_px, n_left_px, n_bottomleft_px);
          uint8_t *const dst_blk = pd.dst.buf + blk_off;
          CheckBlockEq(exp_storage.data(), txwpx, dst_blk, kFacadeStride, txwpx,
                       txhpx, "facade", tx, mode,
                       plane_cfg * 100 + static_cast<int>(fb_idx),
                       delta * 10 + enable_ef + ef_type);
          if (HasFatalFailure()) return;
          // Verify surrounding neighbor/right-margin bytes were untouched.
          ASSERT_EQ(dst_blk[-kFacadeStride], ref_blk[-kFacadeStride]);
          ASSERT_EQ(dst_blk[-1], ref_blk[-1]);
          ASSERT_EQ(dst_blk[txwpx], ref_blk[txwpx]);
          ASSERT_EQ(dst_blk[txhpx * kFacadeStride],
                    ref_blk[txhpx * kFacadeStride]);
        };

        const PREDICTION_MODE kAllModes[] = {
          DC_PRED,       V_PRED,        H_PRED,     D45_PRED, D135_PRED,
          D113_PRED,     D157_PRED,     D203_PRED,  D67_PRED, SMOOTH_PRED,
          SMOOTH_V_PRED, SMOOTH_H_PRED, PAETH_PRED,
        };
        for (PREDICTION_MODE mode : kAllModes) {
          if (!av1_is_directional_mode(mode)) {
            run_facade_case(mode, 0, FILTER_INTRA_MODES, 1, 0);
          } else {
            for (int delta : { -3, 0, 2 }) {
              const int ef =
                  (static_cast<int>(fb_idx) + mode + (delta + 3)) % 3;
              run_facade_case(mode, delta, FILTER_INTRA_MODES, ef > 0 ? 1 : 0,
                              ef == 2 ? 1 : 0);
              if (HasFatalFailure()) return;
            }
          }
          if (HasFatalFailure()) return;
        }
        if (plane == 0 && std::max(txwpx, txhpx) <= 32) {
          for (int fm = 0; fm < FILTER_INTRA_MODES; ++fm) {
            run_facade_case(DC_PRED, 0, static_cast<FILTER_INTRA_MODE>(fm), 1,
                            0);
            if (HasFatalFailure()) return;
          }
        }
      }
    }
  }

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
  // 5. Test UV_CFL_PRED via av1_predict_intra_block_facade across all valid CfL
  // TX sizes (<= 32x32), partial borders, alpha == 0 and alpha != 0, uncached,
  // dc_pred_cache populate, and dc_pred_cache hit (verifying the SIMD DC0
  // fast path and scalar fallback).
  DECLARE_ALIGNED(32, uint16_t, luma_q3[CFL_BUF_SQUARE]);
  for (int i = 0; i < CFL_BUF_SQUARE; ++i) {
    luma_q3[i] = static_cast<uint16_t>(rng(256 * 8));
  }
  for (int ss : { 0, 1 }) {
    xd.plane[AOM_PLANE_U].subsampling_x = ss;
    xd.plane[AOM_PLANE_U].subsampling_y = ss;
    xd.plane[AOM_PLANE_V].subsampling_x = ss;
    xd.plane[AOM_PLANE_V].subsampling_y = ss;
    seq_params.subsampling_x = ss;
    seq_params.subsampling_y = ss;

    for (int tx = 0; tx < TX_SIZES_ALL && !HasFatalFailure(); ++tx) {
      const TX_SIZE tx_size = static_cast<TX_SIZE>(tx);
      if (tabs.pred[V_PRED][tx_size] == nullptr) continue;
      const int txwpx = tx_size_wide[tx_size];
      const int txhpx = tx_size_high[tx_size];
      if ((txwpx << ss) > 32 || (txhpx << ss) > 32) continue;

      BLOCK_SIZE cfl_bsize = BLOCK_INVALID;
      for (int bs = 0; bs < BLOCK_SIZES_ALL; ++bs) {
        const auto cand = static_cast<BLOCK_SIZE>(bs);
        if (block_size_wide[cand] == (txwpx << ss) &&
            block_size_high[cand] == (txhpx << ss)) {
          cfl_bsize = cand;
          break;
        }
      }
      if (cfl_bsize == BLOCK_INVALID) continue;

      for (int b_cfg = 0; b_cfg < 5 && !HasFatalFailure(); ++b_cfg) {
        mbmi = {};
        mbmi.bsize = cfl_bsize;
        mbmi.uv_mode = UV_CFL_PRED;
        xd.chroma_up_available = (b_cfg != 1 && b_cfg != 3);
        xd.chroma_left_available = (b_cfg != 2 && b_cfg != 3);
        xd.up_available = xd.chroma_up_available;
        xd.left_available = xd.chroma_left_available;
        xd.mb_to_left_edge = 0;
        xd.mb_to_top_edge = 0;
        int n_top_px = xd.chroma_up_available ? txwpx : 0;
        int n_left_px = xd.chroma_left_available ? txhpx : 0;
        int buf_w = txwpx;
        int buf_h = txhpx;
        if (b_cfg == 4) {
          const int r_px = std::max(1, txwpx - 3);
          const int b_px = std::max(1, txhpx - 1);
          xd.mb_to_right_edge = (r_px - txwpx) * (8 << ss);
          xd.mb_to_bottom_edge = (b_px - txhpx) * (8 << ss);
          n_top_px = r_px;
          n_left_px = b_px;
          buf_w = r_px;
          buf_h = b_px;
        } else {
          xd.mb_to_right_edge = 256 * 8;
          xd.mb_to_bottom_edge = 256 * 8;
        }

        for (int plane : { AOM_PLANE_U, AOM_PLANE_V }) {
          auto &pd = xd.plane[plane];
          pd.width = txwpx;
          pd.height = txhpx;
          pd.dst.stride = kFacadeStride;
          const size_t base_off = canvas_align_pad + 4 * kFacadeStride + 32;
          const uint8_t *const orig_ref = &canvas_init[base_off];

          // (a) Uncached CfL with zero alpha on one plane and non-zero on the
          // other; (b) cache-store; (c) cache-hit after clobbering neighbors.
          cfl_init(&xd.cfl, &seq_params);
          memcpy(xd.cfl.recon_buf_q3, luma_q3, sizeof(luma_q3));
          xd.cfl.buf_width = buf_w;
          xd.cfl.buf_height = buf_h;
          xd.cfl.are_parameters_computed = 0;
          xd.cfl.use_dc_pred_cache = false;
          xd.cfl.dc_pred_is_cached[plane - 1] = false;
          const int sign_u = (b_cfg % 3 == 0) ? CFL_SIGN_ZERO : CFL_SIGN_POS;
          const int sign_v = (b_cfg % 3 == 1) ? CFL_SIGN_ZERO : CFL_SIGN_NEG;
          mbmi.cfl_alpha_signs =
              static_cast<int8_t>(sign_u * CFL_SIGNS + sign_v - 1);
          mbmi.cfl_alpha_idx = static_cast<uint8_t>((tx * 17 + b_cfg * 29) &
                                                    (CFL_ALPHABET_SIZE - 1));

          memcpy(canvas.data(), canvas_init.data(), canvas.size());
          pd.dst.buf = &canvas[base_off];
          av1_predict_intra_block_facade(cm.get(), &xd, plane, 0, 0, tx_size);
          RefCflPredict8B(tabs, orig_ref, kFacadeStride, n_top_px, n_left_px,
                          luma_q3, buf_w, buf_h, tx_size, mbmi.cfl_alpha_idx,
                          mbmi.cfl_alpha_signs, plane, exp_storage.data(),
                          txwpx);
          CheckBlockEq(exp_storage.data(), txwpx, pd.dst.buf, kFacadeStride,
                       txwpx, txhpx, "cfl_uncached", tx, UV_CFL_PRED, b_cfg,
                       plane + ss * 10);
          if (HasFatalFailure()) return;

          // (b) Populate dc_pred_cache.
          cfl_init(&xd.cfl, &seq_params);
          memcpy(xd.cfl.recon_buf_q3, luma_q3, sizeof(luma_q3));
          xd.cfl.buf_width = buf_w;
          xd.cfl.buf_height = buf_h;
          xd.cfl.use_dc_pred_cache = true;
          mbmi.cfl_alpha_signs =
              static_cast<int8_t>(CFL_SIGN_POS * CFL_SIGNS + CFL_SIGN_NEG - 1);
          mbmi.cfl_alpha_idx = static_cast<uint8_t>(0x6b);
          memcpy(canvas.data(), canvas_init.data(), canvas.size());
          pd.dst.buf = &canvas[base_off];
          av1_predict_intra_block_facade(cm.get(), &xd, plane, 0, 0, tx_size);
          ASSERT_TRUE(xd.cfl.dc_pred_is_cached[plane - 1]);
          RefCflPredict8B(tabs, orig_ref, kFacadeStride, n_top_px, n_left_px,
                          luma_q3, buf_w, buf_h, tx_size, mbmi.cfl_alpha_idx,
                          mbmi.cfl_alpha_signs, plane, exp_storage.data(),
                          txwpx);
          CheckBlockEq(exp_storage.data(), txwpx, pd.dst.buf, kFacadeStride,
                       txwpx, txhpx, "cfl_cache_store", tx, UV_CFL_PRED, b_cfg,
                       plane + ss * 10);
          if (HasFatalFailure()) return;

          // (c) Cache hit with clobbered frame neighbors and new alpha (incl.
          // alpha == 0 where output must equal pure cached DC!).
          memset(canvas.data(), 0xa5, canvas.size());
          pd.dst.buf = &canvas[base_off];
          const int hit_su =
              (plane == AOM_PLANE_U) ? CFL_SIGN_ZERO : CFL_SIGN_NEG;
          const int hit_sv =
              (plane == AOM_PLANE_V) ? CFL_SIGN_ZERO : CFL_SIGN_POS;
          mbmi.cfl_alpha_signs =
              static_cast<int8_t>(hit_su * CFL_SIGNS + hit_sv - 1);
          mbmi.cfl_alpha_idx = static_cast<uint8_t>(0xd2);
          av1_predict_intra_block_facade(cm.get(), &xd, plane, 0, 0, tx_size);
          RefCflPredict8B(tabs, orig_ref, kFacadeStride, n_top_px, n_left_px,
                          luma_q3, buf_w, buf_h, tx_size, mbmi.cfl_alpha_idx,
                          mbmi.cfl_alpha_signs, plane, exp_storage.data(),
                          txwpx);
          CheckBlockEq(exp_storage.data(), txwpx, pd.dst.buf, kFacadeStride,
                       txwpx, txhpx, "cfl_cache_hit", tx, UV_CFL_PRED, b_cfg,
                       plane + ss * 10);
          if (HasFatalFailure()) return;
        }
      }
    }
  }
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
}

#endif  // !CONFIG_SHARED

}  // namespace
