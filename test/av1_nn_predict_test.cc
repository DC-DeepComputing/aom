/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <algorithm>
#include <memory>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"

#include "config/aom_config.h"

#include "aom/aom_integer.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/aom_timer.h"
#include "av1/encoder/ml.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"
#if HAVE_AVX2
#include "aom_ports/x86.h"
#if !CONFIG_REALTIME_ONLY
#include "av1/encoder/block.h"
#endif  // !CONFIG_REALTIME_ONLY
#endif  // HAVE_AVX2
#include "test/util.h"
#include "test/register_state_check.h"
#include "test/acm_random.h"

namespace {
using NnPredict_Func = void (*)(const float *const input_nodes,
                                const NN_CONFIG *const nn_config,
                                int reduce_prec, float *const output);

using NnPredictTestParam = std::tuple<const NnPredict_Func>;

const float epsilon = 1e-3f;  // Error threshold for functional equivalence

class NnPredictTest : public ::testing::TestWithParam<NnPredictTestParam> {
 public:
  void SetUp() override {
    const int MAX_NODES2 = NN_MAX_NODES_PER_LAYER * NN_MAX_NODES_PER_LAYER;
    // Allocate two massive buffers on the heap for edge weights and node bias
    // Then set-up the double-dimension arrays pointing into the big buffers
    weights_buf = (float *)aom_malloc(MAX_NODES2 * (NN_MAX_HIDDEN_LAYERS + 1) *
                                      sizeof(*weights_buf));
    bias_buf =
        (float *)aom_malloc(NN_MAX_NODES_PER_LAYER *
                            (NN_MAX_HIDDEN_LAYERS + 1) * sizeof(*bias_buf));
    ASSERT_NE(weights_buf, nullptr);
    ASSERT_NE(bias_buf, nullptr);
    for (int i = 0; i < NN_MAX_HIDDEN_LAYERS + 1; i++) {
      weights[i] = &weights_buf[i * MAX_NODES2];
      bias[i] = &bias_buf[i * NN_MAX_NODES_PER_LAYER];
    }
    target_func_ = GET_PARAM(0);
  }
  void TearDown() override {
    aom_free(weights_buf);
    aom_free(bias_buf);
  }
  void RunNnPredictTest(const NN_CONFIG *const shape);
  void RunNnPredictSpeedTest(const NN_CONFIG *const shape, const int run_times);
  void RunNnPredictTest_all(const NN_CONFIG *const shapes,
                            const int num_shapes);
  void RunNnPredictSpeedTest_all(const NN_CONFIG *const shapes,
                                 const int num_shapes, const int run_times);

