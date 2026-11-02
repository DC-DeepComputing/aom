/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <immintrin.h>
#include <math.h>

#include "aom_dsp/aom_dsp_common.h"
#include "av1/common/av1_common_int.h"
#include "av1/encoder/cnn.h"

// This mask rearranges source pixels in the order shown below.
// shuffle_src_layer0[0][8]: applied on source pixels 0 to 7.
// shuffle_src_layer0[1][8]: applied on source pixels 7 to 14.
// This shuffling is needed to process 3 5x5 blocks which need
// source pixels in the following order.
// 1st 5x5 block: source pixels needed are 0 to 4,
// 2nd 5x5 block: source pixels needed are 4 to 8,
// 3rd 5x5 block: source pixels needed are 8 to 12.
// Source pixels are loaded like mentioned below.
// load_src0 : 0, 1, 2, 3, 4, 5, 6, 7
// load_src1 : 7, 8, 9, 10, 11, 12, 13, 14
// After applying masks, source bytes will be in the order:
// load_src0 : 0, 1, 2, 3, 4, 4, 5, 6
//             consists 5 pixels needed for 1st 5x5 block and
//             first 3 pixels needed for 2nd 5x5 block.
// load_src1 : 7, 8, 8, 9, 10, 11, 12, x
//             consists last 2 pixels needed for 2nd 5x5 block and
//             5 pixels needed for 3rd 5x5 block.
DECLARE_ALIGNED(32, static const uint32_t,
                shuffle_src_layer0[2][8]) = { { 0, 1, 2, 3, 4, 4, 5, 6 },
                                              { 0, 1, 1, 2, 3, 4, 5, 0 } };

// This mask rearrange the weights to match shuffled source pixels order.
DECLARE_ALIGNED(32, static const uint32_t,
                shuffle_weight_layer0[2][8]) = { { 0, 1, 2, 3, 4, 0, 1, 2 },
                                                 { 3, 4, 0, 1, 2, 3, 4, 0 } };

// After the stages of multiplication and accumulation, the output values
// in the register will be jumbled. In order to store register into
// output buffer in a proper way, the following mask is applied on output
// register.
DECLARE_ALIGNED(32, static const uint32_t,
                shuffle_output_layer_1_and_2[8]) = { 0, 1, 4, 5, 2, 3, 6, 7 };

// Load weights needed for layer 0 (for 5x5 block processing),
// and fill the registers appropriately to match source pixel mapping.
static inline void prepare_weights_for_5x5_convolve(
    const float *layer_config_weights, int off, float weight[5][8],
    const int cstep, __m256 *shuffle_weight, const __m256i weight_mask_0,
    const __m256i weight_mask_1) {
  for (int row = 0; row < 5; ++row) {
    for (int col = 0; col < 5; ++col) {
      weight[row][col] = layer_config_weights[off];
      off += cstep;
    }
  }
  shuffle_weight[0] = _mm256_loadu_ps(weight[0]);
  shuffle_weight[1] = _mm256_loadu_ps(weight[1]);
  shuffle_weight[2] = _mm256_loadu_ps(weight[2]);
  shuffle_weight[3] = _mm256_loadu_ps(weight[3]);
  shuffle_weight[4] = _mm256_loadu_ps(weight[4]);

  shuffle_weight[0] =
      _mm256_permutevar8x32_ps(shuffle_weight[0], weight_mask_0);
  shuffle_weight[1] =
      _mm256_permutevar8x32_ps(shuffle_weight[1], weight_mask_0);
  shuffle_weight[2] =
      _mm256_permutevar8x32_ps(shuffle_weight[2], weight_mask_0);
  shuffle_weight[3] =
      _mm256_permutevar8x32_ps(shuffle_weight[3], weight_mask_0);
  shuffle_weight[4] =
      _mm256_permutevar8x32_ps(shuffle_weight[4], weight_mask_0);
  shuffle_weight[5] =
      _mm256_permutevar8x32_ps(shuffle_weight[0], weight_mask_1);
  shuffle_weight[6] =
      _mm256_permutevar8x32_ps(shuffle_weight[1], weight_mask_1);
  shuffle_weight[7] =
      _mm256_permutevar8x32_ps(shuffle_weight[2], weight_mask_1);
  shuffle_weight[8] =
      _mm256_permutevar8x32_ps(shuffle_weight[3], weight_mask_1);
  shuffle_weight[9] =
      _mm256_permutevar8x32_ps(shuffle_weight[4], weight_mask_1);
}

// For each row, loads source pixels 0 to 7(load_src_0), 7 to 14(load_src_1) and
// arranges them appropriately to process 3 blocks.
#define PERFORM_CONVOLVE_FOR_3_5X5_BLOCKS()                            \
  do {                                                                 \
    for (int row = 0; row < 5; row++) {                                \
      load_src_0 = _mm256_loadu_ps(input_ptr);                         \
      load_src_1 = _mm256_loadu_ps(input_ptr + 7);                     \
      load_src_0 = _mm256_permutevar8x32_ps(load_src_0, block0_1);     \
      load_src_1 = _mm256_permutevar8x32_ps(load_src_1, block1_2);     \
      load_src_0 = _mm256_mul_ps(load_src_0, shuffle_weight[0 + row]); \
      load_src_1 = _mm256_mul_ps(load_src_1, shuffle_weight[5 + row]); \
      accum_src_0 = _mm256_add_ps(load_src_0, accum_src_0);            \
      accum_src_1 = _mm256_add_ps(load_src_1, accum_src_1);            \
      input_ptr += in_stride;                                          \
    }                                                                  \
  } while (0)

