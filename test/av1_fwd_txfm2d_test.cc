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

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tuple>
#include <vector>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "test/acm_random.h"
#include "test/util.h"
#include "test/av1_txfm_test.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/scan.h"
#include "av1/encoder/hybrid_fwd_txfm.h"

using libaom_test::ACMRandom;
using libaom_test::bd;
using libaom_test::compute_avg_abs_error;
using libaom_test::input_base;
using libaom_test::tx_type_name;
using libaom_test::TYPE_TXFM;

using std::vector;

namespace {
// tx_type_, tx_size_, max_error_, max_avg_error_
using AV1FwdTxfm2dParam = std::tuple<TX_TYPE, TX_SIZE, double, double>;

class AV1FwdTxfm2d : public ::testing::TestWithParam<AV1FwdTxfm2dParam> {
 public:
  void SetUp() override {
    tx_type_ = GET_PARAM(0);
    tx_size_ = GET_PARAM(1);
    max_error_ = GET_PARAM(2);
    max_avg_error_ = GET_PARAM(3);
    count_ = 500;
    TXFM_2D_FLIP_CFG fwd_txfm_flip_cfg;
    av1_get_fwd_txfm_cfg(tx_type_, tx_size_, &fwd_txfm_flip_cfg);
    amplify_factor_ = libaom_test::get_amplification_factor(tx_type_, tx_size_);
    tx_width_ = tx_size_wide[fwd_txfm_flip_cfg.tx_size];
    tx_height_ = tx_size_high[fwd_txfm_flip_cfg.tx_size];
    ud_flip_ = fwd_txfm_flip_cfg.ud_flip;
    lr_flip_ = fwd_txfm_flip_cfg.lr_flip;

    fwd_txfm_ = libaom_test::fwd_txfm_func_ls[tx_size_];
    txfm2d_size_ = tx_width_ * tx_height_;
    input_ = reinterpret_cast<int16_t *>(
        aom_memalign(16, sizeof(input_[0]) * txfm2d_size_));
    ASSERT_NE(input_, nullptr);
    output_ = reinterpret_cast<int32_t *>(
        aom_memalign(16, sizeof(output_[0]) * txfm2d_size_));
    ASSERT_NE(output_, nullptr);
    ref_input_ = reinterpret_cast<double *>(
        aom_memalign(16, sizeof(ref_input_[0]) * txfm2d_size_));
    ASSERT_NE(ref_input_, nullptr);
    ref_output_ = reinterpret_cast<double *>(
        aom_memalign(16, sizeof(ref_output_[0]) * txfm2d_size_));
    ASSERT_NE(ref_output_, nullptr);
  }

  void RunFwdAccuracyCheck() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    double avg_abs_error = 0;
    for (int ci = 0; ci < count_; ci++) {
      for (int ni = 0; ni < txfm2d_size_; ++ni) {
        input_[ni] = rnd.Rand16() % input_base;
        ref_input_[ni] = static_cast<double>(input_[ni]);
        output_[ni] = 0;
        ref_output_[ni] = 0;
      }

      fwd_txfm_(input_, output_, tx_width_, tx_type_, bd);

      if (lr_flip_ && ud_flip_) {
        libaom_test::fliplrud(ref_input_, tx_width_, tx_height_, tx_width_);
      } else if (lr_flip_) {
        libaom_test::fliplr(ref_input_, tx_width_, tx_height_, tx_width_);
      } else if (ud_flip_) {
        libaom_test::flipud(ref_input_, tx_width_, tx_height_, tx_width_);
      }

      libaom_test::reference_hybrid_2d(ref_input_, ref_output_, tx_type_,
                                       tx_size_);

      double actual_max_error = 0;
      for (int ni = 0; ni < txfm2d_size_; ++ni) {
        ref_output_[ni] = round(ref_output_[ni]);
        const double this_error =
            fabs(output_[ni] - ref_output_[ni]) / amplify_factor_;
        actual_max_error = AOMMAX(actual_max_error, this_error);
      }
      EXPECT_GE(max_error_, actual_max_error)
          << "tx_w: " << tx_width_ << " tx_h: " << tx_height_
          << ", tx_type = " << (int)tx_type_;
      if (actual_max_error > max_error_) {  // exit early.
        break;
      }

      avg_abs_error += compute_avg_abs_error<int32_t, double>(
          output_, ref_output_, txfm2d_size_);
    }

    avg_abs_error /= amplify_factor_;
    avg_abs_error /= count_;
    EXPECT_GE(max_avg_error_, avg_abs_error)
        << "tx_size = " << tx_size_ << ", tx_type = " << tx_type_;
  }

  void TearDown() override {
    aom_free(input_);
    aom_free(output_);
    aom_free(ref_input_);
    aom_free(ref_output_);
  }

 private:
  double max_error_;
  double max_avg_error_;
  int count_;
  double amplify_factor_;
  TX_TYPE tx_type_;
  TX_SIZE tx_size_;
  int tx_width_;
  int tx_height_;
  int txfm2d_size_;
  FwdTxfm2dFunc fwd_txfm_;
  int16_t *input_;
  int32_t *output_;
  double *ref_input_;
  double *ref_output_;
  int ud_flip_;  // flip upside down
  int lr_flip_;  // flip left to right
};

static constexpr double avg_error_ls[TX_SIZES_ALL] = {
  0.5,   // 4x4 transform
  0.5,   // 8x8 transform
  1.2,   // 16x16 transform
  6.1,   // 32x32 transform
  3.4,   // 64x64 transform
  0.57,  // 4x8 transform
  0.68,  // 8x4 transform
  0.92,  // 8x16 transform
  1.1,   // 16x8 transform
  4.1,   // 16x32 transform
  6,     // 32x16 transform
  3.5,   // 32x64 transform
  5.7,   // 64x32 transform
  0.6,   // 4x16 transform
  0.9,   // 16x4 transform
  1.2,   // 8x32 transform
  1.7,   // 32x8 transform
  2.0,   // 16x64 transform
  4.7,   // 64x16 transform
};

