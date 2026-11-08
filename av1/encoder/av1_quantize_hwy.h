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

#ifndef AOM_AV1_ENCODER_AV1_QUANTIZE_HWY_H_
#define AOM_AV1_ENCODER_AV1_QUANTIZE_HWY_H_

#include <assert.h>
#include <stdint.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/quantize.h"
#include "aom_ports/mem.h"
#include "av1/common/quant_common.h"
#include "av1/common/scan.h"
#include "av1/encoder/av1_quantize.h"
#include "third_party/highway/hwy/highway.h"

HWY_BEFORE_NAMESPACE();

namespace {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

constexpr hn::ScalableTag<int16_t> int16xN_tag;
constexpr hn::ScalableTag<uint16_t> uint16xN_tag;
constexpr hn::ScalableTag<int32_t> int32xN_tag;
constexpr hn::ScalableTag<int64_t> int64xN_tag;
constexpr hn::ScalableTag<uint64_t> uint64xN_tag;

constexpr hn::FixedTag<int16_t, 8> int16x8_tag;
constexpr hn::FixedTag<int64_t, 2> int64x2_tag;

using IVec16 = hn::Vec<decltype(int16xN_tag)>;
using UVec16 = hn::Vec<decltype(uint16xN_tag)>;
using IVec32 = hn::Vec<decltype(int32xN_tag)>;
using IVec64 = hn::Vec<decltype(int64xN_tag)>;

// Negates elements of `a` where `b < 0`, sets to 0 where `b == 0`,
// and keeps `a` where `b > 0`.
template <typename D>
HWY_ATTR HWY_INLINE hn::Vec<D> Sign16(D d, hn::Vec<D> a, hn::Vec<D> b) {
  const auto zero = hn::Zero(d);
  const auto nz_mask = hn::Ne(b, zero);
  const auto neg_mask = hn::Gt(zero, b);
  const auto a_nz = hn::IfThenElseZero(nz_mask, a);
  return hn::IfThenElse(neg_mask, hn::Sub(zero, a_nz), a_nz);
}

// Broadcasts 128-bit [DC, AC, AC, AC, AC, AC, AC, AC] into full vector
// [DC, AC, AC, ...] where block 0 has [DC, AC...] and blocks 1..K-1 have
// [AC, AC...].
HWY_ATTR HWY_INLINE IVec16 InitOneQp(hn::Vec<decltype(int16x8_tag)> p128) {
  const auto p64 = hn::BitCast(int64x2_tag, p128);
  const auto ac64 = hn::InterleaveUpper(int64x2_tag, p64, p64);
  const auto ac128 = hn::BitCast(int16x8_tag, ac64);
  const auto ac_full =
      hn::BroadcastBlock<0>(hn::ResizeBitCast(int16xN_tag, ac128));
  return hn::InsertBlock<0>(ac_full, p128);
}

// Updates a vector from [DC, AC, AC, ...] to [AC, AC, AC, ...] by broadcasting
// 128-bit block 1 across all blocks.
HWY_ATTR HWY_INLINE IVec16 UpdateQp(IVec16 v) {
  return hn::BroadcastBlock<1>(v);
}

HWY_ATTR HWY_INLINE void InitQpFp(const int16_t *round_ptr,
                                  const int16_t *quant_ptr,
                                  const int16_t *dequant_ptr, int log_scale,
                                  IVec16 *thr, IVec16 *round, IVec16 *quant,
                                  IVec16 *dequant) {
  auto round128 = hn::LoadU(int16x8_tag, round_ptr);
  const auto quant128 = hn::LoadU(int16x8_tag, quant_ptr);
  const auto dequant128 = hn::LoadU(int16x8_tag, dequant_ptr);

  if (log_scale > 0) {
    const auto rnd =
        hn::Set(int16x8_tag, static_cast<int16_t>(1 << (log_scale - 1)));
    round128 = hn::ShiftRightSame(hn::Add(round128, rnd), log_scale);
  }

  *round = InitOneQp(round128);
  *quant = InitOneQp(quant128);
  if (log_scale == 1) {
    *quant = hn::ShiftLeft<1>(*quant);
  }
  *dequant = InitOneQp(dequant128);
  *thr = hn::Sub(hn::ShiftRightSame(*dequant, 1 + log_scale),
                 hn::Set(int16xN_tag, 1));
}

HWY_ATTR HWY_INLINE void UpdateQpFp(IVec16 *thr, IVec16 *round, IVec16 *quant,
                                    IVec16 *dequant) {
  *round = UpdateQp(*round);
  *quant = UpdateQp(*quant);
  *dequant = UpdateQp(*dequant);
  *thr = UpdateQp(*thr);
}

HWY_ATTR HWY_INLINE void LoadBValues(const int16_t *zbin_ptr, IVec16 *zbin,
                                     const int16_t *round_ptr, IVec16 *round,
                                     const int16_t *quant_ptr, IVec16 *quant,
                                     const int16_t *dequant_ptr,
                                     IVec16 *dequant, const int16_t *shift_ptr,
                                     IVec16 *shift, int log_scale) {
  auto zbin128 = hn::LoadU(int16x8_tag, zbin_ptr);
  auto round128 = hn::LoadU(int16x8_tag, round_ptr);
  const auto quant128 = hn::LoadU(int16x8_tag, quant_ptr);
  const auto dequant128 = hn::LoadU(int16x8_tag, dequant_ptr);
  const auto shift128 = hn::LoadU(int16x8_tag, shift_ptr);

  if (log_scale > 0) {
    const auto rnd =
        hn::Set(int16x8_tag, static_cast<int16_t>(1 << (log_scale - 1)));
    zbin128 = hn::ShiftRightSame(hn::Add(zbin128, rnd), log_scale);
    round128 = hn::ShiftRightSame(hn::Add(round128, rnd), log_scale);
  }
  zbin128 = hn::Sub(zbin128, hn::Set(int16x8_tag, 1));

  *zbin = InitOneQp(zbin128);
  *round = InitOneQp(round128);
  *quant = InitOneQp(quant128);
  *dequant = InitOneQp(dequant128);
  *shift = InitOneQp(shift128);
}

HWY_ATTR HWY_INLINE void UpdateQpB(IVec16 *zbin, IVec16 *round, IVec16 *quant,
                                   IVec16 *dequant, IVec16 *shift) {
  *zbin = UpdateQp(*zbin);
  *round = UpdateQp(*round);
  *quant = UpdateQp(*quant);
  *dequant = UpdateQp(*dequant);
  *shift = UpdateQp(*shift);
}

HWY_ATTR HWY_INLINE IVec16 LoadCoefficients(const tran_low_t *coeff_ptr) {
  constexpr intptr_t kStep32 = hn::MaxLanes(int32xN_tag);
  const auto c0 = hn::LoadU(int32xN_tag, coeff_ptr);
  const auto c1 = hn::LoadU(int32xN_tag, coeff_ptr + kStep32);
  return hn::ReorderDemote2To(int16xN_tag, c0, c1);
}

HWY_ATTR HWY_INLINE void StoreCoefficients(IVec16 coeff_vals,
                                           tran_low_t *coeff_ptr) {
  constexpr intptr_t kStep32 = hn::MaxLanes(int32xN_tag);
  const auto coeff_sign = hn::ShiftRight<15>(coeff_vals);
  const auto coeff_lo =
      hn::BitCast(int32xN_tag, hn::InterleaveLower(coeff_vals, coeff_sign));
  const auto coeff_hi = hn::BitCast(
      int32xN_tag, hn::InterleaveUpper(int16xN_tag, coeff_vals, coeff_sign));
  hn::StoreU(coeff_lo, int32xN_tag, coeff_ptr);
  hn::StoreU(coeff_hi, int32xN_tag, coeff_ptr + kStep32);
}

HWY_ATTR HWY_INLINE uint16_t QuantGatherEob(IVec16 eob) {
  return static_cast<uint16_t>(hn::ReduceMax(int16xN_tag, eob));
}

// Reorders iscan to match the lane layout produced by ReorderDemote2To(c0, c1)
// where c0 and c1 are 32-bit coefficient vectors.
HWY_ATTR HWY_INLINE IVec16
GetMaxLaneEob(const int16_t *iscan, IVec16 v_eobmax,
              hn::Mask<decltype(int16xN_tag)> nz_mask) {
  const auto v_iscan = hn::LoadU(int16xN_tag, iscan);
  constexpr size_t kLanes64 = hn::MaxLanes(uint64xN_tag);
#if HWY_TARGET <= HWY_AVX3
  static_assert(kLanes64 == 8, "Expected 8 uint64_t lanes on AVX3");
  alignas(
      64) static constexpr uint64_t kPermIdx[8] = { 0, 4, 1, 5, 2, 6, 3, 7 };
#else
  static_assert(kLanes64 == 4, "Expected 4 uint64_t lanes on AVX2");
  alignas(32) static constexpr uint64_t kPermIdx[4] = { 0, 2, 1, 3 };
#endif
  const auto idx = hn::SetTableIndices(uint64xN_tag, kPermIdx);
  const auto v_iscan_perm = hn::BitCast(
      int16xN_tag,
      hn::TableLookupLanes(hn::BitCast(uint64xN_tag, v_iscan), idx));
  const auto v_iscan_plus1 = hn::Add(v_iscan_perm, hn::Set(int16xN_tag, 1));
  const auto v_nz_iscan = hn::IfThenElseZero(nz_mask, v_iscan_plus1);
  return hn::Max(v_eobmax, v_nz_iscan);
}

HWY_ATTR HWY_INLINE void QuantizeLpStep(const int16_t *coeff_ptr,
                                        const int16_t *iscan_ptr,
                                        int16_t *qcoeff_ptr,
                                        int16_t *dqcoeff_ptr, IVec16 round,
                                        IVec16 quant, IVec16 dequant,
                                        IVec16 *eob) {
  const auto coeff = hn::LoadU(int16xN_tag, coeff_ptr);
  const auto abs_coeff = hn::Abs(coeff);
  const auto tmp_rnd = hn::SaturatedAdd(abs_coeff, round);
  const auto abs_qcoeff = hn::MulHigh(tmp_rnd, quant);
  const auto qcoeff = Sign16(int16xN_tag, abs_qcoeff, coeff);
  const auto dqcoeff = hn::Mul(qcoeff, dequant);
  const auto nz_mask = hn::Gt(abs_qcoeff, hn::Zero(int16xN_tag));

  hn::StoreU(qcoeff, int16xN_tag, qcoeff_ptr);
  hn::StoreU(dqcoeff, int16xN_tag, dqcoeff_ptr);

  const auto iscan = hn::LoadU(int16xN_tag, iscan_ptr);
  const auto nz_iscan =
      hn::IfThenElseZero(nz_mask, hn::Add(iscan, hn::Set(int16xN_tag, 1)));
  *eob = hn::Max(*eob, nz_iscan);
}

HWY_ATTR HWY_INLINE void QuantizeLp(const int16_t *coeff_ptr, intptr_t n_coeffs,
                                    const int16_t *round_ptr,
                                    const int16_t *quant_ptr,
                                    int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr,
                                    const int16_t *dequant_ptr,
                                    uint16_t *eob_ptr, const int16_t *scan,
                                    const int16_t *iscan) {
  constexpr intptr_t kStep = hn::MaxLanes(int16xN_tag);
  if (n_coeffs < kStep) {
    av1_quantize_lp_avx2(coeff_ptr, n_coeffs, round_ptr, quant_ptr, qcoeff_ptr,
                         dqcoeff_ptr, dequant_ptr, eob_ptr, scan, iscan);
    return;
  }
  (void)scan;

  IVec16 eob = hn::Zero(int16xN_tag);
  IVec16 round = InitOneQp(hn::LoadU(int16x8_tag, round_ptr));
  IVec16 quant = InitOneQp(hn::LoadU(int16x8_tag, quant_ptr));
  IVec16 dequant = InitOneQp(hn::LoadU(int16x8_tag, dequant_ptr));

  QuantizeLpStep(coeff_ptr, iscan, qcoeff_ptr, dqcoeff_ptr, round, quant,
                 dequant, &eob);

  if (n_coeffs > kStep) {
    round = UpdateQp(round);
    quant = UpdateQp(quant);
    dequant = UpdateQp(dequant);

    for (intptr_t idx = kStep; idx < n_coeffs; idx += kStep) {
      QuantizeLpStep(coeff_ptr + idx, iscan + idx, qcoeff_ptr + idx,
                     dqcoeff_ptr + idx, round, quant, dequant, &eob);
    }
  }

  *eob_ptr = QuantGatherEob(eob);
}

template <int log_scale>
HWY_ATTR HWY_INLINE void QuantizeFpStep(IVec16 thr, IVec16 round, IVec16 quant,
                                        IVec16 dequant,
                                        const tran_low_t *coeff_ptr,
                                        const int16_t *iscan_ptr,
                                        tran_low_t *qcoeff_ptr,
                                        tran_low_t *dqcoeff_ptr, IVec16 *eob) {
  const auto coeff = LoadCoefficients(coeff_ptr);
  const auto abs_coeff = hn::Abs(coeff);
  const auto mask = hn::Gt(abs_coeff, thr);

  if (!hn::AllFalse(int16xN_tag, mask)) {
    const auto tmp_rnd =
        hn::IfThenElseZero(mask, hn::SaturatedAdd(abs_coeff, round));
    IVec16 abs_q;
    IVec16 q;
    IVec16 dq;
    hn::Mask<decltype(int16xN_tag)> nz_mask;

    if (log_scale == 0) {
      abs_q = hn::MulHigh(tmp_rnd, quant);
      q = Sign16(int16xN_tag, abs_q, coeff);
      dq = hn::Mul(q, dequant);
      nz_mask = hn::Gt(abs_q, hn::Zero(int16xN_tag));
    } else if (log_scale == 1) {
      abs_q = hn::BitCast(int16xN_tag,
                          hn::MulHigh(hn::BitCast(uint16xN_tag, tmp_rnd),
                                      hn::BitCast(uint16xN_tag, quant)));
      q = Sign16(int16xN_tag, abs_q, coeff);
      const auto abs_dq =
          hn::BitCast(int16xN_tag, hn::ShiftRight<1>(hn::BitCast(
                                       uint16xN_tag, hn::Mul(abs_q, dequant))));
      dq = Sign16(int16xN_tag, abs_dq, coeff);
      nz_mask = hn::Gt(abs_q, hn::Zero(int16xN_tag));
    } else {
      const auto qh = hn::ShiftLeft<2>(
          hn::BitCast(uint16xN_tag, hn::MulHigh(tmp_rnd, quant)));
      const auto ql = hn::ShiftRight<14>(
          hn::BitCast(uint16xN_tag, hn::Mul(tmp_rnd, quant)));
      abs_q = hn::BitCast(int16xN_tag, hn::Or(qh, ql));
      const auto dqh = hn::ShiftLeft<14>(
          hn::BitCast(uint16xN_tag, hn::MulHigh(abs_q, dequant)));
      const auto dql =
          hn::ShiftRight<2>(hn::BitCast(uint16xN_tag, hn::Mul(abs_q, dequant)));
      const auto abs_dq = hn::BitCast(int16xN_tag, hn::Or(dqh, dql));
      q = Sign16(int16xN_tag, abs_q, coeff);
      dq = Sign16(int16xN_tag, abs_dq, coeff);
      nz_mask = hn::Ne(dq, hn::Zero(int16xN_tag));
    }

    StoreCoefficients(q, qcoeff_ptr);
    StoreCoefficients(dq, dqcoeff_ptr);
    *eob = GetMaxLaneEob(iscan_ptr, *eob, nz_mask);
  } else {
    constexpr intptr_t kStep32 = hn::MaxLanes(int32xN_tag);
    const auto zero = hn::Zero(int32xN_tag);
    hn::StoreU(zero, int32xN_tag, qcoeff_ptr);
    hn::StoreU(zero, int32xN_tag, qcoeff_ptr + kStep32);
    hn::StoreU(zero, int32xN_tag, dqcoeff_ptr);
    hn::StoreU(zero, int32xN_tag, dqcoeff_ptr + kStep32);
  }
}

template <int log_scale>
HWY_ATTR HWY_INLINE void QuantizeFpTemplate(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan_ptr, const int16_t *iscan_ptr) {
  constexpr intptr_t kStep = hn::MaxLanes(int16xN_tag);
  if (log_scale == 0 && n_coeffs < kStep) {
    av1_quantize_fp_avx2(coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr,
                         quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr,
                         eob_ptr, scan_ptr, iscan_ptr);
    return;
  }
  (void)scan_ptr;
  (void)zbin_ptr;
  (void)quant_shift_ptr;

  IVec16 thr, round, quant, dequant;
  IVec16 eob = hn::Zero(int16xN_tag);

  InitQpFp(round_ptr, quant_ptr, dequant_ptr, log_scale, &thr, &round, &quant,
           &dequant);

  QuantizeFpStep<log_scale>(thr, round, quant, dequant, coeff_ptr, iscan_ptr,
                            qcoeff_ptr, dqcoeff_ptr, &eob);

  UpdateQpFp(&thr, &round, &quant, &dequant);

  for (intptr_t idx = kStep; idx < n_coeffs; idx += kStep) {
    QuantizeFpStep<log_scale>(thr, round, quant, dequant, coeff_ptr + idx,
                              iscan_ptr + idx, qcoeff_ptr + idx,
                              dqcoeff_ptr + idx, &eob);
  }
  *eob_ptr = QuantGatherEob(eob);
}

HWY_ATTR HWY_INLINE void QuantizeFp(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan) {
  QuantizeFpTemplate<0>(coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr,
                        quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr,
                        eob_ptr, scan, iscan);
}

HWY_ATTR HWY_INLINE void QuantizeFp32x32(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan) {
  QuantizeFpTemplate<1>(coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr,
                        quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr,
                        eob_ptr, scan, iscan);
}

HWY_ATTR HWY_INLINE void QuantizeFp64x64(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan) {
  QuantizeFpTemplate<2>(coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr,
                        quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr,
                        eob_ptr, scan, iscan);
}

template <int log_scale>
HWY_ATTR HWY_INLINE void QuantizeBStep(const tran_low_t *coeff_ptr,
                                       tran_low_t *qcoeff_ptr,
                                       tran_low_t *dqcoeff_ptr, IVec16 v_quant,
                                       IVec16 v_dequant, IVec16 v_round,
                                       IVec16 v_zbin, IVec16 v_quant_shift,
                                       const int16_t *iscan_ptr,
                                       IVec16 *v_eobmax) {
  const auto v_coeff = LoadCoefficients(coeff_ptr);
  const auto v_abs_coeff = hn::Abs(v_coeff);
  const auto zbin_mask = hn::Gt(v_abs_coeff, v_zbin);

  if (hn::AllFalse(int16xN_tag, zbin_mask)) {
    constexpr intptr_t kStep32 = hn::MaxLanes(int32xN_tag);
    const auto zero = hn::Zero(int32xN_tag);
    hn::StoreU(zero, int32xN_tag, qcoeff_ptr);
    hn::StoreU(zero, int32xN_tag, qcoeff_ptr + kStep32);
    hn::StoreU(zero, int32xN_tag, dqcoeff_ptr);
    hn::StoreU(zero, int32xN_tag, dqcoeff_ptr + kStep32);
    return;
  }

  const auto v_tmp_rnd =
      hn::IfThenElseZero(zbin_mask, hn::SaturatedAdd(v_abs_coeff, v_round));
  const auto v_tmp32_a = hn::MulHigh(v_tmp_rnd, v_quant);
  const auto v_tmp32_b = hn::Add(v_tmp32_a, v_tmp_rnd);

  IVec16 v_tmp32;
  IVec16 v_qcoeff;
  IVec16 v_dqcoeff;
  if (log_scale == 0) {
    v_tmp32 = hn::MulHigh(v_tmp32_b, v_quant_shift);
    v_qcoeff = Sign16(int16xN_tag, v_tmp32, v_coeff);
    v_dqcoeff = hn::Mul(v_qcoeff, v_dequant);
  } else {
    const auto v_tmp32_hi = hn::ShiftLeft<log_scale>(
        hn::BitCast(uint16xN_tag, hn::MulHigh(v_tmp32_b, v_quant_shift)));
    const auto v_tmp32_lo = hn::ShiftRight<16 - log_scale>(
        hn::BitCast(uint16xN_tag, hn::Mul(v_tmp32_b, v_quant_shift)));
    v_tmp32 = hn::BitCast(int16xN_tag, hn::Or(v_tmp32_hi, v_tmp32_lo));
    const auto v_dqcoeff_hi = hn::ShiftLeft<16 - log_scale>(
        hn::BitCast(uint16xN_tag, hn::MulHigh(v_tmp32, v_dequant)));
    const auto v_dqcoeff_lo = hn::ShiftRight<log_scale>(
        hn::BitCast(uint16xN_tag, hn::Mul(v_tmp32, v_dequant)));
    v_dqcoeff = Sign16(
        int16xN_tag,
        hn::BitCast(int16xN_tag, hn::Or(v_dqcoeff_hi, v_dqcoeff_lo)), v_coeff);
    v_qcoeff = Sign16(int16xN_tag, v_tmp32, v_coeff);
  }

  const auto nz_mask = hn::Gt(v_tmp32, hn::Zero(int16xN_tag));
  StoreCoefficients(v_qcoeff, qcoeff_ptr);
  StoreCoefficients(v_dqcoeff, dqcoeff_ptr);
  if (!hn::AllFalse(int16xN_tag, nz_mask)) {
    *v_eobmax = GetMaxLaneEob(iscan_ptr, *v_eobmax, nz_mask);
  }
}

template <int log_scale>
HWY_ATTR HWY_INLINE void QuantizeBTemplate(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan) {
  constexpr intptr_t kStep = hn::MaxLanes(int16xN_tag);
  if (log_scale == 0 && n_coeffs < kStep) {
    aom_quantize_b_avx2(coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr,
                        quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr,
                        eob_ptr, scan, iscan);
    return;
  }
  (void)scan;

  IVec16 v_zbin, v_round, v_quant, v_dequant, v_quant_shift;
  IVec16 v_eobmax = hn::Zero(int16xN_tag);

  LoadBValues(zbin_ptr, &v_zbin, round_ptr, &v_round, quant_ptr, &v_quant,
              dequant_ptr, &v_dequant, quant_shift_ptr, &v_quant_shift,
              log_scale);

  QuantizeBStep<log_scale>(coeff_ptr, qcoeff_ptr, dqcoeff_ptr, v_quant,
                           v_dequant, v_round, v_zbin, v_quant_shift, iscan,
                           &v_eobmax);

  UpdateQpB(&v_zbin, &v_round, &v_quant, &v_dequant, &v_quant_shift);

  for (intptr_t idx = kStep; idx < n_coeffs; idx += kStep) {
    QuantizeBStep<log_scale>(coeff_ptr + idx, qcoeff_ptr + idx,
                             dqcoeff_ptr + idx, v_quant, v_dequant, v_round,
                             v_zbin, v_quant_shift, iscan + idx, &v_eobmax);
  }

  *eob_ptr = QuantGatherEob(v_eobmax);
}

HWY_ATTR HWY_INLINE void QuantizeB(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan) {
  QuantizeBTemplate<0>(coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr,
                       quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr,
                       eob_ptr, scan, iscan);
}

HWY_ATTR HWY_INLINE void QuantizeB32x32(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan) {
  QuantizeBTemplate<1>(coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr,
                       quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr,
                       eob_ptr, scan, iscan);
}

HWY_ATTR HWY_INLINE void QuantizeB64x64(
    const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr,
    const int16_t *round_ptr, const int16_t *quant_ptr,
    const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,
    tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,
    const int16_t *scan, const int16_t *iscan) {
  QuantizeBTemplate<2>(coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr,
                       quant_shift_ptr, qcoeff_ptr, dqcoeff_ptr, dequant_ptr,
                       eob_ptr, scan, iscan);
}

HWY_ATTR HWY_INLINE int64_t BlockErrorLp(const int16_t *coeff,
                                         const int16_t *dqcoeff,
                                         intptr_t block_size) {
  assert(block_size % 16 == 0);
  constexpr intptr_t kStep = hn::MaxLanes(int16xN_tag);
  if (block_size < kStep) {
    return av1_block_error_lp_avx2(coeff, dqcoeff, block_size);
  }

  auto cum_err = hn::Zero(int64xN_tag);
  for (intptr_t i = 0; i < block_size; i += kStep) {
    const auto c = hn::LoadU(int16xN_tag, coeff + i);
    const auto dq = hn::LoadU(int16xN_tag, dqcoeff + i);
    const auto diff = hn::Sub(dq, c);
    const auto err32 = hn::WidenMulPairwiseAdd(int32xN_tag, diff, diff);
    const auto exp_lo = hn::PromoteEvenTo(int64xN_tag, err32);
    const auto exp_hi = hn::PromoteOddTo(int64xN_tag, err32);
    cum_err = hn::Add(cum_err, hn::Add(exp_lo, exp_hi));
  }
  return hn::ReduceSum(int64xN_tag, cum_err);
}

HWY_ATTR HWY_INLINE int64_t BlockError(const tran_low_t *coeff,
                                       const tran_low_t *dqcoeff,
                                       intptr_t block_size, int64_t *ssz) {
  constexpr intptr_t kStep = hn::MaxLanes(int16xN_tag);
  if (block_size < kStep) {
    return av1_block_error_avx2(coeff, dqcoeff, block_size, ssz);
  }

  auto sse_reg = hn::Zero(int64xN_tag);
  auto ssz_reg = hn::Zero(int64xN_tag);
  for (intptr_t i = 0; i < block_size; i += kStep) {
    const auto coeff_reg = LoadCoefficients(coeff + i);
    const auto dqcoeff_reg = LoadCoefficients(dqcoeff + i);
    const auto diff = hn::Sub(dqcoeff_reg, coeff_reg);
    const auto err = hn::WidenMulPairwiseAdd(int32xN_tag, diff, diff);
    const auto sq_coeff =
        hn::WidenMulPairwiseAdd(int32xN_tag, coeff_reg, coeff_reg);

    sse_reg = hn::Add(sse_reg, hn::Add(hn::PromoteEvenTo(int64xN_tag, err),
                                       hn::PromoteOddTo(int64xN_tag, err)));
    ssz_reg =
        hn::Add(ssz_reg, hn::Add(hn::PromoteEvenTo(int64xN_tag, sq_coeff),
                                 hn::PromoteOddTo(int64xN_tag, sq_coeff)));
  }
  constexpr hn::FixedTag<int64_t, 2> d128;
#if HWY_TARGET <= HWY_AVX3
  constexpr hn::FixedTag<int64_t, 4> d256;
  const auto sse256 =
      hn::Add(hn::LowerHalf(d256, sse_reg), hn::UpperHalf(d256, sse_reg));
  const auto ssz256 =
      hn::Add(hn::LowerHalf(d256, ssz_reg), hn::UpperHalf(d256, ssz_reg));
#else
  const auto sse256 = sse_reg;
  const auto ssz256 = ssz_reg;
#endif
  const auto sse128 =
      hn::Add(hn::LowerHalf(d128, sse256), hn::UpperHalf(d128, sse256));
  const auto ssz128 =
      hn::Add(hn::LowerHalf(d128, ssz256), hn::UpperHalf(d128, ssz256));
  const auto sum = hn::Add(hn::InterleaveLower(d128, sse128, ssz128),
                           hn::InterleaveUpper(d128, sse128, ssz128));
  *ssz = hn::ExtractLane(sum, 1);
  return hn::ExtractLane(sum, 0);
}

}  // namespace HWY_NAMESPACE
}  // namespace