 private:
  NnPredict_Func target_func_;
  libaom_test::ACMRandom rng_;
  float *weights[NN_MAX_HIDDEN_LAYERS + 1] = {};
  float *bias[NN_MAX_HIDDEN_LAYERS + 1] = {};
  float *weights_buf = nullptr, *bias_buf = nullptr;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(NnPredictTest);

void NnPredictTest::RunNnPredictTest(const NN_CONFIG *const shape) {
  float inputs[NN_MAX_NODES_PER_LAYER] = { 0 };
  float outputs_test[NN_MAX_NODES_PER_LAYER] = { 0 };
  float outputs_ref[NN_MAX_NODES_PER_LAYER] = { 0 };

  NN_CONFIG nn_config = *shape;

  char shape_str[32] = { 0 };
  snprintf(shape_str, sizeof(shape_str), "%d", shape->num_inputs);
  for (int layer = 0; layer < shape->num_hidden_layers; layer++)
    snprintf(&shape_str[strlen(shape_str)],
             sizeof(shape_str) - strlen(shape_str), "x%d",
             shape->num_hidden_nodes[layer]);
  snprintf(&shape_str[strlen(shape_str)], sizeof(shape_str) - strlen(shape_str),
           "x%d", shape->num_outputs);

  for (int i = 0; i < NN_MAX_HIDDEN_LAYERS + 1; i++) {
    nn_config.weights[i] = weights[i];
    nn_config.bias[i] = bias[i];
  }

  for (int iter = 0; iter < 10000 && !HasFatalFailure(); ++iter) {
    for (int node = 0; node < shape->num_inputs; node++) {
      inputs[node] = ((float)rng_.Rand31() - (1 << 30)) / (1u << 31);
    }
    for (int layer = 0; layer < shape->num_hidden_layers; layer++) {
      for (int node = 0; node < NN_MAX_NODES_PER_LAYER; node++) {
        bias[layer][node] = ((float)rng_.Rand31() - (1 << 30)) / (1u << 31);
      }
      for (int node = 0; node < NN_MAX_NODES_PER_LAYER * NN_MAX_NODES_PER_LAYER;
           node++) {
        weights[layer][node] = ((float)rng_.Rand31() - (1 << 30)) / (1u << 31);
      }
    }
    // Now the outputs:
    int layer = shape->num_hidden_layers;
    for (int node = 0; node < NN_MAX_NODES_PER_LAYER; node++) {
      bias[layer][node] = ((float)rng_.Rand31() - (1 << 30)) / (1u << 31);
    }
    for (int node = 0; node < NN_MAX_NODES_PER_LAYER * NN_MAX_NODES_PER_LAYER;
         node++) {
      weights[layer][node] = ((float)rng_.Rand31() - (1 << 30)) / (1u << 31);
    }

    av1_nn_predict_c(inputs, &nn_config, 0, outputs_ref);
    target_func_(inputs, &nn_config, 0, outputs_test);

    for (int node = 0; node < shape->num_outputs; node++) {
      if (outputs_ref[node] < epsilon) {
        ASSERT_LE(outputs_test[node], epsilon)
            << "Reference output was near-zero, test output was not ("
            << shape_str << ")";
      } else {
        const float error = outputs_ref[node] - outputs_test[node];
        const float relative_error = fabsf(error / outputs_ref[node]);
        ASSERT_LE(relative_error, epsilon)
            << "Excessive relative error between reference and test ("
            << shape_str << ")";
      }
    }
  }
}

void NnPredictTest::RunNnPredictSpeedTest(const NN_CONFIG *const shape,
                                          const int run_times) {
  float inputs[NN_MAX_NODES_PER_LAYER] = { 0 };
  float outputs_test[NN_MAX_NODES_PER_LAYER] = { 0 };
  float outputs_ref[NN_MAX_NODES_PER_LAYER] = { 0 };

  NN_CONFIG nn_config = *shape;

  for (int i = 0; i < NN_MAX_HIDDEN_LAYERS; i++) {
    nn_config.weights[i] = weights[i];
    nn_config.bias[i] = bias[i];
  }
  // Don't bother actually changing the values for inputs/weights/bias: it
  // shouldn't make any difference for a speed test.

  aom_usec_timer timer;
  aom_usec_timer_start(&timer);
  for (int i = 0; i < run_times; ++i) {
    av1_nn_predict_c(inputs, &nn_config, 0, outputs_ref);
  }
  aom_usec_timer_mark(&timer);
  const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
  aom_usec_timer_start(&timer);
  for (int i = 0; i < run_times; ++i) {
    target_func_(inputs, &nn_config, 0, outputs_test);
  }
  aom_usec_timer_mark(&timer);
  const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));

  printf("%d", shape->num_inputs);
  for (int layer = 0; layer < shape->num_hidden_layers; layer++)
    printf("x%d", shape->num_hidden_nodes[layer]);
  printf("x%d: ", shape->num_outputs);
  printf("%7.2f/%7.2fns (%3.2f)\n", time1, time2, time1 / time2);
}

// This is all the neural network shapes observed executed in a few different
// runs of the encoder.  It also conveniently covers all the kernels
// implemented.
static const NN_CONFIG kShapes[] = {
  { 37, 1, 2, { 16, 24 }, {}, {} }, { 24, 24, 1, { 12 }, {}, {} },
  { 10, 16, 1, { 64 }, {}, {} },    { 12, 1, 1, { 12 }, {}, {} },
  { 12, 1, 1, { 24 }, {}, {} },     { 12, 1, 1, { 32 }, {}, {} },
  { 18, 4, 1, { 24 }, {}, {} },     { 18, 4, 1, { 32 }, {}, {} },
  { 4, 1, 1, { 16 }, {}, {} },      { 8, 1, 0, { 0 }, {}, {} },
  { 8, 4, 1, { 16 }, {}, {} },      { 8, 1, 1, { 32 }, {}, {} },
  { 9, 3, 1, { 32 }, {}, {} },      { 8, 4, 0, { 0 }, {}, {} },
  { 8, 8, 0, { 0 }, {}, {} },       { 4, 4, 1, { 8 }, {}, {} },
  { 4, 3, 0, { 64 }, {}, {} },
};

void NnPredictTest::RunNnPredictTest_all(const NN_CONFIG *const shapes,
                                         const int num_shapes) {
  for (int i = 0; i < num_shapes; i++) RunNnPredictTest(&shapes[i]);
}