static constexpr double max_error_ls[TX_SIZES_ALL] = {
  3,    // 4x4 transform
  5,    // 8x8 transform
  11,   // 16x16 transform
  70,   // 32x32 transform
  64,   // 64x64 transform
  3.9,  // 4x8 transform
  4.3,  // 8x4 transform
  12,   // 8x16 transform
  12,   // 16x8 transform
  32,   // 16x32 transform
  46,   // 32x16 transform
  136,  // 32x64 transform
  136,  // 64x32 transform
  5,    // 4x16 transform
  6,    // 16x4 transform
  21,   // 8x32 transform
  13,   // 32x8 transform
  30,   // 16x64 transform
  36,   // 64x16 transform
};

vector<AV1FwdTxfm2dParam> GetTxfm2dParamList() {
  vector<AV1FwdTxfm2dParam> param_list;
  for (int s = 0; s < TX_SIZES; ++s) {
    const double max_error = max_error_ls[s];
    const double avg_error = avg_error_ls[s];
    for (int t = 0; t < TX_TYPES; ++t) {
      const TX_TYPE tx_type = static_cast<TX_TYPE>(t);
      const TX_SIZE tx_size = static_cast<TX_SIZE>(s);
      if (libaom_test::IsTxSizeTypeValid(tx_size, tx_type)) {
        param_list.push_back(
            AV1FwdTxfm2dParam(tx_type, tx_size, max_error, avg_error));
      }
    }
  }
  return param_list;
}

INSTANTIATE_TEST_SUITE_P(C, AV1FwdTxfm2d,
                         ::testing::ValuesIn(GetTxfm2dParamList()));

TEST_P(AV1FwdTxfm2d, RunFwdAccuracyCheck) { RunFwdAccuracyCheck(); }

TEST(AV1FwdTxfm2d, CfgTest) {
  for (int bd_idx = 0; bd_idx < BD_NUM; ++bd_idx) {
    int bd = libaom_test::bd_arr[bd_idx];
    int8_t low_range = libaom_test::low_range_arr[bd_idx];
    int8_t high_range = libaom_test::high_range_arr[bd_idx];
    for (int tx_size = 0; tx_size < TX_SIZES_ALL; ++tx_size) {
      for (int tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
        if (libaom_test::IsTxSizeTypeValid(static_cast<TX_SIZE>(tx_size),
                                           static_cast<TX_TYPE>(tx_type)) ==
            false) {
          continue;
        }
        TXFM_2D_FLIP_CFG cfg;
        av1_get_fwd_txfm_cfg(static_cast<TX_TYPE>(tx_type),
                             static_cast<TX_SIZE>(tx_size), &cfg);
        int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
        int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
        av1_gen_fwd_stage_range(stage_range_col, stage_range_row, &cfg, bd);
        libaom_test::txfm_stage_range_check(stage_range_col, cfg.stage_num_col,
                                            cfg.cos_bit_col, low_range,
                                            high_range);
        libaom_test::txfm_stage_range_check(stage_range_row, cfg.stage_num_row,
                                            cfg.cos_bit_row, low_range,
                                            high_range);
      }
    }
  }
}

using lowbd_fwd_txfm_func = void (*)(const int16_t *src_diff, tran_low_t *coeff,
                                     int diff_stride, TxfmParam *txfm_param);

void AV1FwdTxfm2dMatchTest(TX_SIZE tx_size, lowbd_fwd_txfm_func target_func) {
  const int bd = 8;
  TxfmParam param;
  memset(&param, 0, sizeof(param));
  const int rows = tx_size_high[tx_size];
  const int cols = tx_size_wide[tx_size];
  for (int tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
    if (libaom_test::IsTxSizeTypeValid(
            tx_size, static_cast<TX_TYPE>(tx_type)) == false) {
      continue;
    }

    FwdTxfm2dFunc ref_func = libaom_test::fwd_txfm_func_ls[tx_size];
    if (ref_func != nullptr) {
      DECLARE_ALIGNED(32, int16_t, input[64 * 64]) = { 0 };
      DECLARE_ALIGNED(32, int32_t, output[64 * 64]);
      DECLARE_ALIGNED(32, int32_t, ref_output[64 * 64]);
      int input_stride = 64;
      ACMRandom rnd(ACMRandom::DeterministicSeed());
      for (int cnt = 0; cnt < 500; ++cnt) {
        if (cnt == 0) {
          for (int c = 0; c < cols; ++c) {
            for (int r = 0; r < rows; ++r) {
              input[r * input_stride + c] = (1 << bd) - 1;
            }
          }
        } else {
          for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
              input[r * input_stride + c] = rnd.Rand16() % (1 << bd);
            }
          }
        }
        param.tx_type = (TX_TYPE)tx_type;
        param.tx_size = (TX_SIZE)tx_size;
        param.tx_set_type = EXT_TX_SET_ALL16;
        param.bd = bd;
        ref_func(input, ref_output, input_stride, (TX_TYPE)tx_type, bd);
        target_func(input, output, input_stride, &param);
        const int check_cols = AOMMIN(32, cols);
        const int check_rows = AOMMIN(32, rows * cols / check_cols);
        for (int r = 0; r < check_rows; ++r) {
          for (int c = 0; c < check_cols; ++c) {
            ASSERT_EQ(ref_output[r * check_cols + c],
                      output[r * check_cols + c])
                << "[" << r << "," << c << "] cnt:" << cnt
                << " tx_size: " << cols << "x" << rows
                << " tx_type: " << tx_type_name[tx_type];
          }
        }
      }
    }
  }
}