// Do convolution of one 5x5 block.
#define PERFORM_CONVOLVE_FOR_1_5X5_BLOCK(w, accum0, in_stride)           \
  do {                                                                   \
    __m128 load_src[5];                                                  \
    load_src[0] = _mm_loadu_ps(input_ptr);                               \
    last_column_sum += input_ptr[4] * weight[0][4];                      \
    input_ptr += in_stride;                                              \
    load_src[1] = _mm_loadu_ps(input_ptr);                               \
    last_column_sum += input_ptr[4] * weight[1][4];                      \
    input_ptr += in_stride;                                              \
    load_src[2] = _mm_loadu_ps(input_ptr);                               \
    last_column_sum += input_ptr[4] * weight[2][4];                      \
    input_ptr += in_stride;                                              \
    load_src[3] = _mm_loadu_ps(input_ptr);                               \
    last_column_sum += input_ptr[4] * weight[3][4];                      \
    input_ptr += in_stride;                                              \
    load_src[4] = _mm_loadu_ps(input_ptr);                               \
    last_column_sum += input_ptr[4] * weight[4][4];                      \
                                                                         \
    load_src[0] = _mm_mul_ps(load_src[0], _mm256_castps256_ps128(w[0])); \
    load_src[1] = _mm_mul_ps(load_src[1], _mm256_castps256_ps128(w[1])); \
    load_src[2] = _mm_mul_ps(load_src[2], _mm256_castps256_ps128(w[2])); \
    load_src[3] = _mm_mul_ps(load_src[3], _mm256_castps256_ps128(w[3])); \
    load_src[4] = _mm_mul_ps(load_src[4], _mm256_castps256_ps128(w[4])); \
                                                                         \
    accum0 = _mm_add_ps(load_src[0], accum0);                            \
    load_src[1] = _mm_add_ps(load_src[1], load_src[2]);                  \
    load_src[3] = _mm_add_ps(load_src[3], load_src[4]);                  \
    load_src[1] = _mm_add_ps(load_src[1], load_src[3]);                  \
    accum0 = _mm_add_ps(accum0, load_src[1]);                            \
  } while (0)