HWY_AFTER_NAMESPACE();

#define MAKE_AV1_QUANTIZE_LP(suffix)                                         \
  extern "C" void av1_quantize_lp_##suffix(                                  \
      const int16_t *coeff_ptr, intptr_t n_coeffs, const int16_t *round_ptr, \
      const int16_t *quant_ptr, int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr,   \
      const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan,    \
      const int16_t *iscan);                                                 \
  HWY_ATTR void av1_quantize_lp_##suffix(                                    \
      const int16_t *coeff_ptr, intptr_t n_coeffs, const int16_t *round_ptr, \
      const int16_t *quant_ptr, int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr,   \
      const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan,    \
      const int16_t *iscan) {                                                \
    HWY_NAMESPACE::QuantizeLp(coeff_ptr, n_coeffs, round_ptr, quant_ptr,     \
                              qcoeff_ptr, dqcoeff_ptr, dequant_ptr, eob_ptr, \
                              scan, iscan);                                  \
  }

#define MAKE_AV1_QUANTIZE_FP(suffix)                                           \
  extern "C" void av1_quantize_fp_##suffix(                                    \
      const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, \
      const int16_t *round_ptr, const int16_t *quant_ptr,                      \
      const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,                  \
      tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,  \
      const int16_t *scan, const int16_t *iscan);                              \
  HWY_ATTR void av1_quantize_fp_##suffix(                                      \
      const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, \
      const int16_t *round_ptr, const int16_t *quant_ptr,                      \
      const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,                  \
      tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,  \
      const int16_t *scan, const int16_t *iscan) {                             \
    HWY_NAMESPACE::QuantizeFp(coeff_ptr, n_coeffs, zbin_ptr, round_ptr,        \
                              quant_ptr, quant_shift_ptr, qcoeff_ptr,          \
                              dqcoeff_ptr, dequant_ptr, eob_ptr, scan, iscan); \
  }