void AV1FwdTxfm2dSpeedTest(TX_SIZE tx_size, lowbd_fwd_txfm_func target_func) {
  TxfmParam param;
  memset(&param, 0, sizeof(param));
  const int rows = tx_size_high[tx_size];
  const int cols = tx_size_wide[tx_size];
  const int num_loops = 1000000 / (rows * cols);

  const int bd = 8;
  for (int tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
    if (libaom_test::IsTxSizeTypeValid(
            tx_size, static_cast<TX_TYPE>(tx_type)) == false) {
      continue;
    }

    FwdTxfm2dFunc ref_func = libaom_test::fwd_txfm_func_ls[tx_size];
    if (ref_func != nullptr) {
      DECLARE_ALIGNED(32, int16_t, input[64 * 64]) = { 0 };
      DECLARE_ALIGNED(32, int32_t, output[64 * 64]);
      DECLARE_ALIGNED(32, int32_t, ref_output[64 * 64]);
      int input_stride = 64;
      ACMRandom rnd(ACMRandom::DeterministicSeed());

      for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
          input[r * input_stride + c] = rnd.Rand16() % (1 << bd);
        }
      }

      param.tx_type = (TX_TYPE)tx_type;
      param.tx_size = (TX_SIZE)tx_size;
      param.tx_set_type = EXT_TX_SET_ALL16;
      param.bd = bd;

      aom_usec_timer ref_timer, test_timer;

      aom_usec_timer_start(&ref_timer);
      for (int i = 0; i < num_loops; ++i) {
        ref_func(input, ref_output, input_stride, (TX_TYPE)tx_type, bd);
      }
      aom_usec_timer_mark(&ref_timer);
      const int elapsed_time_c =
          static_cast<int>(aom_usec_timer_elapsed(&ref_timer));

      aom_usec_timer_start(&test_timer);
      for (int i = 0; i < num_loops; ++i) {
        target_func(input, output, input_stride, &param);
      }
      aom_usec_timer_mark(&test_timer);
      const int elapsed_time_simd =
          static_cast<int>(aom_usec_timer_elapsed(&test_timer));

      printf(
          "txfm_size[%2dx%-2d] \t txfm_type[%d] \t c_time=%d \t"
          "simd_time=%d \t gain=%d \n",
          rows, cols, tx_type, elapsed_time_c, elapsed_time_simd,
          (elapsed_time_c / elapsed_time_simd));
    }
  }
}

using LbdFwdTxfm2dParam = std::tuple<TX_SIZE, lowbd_fwd_txfm_func>;

class AV1FwdTxfm2dTest : public ::testing::TestWithParam<LbdFwdTxfm2dParam> {};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1FwdTxfm2dTest);

TEST_P(AV1FwdTxfm2dTest, match) {
  AV1FwdTxfm2dMatchTest(GET_PARAM(0), GET_PARAM(1));
}
TEST_P(AV1FwdTxfm2dTest, DISABLED_Speed) {
  AV1FwdTxfm2dSpeedTest(GET_PARAM(0), GET_PARAM(1));
}
TEST(AV1FwdTxfm2dTest, DCTScaleTest) {
  BitDepthInfo bd_info;
  bd_info.bit_depth = 8;
  bd_info.use_highbitdepth_buf = 0;
  DECLARE_ALIGNED(32, int16_t, src_diff[1024]);
  DECLARE_ALIGNED(32, tran_low_t, coeff[1024]);

  const TX_SIZE tx_size_list[4] = { TX_4X4, TX_8X8, TX_16X16, TX_32X32 };
  const int stride_list[4] = { 4, 8, 16, 32 };
  const int ref_scale_list[4] = { 64, 64, 64, 16 };

  for (int i = 0; i < 4; i++) {
    TX_SIZE tx_size = tx_size_list[i];
    int stride = stride_list[i];
    int array_size = stride * stride;

    for (int j = 0; j < array_size; j++) {
      src_diff[j] = 8;
      coeff[j] = 0;
    }

    av1_quick_txfm(/*use_hadamard=*/0, tx_size, bd_info, src_diff, stride,
                   coeff);

    double input_sse = 0;
    double output_sse = 0;
    for (int j = 0; j < array_size; j++) {
      input_sse += pow(src_diff[j], 2);
      output_sse += pow(coeff[j], 2);
    }

    double scale = output_sse / input_sse;

    EXPECT_NEAR(scale, ref_scale_list[i], 5);
  }
}
TEST(AV1FwdTxfm2dTest, HadamardScaleTest) {
  BitDepthInfo bd_info;
  bd_info.bit_depth = 8;
  bd_info.use_highbitdepth_buf = 0;
  DECLARE_ALIGNED(32, int16_t, src_diff[1024]);
  DECLARE_ALIGNED(32, tran_low_t, coeff[1024]);

  const TX_SIZE tx_size_list[4] = { TX_4X4, TX_8X8, TX_16X16, TX_32X32 };
  const int stride_list[4] = { 4, 8, 16, 32 };
  const int ref_scale_list[4] = { 1, 64, 64, 16 };

  for (int i = 0; i < 4; i++) {
    TX_SIZE tx_size = tx_size_list[i];
    int stride = stride_list[i];
    int array_size = stride * stride;

    for (int j = 0; j < array_size; j++) {
      src_diff[j] = 8;
      coeff[j] = 0;
    }

    av1_quick_txfm(/*use_hadamard=*/1, tx_size, bd_info, src_diff, stride,
                   coeff);

    double input_sse = 0;
    double output_sse = 0;
    for (int j = 0; j < array_size; j++) {
      input_sse += pow(src_diff[j], 2);
      output_sse += pow(coeff[j], 2);
    }

    double scale = output_sse / input_sse;

    EXPECT_NEAR(scale, ref_scale_list[i], 5);
  }
}
using ::testing::Combine;
using ::testing::Values;
using ::testing::ValuesIn;

#if AOM_ARCH_X86 && HAVE_SSE2
static constexpr TX_SIZE fwd_txfm_for_sse2[] = {
  TX_4X4,
  TX_8X8,
  TX_16X16,
  TX_32X32,
  // TX_64X64,
  TX_4X8,
  TX_8X4,
  TX_8X16,
  TX_16X8,
  TX_16X32,
  TX_32X16,
  // TX_32X64,
  // TX_64X32,
  TX_4X16,
  TX_16X4,
  TX_8X32,
  TX_32X8,
  TX_16X64,
  TX_64X16,
};

INSTANTIATE_TEST_SUITE_P(SSE2, AV1FwdTxfm2dTest,
                         Combine(ValuesIn(fwd_txfm_for_sse2),
                                 Values(av1_lowbd_fwd_txfm_sse2)));
#endif  // AOM_ARCH_X86 && HAVE_SSE2