// AVX2 variant of av1_cnn_convolve_no_maxpool_padding_valid_c(), when
// filter_width and filter_height are equal to 5.
// CNN convolve parsing is based on av1_intra_mode_cnn_partition_cnn_config.
// Based on the configuration set for each layer, the current encoder
// always chooses the case of no_maxpool_padding_valid.
// And also for layer 0 convolution happens at 5x5 level as the
// filter_width and filter_height are set as 5.
static void cnn_convolve_no_maxpool_padding_valid_5x5_avx2(
    const float **input, int in_width, int in_height, int in_stride,
    const CNN_LAYER_CONFIG *const layer_config, float **output, int out_stride,
    int start_idx, const int cstep, const int channel_step) {
  const int kFilterWidth = 5;
  const int kFilterHeight = 5;
  const int kSkipWidth = 4;
  const int kSkipHeight = 4;
  assert(layer_config->filter_width == kFilterWidth &&
         layer_config->filter_height == kFilterHeight);
  assert(layer_config->skip_width == kSkipWidth &&
         layer_config->skip_height == kSkipHeight);

  // Load shuffle buffers needed for source.
  const __m256i block0_1 =
      _mm256_load_si256((const __m256i *)shuffle_src_layer0[0]);
  const __m256i block1_2 =
      _mm256_load_si256((const __m256i *)shuffle_src_layer0[1]);

  // Load shuffle buffers needed for weight.
  const __m256i weight_mask_0 =
      _mm256_load_si256((const __m256i *)shuffle_weight_layer0[0]);
  const __m256i weight_mask_1 =
      _mm256_load_si256((const __m256i *)shuffle_weight_layer0[1]);

  // Width needs to be moved to go to next iteration of processing 3 5x5 blocks.
  const int kSkipWidthForNextIter = kSkipWidth * 3;

  // Minimum width required to process 3 5x5 blocks at a time.
  // min width (for processing 3 5x5 block) = 2*skip_width + filter_width
  // Here, skip_width specifies how much width we should move while processing
  // next block convolution and filter_width specifies for how many pixels
  // filter needs to be applied.
  const int kMinWidthFor3_5x5Blocks = (kSkipWidth * 2) + kFilterWidth;
  int i = start_idx;
  if (layer_config->in_channels == 1 && channel_step == 1 && in_width == 65 &&
      in_height <= 68) {
    DECLARE_ALIGNED(32, float, s_buf[68][5][2][8]);
    DECLARE_ALIGNED(16, float, tail_buf[68][4]);
    float col4_buf[68];
    const int max_r = ((in_height - kFilterHeight) & ~3) + kFilterHeight;
    for (int r = 0; r < max_r; ++r) {
      const float *row_ptr = &input[0][r * in_stride];
      for (int g = 0; g < 5; ++g) {
        const float *p = row_ptr + g * 12;
        _mm256_store_ps(s_buf[r][g][0],
                        _mm256_permutevar8x32_ps(_mm256_loadu_ps(p), block0_1));
        _mm256_store_ps(s_buf[r][g][1], _mm256_permutevar8x32_ps(
                                            _mm256_loadu_ps(p + 7), block1_2));
      }
      _mm_store_ps(tail_buf[r], _mm_loadu_ps(row_ptr + 60));
      col4_buf[r] = row_ptr[64];
    }
    DECLARE_ALIGNED(32, float, w_row[8]) = { 0 };
    for (; i + 3 < layer_config->out_channels; i += 4) {
      const float out_ch_bias[4] = { layer_config->bias[i],
                                     layer_config->bias[i + 1],
                                     layer_config->bias[i + 2],
                                     layer_config->bias[i + 3] };
      DECLARE_ALIGNED(32, float, shuffle_weight[4][10][8]);
      float w_col4[4][5];
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 1
#endif
      for (int ch = 0; ch < 4; ++ch) {
        const float *w_ptr = layer_config->weights + i + ch;
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 1
#endif
        for (int row = 0; row < 5; ++row) {
          w_row[0] = w_ptr[(row * 5 + 0) * cstep];
          w_row[1] = w_ptr[(row * 5 + 1) * cstep];
          w_row[2] = w_ptr[(row * 5 + 2) * cstep];
          w_row[3] = w_ptr[(row * 5 + 3) * cstep];
          w_col4[ch][row] = w_row[4] = w_ptr[(row * 5 + 4) * cstep];
          const __m256 w_lo =
              _mm256_permutevar8x32_ps(_mm256_load_ps(w_row), weight_mask_0);
          _mm256_store_ps(shuffle_weight[ch][row], w_lo);
          _mm256_store_ps(shuffle_weight[ch][5 + row],
                          _mm256_permutevar8x32_ps(w_lo, weight_mask_1));
        }
      }
      for (int h = 0, u = 0; h < in_height - kFilterHeight + 1;
           h += kSkipHeight, ++u) {
        const int out_h = u * out_stride;
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 1
#endif
        for (int g = 0; g < 5; ++g) {
          const __m256 s0_0 = _mm256_load_ps(s_buf[h + 0][g][0]);
          const __m256 s1_0 = _mm256_load_ps(s_buf[h + 0][g][1]);
          __m256 accum_src_0[4] = {
            _mm256_mul_ps(s0_0, _mm256_load_ps(shuffle_weight[0][0])),
            _mm256_mul_ps(s0_0, _mm256_load_ps(shuffle_weight[1][0])),
            _mm256_mul_ps(s0_0, _mm256_load_ps(shuffle_weight[2][0])),
            _mm256_mul_ps(s0_0, _mm256_load_ps(shuffle_weight[3][0]))
          };
          __m256 accum_src_1[4] = {
            _mm256_mul_ps(s1_0, _mm256_load_ps(shuffle_weight[0][5])),
            _mm256_mul_ps(s1_0, _mm256_load_ps(shuffle_weight[1][5])),
            _mm256_mul_ps(s1_0, _mm256_load_ps(shuffle_weight[2][5])),
            _mm256_mul_ps(s1_0, _mm256_load_ps(shuffle_weight[3][5]))
          };
          for (int row = 1; row < 5; ++row) {
            const __m256 s0 = _mm256_load_ps(s_buf[h + row][g][0]);
            const __m256 s1 = _mm256_load_ps(s_buf[h + row][g][1]);
            for (int ch = 0; ch < 4; ++ch) {
              accum_src_0[ch] = _mm256_add_ps(
                  _mm256_mul_ps(s0, _mm256_load_ps(shuffle_weight[ch][row])),
                  accum_src_0[ch]);
              accum_src_1[ch] = _mm256_add_ps(
                  _mm256_mul_ps(s1,
                                _mm256_load_ps(shuffle_weight[ch][5 + row])),
                  accum_src_1[ch]);
            }
          }
          const int v = out_h + g * 3;
          for (int ch = 0; ch < 4; ++ch) {
            __m256 accum = _mm256_hadd_ps(accum_src_0[ch], accum_src_1[ch]);
            __m128 tmp_reg_0 = _mm256_extractf128_ps(accum_src_0[ch], 1);
            __m128 tmp_reg_1 = _mm256_extractf128_ps(accum_src_1[ch], 1);
            __m128 accum_l = _mm256_castps256_ps128(accum);
            __m128 accum_h = _mm256_extractf128_ps(accum, 1);
            __m128 tmp_reg_2 = _mm_add_ps(accum_l, tmp_reg_0);
            __m128 tmp_reg_3 = _mm_add_ps(tmp_reg_0, accum_h);
            __m128 tmp_reg_4 = _mm_add_ps(tmp_reg_1, accum_h);
            output[i + ch][v] =
                out_ch_bias[ch] + _mm_cvtss_f32(tmp_reg_2) +
                _mm_cvtss_f32(_mm_shuffle_ps(accum_l, accum_l, 1));
            output[i + ch][v + 1] =
                out_ch_bias[ch] +
                _mm_cvtss_f32(_mm_shuffle_ps(tmp_reg_3, tmp_reg_3, 1)) +
                _mm_cvtss_f32(_mm_shuffle_ps(accum_l, accum_l, 2));
            output[i + ch][v + 2] =
                out_ch_bias[ch] +
                _mm_cvtss_f32(_mm_shuffle_ps(tmp_reg_4, tmp_reg_4, 2)) +
                _mm_cvtss_f32(_mm_shuffle_ps(accum_l, accum_l, 3));
          }
        }
        const __m128 src0 = _mm_load_ps(tail_buf[h + 0]);
        const __m128 src1 = _mm_load_ps(tail_buf[h + 1]);
        const __m128 src2 = _mm_load_ps(tail_buf[h + 2]);
        const __m128 src3 = _mm_load_ps(tail_buf[h + 3]);
        const __m128 src4 = _mm_load_ps(tail_buf[h + 4]);
        const float c4_0 = col4_buf[h + 0];
        const float c4_1 = col4_buf[h + 1];
        const float c4_2 = col4_buf[h + 2];
        const float c4_3 = col4_buf[h + 3];
        const float c4_4 = col4_buf[h + 4];
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 1
#endif
        for (int ch = 0; ch < 4; ++ch) {
          float last_column_sum = c4_0 * w_col4[ch][0] + c4_1 * w_col4[ch][1] +
                                  c4_2 * w_col4[ch][2] + c4_3 * w_col4[ch][3] +
                                  c4_4 * w_col4[ch][4];
          __m128 r0 = _mm_mul_ps(src0, _mm_load_ps(shuffle_weight[ch][0]));
          __m128 r1 = _mm_mul_ps(src1, _mm_load_ps(shuffle_weight[ch][1]));
          __m128 r2 = _mm_mul_ps(src2, _mm_load_ps(shuffle_weight[ch][2]));
          __m128 r3 = _mm_mul_ps(src3, _mm_load_ps(shuffle_weight[ch][3]));
          __m128 r4 = _mm_mul_ps(src4, _mm_load_ps(shuffle_weight[ch][4]));
          r1 = _mm_add_ps(r1, r2);
          r3 = _mm_add_ps(r3, r4);
          r1 = _mm_add_ps(r1, r3);
          __m128 accum = _mm_add_ps(r0, r1);
          accum = _mm_hadd_ps(accum, accum);
          output[i + ch][out_h + 15] =
              out_ch_bias[ch] + last_column_sum + _mm_cvtss_f32(accum) +
              _mm_cvtss_f32(_mm_shuffle_ps(accum, accum, 1));
        }
      }
    }
  }
  for (; i < layer_config->out_channels; i += channel_step) {
    const float out_ch_bias = layer_config->bias[i];
    for (int k = 0; k < layer_config->in_channels; ++k) {
      __m256 shuffle_weight[10];

      // Weights needed are 5x5, for SIMD purpose made this array as 5x8.
      float weight[5][8] = { { 0 } };
      int off = k * layer_config->out_channels + i;

      // In layer 0, the convolution process happens at 5x5.
      // The weights needed for 5x5 block are same across the in-channels,
      // which is why the load of weights happens once for each in-channel.
      prepare_weights_for_5x5_convolve(layer_config->weights, off, weight,
                                       cstep, shuffle_weight, weight_mask_0,
                                       weight_mask_1);

      for (int h = 0, u = 0; h < in_height - kFilterHeight + 1;
           h += kSkipHeight, ++u) {
        const int out_h = u * out_stride;
        int v = 0;
        int w = 0;
        int rem_width = in_width;
        // Processing 3 5x5 blocks at a time, if sufficient width is present.
        while (rem_width >= kMinWidthFor3_5x5Blocks) {
          __m256 load_src_0, load_src_1;
          __m256 accum_src_0 = _mm256_setzero_ps();
          __m256 accum_src_1 = _mm256_setzero_ps();
          const float *input_ptr = &input[k][h * in_stride + w];
          PERFORM_CONVOLVE_FOR_3_5X5_BLOCKS();

          // Accumulate across column.
          __m256 accum = _mm256_hadd_ps(accum_src_0, accum_src_1);
          __m128 tmp_reg_0 = _mm256_extractf128_ps(accum_src_0, 1);
          __m128 tmp_reg_1 = _mm256_extractf128_ps(accum_src_1, 1);

          __m128 accum_l = _mm256_castps256_ps128(accum);
          __m128 accum_h = _mm256_extractf128_ps(accum, 1);

          __m128 tmp_reg_2 = _mm_add_ps(accum_l, tmp_reg_0);
          __m128 tmp_reg_3 = _mm_add_ps(tmp_reg_0, accum_h);
          __m128 tmp_reg_4 = _mm_add_ps(tmp_reg_1, accum_h);

          // 1st 5x5 block output.
          output[i][out_h + v] =
              out_ch_bias + _mm_cvtss_f32(tmp_reg_2) +
              _mm_cvtss_f32(_mm_shuffle_ps(accum_l, accum_l, 1));

          // 2nd 5x5 block output.
          output[i][out_h + v + 1] =
              out_ch_bias +
              _mm_cvtss_f32(_mm_shuffle_ps(tmp_reg_3, tmp_reg_3, 1)) +
              _mm_cvtss_f32(_mm_shuffle_ps(accum_l, accum_l, 2));

          // 3rd 5x5 block output.
          output[i][out_h + v + 2] =
              out_ch_bias +
              _mm_cvtss_f32(_mm_shuffle_ps(tmp_reg_4, tmp_reg_4, 2)) +
              _mm_cvtss_f32(_mm_shuffle_ps(accum_l, accum_l, 3));

          v += 3;
          w += kSkipWidthForNextIter;
          rem_width -= kSkipWidthForNextIter;
        }

        // Process remaining blocks as single 5x5 block at a time.
        while (rem_width >= kFilterWidth) {
          float last_column_sum = 0;
          __m128 accum = _mm_setzero_ps();
          const float *input_ptr = &input[k][h * in_stride + w];
          PERFORM_CONVOLVE_FOR_1_5X5_BLOCK(shuffle_weight, accum, in_stride);

          // Accumulate across column.
          accum = _mm_hadd_ps(accum, accum);
          output[i][out_h + v] = out_ch_bias + last_column_sum +
                                 _mm_cvtss_f32(accum) +
                                 _mm_cvtss_f32(_mm_shuffle_ps(accum, accum, 1));

          v += 1;
          w += kSkipWidth;
          rem_width -= kSkipWidth;
        }
      }
    }
  }
}

