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

#ifndef AOM_AV1_ENCODER_TEMPORAL_FILTER_HWY_H_
#define AOM_AV1_ENCODER_TEMPORAL_FILTER_HWY_H_

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/mathutils.h"
#include "aom_ports/mem.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/temporal_filter.h"
#include "third_party/highway/hwy/highway.h"

HWY_BEFORE_NAMESPACE();

namespace {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

constexpr int kSseStrideHwy = BW + 4;

HWY_ATTR HWY_INLINE double EstimateNoiseFromSinglePlaneHwy(
    const uint8_t *src, int height, int width, int stride, int edge_thresh) {
  int count = 0;
  int64_t accum = 0;
  const hn::ScalableTag<int16_t> di16;
  const hn::Rebind<uint8_t, decltype(di16)> du8;
  const hn::Repartition<int32_t, decltype(di16)> di32;
  const int step = static_cast<int>(hn::Lanes(di16));
  const int w_vec = (width - 2) & ~(step - 1);

  const auto edge_threshold = hn::Set(di16, static_cast<int16_t>(edge_thresh));
  const auto one16 = hn::Set(di16, static_cast<int16_t>(1));
  auto sum_accumulator = hn::Zero(di32);
  auto count_accumulator = hn::Zero(di32);

  for (int w = 1; w < w_vec; w += step) {
    int h = 1;
    const int start_idx = h * stride + w;
    const int stride_0 = start_idx - stride;

    auto count_row_accum = hn::Zero(di16);
    const auto A = hn::PromoteTo(di16, hn::LoadU(du8, &src[stride_0 - 1]));
    const auto C = hn::PromoteTo(di16, hn::LoadU(du8, &src[stride_0 + 1]));
    const auto D = hn::PromoteTo(di16, hn::LoadU(du8, &src[start_idx - 1]));
    const auto F = hn::PromoteTo(di16, hn::LoadU(du8, &src[start_idx + 1]));
    auto B = hn::PromoteTo(di16, hn::LoadU(du8, &src[stride_0]));
    auto E = hn::PromoteTo(di16, hn::LoadU(du8, &src[start_idx]));

    auto sub_AC = hn::Sub(A, C);
    auto sum_AC = hn::Add(A, C);
    auto sub_DF = hn::Sub(D, F);
    auto sum_DF = hn::Add(D, F);

    for (; h < height - 1; h++) {
      const int k = h * stride + w;
      const auto G = hn::PromoteTo(di16, hn::LoadU(du8, &src[k + stride - 1]));
      const auto H = hn::PromoteTo(di16, hn::LoadU(du8, &src[k + stride]));
      const auto I = hn::PromoteTo(di16, hn::LoadU(du8, &src[k + stride + 1]));

      const auto sub_GI = hn::Sub(G, I);
      const auto sum_GI = hn::Add(G, I);
      const auto sub_BH = hn::Sub(B, H);

      const auto gx =
          hn::Abs(hn::Add(hn::Add(sub_AC, sub_GI), hn::ShiftLeft<1>(sub_DF)));
      const auto gy =
          hn::Abs(hn::Add(hn::Sub(sum_AC, sum_GI), hn::ShiftLeft<1>(sub_BH)));
      const auto ga = hn::Add(gx, gy);

      const auto mask = hn::Gt(edge_threshold, ga);
      if (!hn::AllFalse(di16, mask)) {
        const auto sum_BH = hn::Add(B, H);
        const auto sum_DFBH = hn::ShiftLeft<1>(hn::Add(sum_DF, sum_BH));
        const auto sum_ACGI = hn::Add(sum_AC, sum_GI);
        const auto v =
            hn::Add(hn::Sub(hn::ShiftLeft<2>(E), sum_DFBH), sum_ACGI);
        const auto var = hn::IfThenElseZero(mask, hn::Abs(v));
        sum_accumulator =
            hn::Add(sum_accumulator, hn::WidenMulPairwiseAdd(di32, var, one16));
        count_row_accum =
            hn::MaskedAddOr(count_row_accum, mask, count_row_accum, one16);
      }

      sub_AC = sub_DF;
      sub_DF = sub_GI;
      sum_AC = sum_DF;
      sum_DF = sum_GI;
      B = E;
      E = H;
    }
    count_accumulator =
        hn::Add(count_accumulator,
                hn::WidenMulPairwiseAdd(di32, count_row_accum, one16));
  }

  for (int h = 1; h < height - 1; ++h) {
    for (int w = w_vec + 1; w < width - 1; ++w) {
      const int k = h * stride + w;
      const int g_x = (src[k - stride - 1] - src[k - stride + 1]) +
                      (src[k + stride - 1] - src[k + stride + 1]) +
                      2 * (src[k - 1] - src[k + 1]);
      const int g_y = (src[k - stride - 1] - src[k + stride - 1]) +
                      (src[k - stride + 1] - src[k + stride + 1]) +
                      2 * (src[k - stride] - src[k + stride]);
      const int ga = abs(g_x) + abs(g_y);
      if (ga < edge_thresh) {
        const int v =
            4 * src[k] -
            2 * (src[k - 1] + src[k + 1] + src[k - stride] + src[k + stride]) +
            (src[k - stride - 1] + src[k - stride + 1] + src[k + stride - 1] +
             src[k + stride + 1]);
        accum += abs(v);
        ++count;
      }
    }
  }

  accum += hn::ReduceSum(di32, sum_accumulator);
  count += static_cast<int>(hn::ReduceSum(di32, count_accumulator));
  return (count < 16) ? -1.0
                      : static_cast<double>(accum) / (6 * count) * SQRT_PI_BY_2;
}

#if CONFIG_AV1_HIGHBITDEPTH
HWY_ATTR HWY_INLINE double HighbdEstimateNoiseFromSinglePlaneHwy(
    const uint16_t *src16, int height, int width, int stride, int bit_depth,
    int edge_thresh) {
  int count = 0;
  int64_t accum = 0;
  const hn::ScalableTag<int32_t> di32;
  const hn::Rebind<uint16_t, decltype(di32)> du16;
  const int step = static_cast<int>(hn::Lanes(di32));
  const int w_vec = (width - 2) & ~(step - 1);
  const int shift = bit_depth - 8;
  const int round_val = shift > 0 ? (1 << (shift - 1)) : 0;

  const auto v_round = hn::Set(di32, round_val);
  const auto v_edge_thresh = hn::Set(di32, edge_thresh);
  const auto one32 = hn::Set(di32, 1);
  auto sum_accumulator = hn::Zero(di32);
  auto count_accumulator = hn::Zero(di32);

  for (int w = 1; w < w_vec; w += step) {
    int h = 1;
    const int start_idx = h * stride + w;
    const int stride_0 = start_idx - stride;

    const auto A = hn::PromoteTo(di32, hn::LoadU(du16, &src16[stride_0 - 1]));
    const auto C = hn::PromoteTo(di32, hn::LoadU(du16, &src16[stride_0 + 1]));
    const auto D = hn::PromoteTo(di32, hn::LoadU(du16, &src16[start_idx - 1]));
    const auto F = hn::PromoteTo(di32, hn::LoadU(du16, &src16[start_idx + 1]));
    auto B = hn::PromoteTo(di32, hn::LoadU(du16, &src16[stride_0]));
    auto E = hn::PromoteTo(di32, hn::LoadU(du16, &src16[start_idx]));

    auto sub_AC = hn::Sub(A, C);
    auto sum_AC = hn::Add(A, C);
    auto sub_DF = hn::Sub(D, F);
    auto sum_DF = hn::Add(D, F);

    for (; h < height - 1; h++) {
      const int k = h * stride + w;
      const auto G =
          hn::PromoteTo(di32, hn::LoadU(du16, &src16[k + stride - 1]));
      const auto H = hn::PromoteTo(di32, hn::LoadU(du16, &src16[k + stride]));
      const auto I =
          hn::PromoteTo(di32, hn::LoadU(du16, &src16[k + stride + 1]));

      const auto sub_GI = hn::Sub(G, I);
      const auto sum_GI = hn::Add(G, I);
      const auto sub_BH = hn::Sub(B, H);

      const auto gx =
          hn::Abs(hn::Add(hn::Add(sub_AC, sub_GI), hn::ShiftLeft<1>(sub_DF)));
      const auto gy =
          hn::Abs(hn::Add(hn::Sub(sum_AC, sum_GI), hn::ShiftLeft<1>(sub_BH)));
      const auto ga =
          hn::ShiftRightSame(hn::Add(hn::Add(gx, gy), v_round), shift);

      const auto mask = hn::Gt(v_edge_thresh, ga);
      if (!hn::AllFalse(di32, mask)) {
        const auto sum_BH = hn::Add(B, H);
        const auto sum_DFBH = hn::ShiftLeft<1>(hn::Add(sum_DF, sum_BH));
        const auto sum_ACGI = hn::Add(sum_AC, sum_GI);
        const auto v =
            hn::Add(hn::Sub(hn::ShiftLeft<2>(E), sum_DFBH), sum_ACGI);
        const auto abs_v = hn::Abs(v);
        const auto scaled_v =
            hn::ShiftRightSame(hn::Add(abs_v, v_round), shift);
        sum_accumulator =
            hn::MaskedAddOr(sum_accumulator, mask, sum_accumulator, scaled_v);
        count_accumulator =
            hn::MaskedAddOr(count_accumulator, mask, count_accumulator, one32);
      }

      sub_AC = sub_DF;
      sub_DF = sub_GI;
      sum_AC = sum_DF;
      sum_DF = sum_GI;
      B = E;
      E = H;
    }
  }

  for (int i = 1; i < height - 1; ++i) {
    for (int j = w_vec + 1; j < width - 1; ++j) {
      const int k = i * stride + j;
      const int Gx = (src16[k - stride - 1] - src16[k - stride + 1]) +
                     (src16[k + stride - 1] - src16[k + stride + 1]) +
                     2 * (src16[k - 1] - src16[k + 1]);
      const int Gy = (src16[k - stride - 1] - src16[k + stride - 1]) +
                     (src16[k - stride + 1] - src16[k + stride + 1]) +
                     2 * (src16[k - stride] - src16[k + stride]);
      const int Ga = ROUND_POWER_OF_TWO(abs(Gx) + abs(Gy), shift);
      if (Ga < edge_thresh) {
        const int v = 4 * src16[k] -
                      2 * (src16[k - 1] + src16[k + 1] + src16[k - stride] +
                           src16[k + stride]) +
                      (src16[k - stride - 1] + src16[k - stride + 1] +
                       src16[k + stride - 1] + src16[k + stride + 1]);
        accum += ROUND_POWER_OF_TWO(abs(v), shift);
        ++count;
      }
    }
  }

  accum += hn::ReduceSum(di32, sum_accumulator);
  count += static_cast<int>(hn::ReduceSum(di32, count_accumulator));
  return (count < 16) ? -1.0
                      : static_cast<double>(accum) / (6 * count) * SQRT_PI_BY_2;
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

HWY_INLINE HWY_ATTR void GetSquaredErrorHwy(
    const uint8_t *frame1, const unsigned int stride, const uint8_t *frame2,
    const unsigned int stride2, const int block_width, const int block_height,
    uint16_t *frame_sse, const unsigned int sse_stride) {
  const uint8_t *src1 = frame1;
  const uint8_t *src2 = frame2;
  uint16_t *dst = frame_sse + 2;
  const hn::ScalableTag<int16_t> di16;
  const hn::Rebind<uint8_t, decltype(di16)> du8;
  const hn::Rebind<uint16_t, decltype(di16)> du16;
  const int step = static_cast<int>(hn::Lanes(di16));

  for (int i = 0; i < block_height; i++) {
    for (int j = 0; j < block_width; j += step) {
      const auto vsrc1 = hn::LoadU(du8, src1 + j);
      const auto vsrc2 = hn::LoadU(du8, src2 + j);
      const auto s1 = hn::PromoteTo(di16, vsrc1);
      const auto s2 = hn::PromoteTo(di16, vsrc2);
      const auto diff = hn::Sub(s1, s2);
      const auto res = hn::BitCast(du16, hn::Mul(diff, diff));
      hn::StoreU(res, du16, dst + j);
    }
    dst[-2] = dst[-1] = dst[0];
    dst[block_width] = dst[block_width + 1] = dst[block_width - 1];
    src1 += stride;
    src2 += stride2;
    dst += sse_stride;
  }
}

HWY_INLINE HWY_ATTR hn::Vec<hn::ScalableTag<uint32_t>> LoadAndSum5RowHwy(
    const uint16_t *src) {
  const hn::ScalableTag<uint32_t> du32;
  const hn::Rebind<uint16_t, decltype(du32)> du16;
  const auto c0 = hn::PromoteTo(du32, hn::LoadU(du16, src - 2));
  const auto c1 = hn::PromoteTo(du32, hn::LoadU(du16, src - 1));
  const auto c2 = hn::PromoteTo(du32, hn::LoadU(du16, src));
  const auto c3 = hn::PromoteTo(du32, hn::LoadU(du16, src + 1));
  const auto c4 = hn::PromoteTo(du32, hn::LoadU(du16, src + 2));
  return hn::Add(hn::Add(c0, c1), hn::Add(hn::Add(c2, c3), c4));
}

template <class DF32>
HWY_INLINE HWY_ATTR hn::Vec<DF32> ApproxExpHwy(DF32 df32, hn::Vec<DF32> y) {
  const hn::RebindToSigned<DF32> di32;
  constexpr float kA = (1 << 23) / 0.69314718056f;
  constexpr int32_t kB = 127;
  constexpr int32_t kC = 60801;
  const auto multiplier = hn::Set(df32, kA);
  const auto offset = hn::Set(di32, kB * (1 << 23) - kC);

  y = hn::Mul(y, multiplier);
  y = hn::BitCast(df32, hn::Add(hn::ConvertInRangeTo(di32, y), offset));
  return y;
}

// Bit-exact double-precision implementation of exp(-x) on x in [0, 7].
HWY_INLINE HWY_ATTR hn::Vec<hn::ScalableTag<double>> ExactExpNegPdHwy(
    hn::Vec<hn::ScalableTag<double>> x) {
  const hn::ScalableTag<double> df64;
  const hn::Rebind<int32_t, decltype(df64)> di32;
  const hn::Rebind<int64_t, decltype(df64)> di64;

  x = hn::Min(x, hn::Set(df64, 7.0));
  const auto inv_ln2 = hn::Set(df64, 1.44269504088896340736);
  const auto ln2_hi = hn::Set(df64, 0.6931471805596629565116);
  const auto ln2_lo = hn::Set(df64, 0.2823529056303157712259e-12);
  const auto k = hn::Round(hn::Mul(x, inv_ln2));
  const auto r = hn::Add(hn::Sub(hn::Mul(k, ln2_hi), x), hn::Mul(k, ln2_lo));
  const auto c1 = hn::Set(df64, 1.0);
  const auto c2 = hn::Set(df64, 0.5);
  const auto c3 = hn::Set(df64, 1.0 / 6.0);
  const auto c4 = hn::Set(df64, 1.0 / 24.0);
  const auto c5 = hn::Set(df64, 1.0 / 120.0);
  const auto c6 = hn::Set(df64, 1.0 / 720.0);
  const auto c7 = hn::Set(df64, 1.0 / 5040.0);
  auto p = hn::Add(hn::Mul(c7, r), c6);
  p = hn::Add(hn::Mul(p, r), c5);
  p = hn::Add(hn::Mul(p, r), c4);
  p = hn::Add(hn::Mul(p, r), c3);
  p = hn::Add(hn::Mul(p, r), c2);
  p = hn::Add(hn::Mul(p, r), c1);
  p = hn::Add(hn::Mul(p, r), c1);
  const auto ki = hn::DemoteInRangeTo(di32, k);
  const auto ki64 = hn::PromoteTo(di64, hn::Sub(hn::Set(di32, 1023), ki));
  const auto pow2 = hn::BitCast(df64, hn::ShiftLeft<52>(ki64));
  return hn::Mul(p, pow2);
}

HWY_ATTR void ApplyTemporalFilterPlaneHwy(
    const uint8_t *frame1, const unsigned int stride, const uint8_t *frame2,
    const unsigned int stride2, const int block_width, const int block_height,
    const int *subblock_mses, unsigned int *accumulator, uint16_t *count,
    uint16_t *frame_sse, uint32_t *luma_sse_sum,
    const double inv_num_ref_pixels, const double decay_factor,
    const double inv_factor, const double weight_factor, double *d_factor,
    int tf_wgt_calc_lvl) {
  assert(((block_width == 64) || (block_width == 32)) &&
         ((block_height == 64) || (block_height == 32)));

  DECLARE_ALIGNED(64, uint32_t, acc_5x5_sse[BH][BW]);

  GetSquaredErrorHwy(frame1, stride, frame2, stride2, block_width, block_height,
                     frame_sse, kSseStrideHwy);

  const hn::ScalableTag<uint32_t> du32_vec;
  const int col_step = static_cast<int>(hn::Lanes(du32_vec));
  hn::Vec<hn::ScalableTag<uint32_t>> vsrc[5];

  for (int col = 0; col < block_width; col += col_step) {
    const uint16_t *src = frame_sse + 2 + col;

    for (int i = 2; i < 5; i++) {
      vsrc[i] = LoadAndSum5RowHwy(src);
      src += kSseStrideHwy;
    }

    vsrc[0] = vsrc[2];
    vsrc[1] = vsrc[2];

    for (int row = 0; row < block_height; row++) {
      const auto vsum01 = hn::Add(vsrc[0], vsrc[1]);
      const auto vsum23 = hn::Add(vsrc[2], vsrc[3]);
      const auto vsum = hn::Add(hn::Add(vsum01, vsum23), vsrc[4]);

      hn::StoreU(vsum, du32_vec, &acc_5x5_sse[row][col]);

      vsrc[0] = vsrc[1];
      vsrc[1] = vsrc[2];
      vsrc[2] = vsrc[3];
      vsrc[3] = vsrc[4];

      if (row <= block_height - 4) {
        vsrc[4] = LoadAndSum5RowHwy(src);
        src += kSseStrideHwy;
      } else {
        vsrc[4] = vsrc[3];
      }
    }
  }

  const hn::ScalableTag<double> df64;
  const hn::Rebind<float, decltype(df64)> df32;
  const hn::Rebind<int32_t, decltype(df64)> di32_half;
  const hn::Rebind<uint32_t, decltype(df64)> du32_half;
  const hn::Rebind<uint16_t, decltype(df64)> du16_qtr;
  const hn::Rebind<uint8_t, decltype(df64)> du8_oct;
  const int wgt_step = static_cast<int>(hn::Lanes(df64));

  hn::Vec<hn::ScalableTag<double>> subblock_mses_reg[NUM_16X16];
  hn::Vec<hn::ScalableTag<double>> d_factor_mul_n_decay_qr_invs[NUM_16X16];
  for (int idx = 0; idx < NUM_16X16; idx++) {
    subblock_mses_reg[idx] = hn::Set(df64, subblock_mses[idx] * inv_factor);
    d_factor_mul_n_decay_qr_invs[idx] =
        hn::Set(df64, d_factor[idx] * decay_factor);
  }

  const auto zero = hn::Set(df32, 0.0f);
  const auto point_five = hn::Set(df32, 0.5f);
  const auto seven = hn::Set(df32, 7.0f);
  const auto inv_num_ref_pixel_512 = hn::Set(df64, inv_num_ref_pixels);
  const auto weight_factor_512 = hn::Set(df64, weight_factor);
  const auto tf_weight_scale =
      hn::Set(df32, static_cast<float>(TF_WEIGHT_SCALE));
  const auto tf_weight_scale_pd =
      hn::Set(df64, static_cast<double>(TF_WEIGHT_SCALE));

  for (int i = 0; i < block_height; i++) {
    const int y32_blk_raster_offset = (i >= (block_height >> 1)) << 1;
    const int y16_blk_raster_offset =
        ((i % (block_height >> 1)) >= (block_height >> 2)) << 1;
    uint32_t *luma_sse_sum_temp = luma_sse_sum + i * BW;
    for (int j = 0; j < block_width; j += wgt_step) {
      const auto acc_sse =
          hn::BitCast(di32_half, hn::LoadU(du32_half, acc_5x5_sse[i] + j));
      const auto luma_sse =
          hn::BitCast(di32_half, hn::LoadU(du32_half, luma_sse_sum_temp + j));
      const auto diff_sse = hn::Add(acc_sse, luma_sse);
      const auto diff_sse_pd = hn::PromoteTo(df64, diff_sse);
      const auto window_error = hn::Mul(diff_sse_pd, inv_num_ref_pixel_512);

      const int x32_blk_raster_offset = (j >= (block_width >> 1));
      const int x16_blk_raster_offset =
          ((j % (block_width >> 1)) >= (block_width >> 2));
      const int subblock_idx =
          ((y32_blk_raster_offset + x32_blk_raster_offset) << 2) +
          (y16_blk_raster_offset + x16_blk_raster_offset);

      const auto combined_error =
          hn::Add(hn::Mul(window_error, weight_factor_512),
                  subblock_mses_reg[subblock_idx]);
      const auto scaled_error =
          hn::Mul(combined_error, d_factor_mul_n_decay_qr_invs[subblock_idx]);

      hn::Vec<decltype(di32_half)> weights_in_32bit;
      if (tf_wgt_calc_lvl == 0) {
        weights_in_32bit = hn::DemoteInRangeTo(
            di32_half,
            hn::Mul(ExactExpNegPdHwy(scaled_error), tf_weight_scale_pd));
      } else {
        const auto scaled_error_ps = hn::DemoteTo(df32, scaled_error);
        const auto scaled_diff_ps = hn::Min(scaled_error_ps, seven);
        const auto minus_scaled_diff_ps = hn::Sub(zero, scaled_diff_ps);
        const auto exp_result = ApproxExpHwy(df32, minus_scaled_diff_ps);
        const auto scale_weight_exp_result =
            hn::Mul(exp_result, tf_weight_scale);
        const auto round_result = hn::Add(scale_weight_exp_result, point_five);
        weights_in_32bit = hn::ConvertInRangeTo(di32_half, round_result);
      }

      const auto weights_in_16bit = hn::DemoteTo(du16_qtr, weights_in_32bit);

      const int stride_idx = i * stride2 + j;
      const auto count_array = hn::LoadU(du16_qtr, count + stride_idx);
      hn::StoreU(hn::Add(count_array, weights_in_16bit), du16_qtr,
                 count + stride_idx);

      const auto accumulator_array =
          hn::LoadU(du32_half, accumulator + stride_idx);
      const auto pred_values = hn::LoadU(du8_oct, frame2 + stride_idx);
      const auto pred_values_u32 = hn::PromoteTo(du32_half, pred_values);
      const auto mull_frame2_weight_u32 =
          hn::Mul(pred_values_u32, hn::BitCast(du32_half, weights_in_32bit));
      hn::StoreU(hn::Add(accumulator_array, mull_frame2_weight_u32), du32_half,
                 accumulator + stride_idx);
    }
  }
}

HWY_ATTR HWY_INLINE void ApplyTemporalFilterHwy(
    const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,
    const BLOCK_SIZE block_size, const int mb_row, const int mb_col,
    const int num_planes, const double *noise_levels, const MV *subblock_mvs,
    const int *subblock_mses, const int q_factor, const int filter_strength,
    int tf_wgt_calc_lvl, const uint8_t *pred, uint32_t *accum,
    uint16_t *count) {
  const int is_high_bitdepth = frame_to_filter->flags & YV12_FLAG_HIGHBITDEPTH;
  assert(block_size == BLOCK_64X64 && "Only support 64x64 block with avx512!");
  assert(TF_WINDOW_LENGTH == 5 && "Only support window length 5 with avx512!");
  assert(!is_high_bitdepth && "Only support low bit-depth with avx512!");
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);
  (void)is_high_bitdepth;

  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int frame_height = frame_to_filter->y_crop_height;
  const int frame_width = frame_to_filter->y_crop_width;
  const int min_frame_size = AOMMIN(frame_height, frame_width);
  const double inv_factor = 1.0 / ((TF_WINDOW_BLOCK_BALANCE_WEIGHT + 1) *
                                   TF_SEARCH_ERROR_NORM_WEIGHT);
  const double weight_factor =
      static_cast<double>(TF_WINDOW_BLOCK_BALANCE_WEIGHT) * inv_factor;
  double q_decay = pow(static_cast<double>(q_factor) / TF_Q_DECAY_THRESHOLD, 2);
  q_decay = CLIP(q_decay, 1e-5, 1);
  if (q_factor >= TF_QINDEX_CUTOFF) {
    q_decay = 0.5 * pow(static_cast<double>(q_factor) / 64, 2);
  }
  double s_decay =
      pow(static_cast<double>(filter_strength) / TF_STRENGTH_THRESHOLD, 2);
  s_decay = CLIP(s_decay, 1e-5, 1);
  double d_factor[NUM_16X16] = { 0 };
  uint16_t *frame_sse = static_cast<uint16_t *>(
      aom_memalign(64, sizeof(frame_sse[0]) * kSseStrideHwy * BH));
  uint32_t *luma_sse_sum = static_cast<uint32_t *>(
      aom_memalign(64, sizeof(luma_sse_sum[0]) * BW * BH));
  memset(frame_sse, 0, sizeof(frame_sse[0]) * kSseStrideHwy * BH);
  memset(luma_sse_sum, 0, sizeof(luma_sse_sum[0]) * BW * BH);

  double distance_threshold = min_frame_size * TF_SEARCH_DISTANCE_THRESHOLD;
  distance_threshold = AOMMAX(distance_threshold, 1);
  for (int subblock_idx = 0; subblock_idx < NUM_16X16; subblock_idx++) {
    const MV mv = subblock_mvs[subblock_idx];
    const double distance = sqrt(pow(mv.row, 2) + pow(mv.col, 2));
    d_factor[subblock_idx] = distance / distance_threshold;
    d_factor[subblock_idx] = AOMMAX(d_factor[subblock_idx], 1);
  }

  int plane_offset = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    const uint32_t plane_h = mb_height >> mbd->plane[plane].subsampling_y;
    const uint32_t plane_w = mb_width >> mbd->plane[plane].subsampling_x;
    const uint32_t frame_stride = frame_to_filter->strides[plane == 0 ? 0 : 1];
    const int frame_offset = mb_row * plane_h * frame_stride + mb_col * plane_w;

    const uint8_t *ref = frame_to_filter->buffers[plane] + frame_offset;
    const int ss_x_shift =
        mbd->plane[plane].subsampling_x - mbd->plane[AOM_PLANE_Y].subsampling_x;
    const int ss_y_shift =
        mbd->plane[plane].subsampling_y - mbd->plane[AOM_PLANE_Y].subsampling_y;
    const int num_ref_pixels = TF_WINDOW_LENGTH * TF_WINDOW_LENGTH +
                               ((plane) ? (1 << (ss_x_shift + ss_y_shift)) : 0);
    const double inv_num_ref_pixels = 1.0 / num_ref_pixels;
    const double n_decay = 0.5 + log(2 * noise_levels[plane] + 5.0);
    const double decay_factor = 1 / (n_decay * q_decay * s_decay);

    if (plane == AOM_PLANE_U) {
      if (ss_x_shift == 1 && ss_y_shift == 1) {
        const hn::ScalableTag<uint32_t> du32_vec;
        const unsigned int step =
            static_cast<unsigned int>(hn::Lanes(du32_vec));
        const auto mask_lo16 = hn::Set(du32_vec, 0x0000FFFFu);
        for (unsigned int i = 0; i < plane_h; i++) {
          const uint16_t *src_0 = &frame_sse[2 * i * kSseStrideHwy + 2];
          const uint16_t *src_1 = &frame_sse[(2 * i + 1) * kSseStrideHwy + 2];
          for (unsigned int j = 0; j < plane_w; j += step) {
            const auto r0 = hn::LoadU(
                du32_vec, reinterpret_cast<const uint32_t *>(src_0 + j * 2));
            const auto r1 = hn::LoadU(
                du32_vec, reinterpret_cast<const uint32_t *>(src_1 + j * 2));
            const auto sum0 =
                hn::Add(hn::And(r0, mask_lo16), hn::ShiftRight<16>(r0));
            const auto sum1 =
                hn::Add(hn::And(r1, mask_lo16), hn::ShiftRight<16>(r1));
            const auto res = hn::Add(sum0, sum1);
            hn::StoreU(res, du32_vec, &luma_sse_sum[i * BW + j]);
          }
        }
      } else {
        for (unsigned int i = 0; i < plane_h; i++) {
          for (unsigned int j = 0; j < plane_w; j++) {
            for (int ii = 0; ii < (1 << ss_y_shift); ++ii) {
              for (int jj = 0; jj < (1 << ss_x_shift); ++jj) {
                const int yy = (i << ss_y_shift) + ii;
                const int xx = (j << ss_x_shift) + jj;
                luma_sse_sum[i * BW + j] +=
                    frame_sse[yy * kSseStrideHwy + xx + 2];
              }
            }
          }
        }
      }
    }

    ApplyTemporalFilterPlaneHwy(
        ref, frame_stride, pred + plane_offset, plane_w, plane_w, plane_h,
        subblock_mses, accum + plane_offset, count + plane_offset, frame_sse,
        luma_sse_sum, inv_num_ref_pixels, decay_factor, inv_factor,
        weight_factor, d_factor, tf_wgt_calc_lvl);
    plane_offset += plane_h * plane_w;
  }
  aom_free(frame_sse);
  aom_free(luma_sse_sum);
}