#define MAKE_AV1_QUANTIZE_FP_32X32(suffix)                                     \
  extern "C" void av1_quantize_fp_32x32_##suffix(                              \
      const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, \
      const int16_t *round_ptr, const int16_t *quant_ptr,                      \
      const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,                  \
      tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,  \
      const int16_t *scan, const int16_t *iscan);                              \
  HWY_ATTR void av1_quantize_fp_32x32_##suffix(                                \
      const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, \
      const int16_t *round_ptr, const int16_t *quant_ptr,                      \
      const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,                  \
      tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,  \
      const int16_t *scan, const int16_t *iscan) {                             \
    HWY_NAMESPACE::QuantizeFp32x32(                                            \
        coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr, quant_shift_ptr,  \
        qcoeff_ptr, dqcoeff_ptr, dequant_ptr, eob_ptr, scan, iscan);           \
  }

#define MAKE_AV1_QUANTIZE_FP_64X64(suffix)                                     \
  extern "C" void av1_quantize_fp_64x64_##suffix(                              \
      const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, \
      const int16_t *round_ptr, const int16_t *quant_ptr,                      \
      const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,                  \
      tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,  \
      const int16_t *scan, const int16_t *iscan);                              \
  HWY_ATTR void av1_quantize_fp_64x64_##suffix(                                \
      const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, \
      const int16_t *round_ptr, const int16_t *quant_ptr,                      \
      const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,                  \
      tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,  \
      const int16_t *scan, const int16_t *iscan) {                             \
    HWY_NAMESPACE::QuantizeFp64x64(                                            \
        coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr, quant_shift_ptr,  \
        qcoeff_ptr, dqcoeff_ptr, dequant_ptr, eob_ptr, scan, iscan);           \
  }