// AVX2 implementation for layer 1.
static inline void cnn_convolve_no_maxpool_padding_valid_layer1_avx2(
    const float **input, int in_stride,
    const CNN_LAYER_CONFIG *const layer_config, float **output, int out_stride,
    int start_idx, const int cstep, const int channel_step) {
  const __m256i shuffle_output_mask =
      _mm256_load_si256((const __m256i *)shuffle_output_layer_1_and_2);

  const float *const w = layer_config->weights;
  const int in_channels = layer_config->in_channels;
  const int out_channels = layer_config->out_channels;
  const int cstep2 = cstep * 2;
  const int cstep3 = cstep * 3;
  if (in_channels <= 20) {
    DECLARE_ALIGNED(32, float, in_shuf[20][8][4][8]);
    for (int k = 0; k < in_channels; ++k) {
      const float *in_k = input[k];
      for (int u = 0; u < 8; ++u) {
        const __m256 r0_lo = _mm256_loadu_ps(in_k);
        const __m256 r0_hi = _mm256_loadu_ps(in_k + 8);
        const __m256 r1_lo = _mm256_loadu_ps(in_k + in_stride);
        const __m256 r1_hi = _mm256_loadu_ps(in_k + in_stride + 8);
        _mm256_store_ps(
            in_shuf[k][u][0],
            _mm256_shuffle_ps(r0_lo, r0_hi, _MM_SHUFFLE(2, 0, 2, 0)));
        _mm256_store_ps(
            in_shuf[k][u][1],
            _mm256_shuffle_ps(r0_lo, r0_hi, _MM_SHUFFLE(3, 1, 3, 1)));
        _mm256_store_ps(
            in_shuf[k][u][2],
            _mm256_shuffle_ps(r1_lo, r1_hi, _MM_SHUFFLE(2, 0, 2, 0)));
        _mm256_store_ps(
            in_shuf[k][u][3],
            _mm256_shuffle_ps(r1_lo, r1_hi, _MM_SHUFFLE(3, 1, 3, 1)));
        in_k += in_stride * 2;
      }
    }
    for (int i = start_idx; i < out_channels; i += channel_step) {
      const __m256 b0 = _mm256_set1_ps(layer_config->bias[i]);
      __m256 s0 = b0, s1 = b0, s2 = b0, s3 = b0;
      __m256 s4 = b0, s5 = b0, s6 = b0, s7 = b0;
      for (int k = 0; k < in_channels; ++k) {
        const int off0 = k * out_channels + i;
        const __m256 w0_0 = _mm256_set1_ps(w[off0]);
        const __m256 w0_1 = _mm256_set1_ps(w[off0 + cstep]);
        const __m256 w0_2 = _mm256_set1_ps(w[off0 + cstep2]);
        const __m256 w0_3 = _mm256_set1_ps(w[off0 + cstep3]);
#define ACCUM_L1(u, acc)                                                      \
  do {                                                                        \
    const __m256 pl =                                                         \
        _mm256_add_ps(_mm256_mul_ps(_mm256_load_ps(in_shuf[k][u][0]), w0_0),  \
                      _mm256_mul_ps(_mm256_load_ps(in_shuf[k][u][2]), w0_2)); \
    const __m256 pr =                                                         \
        _mm256_add_ps(_mm256_mul_ps(_mm256_load_ps(in_shuf[k][u][1]), w0_1),  \
                      _mm256_mul_ps(_mm256_load_ps(in_shuf[k][u][3]), w0_3)); \
    acc = _mm256_add_ps(acc, _mm256_add_ps(pl, pr));                          \
  } while (0)
        ACCUM_L1(0, s0);
        ACCUM_L1(1, s1);
        ACCUM_L1(2, s2);
        ACCUM_L1(3, s3);
        ACCUM_L1(4, s4);
        ACCUM_L1(5, s5);
        ACCUM_L1(6, s6);
        ACCUM_L1(7, s7);
#undef ACCUM_L1
      }
      _mm256_storeu_ps(&output[i][0 * out_stride],
                       _mm256_permutevar8x32_ps(s0, shuffle_output_mask));
      _mm256_storeu_ps(&output[i][1 * out_stride],
                       _mm256_permutevar8x32_ps(s1, shuffle_output_mask));
      _mm256_storeu_ps(&output[i][2 * out_stride],
                       _mm256_permutevar8x32_ps(s2, shuffle_output_mask));
      _mm256_storeu_ps(&output[i][3 * out_stride],
                       _mm256_permutevar8x32_ps(s3, shuffle_output_mask));
      _mm256_storeu_ps(&output[i][4 * out_stride],
                       _mm256_permutevar8x32_ps(s4, shuffle_output_mask));
      _mm256_storeu_ps(&output[i][5 * out_stride],
                       _mm256_permutevar8x32_ps(s5, shuffle_output_mask));
      _mm256_storeu_ps(&output[i][6 * out_stride],
                       _mm256_permutevar8x32_ps(s6, shuffle_output_mask));
      _mm256_storeu_ps(&output[i][7 * out_stride],
                       _mm256_permutevar8x32_ps(s7, shuffle_output_mask));
    }
    return;
  }

  av1_cnn_convolve_no_maxpool_padding_valid_c(input, 16, 16, in_stride,
                                              layer_config, output, out_stride,
                                              start_idx, cstep, channel_step);
}