#if CONFIG_AV1_HIGHBITDEPTH
HWY_INLINE HWY_ATTR void HighbdGetSquaredErrorHwy(
    const uint16_t *frame1, const unsigned int stride, const uint16_t *frame2,
    const unsigned int stride2, const int block_width, const int block_height,
    uint32_t *frame_sse, const unsigned int sse_stride) {
  const uint16_t *src1 = frame1;
  const uint16_t *src2 = frame2;
  uint32_t *dst = frame_sse + 2;
  const hn::ScalableTag<int32_t> di32;
  const hn::Rebind<uint16_t, decltype(di32)> du16;
  const hn::Rebind<uint32_t, decltype(di32)> du32;
  const int step = static_cast<int>(hn::Lanes(di32));

  for (int i = 0; i < block_height; i++) {
    for (int j = 0; j < block_width; j += step) {
      const auto s1 = hn::PromoteTo(di32, hn::LoadU(du16, src1 + j));
      const auto s2 = hn::PromoteTo(di32, hn::LoadU(du16, src2 + j));
      const auto diff = hn::Sub(s1, s2);
      const auto res = hn::BitCast(du32, hn::Mul(diff, diff));
      hn::StoreU(res, du32, dst + j);
    }
    dst[-2] = dst[-1] = dst[0];
    dst[block_width] = dst[block_width + 1] = dst[block_width - 1];
    src1 += stride;
    src2 += stride2;
    dst += sse_stride;
  }
}