#define MAKE_AOM_QUANTIZE_B(suffix)                                            \
  extern "C" void aom_quantize_b_##suffix(                                     \
      const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, \
      const int16_t *round_ptr, const int16_t *quant_ptr,                      \
      const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,                  \
      tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,  \
      const int16_t *scan, const int16_t *iscan);                              \
  HWY_ATTR void aom_quantize_b_##suffix(                                       \
      const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, \
      const int16_t *round_ptr, const int16_t *quant_ptr,                      \
      const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,                  \
      tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,  \
      const int16_t *scan, const int16_t *iscan) {                             \
    HWY_NAMESPACE::QuantizeB(coeff_ptr, n_coeffs, zbin_ptr, round_ptr,         \
                             quant_ptr, quant_shift_ptr, qcoeff_ptr,           \
                             dqcoeff_ptr, dequant_ptr, eob_ptr, scan, iscan);  \
  }

#define MAKE_AOM_QUANTIZE_B_32X32(suffix)                                      \
  extern "C" void aom_quantize_b_32x32_##suffix(                               \
      const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, \
      const int16_t *round_ptr, const int16_t *quant_ptr,                      \
      const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,                  \
      tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,  \
      const int16_t *scan, const int16_t *iscan);                              \
  HWY_ATTR void aom_quantize_b_32x32_##suffix(                                 \
      const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, \
      const int16_t *round_ptr, const int16_t *quant_ptr,                      \
      const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,                  \
      tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,  \
      const int16_t *scan, const int16_t *iscan) {                             \
    HWY_NAMESPACE::QuantizeB32x32(                                             \
        coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr, quant_shift_ptr,  \
        qcoeff_ptr, dqcoeff_ptr, dequant_ptr, eob_ptr, scan, iscan);           \
  }