// AVX2 implementation for layer 2.
static inline void cnn_convolve_no_maxpool_padding_valid_layer2_avx2(
    const float **input, int in_stride,
    const CNN_LAYER_CONFIG *const layer_config, float **output, int out_stride,
    int start_idx, const int cstep, const int channel_step) {
  const __m256i shuffle_output_mask =
      _mm256_load_si256((const __m256i *)shuffle_output_layer_1_and_2);

  const float *const w = layer_config->weights;
  const int in_channels = layer_config->in_channels;
  const int out_channels = layer_config->out_channels;
  const int cstep2 = cstep * 2;
  const int cstep3 = cstep * 3;
  if (in_channels <= 20) {
    DECLARE_ALIGNED(32, float, in_shuf[20][2][4][8]);
    for (int k = 0; k < in_channels; ++k) {
      const float *in_k = input[k];
      for (int u = 0; u < 2; ++u) {
        const __m256 r0 = _mm256_loadu_ps(in_k);
        const __m256 r1 = _mm256_loadu_ps(in_k + in_stride);
        const __m256 r2 = _mm256_loadu_ps(in_k + in_stride * 2);
        const __m256 r3 = _mm256_loadu_ps(in_k + in_stride * 3);
        _mm256_store_ps(in_shuf[k][u][0],
                        _mm256_shuffle_ps(r0, r2, _MM_SHUFFLE(2, 0, 2, 0)));
        _mm256_store_ps(in_shuf[k][u][1],
                        _mm256_shuffle_ps(r0, r2, _MM_SHUFFLE(3, 1, 3, 1)));
        _mm256_store_ps(in_shuf[k][u][2],
                        _mm256_shuffle_ps(r1, r3, _MM_SHUFFLE(2, 0, 2, 0)));
        _mm256_store_ps(in_shuf[k][u][3],
                        _mm256_shuffle_ps(r1, r3, _MM_SHUFFLE(3, 1, 3, 1)));
        in_k += in_stride * 4;
      }
    }
    int i = start_idx;
    if (channel_step == 1) {
      for (; i + 1 < out_channels; i += 2) {
        const __m256 b0 = _mm256_set1_ps(layer_config->bias[i]);
        const __m256 b1 = _mm256_set1_ps(layer_config->bias[i + 1]);
        __m256 s0_0 = b0, s0_1 = b0;
        __m256 s1_0 = b1, s1_1 = b1;
        for (int k = 0; k < in_channels; ++k) {
          const int off0 = k * out_channels + i;
#define ACCUM_L2_CH(ch, s_u0, s_u1)                                         \
  do {                                                                      \
    const __m256 wa = _mm256_set1_ps(w[off0 + (ch)]);                       \
    const __m256 wb = _mm256_set1_ps(w[off0 + (ch) + cstep]);               \
    const __m256 wc = _mm256_set1_ps(w[off0 + (ch) + cstep2]);              \
    const __m256 wd = _mm256_set1_ps(w[off0 + (ch) + cstep3]);              \
    const __m256 pl0 =                                                      \
        _mm256_add_ps(_mm256_mul_ps(_mm256_load_ps(in_shuf[k][0][0]), wa),  \
                      _mm256_mul_ps(_mm256_load_ps(in_shuf[k][0][2]), wc)); \
    const __m256 pr0 =                                                      \
        _mm256_add_ps(_mm256_mul_ps(_mm256_load_ps(in_shuf[k][0][1]), wb),  \
                      _mm256_mul_ps(_mm256_load_ps(in_shuf[k][0][3]), wd)); \
    s_u0 = _mm256_add_ps(s_u0, _mm256_add_ps(pl0, pr0));                    \
    const __m256 pl1 =                                                      \
        _mm256_add_ps(_mm256_mul_ps(_mm256_load_ps(in_shuf[k][1][0]), wa),  \
                      _mm256_mul_ps(_mm256_load_ps(in_shuf[k][1][2]), wc)); \
    const __m256 pr1 =                                                      \
        _mm256_add_ps(_mm256_mul_ps(_mm256_load_ps(in_shuf[k][1][1]), wb),  \
                      _mm256_mul_ps(_mm256_load_ps(in_shuf[k][1][3]), wd)); \
    s_u1 = _mm256_add_ps(s_u1, _mm256_add_ps(pl1, pr1));                    \
  } while (0)
          ACCUM_L2_CH(0, s0_0, s0_1);
          ACCUM_L2_CH(1, s1_0, s1_1);
        }
        _mm256_storeu_ps(&output[i][0],
                         _mm256_permutevar8x32_ps(s0_0, shuffle_output_mask));
        _mm256_storeu_ps(&output[i][out_stride * 2],
                         _mm256_permutevar8x32_ps(s0_1, shuffle_output_mask));
        _mm256_storeu_ps(&output[i + 1][0],
                         _mm256_permutevar8x32_ps(s1_0, shuffle_output_mask));
        _mm256_storeu_ps(&output[i + 1][out_stride * 2],
                         _mm256_permutevar8x32_ps(s1_1, shuffle_output_mask));
      }
    }
    for (; i < out_channels; i += channel_step) {
      const __m256 b0 = _mm256_set1_ps(layer_config->bias[i]);
      __m256 s0_0 = b0, s0_1 = b0;
      for (int k = 0; k < in_channels; ++k) {
        const int off0 = k * out_channels + i;
        ACCUM_L2_CH(0, s0_0, s0_1);
      }
      _mm256_storeu_ps(&output[i][0],
                       _mm256_permutevar8x32_ps(s0_0, shuffle_output_mask));
      _mm256_storeu_ps(&output[i][out_stride * 2],
                       _mm256_permutevar8x32_ps(s0_1, shuffle_output_mask));
    }
#undef ACCUM_L2_CH
    return;
  }
  av1_cnn_convolve_no_maxpool_padding_valid_c(input, 8, 8, in_stride,
                                              layer_config, output, out_stride,
                                              start_idx, cstep, channel_step);
}