HWY_INLINE HWY_ATTR hn::Vec<hn::ScalableTag<uint32_t>> HighbdLoadAndSum5RowHwy(
    const uint32_t *src) {
  const hn::ScalableTag<uint32_t> du32;
  const auto c0 = hn::LoadU(du32, src - 2);
  const auto c1 = hn::LoadU(du32, src - 1);
  const auto c2 = hn::LoadU(du32, src);
  const auto c3 = hn::LoadU(du32, src + 1);
  const auto c4 = hn::LoadU(du32, src + 2);
  return hn::Add(hn::Add(c0, c1), hn::Add(hn::Add(c2, c3), c4));
}

HWY_ATTR void HighbdApplyTemporalFilterPlaneHwy(
    const uint16_t *frame1, const unsigned int stride, const uint16_t *frame2,
    const unsigned int stride2, const int block_width, const int block_height,
    const int *subblock_mses, unsigned int *accumulator, uint16_t *count,
    uint32_t *frame_sse, uint32_t *luma_sse_sum, int bd,
    const double inv_num_ref_pixels, const double decay_factor,
    const double inv_factor, const double weight_factor, double *d_factor,
    int tf_wgt_calc_lvl) {
  assert(((block_width == 64) || (block_width == 32)) &&
         ((block_height == 64) || (block_height == 32)));

  DECLARE_ALIGNED(64, uint32_t, acc_5x5_sse[BH][BW]);

  HighbdGetSquaredErrorHwy(frame1, stride, frame2, stride2, block_width,
                           block_height, frame_sse, kSseStrideHwy);

  const hn::ScalableTag<uint32_t> du32_vec;
  const int col_step = static_cast<int>(hn::Lanes(du32_vec));
  hn::Vec<hn::ScalableTag<uint32_t>> vsrc[5];

  for (int col = 0; col < block_width; col += col_step) {
    const uint32_t *src = frame_sse + 2 + col;

    for (int i = 2; i < 5; i++) {
      vsrc[i] = HighbdLoadAndSum5RowHwy(src);
      src += kSseStrideHwy;
    }

    vsrc[0] = vsrc[2];
    vsrc[1] = vsrc[2];

    for (int row = 0; row < block_height; row++) {
      const auto vsum01 = hn::Add(vsrc[0], vsrc[1]);
      const auto vsum23 = hn::Add(vsrc[2], vsrc[3]);
      const auto vsum = hn::Add(hn::Add(vsum01, vsum23), vsrc[4]);

      hn::StoreU(vsum, du32_vec, &acc_5x5_sse[row][col]);

      vsrc[0] = vsrc[1];
      vsrc[1] = vsrc[2];
      vsrc[2] = vsrc[3];
      vsrc[3] = vsrc[4];

      if (row <= block_height - 4) {
        vsrc[4] = HighbdLoadAndSum5RowHwy(src);
        src += kSseStrideHwy;
      } else {
        vsrc[4] = vsrc[3];
      }
    }
  }

  const hn::ScalableTag<double> df64;
  const hn::Rebind<float, decltype(df64)> df32;
  const hn::Rebind<int32_t, decltype(df64)> di32_half;
  const hn::Rebind<uint32_t, decltype(df64)> du32_half;
  const hn::Rebind<uint16_t, decltype(df64)> du16_qtr;
  const int wgt_step = static_cast<int>(hn::Lanes(df64));

  hn::Vec<hn::ScalableTag<double>> subblock_mses_reg[NUM_16X16];
  hn::Vec<hn::ScalableTag<double>> d_factor_mul_n_decay_qr_invs[NUM_16X16];
  for (int idx = 0; idx < NUM_16X16; idx++) {
    subblock_mses_reg[idx] = hn::Set(df64, subblock_mses[idx] * inv_factor);
    d_factor_mul_n_decay_qr_invs[idx] =
        hn::Set(df64, d_factor[idx] * decay_factor);
  }

  const int bd_shift = (bd - 8) * 2;
  const auto zero = hn::Set(df32, 0.0f);
  const auto point_five = hn::Set(df32, 0.5f);
  const auto seven = hn::Set(df32, 7.0f);
  const auto inv_num_ref_pixel_512 = hn::Set(df64, inv_num_ref_pixels);
  const auto weight_factor_512 = hn::Set(df64, weight_factor);
  const auto tf_weight_scale =
      hn::Set(df32, static_cast<float>(TF_WEIGHT_SCALE));
  const auto tf_weight_scale_pd =
      hn::Set(df64, static_cast<double>(TF_WEIGHT_SCALE));

  for (int i = 0; i < block_height; i++) {
    const int y32_blk_raster_offset = (i >= (block_height >> 1)) << 1;
    const int y16_blk_raster_offset =
        ((i % (block_height >> 1)) >= (block_height >> 2)) << 1;
    uint32_t *luma_sse_sum_temp = luma_sse_sum + i * BW;
    for (int j = 0; j < block_width; j += wgt_step) {
      const auto acc_sse = hn::LoadU(du32_half, acc_5x5_sse[i] + j);
      const auto luma_sse = hn::LoadU(du32_half, luma_sse_sum_temp + j);
      const auto diff_sse =
          hn::ShiftRightSame(hn::Add(acc_sse, luma_sse), bd_shift);
      const auto diff_sse_pd = hn::PromoteTo(df64, diff_sse);
      const auto window_error = hn::Mul(diff_sse_pd, inv_num_ref_pixel_512);

      const int x32_blk_raster_offset = (j >= (block_width >> 1));
      const int x16_blk_raster_offset =
          ((j % (block_width >> 1)) >= (block_width >> 2));
      const int subblock_idx =
          ((y32_blk_raster_offset + x32_blk_raster_offset) << 2) +
          (y16_blk_raster_offset + x16_blk_raster_offset);

      const auto combined_error =
          hn::Add(hn::Mul(window_error, weight_factor_512),
                  subblock_mses_reg[subblock_idx]);
      const auto scaled_error =
          hn::Mul(combined_error, d_factor_mul_n_decay_qr_invs[subblock_idx]);

      hn::Vec<decltype(di32_half)> weights_in_32bit;
      if (tf_wgt_calc_lvl == 0) {
        weights_in_32bit = hn::DemoteInRangeTo(
            di32_half,
            hn::Mul(ExactExpNegPdHwy(scaled_error), tf_weight_scale_pd));
      } else {
        const auto scaled_error_ps = hn::DemoteTo(df32, scaled_error);
        const auto scaled_diff_ps = hn::Min(scaled_error_ps, seven);
        const auto minus_scaled_diff_ps = hn::Sub(zero, scaled_diff_ps);
        const auto exp_result = ApproxExpHwy(df32, minus_scaled_diff_ps);
        const auto scale_weight_exp_result =
            hn::Mul(exp_result, tf_weight_scale);
        const auto round_result = hn::Add(scale_weight_exp_result, point_five);
        weights_in_32bit = hn::ConvertInRangeTo(di32_half, round_result);
      }

      const auto weights_in_16bit = hn::DemoteTo(du16_qtr, weights_in_32bit);

      const int stride_idx = i * stride2 + j;
      const auto count_array = hn::LoadU(du16_qtr, count + stride_idx);
      hn::StoreU(hn::Add(count_array, weights_in_16bit), du16_qtr,
                 count + stride_idx);

      const auto accumulator_array =
          hn::LoadU(du32_half, accumulator + stride_idx);
      const auto pred_values = hn::LoadU(du16_qtr, frame2 + stride_idx);
      const auto pred_values_u32 = hn::PromoteTo(du32_half, pred_values);
      const auto mull_frame2_weight_u32 =
          hn::Mul(pred_values_u32, hn::BitCast(du32_half, weights_in_32bit));
      hn::StoreU(hn::Add(accumulator_array, mull_frame2_weight_u32), du32_half,
                 accumulator + stride_idx);
    }
  }
}