#if HAVE_SSE4_1
static constexpr TX_SIZE fwd_txfm_for_sse41[] = {
  TX_4X4,  TX_8X8,  TX_16X16, TX_32X32, TX_64X64, TX_4X8,   TX_8X4,
  TX_8X16, TX_16X8, TX_16X32, TX_32X16, TX_32X64, TX_64X32, TX_4X16,
  TX_16X4, TX_8X32, TX_32X8,  TX_16X64, TX_64X16
};

INSTANTIATE_TEST_SUITE_P(SSE4_1, AV1FwdTxfm2dTest,
                         Combine(ValuesIn(fwd_txfm_for_sse41),
                                 Values(av1_lowbd_fwd_txfm_sse4_1)));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
static constexpr TX_SIZE fwd_txfm_for_avx2[] = {
  TX_4X4,  TX_8X8,  TX_16X16, TX_32X32, TX_64X64, TX_4X8,   TX_8X4,
  TX_8X16, TX_16X8, TX_16X32, TX_32X16, TX_32X64, TX_64X32, TX_4X16,
  TX_16X4, TX_8X32, TX_32X8,  TX_16X64, TX_64X16,
};

INSTANTIATE_TEST_SUITE_P(AVX2, AV1FwdTxfm2dTest,
                         Combine(ValuesIn(fwd_txfm_for_avx2),
                                 Values(av1_lowbd_fwd_txfm_avx2)));
#endif  // HAVE_AVX2

#if CONFIG_HIGHWAY && HAVE_AVX512
static constexpr TX_SIZE fwd_txfm_for_avx512[] = {
  TX_4X4,  TX_8X8,  TX_16X16, TX_32X32, TX_64X64, TX_4X8,   TX_8X4,
  TX_8X16, TX_16X8, TX_16X32, TX_32X16, TX_32X64, TX_64X32, TX_4X16,
  TX_16X4, TX_8X32, TX_32X8,  TX_16X64, TX_64X16,
};

INSTANTIATE_TEST_SUITE_P(AVX512, AV1FwdTxfm2dTest,
                         Combine(ValuesIn(fwd_txfm_for_avx512),
                                 Values(av1_lowbd_fwd_txfm_avx512)));
#endif  // HAVE_AVX512

#if HAVE_NEON

static constexpr TX_SIZE fwd_txfm_for_neon[] = {
  TX_4X4,  TX_8X8,  TX_16X16, TX_32X32, TX_64X64, TX_4X8,   TX_8X4,
  TX_8X16, TX_16X8, TX_16X32, TX_32X16, TX_32X64, TX_64X32, TX_4X16,
  TX_16X4, TX_8X32, TX_32X8,  TX_16X64, TX_64X16
};

INSTANTIATE_TEST_SUITE_P(NEON, AV1FwdTxfm2dTest,
                         Combine(ValuesIn(fwd_txfm_for_neon),
                                 Values(av1_lowbd_fwd_txfm_neon)));

#endif  // HAVE_NEON

using Highbd_fwd_txfm_func = void (*)(const int16_t *src_diff,
                                      tran_low_t *coeff, int diff_stride,
                                      TxfmParam *txfm_param);

void AV1HighbdFwdTxfm2dMatchTest(TX_SIZE tx_size,
                                 Highbd_fwd_txfm_func target_func) {
  const int bd_ar[2] = { 10, 12 };
  TxfmParam param;
  memset(&param, 0, sizeof(param));
  const int rows = tx_size_high[tx_size];
  const int cols = tx_size_wide[tx_size];
  for (int i = 0; i < 2; ++i) {
    const int bd = bd_ar[i];
    for (int tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
      if (libaom_test::IsTxSizeTypeValid(
              tx_size, static_cast<TX_TYPE>(tx_type)) == false) {
        continue;
      }

      FwdTxfm2dFunc ref_func = libaom_test::fwd_txfm_func_ls[tx_size];
      if (ref_func != nullptr) {
        DECLARE_ALIGNED(32, int16_t, input[64 * 64]) = { 0 };
        DECLARE_ALIGNED(32, int32_t, output[64 * 64]);
        DECLARE_ALIGNED(32, int32_t, ref_output[64 * 64]);
        int input_stride = 64;
        ACMRandom rnd(ACMRandom::DeterministicSeed());
        for (int cnt = 0; cnt < 500; ++cnt) {
          if (cnt == 0) {
            for (int r = 0; r < rows; ++r) {
              for (int c = 0; c < cols; ++c) {
                input[r * input_stride + c] = (1 << bd) - 1;
              }
            }
          } else {
            for (int r = 0; r < rows; ++r) {
              for (int c = 0; c < cols; ++c) {
                input[r * input_stride + c] = rnd.Rand16() % (1 << bd);
              }
            }
          }
          param.tx_type = (TX_TYPE)tx_type;
          param.tx_size = (TX_SIZE)tx_size;
          param.tx_set_type = EXT_TX_SET_ALL16;
          param.bd = bd;

          ref_func(input, ref_output, input_stride, (TX_TYPE)tx_type, bd);
          target_func(input, output, input_stride, &param);
          const int check_cols = AOMMIN(32, cols);
          const int check_rows = AOMMIN(32, rows * cols / check_cols);
          for (int r = 0; r < check_rows; ++r) {
            for (int c = 0; c < check_cols; ++c) {
              ASSERT_EQ(ref_output[c * check_rows + r],
                        output[c * check_rows + r])
                  << "[" << r << "," << c << "] cnt:" << cnt
                  << " tx_size: " << cols << "x" << rows
                  << " tx_type: " << tx_type;
            }
          }
        }
      }
    }
  }
}