static inline void cnn_convolve_no_maxpool_padding_valid_layer3_avx2(
    const float **input, int in_stride,
    const CNN_LAYER_CONFIG *const layer_config, float **output, int out_stride,
    int start_idx, const int cstep, const int channel_step) {
  const float *const w = layer_config->weights;
  const int out_channels = layer_config->out_channels;
  const int in_channels = layer_config->in_channels;
  const int cstep2 = cstep * 2;
  const int cstep3 = cstep * 3;
  int i = start_idx;
  if (channel_step == 1) {
    for (; i + 3 < out_channels; i += 4) {
      __m128 sum0 = _mm_set1_ps(layer_config->bias[i]);
      __m128 sum1 = _mm_set1_ps(layer_config->bias[i + 1]);
      __m128 sum2 = _mm_set1_ps(layer_config->bias[i + 2]);
      __m128 sum3 = _mm_set1_ps(layer_config->bias[i + 3]);
      for (int k = 0; k < in_channels; ++k) {
        const float *in_k = input[k];
        const __m128 r0 = _mm_loadu_ps(in_k);
        const __m128 r1 = _mm_loadu_ps(in_k + in_stride);
        const __m128 r2 = _mm_loadu_ps(in_k + in_stride * 2);
        const __m128 r3 = _mm_loadu_ps(in_k + in_stride * 3);
        const __m128 p0 = _mm_shuffle_ps(r0, r2, _MM_SHUFFLE(2, 0, 2, 0));
        const __m128 p1 = _mm_shuffle_ps(r0, r2, _MM_SHUFFLE(3, 1, 3, 1));
        const __m128 p2 = _mm_shuffle_ps(r1, r3, _MM_SHUFFLE(2, 0, 2, 0));
        const __m128 p3 = _mm_shuffle_ps(r1, r3, _MM_SHUFFLE(3, 1, 3, 1));
        const int off = k * out_channels + i;

        sum0 = _mm_add_ps(sum0, _mm_mul_ps(_mm_set1_ps(w[off]), p0));
        sum0 = _mm_add_ps(sum0, _mm_mul_ps(_mm_set1_ps(w[off + cstep]), p1));
        sum0 = _mm_add_ps(sum0, _mm_mul_ps(_mm_set1_ps(w[off + cstep2]), p2));
        sum0 = _mm_add_ps(sum0, _mm_mul_ps(_mm_set1_ps(w[off + cstep3]), p3));

        sum1 = _mm_add_ps(sum1, _mm_mul_ps(_mm_set1_ps(w[off + 1]), p0));
        sum1 =
            _mm_add_ps(sum1, _mm_mul_ps(_mm_set1_ps(w[off + 1 + cstep]), p1));
        sum1 =
            _mm_add_ps(sum1, _mm_mul_ps(_mm_set1_ps(w[off + 1 + cstep2]), p2));
        sum1 =
            _mm_add_ps(sum1, _mm_mul_ps(_mm_set1_ps(w[off + 1 + cstep3]), p3));

        sum2 = _mm_add_ps(sum2, _mm_mul_ps(_mm_set1_ps(w[off + 2]), p0));
        sum2 =
            _mm_add_ps(sum2, _mm_mul_ps(_mm_set1_ps(w[off + 2 + cstep]), p1));
        sum2 =
            _mm_add_ps(sum2, _mm_mul_ps(_mm_set1_ps(w[off + 2 + cstep2]), p2));
        sum2 =
            _mm_add_ps(sum2, _mm_mul_ps(_mm_set1_ps(w[off + 2 + cstep3]), p3));

        sum3 = _mm_add_ps(sum3, _mm_mul_ps(_mm_set1_ps(w[off + 3]), p0));
        sum3 =
            _mm_add_ps(sum3, _mm_mul_ps(_mm_set1_ps(w[off + 3 + cstep]), p1));
        sum3 =
            _mm_add_ps(sum3, _mm_mul_ps(_mm_set1_ps(w[off + 3 + cstep2]), p2));
        sum3 =
            _mm_add_ps(sum3, _mm_mul_ps(_mm_set1_ps(w[off + 3 + cstep3]), p3));
      }
      _mm_storel_pi((__m64 *)&output[i][0], sum0);
      _mm_storeh_pi((__m64 *)&output[i][out_stride], sum0);
      _mm_storel_pi((__m64 *)&output[i + 1][0], sum1);
      _mm_storeh_pi((__m64 *)&output[i + 1][out_stride], sum1);
      _mm_storel_pi((__m64 *)&output[i + 2][0], sum2);
      _mm_storeh_pi((__m64 *)&output[i + 2][out_stride], sum2);
      _mm_storel_pi((__m64 *)&output[i + 3][0], sum3);
      _mm_storeh_pi((__m64 *)&output[i + 3][out_stride], sum3);
    }
  }
  for (; i < out_channels; i += channel_step) {
    __m128 sum = _mm_set1_ps(layer_config->bias[i]);
    for (int k = 0; k < in_channels; ++k) {
      const float *in_k = input[k];
      const __m128 r0 = _mm_loadu_ps(in_k);
      const __m128 r1 = _mm_loadu_ps(in_k + in_stride);
      const __m128 r2 = _mm_loadu_ps(in_k + in_stride * 2);
      const __m128 r3 = _mm_loadu_ps(in_k + in_stride * 3);
      const __m128 p0 = _mm_shuffle_ps(r0, r2, _MM_SHUFFLE(2, 0, 2, 0));
      const __m128 p1 = _mm_shuffle_ps(r0, r2, _MM_SHUFFLE(3, 1, 3, 1));
      const __m128 p2 = _mm_shuffle_ps(r1, r3, _MM_SHUFFLE(2, 0, 2, 0));
      const __m128 p3 = _mm_shuffle_ps(r1, r3, _MM_SHUFFLE(3, 1, 3, 1));
      const int off = k * out_channels + i;
      sum = _mm_add_ps(sum, _mm_mul_ps(_mm_set1_ps(w[off]), p0));
      sum = _mm_add_ps(sum, _mm_mul_ps(_mm_set1_ps(w[off + cstep]), p1));
      sum = _mm_add_ps(sum, _mm_mul_ps(_mm_set1_ps(w[off + cstep2]), p2));
      sum = _mm_add_ps(sum, _mm_mul_ps(_mm_set1_ps(w[off + cstep3]), p3));
    }
    _mm_storel_pi((__m64 *)&output[i][0], sum);
    _mm_storeh_pi((__m64 *)&output[i][out_stride], sum);
  }
}