HWY_ATTR HWY_INLINE void HighbdApplyTemporalFilterHwy(
    const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,
    const BLOCK_SIZE block_size, const int mb_row, const int mb_col,
    const int num_planes, const double *noise_levels, const MV *subblock_mvs,
    const int *subblock_mses, const int q_factor, const int filter_strength,
    int tf_wgt_calc_lvl, const uint8_t *pred, uint32_t *accum,
    uint16_t *count) {
  const int is_high_bitdepth = frame_to_filter->flags & YV12_FLAG_HIGHBITDEPTH;
  assert(block_size == BLOCK_64X64 && "Only support 64x64 block with avx512!");
  assert(TF_WINDOW_LENGTH == 5 && "Only support window length 5 with avx512!");
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);
  (void)is_high_bitdepth;

  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int frame_height = frame_to_filter->y_crop_height;
  const int frame_width = frame_to_filter->y_crop_width;
  const int min_frame_size = AOMMIN(frame_height, frame_width);
  const double inv_factor = 1.0 / ((TF_WINDOW_BLOCK_BALANCE_WEIGHT + 1) *
                                   TF_SEARCH_ERROR_NORM_WEIGHT);
  const double weight_factor =
      static_cast<double>(TF_WINDOW_BLOCK_BALANCE_WEIGHT) * inv_factor;
  double q_decay = pow(static_cast<double>(q_factor) / TF_Q_DECAY_THRESHOLD, 2);
  q_decay = CLIP(q_decay, 1e-5, 1);
  if (q_factor >= TF_QINDEX_CUTOFF) {
    q_decay = 0.5 * pow(static_cast<double>(q_factor) / 64, 2);
  }
  double s_decay =
      pow(static_cast<double>(filter_strength) / TF_STRENGTH_THRESHOLD, 2);
  s_decay = CLIP(s_decay, 1e-5, 1);
  double d_factor[NUM_16X16] = { 0 };
  uint32_t *frame_sse = static_cast<uint32_t *>(
      aom_memalign(64, sizeof(frame_sse[0]) * kSseStrideHwy * BH));
  uint32_t *luma_sse_sum = static_cast<uint32_t *>(
      aom_memalign(64, sizeof(luma_sse_sum[0]) * BW * BH));
  memset(frame_sse, 0, sizeof(frame_sse[0]) * kSseStrideHwy * BH);
  memset(luma_sse_sum, 0, sizeof(luma_sse_sum[0]) * BW * BH);
  uint16_t *pred1 = CONVERT_TO_SHORTPTR(pred);

  double distance_threshold = min_frame_size * TF_SEARCH_DISTANCE_THRESHOLD;
  distance_threshold = AOMMAX(distance_threshold, 1);
  for (int subblock_idx = 0; subblock_idx < NUM_16X16; subblock_idx++) {
    const MV mv = subblock_mvs[subblock_idx];
    const double distance = sqrt(pow(mv.row, 2) + pow(mv.col, 2));
    d_factor[subblock_idx] = distance / distance_threshold;
    d_factor[subblock_idx] = AOMMAX(d_factor[subblock_idx], 1);
  }

  int plane_offset = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    const uint32_t plane_h = mb_height >> mbd->plane[plane].subsampling_y;
    const uint32_t plane_w = mb_width >> mbd->plane[plane].subsampling_x;
    const uint32_t frame_stride = frame_to_filter->strides[plane == 0 ? 0 : 1];
    const int frame_offset = mb_row * plane_h * frame_stride + mb_col * plane_w;

    const uint16_t *ref =
        CONVERT_TO_SHORTPTR(frame_to_filter->buffers[plane]) + frame_offset;
    const int ss_x_shift =
        mbd->plane[plane].subsampling_x - mbd->plane[AOM_PLANE_Y].subsampling_x;
    const int ss_y_shift =
        mbd->plane[plane].subsampling_y - mbd->plane[AOM_PLANE_Y].subsampling_y;
    const int num_ref_pixels = TF_WINDOW_LENGTH * TF_WINDOW_LENGTH +
                               ((plane) ? (1 << (ss_x_shift + ss_y_shift)) : 0);
    const double inv_num_ref_pixels = 1.0 / num_ref_pixels;
    const double n_decay = 0.5 + log(2 * noise_levels[plane] + 5.0);
    const double decay_factor = 1 / (n_decay * q_decay * s_decay);

    if (plane == AOM_PLANE_U) {
      if (ss_x_shift == 1 && ss_y_shift == 1) {
        const hn::ScalableTag<uint32_t> du32_vec;
        const unsigned int step =
            static_cast<unsigned int>(hn::Lanes(du32_vec));
        for (unsigned int i = 0; i < plane_h; i++) {
          const uint32_t *src_0 = &frame_sse[2 * i * kSseStrideHwy + 2];
          const uint32_t *src_1 = &frame_sse[(2 * i + 1) * kSseStrideHwy + 2];
          for (unsigned int j = 0; j < plane_w; j += step) {
            const auto r0_lo = hn::LoadU(du32_vec, src_0 + 2 * j);
            const auto r0_hi = hn::LoadU(du32_vec, src_0 + 2 * j + step);
            const auto r1_lo = hn::LoadU(du32_vec, src_1 + 2 * j);
            const auto r1_hi = hn::LoadU(du32_vec, src_1 + 2 * j + step);
            const auto s_lo = hn::Add(r0_lo, r1_lo);
            const auto s_hi = hn::Add(r0_hi, r1_hi);
            const auto evens = hn::ConcatEven(du32_vec, s_hi, s_lo);
            const auto odds = hn::ConcatOdd(du32_vec, s_hi, s_lo);
            const auto res = hn::Add(evens, odds);
            hn::StoreU(res, du32_vec, &luma_sse_sum[i * BW + j]);
          }
        }
      } else {
        for (unsigned int i = 0; i < plane_h; i++) {
          for (unsigned int j = 0; j < plane_w; j++) {
            for (int ii = 0; ii < (1 << ss_y_shift); ++ii) {
              for (int jj = 0; jj < (1 << ss_x_shift); ++jj) {
                const int yy = (i << ss_y_shift) + ii;
                const int xx = (j << ss_x_shift) + jj;
                luma_sse_sum[i * BW + j] +=
                    frame_sse[yy * kSseStrideHwy + xx + 2];
              }
            }
          }
        }
      }
    }

    HighbdApplyTemporalFilterPlaneHwy(
        ref, frame_stride, pred1 + plane_offset, plane_w, plane_w, plane_h,
        subblock_mses, accum + plane_offset, count + plane_offset, frame_sse,
        luma_sse_sum, mbd->bd, inv_num_ref_pixels, decay_factor, inv_factor,
        weight_factor, d_factor, tf_wgt_calc_lvl);
    plane_offset += plane_h * plane_w;
  }
  aom_free(frame_sse);
  aom_free(luma_sse_sum);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

}  // namespace HWY_NAMESPACE
}  // namespace