void AV1HighbdFwdTxfm2dSpeedTest(TX_SIZE tx_size,
                                 Highbd_fwd_txfm_func target_func) {
  const int bd_ar[2] = { 10, 12 };
  TxfmParam param;
  memset(&param, 0, sizeof(param));
  const int rows = tx_size_high[tx_size];
  const int cols = tx_size_wide[tx_size];
  const int num_loops = 1000000 / (rows * cols);

  for (int i = 0; i < 2; ++i) {
    const int bd = bd_ar[i];
    for (int tx_type = 0; tx_type < TX_TYPES; ++tx_type) {
      if (libaom_test::IsTxSizeTypeValid(
              tx_size, static_cast<TX_TYPE>(tx_type)) == false) {
        continue;
      }

      FwdTxfm2dFunc ref_func = libaom_test::fwd_txfm_func_ls[tx_size];
      if (ref_func != nullptr) {
        DECLARE_ALIGNED(32, int16_t, input[64 * 64]) = { 0 };
        DECLARE_ALIGNED(32, int32_t, output[64 * 64]);
        DECLARE_ALIGNED(32, int32_t, ref_output[64 * 64]);
        int input_stride = 64;
        ACMRandom rnd(ACMRandom::DeterministicSeed());

        for (int r = 0; r < rows; ++r) {
          for (int c = 0; c < cols; ++c) {
            input[r * input_stride + c] = rnd.Rand16() % (1 << bd);
          }
        }

        param.tx_type = (TX_TYPE)tx_type;
        param.tx_size = (TX_SIZE)tx_size;
        param.tx_set_type = EXT_TX_SET_ALL16;
        param.bd = bd;

        aom_usec_timer ref_timer, test_timer;

        aom_usec_timer_start(&ref_timer);
        for (int j = 0; j < num_loops; ++j) {
          ref_func(input, ref_output, input_stride, (TX_TYPE)tx_type, bd);
        }
        aom_usec_timer_mark(&ref_timer);
        const int elapsed_time_c =
            static_cast<int>(aom_usec_timer_elapsed(&ref_timer));

        aom_usec_timer_start(&test_timer);
        for (int j = 0; j < num_loops; ++j) {
          target_func(input, output, input_stride, &param);
        }
        aom_usec_timer_mark(&test_timer);
        const int elapsed_time_simd =
            static_cast<int>(aom_usec_timer_elapsed(&test_timer));

        printf(
            "txfm_size[%2dx%-2d] \t txfm_type[%d] \t c_time=%d \t"
            "simd_time=%d \t gain=%d \n",
            cols, rows, tx_type, elapsed_time_c, elapsed_time_simd,
            (elapsed_time_c / elapsed_time_simd));
      }
    }
  }
}

using HighbdFwdTxfm2dParam = std::tuple<TX_SIZE, Highbd_fwd_txfm_func>;

class AV1HighbdFwdTxfm2dTest
    : public ::testing::TestWithParam<HighbdFwdTxfm2dParam> {};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AV1HighbdFwdTxfm2dTest);

TEST_P(AV1HighbdFwdTxfm2dTest, match) {
  AV1HighbdFwdTxfm2dMatchTest(GET_PARAM(0), GET_PARAM(1));
}

TEST_P(AV1HighbdFwdTxfm2dTest, DISABLED_Speed) {
  AV1HighbdFwdTxfm2dSpeedTest(GET_PARAM(0), GET_PARAM(1));
}

using ::testing::Combine;
using ::testing::Values;
using ::testing::ValuesIn;

#if HAVE_SSE4_1
static constexpr TX_SIZE Highbd_fwd_txfm_for_sse4_1[] = {
  TX_4X4,  TX_8X8,  TX_16X16, TX_32X32, TX_64X64, TX_4X8,   TX_8X4,
  TX_8X16, TX_16X8, TX_16X32, TX_32X16, TX_32X64, TX_64X32,
#if !CONFIG_REALTIME_ONLY
  TX_4X16, TX_16X4, TX_8X32,  TX_32X8,  TX_16X64, TX_64X16,
#endif  // !CONFIG_REALTIME_ONLY
};

INSTANTIATE_TEST_SUITE_P(SSE4_1, AV1HighbdFwdTxfm2dTest,
                         Combine(ValuesIn(Highbd_fwd_txfm_for_sse4_1),
                                 Values(av1_highbd_fwd_txfm)));
#endif  // HAVE_SSE4_1
#if HAVE_AVX2
static constexpr TX_SIZE Highbd_fwd_txfm_for_avx2[] = { TX_8X8,   TX_16X16,
                                                        TX_32X32, TX_64X64,
                                                        TX_8X16,  TX_16X8 };

INSTANTIATE_TEST_SUITE_P(AVX2, AV1HighbdFwdTxfm2dTest,
                         Combine(ValuesIn(Highbd_fwd_txfm_for_avx2),
                                 Values(av1_highbd_fwd_txfm)));
#endif  // HAVE_AVX2

#if HAVE_NEON
static constexpr TX_SIZE Highbd_fwd_txfm_for_neon[] = {
  TX_4X4,  TX_8X8,  TX_16X16, TX_32X32, TX_64X64, TX_4X8,   TX_8X4,
  TX_8X16, TX_16X8, TX_16X32, TX_32X16, TX_32X64, TX_64X32,
#if !CONFIG_REALTIME_ONLY
  TX_4X16, TX_16X4, TX_8X32,  TX_32X8,  TX_16X64, TX_64X16
#endif  // !CONFIG_REALTIME_ONLY
};

INSTANTIATE_TEST_SUITE_P(NEON, AV1HighbdFwdTxfm2dTest,
                         Combine(ValuesIn(Highbd_fwd_txfm_for_neon),
                                 Values(av1_highbd_fwd_txfm)));
#endif  // HAVE_NEON