static inline void cnn_convolve_no_maxpool_padding_valid_layer4_avx2(
    const float **input, int in_stride,
    const CNN_LAYER_CONFIG *const layer_config, float **output, int start_idx,
    const int cstep, const int channel_step) {
  const float *const w = layer_config->weights;
  const int out_channels = layer_config->out_channels;
  const int in_channels = layer_config->in_channels;
  const int cstep2 = cstep * 2;
  const int cstep3 = cstep * 3;
  int i = start_idx;
  if (channel_step == 1) {
    for (; i + 7 < out_channels; i += 8) {
      __m256 sum = _mm256_loadu_ps(&layer_config->bias[i]);
      for (int k = 0; k < in_channels; ++k) {
        const float *in_k = input[k];
        const int off = k * out_channels + i;
        sum = _mm256_add_ps(sum, _mm256_mul_ps(_mm256_loadu_ps(&w[off]),
                                               _mm256_set1_ps(in_k[0])));
        sum = _mm256_add_ps(sum, _mm256_mul_ps(_mm256_loadu_ps(&w[off + cstep]),
                                               _mm256_set1_ps(in_k[1])));
        sum =
            _mm256_add_ps(sum, _mm256_mul_ps(_mm256_loadu_ps(&w[off + cstep2]),
                                             _mm256_set1_ps(in_k[in_stride])));
        sum = _mm256_add_ps(sum,
                            _mm256_mul_ps(_mm256_loadu_ps(&w[off + cstep3]),
                                          _mm256_set1_ps(in_k[in_stride + 1])));
      }
      float res[8];
      _mm256_storeu_ps(res, sum);
      for (int c = 0; c < 8; ++c) output[i + c][0] = res[c];
    }
    for (; i + 3 < out_channels; i += 4) {
      __m128 sum = _mm_loadu_ps(&layer_config->bias[i]);
      for (int k = 0; k < in_channels; ++k) {
        const float *in_k = input[k];
        const int off = k * out_channels + i;
        sum = _mm_add_ps(
            sum, _mm_mul_ps(_mm_loadu_ps(&w[off]), _mm_set1_ps(in_k[0])));
        sum = _mm_add_ps(sum, _mm_mul_ps(_mm_loadu_ps(&w[off + cstep]),
                                         _mm_set1_ps(in_k[1])));
        sum = _mm_add_ps(sum, _mm_mul_ps(_mm_loadu_ps(&w[off + cstep2]),
                                         _mm_set1_ps(in_k[in_stride])));
        sum = _mm_add_ps(sum, _mm_mul_ps(_mm_loadu_ps(&w[off + cstep3]),
                                         _mm_set1_ps(in_k[in_stride + 1])));
      }
      float res[4];
      _mm_storeu_ps(res, sum);
      for (int c = 0; c < 4; ++c) output[i + c][0] = res[c];
    }
  }
  for (; i < out_channels; i += channel_step) {
    float sum = layer_config->bias[i];
    for (int k = 0; k < in_channels; ++k) {
      const float *in_k = input[k];
      const int off = k * out_channels + i;
      sum += w[off] * in_k[0];
      sum += w[off + cstep] * in_k[1];
      sum += w[off + cstep2] * in_k[in_stride];
      sum += w[off + cstep3] * in_k[in_stride + 1];
    }
    output[i][0] = sum;
  }
}