HWY_AFTER_NAMESPACE();

#if defined(__GNUC__) || defined(__clang__)
#define AOM_TF_FORCE_ALIGN_ARG_POINTER __attribute__((force_align_arg_pointer))
#else
#define AOM_TF_FORCE_ALIGN_ARG_POINTER
#endif

#define MAKE_TEMPORAL_FILTER(suffix)                                           \
  extern "C" double av1_estimate_noise_from_single_plane_##suffix(             \
      const uint8_t *src, int height, int width, int stride, int edge_thresh); \
  HWY_ATTR double av1_estimate_noise_from_single_plane_##suffix(               \
      const uint8_t *src, int height, int width, int stride,                   \
      int edge_thresh) {                                                       \
    return HWY_NAMESPACE::EstimateNoiseFromSinglePlaneHwy(                     \
        src, height, width, stride, edge_thresh);                              \
  }                                                                            \
  extern "C" void av1_apply_temporal_filter_##suffix(                          \
      const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,       \
      const BLOCK_SIZE block_size, const int mb_row, const int mb_col,         \
      const int num_planes, const double *noise_levels,                        \
      const MV *subblock_mvs, const int *subblock_mses, const int q_factor,    \
      const int filter_strength, int tf_wgt_calc_lvl, const uint8_t *pred,     \
      uint32_t *accum, uint16_t *count);                                       \
  AOM_TF_FORCE_ALIGN_ARG_POINTER HWY_ATTR void                                 \
  av1_apply_temporal_filter_##suffix(                                          \
      const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,       \
      const BLOCK_SIZE block_size, const int mb_row, const int mb_col,         \
      const int num_planes, const double *noise_levels,                        \
      const MV *subblock_mvs, const int *subblock_mses, const int q_factor,    \
      const int filter_strength, int tf_wgt_calc_lvl, const uint8_t *pred,     \
      uint32_t *accum, uint16_t *count) {                                      \
    HWY_NAMESPACE::ApplyTemporalFilterHwy(                                     \
        frame_to_filter, mbd, block_size, mb_row, mb_col, num_planes,          \
        noise_levels, subblock_mvs, subblock_mses, q_factor, filter_strength,  \
        tf_wgt_calc_lvl, pred, accum, count);                                  \
  }