#if CONFIG_AV1_ENCODER && HAVE_AVX2 && !CONFIG_HIGHWAY && !CONFIG_SHARED
TEST(AVX2, Av1LowbdFwdTxfm2dFuncAndNlMatchC) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(32, tran_low_t, out_fn[64 * 64]);
  DECLARE_ALIGNED(32, tran_low_t, out_nl[64 * 64]);
  DECLARE_ALIGNED(32, tran_low_t, out_avx2[64 * 64]);
  DECLARE_ALIGNED(32, tran_low_t, out_ref[64 * 64]);
  DECLARE_ALIGNED(32, int16_t, diff_buf[128 * 72]);

  for (int s = 0; s < TX_SIZES_ALL; ++s) {
    const TX_SIZE tx_size = static_cast<TX_SIZE>(s);
    const int rows = tx_size_high[tx_size];
    const int cols = tx_size_wide[tx_size];
    const int check_cols = AOMMIN(32, cols);
    const int check_rows = AOMMIN(32, rows * cols / check_cols);
    const int check_n = check_rows * check_cols;
    const FwdTxfm2dFunc fn = av1_lowbd_fwd_txfm2d_func_avx2(tx_size);
    ASSERT_NE(fn, nullptr);
    const FwdTxfm2dFunc ref_fn = libaom_test::fwd_txfm_func_ls[tx_size];
    const int strides[4] = { cols, AOMMAX(cols, 16), AOMMAX(cols, 64), 128 };

    for (int t = 0; t < TX_TYPES; ++t) {
      const TX_TYPE tx_type = static_cast<TX_TYPE>(t);
      if (!libaom_test::IsTxSizeTypeValid(tx_size, tx_type)) continue;

      TxfmParam param;
      memset(&param, 0, sizeof(param));
      param.tx_type = tx_type;
      param.tx_size = tx_size;
      param.tx_set_type = EXT_TX_SET_ALL16;
      param.bd = 8;
      param.lossless = 0;

      for (int si = 0; si < 4; ++si) {
        const int stride = strides[si];
        // Test non-32B-aligned src_diff rows on odd stride cases: 8B (+4, AV1
        // 4x4 sub-block offset) and 2B (+3) for all AVX2 and 4xH SSE2 kernels;
        // 16B (+8, addr % 32 == 16) for legacy 8/16/32-wide SSE2 delegates.
        const bool sse2_16b_aligned = tx_size == TX_8X4 || tx_size == TX_16X4 ||
                                      tx_size == TX_8X32 || tx_size == TX_32X8;
        const int in_off =
            (si & 1) ? (sse2_16b_aligned ? 8 : (si == 1 ? 4 : 3)) : 0;
        int16_t *const input = diff_buf + in_off;
        for (int pat = 0; pat < 6; ++pat) {
          for (size_t i = 0; i < sizeof(diff_buf) / sizeof(diff_buf[0]); ++i) {
            diff_buf[i] = static_cast<int16_t>(rnd.Rand8()) -
                          static_cast<int16_t>(rnd.Rand8());
          }
          for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
              int16_t v = 0;
              if (pat == 0) {
                v = 255;
              } else if (pat == 1) {
                v = -255;
              } else if (pat == 2) {
                v = 0;
              } else if (pat == 3) {
                v = ((r + c) & 1) ? 255 : -255;
              } else if (pat == 4) {
                v = static_cast<int16_t>(rnd.Rand8Extremes()) -
                    static_cast<int16_t>(rnd.Rand8Extremes());
              } else {
                v = static_cast<int16_t>(rnd.Rand8()) -
                    static_cast<int16_t>(rnd.Rand8());
              }
              input[r * stride + c] = v;
            }
          }

          memset(out_fn, 0xa5, sizeof(out_fn));
          memset(out_nl, 0x5a, sizeof(out_nl));
          memset(out_avx2, 0x3c, sizeof(out_avx2));
          fn(input, out_fn, stride, tx_type, 8);
          av1_lowbd_fwd_txfm_nl_avx2(input, out_nl, stride, tx_size, tx_type);
          av1_lowbd_fwd_txfm_avx2(input, out_avx2, stride, &param);

          for (int i = 0; i < check_n; ++i) {
            ASSERT_EQ(out_avx2[i], out_fn[i])
                << "tx_size=" << cols << "x" << rows << " tx_type=" << t
                << " stride=" << stride << " pat=" << pat << " idx=" << i;
            ASSERT_EQ(out_avx2[i], out_nl[i])
                << "tx_size=" << cols << "x" << rows << " tx_type=" << t
                << " stride=" << stride << " pat=" << pat << " idx=" << i;
          }
          if (ref_fn != nullptr) {
            memset(out_ref, 0, sizeof(out_ref));
            ref_fn(input, out_ref, stride, tx_type, 8);
            for (int i = 0; i < check_n; ++i) {
              ASSERT_EQ(out_ref[i], out_nl[i])
                  << "ref mismatch tx_size=" << cols << "x" << rows
                  << " tx_type=" << t << " stride=" << stride << " pat=" << pat
                  << " idx=" << i;
            }
          }
        }
      }
    }
  }
}