// AVX2 variant of av1_cnn_convolve_no_maxpool_padding_valid_c(), when
// filter_width and filter_height are equal to 2.
// As per the layer config set by av1_intra_mode_cnn_partition_cnn_config,
// the filter_width and filter_height are equal to 2 for layer >= 1. So
// convolution happens at 2x2 for layer >= 1.
static void cnn_convolve_no_maxpool_padding_valid_2x2_avx2(
    const float **input, int in_width, int in_height, int in_stride,
    const CNN_LAYER_CONFIG *const layer_config, float **output, int out_stride,
    int start_idx, const int cstep, const int channel_step) {
  assert(layer_config->filter_width == 2 && layer_config->filter_height == 2);
  assert(layer_config->skip_width == 2 && layer_config->skip_height == 2);

  if (in_width == 16 && in_height == 16) {
    // This case of in_width and in_height equal to 16 corresponds to layer 1.
    // The output size of this layer is 8x8.
    cnn_convolve_no_maxpool_padding_valid_layer1_avx2(
        input, in_stride, layer_config, output, out_stride, start_idx, cstep,
        channel_step);
  } else if (in_width == 8 && in_height == 8) {
    // This case of in_width and in_height equal to 8 corresponds to layer 2.
    // The output size of this layer is 4x4.
    cnn_convolve_no_maxpool_padding_valid_layer2_avx2(
        input, in_stride, layer_config, output, out_stride, start_idx, cstep,
        channel_step);
  } else if (in_width == 4 && in_height == 4) {
    cnn_convolve_no_maxpool_padding_valid_layer3_avx2(
        input, in_stride, layer_config, output, out_stride, start_idx, cstep,
        channel_step);
  } else if (in_width == 2 && in_height == 2) {
    cnn_convolve_no_maxpool_padding_valid_layer4_avx2(
        input, in_stride, layer_config, output, start_idx, cstep, channel_step);
  } else {
    av1_cnn_convolve_no_maxpool_padding_valid_c(
        input, in_width, in_height, in_stride, layer_config, output, out_stride,
        start_idx, cstep, channel_step);
  }
}

// AVX2 variant of av1_cnn_convolve_no_maxpool_padding_valid_c().
// As per the current encoder, av1_cnn_convolve function gets called for
// block size equal to 64x64. av1_cnn_convolve() uses layer config values
// set by av1_intra_mode_cnn_partition_cnn_config. The following are a few
// details related to each layer's config parameters.
// Layer_Number in_size out_size filter_wd filter_ht skip_wd skip_ht
//     0         64x64    16x16      5         5         4       4
//     1         16x16    8x8        2         2         2       2
//     2         8x8      4x4        2         2         2       2
//     3         4x4      2x2        2         2         2       2
//     4         2x2      1x1        2         2         2       2
// Here,
// filter_wd = filter_width and filter_ht = filter_height,
// skip_wd = skip_width and skip_ht = skip_height.
void av1_cnn_convolve_no_maxpool_padding_valid_avx2(
    const float **input, int in_width, int in_height, int in_stride,
    const CNN_LAYER_CONFIG *layer_config, float **output, int out_stride,
    int start_idx, int cstep, int channel_step) {
  if (layer_config->filter_width == 5 && layer_config->filter_height == 5 &&
      layer_config->skip_width == 4 && layer_config->skip_height == 4) {
    cnn_convolve_no_maxpool_padding_valid_5x5_avx2(
        input, in_width, in_height, in_stride, layer_config, output, out_stride,
        start_idx, cstep, channel_step);
  } else if (layer_config->filter_width == 2 &&
             layer_config->filter_height == 2 &&
             layer_config->skip_width == 2 && layer_config->skip_height == 2) {
    cnn_convolve_no_maxpool_padding_valid_2x2_avx2(
        input, in_width, in_height, in_stride, layer_config, output, out_stride,
        start_idx, cstep, channel_step);
  } else {
    av1_cnn_convolve_no_maxpool_padding_valid_c(
        input, in_width, in_height, in_stride, layer_config, output, out_stride,
        start_idx, cstep, channel_step);
  }
}