#if CONFIG_AV1_HIGHBITDEPTH
#define MAKE_HIGHBD_TEMPORAL_FILTER(suffix)                                   \
  extern "C" double av1_highbd_estimate_noise_from_single_plane_##suffix(     \
      const uint16_t *src, int height, int width, int stride, int bit_depth,  \
      int edge_thresh);                                                       \
  HWY_ATTR double av1_highbd_estimate_noise_from_single_plane_##suffix(       \
      const uint16_t *src, int height, int width, int stride, int bit_depth,  \
      int edge_thresh) {                                                      \
    return HWY_NAMESPACE::HighbdEstimateNoiseFromSinglePlaneHwy(              \
        src, height, width, stride, bit_depth, edge_thresh);                  \
  }                                                                           \
  extern "C" void av1_highbd_apply_temporal_filter_##suffix(                  \
      const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,      \
      const BLOCK_SIZE block_size, const int mb_row, const int mb_col,        \
      const int num_planes, const double *noise_levels,                       \
      const MV *subblock_mvs, const int *subblock_mses, const int q_factor,   \
      const int filter_strength, int tf_wgt_calc_lvl, const uint8_t *pred,    \
      uint32_t *accum, uint16_t *count);                                      \
  AOM_TF_FORCE_ALIGN_ARG_POINTER HWY_ATTR void                                \
  av1_highbd_apply_temporal_filter_##suffix(                                  \
      const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,      \
      const BLOCK_SIZE block_size, const int mb_row, const int mb_col,        \
      const int num_planes, const double *noise_levels,                       \
      const MV *subblock_mvs, const int *subblock_mses, const int q_factor,   \
      const int filter_strength, int tf_wgt_calc_lvl, const uint8_t *pred,    \
      uint32_t *accum, uint16_t *count) {                                     \
    HWY_NAMESPACE::HighbdApplyTemporalFilterHwy(                              \
        frame_to_filter, mbd, block_size, mb_row, mb_col, num_planes,         \
        noise_levels, subblock_mvs, subblock_mses, q_factor, filter_strength, \
        tf_wgt_calc_lvl, pred, accum, count);                                 \
  }
#endif  // CONFIG_AV1_HIGHBITDEPTH

#endif  // AOM_AV1_ENCODER_TEMPORAL_FILTER_HWY_H_