TEST(AVX2, Av1SubSumSseLbdMatchC) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  constexpr int kDims[4] = { 4, 8, 16, 32 };
  constexpr int16_t kCanary = static_cast<int16_t>(0x5a5a);
  vector<uint8_t> src_buf(128 * 40);
  vector<uint8_t> dst_buf(128 * 40);
  vector<int16_t> diff_buf(128 * 40);

  for (int wi = 0; wi < 4; ++wi) {
    const int bsw = kDims[wi];
    for (int hi = 0; hi < 4; ++hi) {
      const int bsh = kDims[hi];
      const int sstrides[5] = { bsw, bsw + 1, 67, 99, bsw + 3 };
      const int dstrides[5] = { bsw, bsw + 3, 73, 64, bsw + 1 };
      const int fstrides[5] = { bsw, bsw + 5, 32 < bsw ? bsw : 32, 71, bsw };

      for (int si = 0; si < 5; ++si) {
        const int sstride = sstrides[si];
        const int dstride = dstrides[si];
        const int fstride = fstrides[si];
        constexpr size_t kGuard = 37;
        const bool unaligned = (si & 1) != 0 || si == 4;
        uint8_t *const src = src_buf.data() + (unaligned ? 3 : 16);
        uint8_t *const dst = dst_buf.data() + (unaligned ? 7 : 16);
        int16_t *const diff = diff_buf.data() + kGuard;
        const uint32_t dims_pack = static_cast<uint32_t>(fstride) |
                                   (static_cast<uint32_t>(bsw) << 16) |
                                   (static_cast<uint32_t>(bsh) << 24);

        for (int pat = 0; pat < 10; ++pat) {
          for (size_t i = 0; i < src_buf.size(); ++i) {
            src_buf[i] = rnd.Rand8();
            dst_buf[i] = rnd.Rand8();
          }
          std::fill(diff_buf.begin(), diff_buf.end(), kCanary);

          int32_t ref_sum = 0;
          uint32_t ref_sse = 0;
          for (int r = 0; r < bsh; ++r) {
            for (int c = 0; c < bsw; ++c) {
              uint8_t sv = 0;
              uint8_t dv = 0;
              if (pat == 0) {
                sv = 0;
                dv = 255;
              } else if (pat == 1) {
                sv = 255;
                dv = 0;
              } else if (pat == 2) {
                sv = 128;
                dv = 128;
              } else if (pat == 3) {
                sv = ((r + c) & 1) ? 255 : 0;
                dv = ((r + c) & 1) ? 0 : 255;
              } else if (pat == 4) {
                sv = (r & 1) ? 255 : 0;
                dv = (c & 1) ? 255 : 0;
              } else if (pat < 7) {
                sv = rnd.Rand8Extremes();
                dv = rnd.Rand8Extremes();
              } else {
                sv = rnd.Rand8();
                dv = rnd.Rand8();
              }
              src[r * sstride + c] = sv;
              dst[r * dstride + c] = dv;
              const int32_t d = static_cast<int32_t>(sv) - dv;
              ref_sum += d;
              ref_sse += static_cast<uint32_t>(d * d);
            }
          }

          const uint64_t got = av1_sub_sum_sse_lbd_avx2(
              src, sstride, dst, dstride, diff, dims_pack);
          const int32_t got_sum = static_cast<int32_t>(got >> 32);
          const uint32_t got_sse = static_cast<uint32_t>(got);
          ASSERT_EQ(ref_sum, got_sum)
              << bsw << "x" << bsh << " si=" << si << " pat=" << pat;
          ASSERT_EQ(ref_sse, got_sse)
              << bsw << "x" << bsh << " si=" << si << " pat=" << pat;

          for (size_t i = 0; i < kGuard; ++i) {
            ASSERT_EQ(kCanary, diff_buf[i]);
          }
          for (int r = 0; r < bsh; ++r) {
            for (int c = 0; c < fstride; ++c) {
              const int16_t got_d = diff[r * fstride + c];
              if (c < bsw) {
                const int16_t exp_d =
                    static_cast<int16_t>(src[r * sstride + c]) -
                    static_cast<int16_t>(dst[r * dstride + c]);
                ASSERT_EQ(exp_d, got_d)
                    << bsw << "x" << bsh << " r=" << r << " c=" << c;
              } else {
                ASSERT_EQ(kCanary, got_d)
                    << "clobbered stride padding " << bsw << "x" << bsh
                    << " r=" << r << " c=" << c;
              }
            }
          }
          for (size_t i = kGuard + static_cast<size_t>(bsh) * fstride;
               i < diff_buf.size(); ++i) {
            ASSERT_EQ(kCanary, diff_buf[i]);
          }
        }
      }
    }
  }
}

TEST(AVX2, Av1SseDcAdd8bitMatchC) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  constexpr int kDims[5] = { 4, 8, 16, 32, 64 };
  constexpr int kDcs[11] = {
    -255, -200, -128, -17, -1, 0, 1, 17, 127, 200, 255
  };
  vector<uint8_t> src_buf(128 * 72);
  vector<uint8_t> dst_buf(128 * 72);

  for (int wi = 0; wi < 5; ++wi) {
    const int bsw = kDims[wi];
    for (int hi = 0; hi < 5; ++hi) {
      const int bsh = kDims[hi];
      const int sstrides[3] = { bsw, bsw + 1, 97 };
      const int dstrides[3] = { bsw, bsw + 3, 101 };

      for (int si = 0; si < 3; ++si) {
        const int sstride = sstrides[si];
        const int dstride = dstrides[si];
        uint8_t *const src = src_buf.data() + (si == 1 ? 3 : 0);
        uint8_t *const dst = dst_buf.data() + (si == 1 ? 5 : 0);

        for (int pat = 0; pat < 7; ++pat) {
          for (size_t i = 0; i < src_buf.size(); ++i) {
            src_buf[i] = rnd.Rand8();
            dst_buf[i] = rnd.Rand8();
          }
          for (int r = 0; r < bsh; ++r) {
            for (int c = 0; c < bsw; ++c) {
              uint8_t sv = 0;
              uint8_t dv = 0;
              if (pat == 0) {
                sv = 0;
                dv = 0;
              } else if (pat == 1) {
                sv = 0;
                dv = 255;
              } else if (pat == 2) {
                sv = 255;
                dv = 0;
              } else if (pat == 3) {
                sv = 255;
                dv = 255;
              } else if (pat == 4) {
                sv = ((r + c) & 1) ? 255 : 0;
                dv = ((r + c) & 1) ? 0 : 255;
              } else if (pat == 5) {
                sv = rnd.Rand8Extremes();
                dv = rnd.Rand8Extremes();
              } else {
                sv = rnd.Rand8();
                dv = rnd.Rand8();
              }
              src[r * sstride + c] = sv;
              dst[r * dstride + c] = dv;
            }
          }

          for (int di = 0; di < 11; ++di) {
            const int dc = kDcs[di];
            const uint32_t dc_pos = dc > 0 ? static_cast<uint32_t>(dc) : 0u;
            const uint32_t dc_neg = dc < 0 ? static_cast<uint32_t>(-dc) : 0u;
            const uint32_t dims_dc = static_cast<uint32_t>(bsw) |
                                     (static_cast<uint32_t>(bsh) << 8) |
                                     (dc_pos << 16) | (dc_neg << 24);
            int64_t ref_sse = 0;
            for (int r = 0; r < bsh; ++r) {
              for (int c = 0; c < bsw; ++c) {
                int recon = static_cast<int>(dst[r * dstride + c]) + dc;
                if (recon < 0) recon = 0;
                if (recon > 255) recon = 255;
                const int64_t err =
                    static_cast<int64_t>(src[r * sstride + c]) - recon;
                ref_sse += err * err;
              }
            }
            const int64_t ref_dist = 16 * ref_sse;
            const int64_t got_dist =
                av1_sse_dc_add_8bit_avx2(src, sstride, dst, dstride, dims_dc);
            ASSERT_EQ(ref_dist, got_dist) << bsw << "x" << bsh << " si=" << si
                                          << " pat=" << pat << " dc=" << dc;
          }
        }
      }
    }
  }
}