#define MAKE_AOM_QUANTIZE_B_64X64(suffix)                                      \
  extern "C" void aom_quantize_b_64x64_##suffix(                               \
      const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, \
      const int16_t *round_ptr, const int16_t *quant_ptr,                      \
      const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,                  \
      tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,  \
      const int16_t *scan, const int16_t *iscan);                              \
  HWY_ATTR void aom_quantize_b_64x64_##suffix(                                 \
      const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, \
      const int16_t *round_ptr, const int16_t *quant_ptr,                      \
      const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr,                  \
      tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr,  \
      const int16_t *scan, const int16_t *iscan) {                             \
    HWY_NAMESPACE::QuantizeB64x64(                                             \
        coeff_ptr, n_coeffs, zbin_ptr, round_ptr, quant_ptr, quant_shift_ptr,  \
        qcoeff_ptr, dqcoeff_ptr, dequant_ptr, eob_ptr, scan, iscan);           \
  }

#define MAKE_AV1_BLOCK_ERROR_LP(suffix)                                    \
  extern "C" int64_t av1_block_error_lp_##suffix(                          \
      const int16_t *coeff, const int16_t *dqcoeff, intptr_t block_size);  \
  HWY_ATTR int64_t av1_block_error_lp_##suffix(                            \
      const int16_t *coeff, const int16_t *dqcoeff, intptr_t block_size) { \
    return HWY_NAMESPACE::BlockErrorLp(coeff, dqcoeff, block_size);        \
  }

#define MAKE_AV1_BLOCK_ERROR(suffix)                                           \
  extern "C" int64_t av1_block_error_##suffix(                                 \
      const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, \
      int64_t *ssz);                                                           \
  HWY_ATTR int64_t av1_block_error_##suffix(                                   \
      const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, \
      int64_t *ssz) {                                                          \
    return HWY_NAMESPACE::BlockError(coeff, dqcoeff, block_size, ssz);         \
  }

#endif  // AOM_AV1_ENCODER_AV1_QUANTIZE_HWY_H_