void NnPredictTest::RunNnPredictSpeedTest_all(const NN_CONFIG *const shapes,
                                              const int num_shapes,
                                              const int run_times) {
  for (int i = 0; i < num_shapes; i++)
    NnPredictTest::RunNnPredictSpeedTest(&shapes[i], run_times);
}

TEST_P(NnPredictTest, RandomValues) {
  RunNnPredictTest_all(kShapes, sizeof(kShapes) / sizeof(kShapes[0]));
}

TEST_P(NnPredictTest, DISABLED_Speed) {
  RunNnPredictSpeedTest_all(kShapes, sizeof(kShapes) / sizeof(kShapes[0]),
                            10000000);
}

#if !CONFIG_EXCLUDE_SIMD_MISMATCH
#if HAVE_SSE3
INSTANTIATE_TEST_SUITE_P(SSE3, NnPredictTest,
                         ::testing::Values(av1_nn_predict_sse3));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, NnPredictTest,
                         ::testing::Values(av1_nn_predict_avx2));

#if !CONFIG_REALTIME_ONLY
TEST(LowbdGradientInfoSbAvx2Test, EquivalenceWithC) {
  if (!(x86_simd_caps() & HAS_AVX2)) GTEST_SKIP();
  libaom_test::ACMRandom rnd(libaom_test::ACMRandom::DeterministicSeed());
  const BLOCK_SIZE sb_sizes[] = { BLOCK_32X32, BLOCK_64X64, BLOCK_128X128 };
  const int strides[] = { 32, 64, 128, 160 };
  std::unique_ptr<MACROBLOCK, decltype(&aom_free)> x(
      static_cast<MACROBLOCK *>(aom_calloc(1, sizeof(MACROBLOCK))), &aom_free);
  ASSERT_NE(x.get(), nullptr);
  std::vector<PixelLevelGradientInfo> grad_ref(MAX_SB_SQUARE * 3);
  std::vector<PixelLevelGradientInfo> grad_avx2(MAX_SB_SQUARE * 3);
  std::vector<uint8_t> src(128 * 160 + 32);

  for (BLOCK_SIZE sb_size : sb_sizes) {
    const int sb_h = block_size_high[sb_size];
    const int sb_w = block_size_wide[sb_size];
    for (int stride : strides) {
      if (stride < sb_w) continue;
      for (int iter = 0; iter < 5; ++iter) {
        for (int r = 0; r < sb_h; ++r) {
          for (int c = 0; c < sb_w; ++c) {
            src[r * stride + c] =
                (iter == 0) ? 128 : static_cast<uint8_t>(rnd.Rand8());
          }
        }
        std::fill(grad_ref.begin(), grad_ref.end(),
                  PixelLevelGradientInfo{ 0, 0, false });
        std::fill(grad_avx2.begin(), grad_avx2.end(),
                  PixelLevelGradientInfo{ 0, 0, false });
        x->plane[0].src.buf = src.data();
        x->plane[0].src.stride = stride;
        x->e_mbd.plane[0].subsampling_x = 0;
        x->e_mbd.plane[0].subsampling_y = 0;
        x->pixel_gradient_info = grad_ref.data();
        av1_lowbd_compute_gradient_info_sb_c(x.get(), sb_size, PLANE_TYPE_Y);
        av1_lowbd_compute_gradient_info_sb_avx2(src.data(), stride,
                                                grad_avx2.data(), sb_h, sb_w);
        for (int r = 1; r < sb_h - 1; ++r) {
          for (int c = 1; c < sb_w - 1; ++c) {
            const int idx = r * sb_w + c;
            ASSERT_EQ(grad_avx2[idx].abs_dx_abs_dy_sum,
                      grad_ref[idx].abs_dx_abs_dy_sum)
                << "sb=" << sb_size << " stride=" << stride << " r=" << r
                << " c=" << c;
            if (grad_ref[idx].abs_dx_abs_dy_sum > 0) {
              ASSERT_EQ(grad_avx2[idx].hist_bin_idx, grad_ref[idx].hist_bin_idx)
                  << "sb=" << sb_size << " stride=" << stride << " r=" << r
                  << " c=" << c;
            } else {
              ASSERT_GE(grad_avx2[idx].hist_bin_idx, 0);
              ASSERT_LE(grad_avx2[idx].hist_bin_idx, 32);
            }
          }
        }
      }
    }
  }
}
#endif  // !CONFIG_REALTIME_ONLY
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, NnPredictTest,
                         ::testing::Values(av1_nn_predict_neon));
#endif
#endif  // !CONFIG_EXCLUDE_SIMD_MISMATCH

}  // namespace