TEST(AVX2, Av1DistBlockPxLbdReconMatchC) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(32, int16_t, res_diff[32 * 32]);
  DECLARE_ALIGNED(32, tran_low_t, coeff_full[32 * 32]);
  DECLARE_ALIGNED(32, tran_low_t, dqcoeff[32 * 32]);
  DECLARE_ALIGNED(32, uint16_t, ref_recon[64 * 64]);
  vector<uint8_t> src_buf(96 * 40);
  vector<uint8_t> dst_buf(96 * 40);

  for (int s = 0; s < TX_SIZES_ALL; ++s) {
    const TX_SIZE tx_size = static_cast<TX_SIZE>(s);
    const int bsw = tx_size_wide[tx_size];
    const int bsh = tx_size_high[tx_size];
    if (bsw > 32 || bsh > 32) continue;

    const FwdTxfm2dFunc fwd_fn =
        libaom_test::fwd_txfm_func_ls[tx_size] != nullptr
            ? libaom_test::fwd_txfm_func_ls[tx_size]
            : av1_lowbd_fwd_txfm2d_func_avx2(tx_size);
    const libaom_test::InvTxfm2dFunc inv_ref =
        libaom_test::inv_txfm_func_ls[tx_size];
    ASSERT_NE(fwd_fn, nullptr);
    ASSERT_NE(inv_ref, nullptr);

    const int eobmax = bsw * bsh;
    const int cutoffs[4] = { 1, 2, AOMMAX(1, eobmax / 4), eobmax };
    const int sstrides[3] = { bsw, bsw + 1, 67 };
    const int dstrides[3] = { bsw, bsw + 3, 71 };

    for (int t = 0; t < TX_TYPES; ++t) {
      const TX_TYPE tx_type = static_cast<TX_TYPE>(t);
      if (!libaom_test::IsTxSizeTypeValid(tx_size, tx_type)) continue;
      const int16_t *const scan = get_default_scan(tx_size, tx_type)->scan;

      for (int si = 0; si < 3; ++si) {
        const int sstride = sstrides[si];
        const int dstride = dstrides[si];
        uint8_t *const src = src_buf.data() + (si == 1 ? 3 : 0);
        uint8_t *const dst = dst_buf.data() + (si == 1 ? 5 : 0);

        for (int pat = 0; pat < 5; ++pat) {
          for (size_t i = 0; i < src_buf.size(); ++i) {
            src_buf[i] = rnd.Rand8();
            dst_buf[i] = rnd.Rand8();
          }
          for (int r = 0; r < bsh; ++r) {
            for (int c = 0; c < bsw; ++c) {
              uint8_t sv = 0;
              uint8_t dv = 0;
              int16_t rv = 0;
              if (pat == 0) {
                sv = 0;
                dv = 255;
                rv = -255;
              } else if (pat == 1) {
                sv = 255;
                dv = 0;
                rv = 255;
              } else if (pat == 2) {
                const int odd = (r + c) & 1;
                sv = odd ? 255 : 0;
                dv = odd ? 0 : 255;
                rv = odd ? 255 : -255;
              } else if (pat == 3) {
                sv = rnd.Rand8Extremes();
                dv = rnd.Rand8Extremes();
                rv = static_cast<int16_t>(rnd.Rand8Extremes()) -
                     static_cast<int16_t>(rnd.Rand8Extremes());
              } else {
                sv = rnd.Rand8();
                dv = rnd.Rand8();
                rv = static_cast<int16_t>(sv) - static_cast<int16_t>(dv);
              }
              src[r * sstride + c] = sv;
              dst[r * dstride + c] = dv;
              res_diff[r * bsw + c] = rv;
            }
          }

          memset(coeff_full, 0, sizeof(coeff_full));
          fwd_fn(res_diff, coeff_full, bsw, tx_type, 8);

          for (int ci = 0; ci < 4; ++ci) {
            const int cutoff = cutoffs[ci];
            memcpy(dqcoeff, coeff_full, sizeof(dqcoeff));
            for (int i = cutoff; i < eobmax; ++i) {
              dqcoeff[scan[i]] = 0;
            }
            int actual_eob = 0;
            for (int i = cutoff - 1; i >= 0; --i) {
              if (dqcoeff[scan[i]] != 0) {
                actual_eob = i + 1;
                break;
              }
            }
            if (actual_eob == 0) {
              dqcoeff[scan[0]] = 16;
              actual_eob = 1;
            }

            memset(ref_recon, 0, sizeof(ref_recon));
            for (int r = 0; r < bsh; ++r) {
              for (int c = 0; c < bsw; ++c) {
                ref_recon[r * 64 + c] = dst[r * dstride + c];
              }
            }
            inv_ref(dqcoeff, ref_recon, 64, tx_type, 8);

            int64_t ref_sse = 0;
            for (int r = 0; r < bsh; ++r) {
              for (int c = 0; c < bsw; ++c) {
                const int64_t err = static_cast<int64_t>(src[r * sstride + c]) -
                                    static_cast<int64_t>(ref_recon[r * 64 + c]);
                ref_sse += err * err;
              }
            }
            const int64_t ref_dist = 16 * ref_sse;
            const uint32_t tx_info = static_cast<uint32_t>(tx_size) |
                                     (static_cast<uint32_t>(tx_type) << 5) |
                                     (static_cast<uint32_t>(actual_eob) << 9) |
                                     (static_cast<uint32_t>(bsw) << 20) |
                                     (static_cast<uint32_t>(bsh) << 26);
            const int64_t got_dist = av1_dist_block_px_lbd_recon_avx2(
                src, sstride, dst, dstride, dqcoeff, tx_info);
            ASSERT_EQ(ref_dist, got_dist)
                << bsw << "x" << bsh << " tx_type=" << t << " si=" << si
                << " pat=" << pat << " eob=" << actual_eob;
          }
        }
      }
    }
  }
}
#endif  // CONFIG_AV1_ENCODER && HAVE_AVX2 && !CONFIG_HIGHWAY && !CONFIG_SHARED

}  // namespace
