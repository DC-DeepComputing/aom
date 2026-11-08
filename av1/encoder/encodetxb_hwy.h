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

#ifndef AOM_AV1_ENCODER_ENCODETXB_HWY_H_
#define AOM_AV1_ENCODER_ENCODETXB_HWY_H_

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/reconinter.h"
#include "av1/common/txb_common.h"
#include "av1/encoder/encodetxb.h"
#include "third_party/highway/hwy/highway.h"

HWY_BEFORE_NAMESPACE();

namespace {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

constexpr hn::FixedTag<uint8_t, 8> uint8x8_tag;
constexpr hn::FixedTag<int8_t, 16> int8x16_tag;
constexpr hn::FixedTag<uint8_t, 16> uint8x16_tag;
constexpr hn::FixedTag<int8_t, 32> int8x32_tag;
constexpr hn::FixedTag<uint8_t, 32> uint8x32_tag;

constexpr hn::ScalableTag<int8_t> int8xN_tag;
constexpr hn::ScalableTag<uint8_t> uint8xN_tag;

using IVec8 = hn::Vec<decltype(int8xN_tag)>;
using UVec8 = hn::Vec<decltype(uint8xN_tag)>;

// Packs 32 tran_low_t values into 32 uint8_t levels in sequential order.
HWY_ATTR HWY_INLINE HWY_MAYBE_UNUSED hn::Vec<decltype(uint8x32_tag)>
Pack32Levels(const tran_low_t *coeff) {
  constexpr hn::FixedTag<int32_t, 8> d32;
  constexpr hn::FixedTag<int16_t, 16> d16;
  constexpr hn::FixedTag<int8_t, 32> d8;
  alignas(
      32) static constexpr int32_t kPermIdx8[8] = { 0, 4, 1, 5, 2, 6, 3, 7 };

  const auto c0 = hn::Abs(hn::LoadU(d32, coeff));
  const auto c1 = hn::Abs(hn::LoadU(d32, coeff + 8));
  const auto c2 = hn::Abs(hn::LoadU(d32, coeff + 16));
  const auto c3 = hn::Abs(hn::LoadU(d32, coeff + 24));

  const auto p01 = hn::ReorderDemote2To(d16, c0, c1);
  const auto p23 = hn::ReorderDemote2To(d16, c2, c3);
  const auto p0123 = hn::ReorderDemote2To(d8, p01, p23);
  const auto idx = hn::SetTableIndices(d32, kPermIdx8);
  return hn::BitCast(uint8x32_tag,
                     hn::TableLookupLanes(hn::BitCast(d32, p0123), idx));
}

#if HWY_TARGET <= HWY_AVX3
// Packs 64 tran_low_t values into 64 uint8_t levels in sequential order in a
// single 512-bit ZMM register.
HWY_ATTR HWY_INLINE UVec8 Pack64Levels(const tran_low_t *coeff) {
  constexpr hn::ScalableTag<int16_t> int16xN_tag;
  constexpr hn::ScalableTag<int32_t> int32xN_tag;
  alignas(64) static constexpr int32_t kPermIdx16[16] = { 0, 4,  8,  12, 1,  5,
                                                          9, 13, 2,  6,  10, 14,
                                                          3, 7,  11, 15 };
  const auto c0 = hn::Abs(hn::LoadU(int32xN_tag, coeff));
  const auto c1 = hn::Abs(hn::LoadU(int32xN_tag, coeff + 16));
  const auto c2 = hn::Abs(hn::LoadU(int32xN_tag, coeff + 32));
  const auto c3 = hn::Abs(hn::LoadU(int32xN_tag, coeff + 48));

  const auto p01 = hn::ReorderDemote2To(int16xN_tag, c0, c1);
  const auto p23 = hn::ReorderDemote2To(int16xN_tag, c2, c3);
  const auto p0123 = hn::ReorderDemote2To(int8xN_tag, p01, p23);
  const auto idx = hn::SetTableIndices(int32xN_tag, kPermIdx16);
  return hn::BitCast(
      uint8xN_tag, hn::TableLookupLanes(hn::BitCast(int32xN_tag, p0123), idx));
}
#endif

HWY_ATTR void TxbInitLevels(const tran_low_t *const coeff, const int width,
                            const int height, uint8_t *const levels) {
  const ptrdiff_t stride = height + TX_PAD_HOR;
  if (width * height < 64 || height == 4) {
    av1_txb_init_levels_avx2(coeff, width, height, levels);
    return;
  }

  const auto y_zeros = hn::Zero(uint8x32_tag);
  const size_t bottom_len = sizeof(*levels) * (TX_PAD_BOTTOM * stride);
  uint8_t *bottom_buf_end = levels + (width + TX_PAD_BOTTOM) * stride;
  uint8_t *bottom_buf = bottom_buf_end - ((bottom_len + 31) & ~size_t{ 31 });
  do {
    hn::StoreU(y_zeros, uint8x32_tag, bottom_buf);
    bottom_buf += 32;
  } while (bottom_buf < bottom_buf_end);

  uint8_t *ls = levels;
  const tran_low_t *cf = coeff;

#if HWY_TARGET <= HWY_AVX3
  if (height == 32) {
    for (int i = 0; i < width; i += 2) {
      const auto lv = Pack64Levels(cf);
      hn::StoreU(hn::LowerHalf(uint8x32_tag, lv), uint8x32_tag, ls);
      memset(ls + 32, 0, 4);
      ls += stride;
      hn::StoreU(hn::UpperHalf(uint8x32_tag, lv), uint8x32_tag, ls);
      memset(ls + 32, 0, 4);
      ls += stride;
      cf += 64;
    }
  } else if (height == 16) {
    for (int i = 0; i < width; i += 4) {
      const auto lv = Pack64Levels(cf);
      const auto lv_lo = hn::LowerHalf(uint8x32_tag, lv);
      const auto lv_hi = hn::UpperHalf(uint8x32_tag, lv);
      hn::StoreU(hn::LowerHalf(uint8x16_tag, lv_lo), uint8x16_tag, ls);
      memset(ls + 16, 0, 4);
      ls += stride;
      hn::StoreU(hn::UpperHalf(uint8x16_tag, lv_lo), uint8x16_tag, ls);
      memset(ls + 16, 0, 4);
      ls += stride;
      hn::StoreU(hn::LowerHalf(uint8x16_tag, lv_hi), uint8x16_tag, ls);
      memset(ls + 16, 0, 4);
      ls += stride;
      hn::StoreU(hn::UpperHalf(uint8x16_tag, lv_hi), uint8x16_tag, ls);
      memset(ls + 16, 0, 4);
      ls += stride;
      cf += 64;
    }
  } else if (height == 8) {
    for (int i = 0; i < width; i += 8) {
      const auto lv = Pack64Levels(cf);
      const auto lv_lo = hn::LowerHalf(uint8x32_tag, lv);
      const auto lv_hi = hn::UpperHalf(uint8x32_tag, lv);
      const auto r01 = hn::LowerHalf(uint8x16_tag, lv_lo);
      const auto r23 = hn::UpperHalf(uint8x16_tag, lv_lo);
      const auto r45 = hn::LowerHalf(uint8x16_tag, lv_hi);
      const auto r67 = hn::UpperHalf(uint8x16_tag, lv_hi);
      hn::StoreU(hn::LowerHalf(uint8x8_tag, r01), uint8x8_tag, ls + 0 * stride);
      memset(ls + 0 * stride + 8, 0, 4);
      hn::StoreU(hn::UpperHalf(uint8x8_tag, r01), uint8x8_tag, ls + 1 * stride);
      memset(ls + 1 * stride + 8, 0, 4);
      hn::StoreU(hn::LowerHalf(uint8x8_tag, r23), uint8x8_tag, ls + 2 * stride);
      memset(ls + 2 * stride + 8, 0, 4);
      hn::StoreU(hn::UpperHalf(uint8x8_tag, r23), uint8x8_tag, ls + 3 * stride);
      memset(ls + 3 * stride + 8, 0, 4);
      hn::StoreU(hn::LowerHalf(uint8x8_tag, r45), uint8x8_tag, ls + 4 * stride);
      memset(ls + 4 * stride + 8, 0, 4);
      hn::StoreU(hn::UpperHalf(uint8x8_tag, r45), uint8x8_tag, ls + 5 * stride);
      memset(ls + 5 * stride + 8, 0, 4);
      hn::StoreU(hn::LowerHalf(uint8x8_tag, r67), uint8x8_tag, ls + 6 * stride);
      memset(ls + 6 * stride + 8, 0, 4);
      hn::StoreU(hn::UpperHalf(uint8x8_tag, r67), uint8x8_tag, ls + 7 * stride);
      memset(ls + 7 * stride + 8, 0, 4);
      ls += 8 * stride;
      cf += 64;
    }
  } else {
    av1_txb_init_levels_avx2(coeff, width, height, levels);
  }
#else
  if (height == 32) {
    for (int i = 0; i < width; i += 2) {
      const auto lv0 = Pack32Levels(cf);
      const auto lv1 = Pack32Levels(cf + 32);
      hn::StoreU(lv0, uint8x32_tag, ls);
      memset(ls + 32, 0, 4);
      ls += stride;
      hn::StoreU(lv1, uint8x32_tag, ls);
      memset(ls + 32, 0, 4);
      ls += stride;
      cf += 64;
    }
  } else if (height == 16) {
    for (int i = 0; i < width; i += 4) {
      const auto lv_lo = Pack32Levels(cf);
      const auto lv_hi = Pack32Levels(cf + 32);
      hn::StoreU(hn::LowerHalf(uint8x16_tag, lv_lo), uint8x16_tag, ls);
      memset(ls + 16, 0, 4);
      ls += stride;
      hn::StoreU(hn::UpperHalf(uint8x16_tag, lv_lo), uint8x16_tag, ls);
      memset(ls + 16, 0, 4);
      ls += stride;
      hn::StoreU(hn::LowerHalf(uint8x16_tag, lv_hi), uint8x16_tag, ls);
      memset(ls + 16, 0, 4);
      ls += stride;
      hn::StoreU(hn::UpperHalf(uint8x16_tag, lv_hi), uint8x16_tag, ls);
      memset(ls + 16, 0, 4);
      ls += stride;
      cf += 64;
    }
  } else if (height == 8) {
    for (int i = 0; i < width; i += 8) {
      const auto lv_lo = Pack32Levels(cf);
      const auto lv_hi = Pack32Levels(cf + 32);
      const auto r01 = hn::LowerHalf(uint8x16_tag, lv_lo);
      const auto r23 = hn::UpperHalf(uint8x16_tag, lv_lo);
      const auto r45 = hn::LowerHalf(uint8x16_tag, lv_hi);
      const auto r67 = hn::UpperHalf(uint8x16_tag, lv_hi);
      hn::StoreU(hn::LowerHalf(uint8x8_tag, r01), uint8x8_tag, ls + 0 * stride);
      memset(ls + 0 * stride + 8, 0, 4);
      hn::StoreU(hn::UpperHalf(uint8x8_tag, r01), uint8x8_tag, ls + 1 * stride);
      memset(ls + 1 * stride + 8, 0, 4);
      hn::StoreU(hn::LowerHalf(uint8x8_tag, r23), uint8x8_tag, ls + 2 * stride);
      memset(ls + 2 * stride + 8, 0, 4);
      hn::StoreU(hn::UpperHalf(uint8x8_tag, r23), uint8x8_tag, ls + 3 * stride);
      memset(ls + 3 * stride + 8, 0, 4);
      hn::StoreU(hn::LowerHalf(uint8x8_tag, r45), uint8x8_tag, ls + 4 * stride);
      memset(ls + 4 * stride + 8, 0, 4);
      hn::StoreU(hn::UpperHalf(uint8x8_tag, r45), uint8x8_tag, ls + 5 * stride);
      memset(ls + 5 * stride + 8, 0, 4);
      hn::StoreU(hn::LowerHalf(uint8x8_tag, r67), uint8x8_tag, ls + 6 * stride);
      memset(ls + 6 * stride + 8, 0, 4);
      hn::StoreU(hn::UpperHalf(uint8x8_tag, r67), uint8x8_tag, ls + 7 * stride);
      memset(ls + 7 * stride + 8, 0, 4);
      ls += 8 * stride;
      cf += 64;
    }
  } else {
    av1_txb_init_levels_avx2(coeff, width, height, levels);
  }
#endif
}

// -----------------------------------------------------------------------------
// av1_get_nz_map_contexts
// -----------------------------------------------------------------------------

// A 64-byte block of int8_t/uint8_t represented as either 1 Scalable vector
// (on 512-bit AVX3) or 2 256-bit vectors (on 256-bit AVX2).
struct Vec64 {
#if HWY_TARGET <= HWY_AVX3
  IVec8 v;
#else
  hn::Vec<decltype(int8x32_tag)> lo;
  hn::Vec<decltype(int8x32_tag)> hi;
#endif
};

HWY_ATTR HWY_INLINE Vec64 Set1_64(int8_t val) {
  Vec64 res;
#if HWY_TARGET <= HWY_AVX3
  res.v = hn::Set(int8xN_tag, val);
#else
  res.lo = hn::Set(int8x32_tag, val);
  res.hi = hn::Set(int8x32_tag, val);
#endif
  return res;
}

HWY_ATTR HWY_INLINE Vec64 Broadcast128To64(hn::Vec<decltype(int8x16_tag)> a) {
  Vec64 res;
#if HWY_TARGET <= HWY_AVX3
  res.v = hn::BroadcastBlock<0>(hn::ResizeBitCast(int8xN_tag, a));
#else
  res.lo = hn::Combine(int8x32_tag, a, a);
  res.hi = res.lo;
#endif
  return res;
}

HWY_ATTR HWY_INLINE Vec64 Pack4x128To64(hn::Vec<decltype(int8x16_tag)> a,
                                        hn::Vec<decltype(int8x16_tag)> b,
                                        hn::Vec<decltype(int8x16_tag)> c,
                                        hn::Vec<decltype(int8x16_tag)> d) {
  Vec64 res;
#if HWY_TARGET <= HWY_AVX3
  res.v = hn::InsertBlock<3>(
      hn::InsertBlock<2>(
          hn::InsertBlock<1>(hn::ResizeBitCast(int8xN_tag, a), b), c),
      d);
#else
  res.lo = hn::Combine(int8x32_tag, b, a);
  res.hi = hn::Combine(int8x32_tag, d, c);
#endif
  return res;
}

HWY_ATTR HWY_INLINE Vec64 Add64(Vec64 a, Vec64 b) {
  Vec64 res;
#if HWY_TARGET <= HWY_AVX3
  res.v = hn::Add(a.v, b.v);
#else
  res.lo = hn::Add(a.lo, b.lo);
  res.hi = hn::Add(a.hi, b.hi);
#endif
  return res;
}

HWY_ATTR HWY_INLINE void StoreU64(Vec64 a, int8_t *dst) {
#if HWY_TARGET <= HWY_AVX3
  hn::StoreU(a.v, int8xN_tag, dst);
#else
  hn::StoreU(a.lo, int8x32_tag, dst);
  hn::StoreU(a.hi, int8x32_tag, dst + 32);
#endif
}

HWY_ATTR HWY_INLINE hn::Vec<decltype(int8x16_tag)> Load8bit8x2To128(
    const uint8_t *const src, const ptrdiff_t stride) {
  const auto r0 = hn::LoadU(uint8x8_tag, src + 0 * stride);
  const auto r1 = hn::LoadU(uint8x8_tag, src + 1 * stride);
  return hn::BitCast(int8x16_tag, hn::Combine(uint8x16_tag, r1, r0));
}

HWY_ATTR HWY_INLINE Vec64 Load8bit8x8To64(const uint8_t *const src,
                                          const ptrdiff_t stride) {
  const auto r01 = Load8bit8x2To128(src + 0 * stride, stride);
  const auto r23 = Load8bit8x2To128(src + 2 * stride, stride);
  const auto r45 = Load8bit8x2To128(src + 4 * stride, stride);
  const auto r67 = Load8bit8x2To128(src + 6 * stride, stride);
  return Pack4x128To64(r01, r23, r45, r67);
}

HWY_ATTR HWY_INLINE Vec64 Load8bit16x4To64(const uint8_t *const src,
                                           const ptrdiff_t stride) {
  const auto r0 =
      hn::BitCast(int8x16_tag, hn::LoadU(uint8x16_tag, src + 0 * stride));
  const auto r1 =
      hn::BitCast(int8x16_tag, hn::LoadU(uint8x16_tag, src + 1 * stride));
  const auto r2 =
      hn::BitCast(int8x16_tag, hn::LoadU(uint8x16_tag, src + 2 * stride));
  const auto r3 =
      hn::BitCast(int8x16_tag, hn::LoadU(uint8x16_tag, src + 3 * stride));
  return Pack4x128To64(r0, r1, r2, r3);
}

HWY_ATTR HWY_INLINE Vec64 Load8bit32x2To64(const uint8_t *const src,
                                           const ptrdiff_t stride) {
  const auto r0 =
      hn::BitCast(int8x32_tag, hn::LoadU(uint8x32_tag, src + 0 * stride));
  const auto r1 =
      hn::BitCast(int8x32_tag, hn::LoadU(uint8x32_tag, src + 1 * stride));
  Vec64 res;
#if HWY_TARGET <= HWY_AVX3
  res.v = hn::ConcatLowerLower(int8xN_tag, hn::ResizeBitCast(int8xN_tag, r1),
                               hn::ResizeBitCast(int8xN_tag, r0));
#else
  res.lo = r0;
  res.hi = r1;
#endif
  return res;
}

HWY_ATTR HWY_INLINE void LoadLevels8x8x5(const uint8_t *const src,
                                         const ptrdiff_t stride,
                                         const ptrdiff_t *const offsets,
                                         Vec64 *const level) {
  level[0] = Load8bit8x8To64(src + 1, stride);
  level[1] = Load8bit8x8To64(src + stride, stride);
  level[2] = Load8bit8x8To64(src + offsets[0], stride);
  level[3] = Load8bit8x8To64(src + offsets[1], stride);
  level[4] = Load8bit8x8To64(src + offsets[2], stride);
}

HWY_ATTR HWY_INLINE void LoadLevels16x4x5(const uint8_t *const src,
                                          const ptrdiff_t stride,
                                          const ptrdiff_t *const offsets,
                                          Vec64 *const level) {
  level[0] = Load8bit16x4To64(src + 1, stride);
  level[1] = Load8bit16x4To64(src + stride, stride);
  level[2] = Load8bit16x4To64(src + offsets[0], stride);
  level[3] = Load8bit16x4To64(src + offsets[1], stride);
  level[4] = Load8bit16x4To64(src + offsets[2], stride);
}

HWY_ATTR HWY_INLINE void LoadLevels32x2x5(const uint8_t *const src,
                                          const ptrdiff_t stride,
                                          const ptrdiff_t *const offsets,
                                          Vec64 *const level) {
  level[0] = Load8bit32x2To64(src + 1, stride);
  level[1] = Load8bit32x2To64(src + stride, stride);
  level[2] = Load8bit32x2To64(src + offsets[0], stride);
  level[3] = Load8bit32x2To64(src + offsets[1], stride);
  level[4] = Load8bit32x2To64(src + offsets[2], stride);
}

template <typename D>
HWY_ATTR HWY_INLINE hn::Vec<D> GetCoeffContextsKernelVec(D d, hn::Vec<D> l0,
                                                         hn::Vec<D> l1,
                                                         hn::Vec<D> l2,
                                                         hn::Vec<D> l3,
                                                         hn::Vec<D> l4) {
  const auto du = hn::Repartition<uint8_t, D>();
  const auto const_3 = hn::Set(du, 3);
  const auto const_4 = hn::Set(du, 4);
  auto count = hn::Min(hn::BitCast(du, l0), const_3);
  const auto u1 = hn::Min(hn::BitCast(du, l1), const_3);
  const auto u2 = hn::Min(hn::BitCast(du, l2), const_3);
  const auto u3 = hn::Min(hn::BitCast(du, l3), const_3);
  const auto u4 = hn::Min(hn::BitCast(du, l4), const_3);
  count = hn::Add(count, u1);
  count = hn::Add(count, u2);
  count = hn::Add(count, u3);
  count = hn::Add(count, u4);
  count = hn::AverageRound(count, hn::Zero(du));
  count = hn::Min(count, const_4);
  return hn::BitCast(d, count);
}

HWY_ATTR HWY_INLINE Vec64 GetCoeffContextsKernel(const Vec64 *const level) {
  Vec64 res;
#if HWY_TARGET <= HWY_AVX3
  res.v = GetCoeffContextsKernelVec(int8xN_tag, level[0].v, level[1].v,
                                    level[2].v, level[3].v, level[4].v);
#else
  res.lo = GetCoeffContextsKernelVec(int8x32_tag, level[0].lo, level[1].lo,
                                     level[2].lo, level[3].lo, level[4].lo);
  res.hi = GetCoeffContextsKernelVec(int8x32_tag, level[0].hi, level[1].hi,
                                     level[2].hi, level[3].hi, level[4].hi);
#endif
  return res;
}

HWY_ATTR HWY_INLINE void Get8CoeffContexts2D(const uint8_t *levels,
                                             const int width,
                                             const ptrdiff_t *const offsets,
                                             int8_t *coeff_contexts) {
  const ptrdiff_t stride = 8 + TX_PAD_HOR;
  int8_t *cc = coeff_contexts;
  int col = width;
  Vec64 level[5];

  alignas(16) static constexpr int8_t kP0Eq8[16] = { 0,  1,  6,  6, 21, 21,
                                                     21, 21, 1,  6, 6,  21,
                                                     21, 21, 21, 21 };
  alignas(16) static constexpr int8_t kP0Ne8[16] = { 0,  16, 16, 16, 16, 16,
                                                     16, 16, 16, 16, 16, 16,
                                                     16, 16, 16, 16 };
  alignas(16) static constexpr int8_t kP1[16] = {
    6, 6, 21, 21, 21, 21, 21, 21, 6, 21, 21, 21, 21, 21, 21, 21
  };

  const auto p0 = hn::Load(int8x16_tag, (width == 8) ? kP0Eq8 : kP0Ne8);
  const auto p1 = hn::Load(int8x16_tag, kP1);
  const auto p2 = hn::Set(int8x16_tag, 21);

  Vec64 pos_to_offset = Pack4x128To64(p0, p1, p2, p2);
  const Vec64 pos_to_offset_large = Set1_64(21);

  do {
    LoadLevels8x8x5(levels, stride, offsets, level);
    Vec64 count = GetCoeffContextsKernel(level);
    count = Add64(count, pos_to_offset);
    StoreU64(count, cc);
    pos_to_offset = pos_to_offset_large;
    levels += 8 * stride;
    cc += 64;
    col -= 8;
  } while (col > 0);

  coeff_contexts[0] = 0;
}

HWY_ATTR HWY_INLINE void Get8CoeffContextsVer(const uint8_t *levels,
                                              const int width,
                                              const ptrdiff_t *const offsets,
                                              int8_t *coeff_contexts) {
  const ptrdiff_t stride = 8 + TX_PAD_HOR;
  alignas(16) static constexpr int8_t kP0[16] = {
    SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 5,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 5,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10
  };
  const Vec64 pos_to_offset = Broadcast128To64(hn::Load(int8x16_tag, kP0));
  int col = width;
  Vec64 level[5];

  do {
    LoadLevels8x8x5(levels, stride, offsets, level);
    Vec64 count = GetCoeffContextsKernel(level);
    count = Add64(count, pos_to_offset);
    StoreU64(count, coeff_contexts);
    levels += 8 * stride;
    coeff_contexts += 64;
    col -= 8;
  } while (col > 0);
}

HWY_ATTR HWY_INLINE void Get8CoeffContextsHor(const uint8_t *levels,
                                              const int width,
                                              const ptrdiff_t *const offsets,
                                              int8_t *coeff_contexts) {
  const ptrdiff_t stride = 8 + TX_PAD_HOR;
  alignas(16) static constexpr int8_t kP0[16] = {
    SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 0,
    SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 0,
    SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 0,
    SIG_COEF_CONTEXTS_2D + 0, SIG_COEF_CONTEXTS_2D + 0,
    SIG_COEF_CONTEXTS_2D + 5, SIG_COEF_CONTEXTS_2D + 5,
    SIG_COEF_CONTEXTS_2D + 5, SIG_COEF_CONTEXTS_2D + 5,
    SIG_COEF_CONTEXTS_2D + 5, SIG_COEF_CONTEXTS_2D + 5,
    SIG_COEF_CONTEXTS_2D + 5, SIG_COEF_CONTEXTS_2D + 5
  };
  const auto p0 = hn::Load(int8x16_tag, kP0);
  const auto p_large = hn::Set(int8x16_tag, SIG_COEF_CONTEXTS_2D + 10);
  Vec64 pos_to_offset = Pack4x128To64(p0, p_large, p_large, p_large);
  const Vec64 pos_to_offset_large = Set1_64(SIG_COEF_CONTEXTS_2D + 10);
  int col = width;
  Vec64 level[5];

  do {
    LoadLevels8x8x5(levels, stride, offsets, level);
    Vec64 count = GetCoeffContextsKernel(level);
    count = Add64(count, pos_to_offset);
    StoreU64(count, coeff_contexts);
    pos_to_offset = pos_to_offset_large;
    levels += 8 * stride;
    coeff_contexts += 64;
    col -= 8;
  } while (col > 0);
}

HWY_ATTR HWY_INLINE void Get16CoeffContexts2D(
    const uint8_t *levels, const int real_width, const int real_height,
    const int width, const ptrdiff_t *const offsets, int8_t *coeff_contexts) {
  const ptrdiff_t stride = 16 + TX_PAD_HOR;
  int8_t *cc = coeff_contexts;
  int col = width;
  Vec64 level[5];
  Vec64 pos_to_offset;
  Vec64 pos_to_offset_large;

  alignas(16) static constexpr int8_t kP0Eq[16] = { 0,  1,  6,  6,  21, 21,
                                                    21, 21, 21, 21, 21, 21,
                                                    21, 21, 21, 21 };
  alignas(16) static constexpr int8_t kP1Eq[16] = { 1,  6,  6,  21, 21, 21,
                                                    21, 21, 21, 21, 21, 21,
                                                    21, 21, 21, 21 };
  alignas(16) static constexpr int8_t kP2Eq[16] = { 6,  6,  21, 21, 21, 21,
                                                    21, 21, 21, 21, 21, 21,
                                                    21, 21, 21, 21 };
  alignas(16) static constexpr int8_t kP3Eq[16] = { 6,  21, 21, 21, 21, 21,
                                                    21, 21, 21, 21, 21, 21,
                                                    21, 21, 21, 21 };
  alignas(16) static constexpr int8_t kP0Lt[16] = { 0,  11, 6,  6,  21, 21,
                                                    21, 21, 21, 21, 21, 21,
                                                    21, 21, 21, 21 };
  alignas(16) static constexpr int8_t kP1Lt[16] = { 11, 11, 6,  21, 21, 21,
                                                    21, 21, 21, 21, 21, 21,
                                                    21, 21, 21, 21 };
  alignas(16) static constexpr int8_t kP2Lt[16] = { 11, 11, 21, 21, 21, 21,
                                                    21, 21, 21, 21, 21, 21,
                                                    21, 21, 21, 21 };

  if (real_width == real_height) {
    const auto p0 = hn::Load(int8x16_tag, kP0Eq);
    const auto p1 = hn::Load(int8x16_tag, kP1Eq);
    const auto p2 = hn::Load(int8x16_tag, kP2Eq);
    const auto p3 = hn::Load(int8x16_tag, kP3Eq);
    pos_to_offset = Pack4x128To64(p0, p1, p2, p3);
    pos_to_offset_large = Set1_64(21);
  } else if (real_width < real_height) {
    const auto p0 = hn::Load(int8x16_tag, kP0Lt);
    const auto p1 = hn::Load(int8x16_tag, kP1Lt);
    const auto p2 = hn::Load(int8x16_tag, kP2Lt);
    pos_to_offset = Pack4x128To64(p0, p1, p2, p2);
    pos_to_offset_large = Broadcast128To64(p2);
  } else {
    const auto p0 = hn::Set(int8x16_tag, 16);
    const auto p2 = hn::Load(int8x16_tag, kP2Eq);
    const auto p3 = hn::Load(int8x16_tag, kP3Eq);
    pos_to_offset = Pack4x128To64(p0, p0, p2, p3);
    pos_to_offset_large = Set1_64(21);
  }

  do {
    LoadLevels16x4x5(levels, stride, offsets, level);
    Vec64 count = GetCoeffContextsKernel(level);
    count = Add64(count, pos_to_offset);
    StoreU64(count, cc);
    pos_to_offset = pos_to_offset_large;
    levels += 4 * stride;
    cc += 64;
    col -= 4;
  } while (col > 0);

  coeff_contexts[0] = 0;
}

HWY_ATTR HWY_INLINE void Get16CoeffContextsVer(const uint8_t *levels,
                                               const int width,
                                               const ptrdiff_t *const offsets,
                                               int8_t *coeff_contexts) {
  const ptrdiff_t stride = 16 + TX_PAD_HOR;
  alignas(16) static constexpr int8_t kP0[16] = {
    SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 5,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10
  };
  const Vec64 pos_to_offset = Broadcast128To64(hn::Load(int8x16_tag, kP0));
  int col = width;
  Vec64 level[5];

  do {
    LoadLevels16x4x5(levels, stride, offsets, level);
    Vec64 count = GetCoeffContextsKernel(level);
    count = Add64(count, pos_to_offset);
    StoreU64(count, coeff_contexts);
    levels += 4 * stride;
    coeff_contexts += 64;
    col -= 4;
  } while (col > 0);
}

HWY_ATTR HWY_INLINE void Get16CoeffContextsHor(const uint8_t *levels,
                                               const int width,
                                               const ptrdiff_t *const offsets,
                                               int8_t *coeff_contexts) {
  const ptrdiff_t stride = 16 + TX_PAD_HOR;
  const auto p0 = hn::Set(int8x16_tag, SIG_COEF_CONTEXTS_2D + 0);
  const auto p1 = hn::Set(int8x16_tag, SIG_COEF_CONTEXTS_2D + 5);
  const auto p2 = hn::Set(int8x16_tag, SIG_COEF_CONTEXTS_2D + 10);
  Vec64 pos_to_offset = Pack4x128To64(p0, p1, p2, p2);
  const Vec64 pos_to_offset_large = Set1_64(SIG_COEF_CONTEXTS_2D + 10);
  int col = width;
  Vec64 level[5];

  do {
    LoadLevels16x4x5(levels, stride, offsets, level);
    Vec64 count = GetCoeffContextsKernel(level);
    count = Add64(count, pos_to_offset);
    StoreU64(count, coeff_contexts);
    pos_to_offset = pos_to_offset_large;
    levels += 4 * stride;
    coeff_contexts += 64;
    col -= 4;
  } while (col > 0);
}

HWY_ATTR HWY_INLINE void Get32CoeffContexts2D(
    const uint8_t *levels, const int real_width, const int real_height,
    const int width, const ptrdiff_t *const offsets, int8_t *coeff_contexts) {
  const ptrdiff_t stride = 32 + TX_PAD_HOR;
  int8_t *cc = coeff_contexts;
  int col = width;
  Vec64 level[5];
  Vec64 pos_to_offset[3];
  const auto p_21 = hn::Set(int8x16_tag, 21);

  alignas(16) static constexpr int8_t kP0Eq[16] = { 0,  1,  6,  6,  21, 21,
                                                    21, 21, 21, 21, 21, 21,
                                                    21, 21, 21, 21 };
  alignas(16) static constexpr int8_t kP1Eq[16] = { 1,  6,  6,  21, 21, 21,
                                                    21, 21, 21, 21, 21, 21,
                                                    21, 21, 21, 21 };
  alignas(16) static constexpr int8_t kP2Eq[16] = { 6,  6,  21, 21, 21, 21,
                                                    21, 21, 21, 21, 21, 21,
                                                    21, 21, 21, 21 };
  alignas(16) static constexpr int8_t kP3Eq[16] = { 6,  21, 21, 21, 21, 21,
                                                    21, 21, 21, 21, 21, 21,
                                                    21, 21, 21, 21 };
  alignas(16) static constexpr int8_t kP0Lt[16] = { 0,  11, 6,  6,  21, 21,
                                                    21, 21, 21, 21, 21, 21,
                                                    21, 21, 21, 21 };
  alignas(16) static constexpr int8_t kP1Lt[16] = { 11, 11, 6,  21, 21, 21,
                                                    21, 21, 21, 21, 21, 21,
                                                    21, 21, 21, 21 };
  alignas(16) static constexpr int8_t kP2Lt[16] = { 11, 11, 21, 21, 21, 21,
                                                    21, 21, 21, 21, 21, 21,
                                                    21, 21, 21, 21 };

  if (real_width == real_height) {
    const auto p0 = hn::Load(int8x16_tag, kP0Eq);
    const auto p1 = hn::Load(int8x16_tag, kP1Eq);
    const auto p2 = hn::Load(int8x16_tag, kP2Eq);
    const auto p3 = hn::Load(int8x16_tag, kP3Eq);
    pos_to_offset[0] = Pack4x128To64(p0, p_21, p1, p_21);
    pos_to_offset[1] = Pack4x128To64(p2, p_21, p3, p_21);
    pos_to_offset[2] = Set1_64(21);
  } else if (real_width < real_height) {
    const auto p0 = hn::Load(int8x16_tag, kP0Lt);
    const auto p1 = hn::Load(int8x16_tag, kP1Lt);
    const auto p2 = hn::Load(int8x16_tag, kP2Lt);
    pos_to_offset[0] = Pack4x128To64(p0, p_21, p1, p_21);
    pos_to_offset[1] = pos_to_offset[2] = Pack4x128To64(p2, p_21, p2, p_21);
  } else {
    const auto p_16 = hn::Set(int8x16_tag, 16);
    const auto p2 = hn::Load(int8x16_tag, kP2Eq);
    const auto p3 = hn::Load(int8x16_tag, kP3Eq);
    pos_to_offset[0] = Pack4x128To64(p_16, p_16, p_16, p_16);
    pos_to_offset[1] = Pack4x128To64(p2, p_21, p3, p_21);
    pos_to_offset[2] = Set1_64(21);
  }

  do {
    LoadLevels32x2x5(levels, stride, offsets, level);
    Vec64 count = GetCoeffContextsKernel(level);
    count = Add64(count, pos_to_offset[0]);
    StoreU64(count, cc);
    pos_to_offset[0] = pos_to_offset[1];
    pos_to_offset[1] = pos_to_offset[2];
    levels += 2 * stride;
    cc += 64;
    col -= 2;
  } while (col > 0);

  coeff_contexts[0] = 0;
}

HWY_ATTR HWY_INLINE void Get32CoeffContextsVer(const uint8_t *levels,
                                               const int width,
                                               const ptrdiff_t *const offsets,
                                               int8_t *coeff_contexts) {
  const ptrdiff_t stride = 32 + TX_PAD_HOR;
  alignas(16) static constexpr int8_t kP0[16] = {
    SIG_COEF_CONTEXTS_2D + 0,  SIG_COEF_CONTEXTS_2D + 5,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10,
    SIG_COEF_CONTEXTS_2D + 10, SIG_COEF_CONTEXTS_2D + 10
  };
  const auto p0 = hn::Load(int8x16_tag, kP0);
  const auto p1 = hn::Set(int8x16_tag, SIG_COEF_CONTEXTS_2D + 10);
  const Vec64 pos_to_offset = Pack4x128To64(p0, p1, p0, p1);
  int col = width;
  Vec64 level[5];

  do {
    LoadLevels32x2x5(levels, stride, offsets, level);
    Vec64 count = GetCoeffContextsKernel(level);
    count = Add64(count, pos_to_offset);
    StoreU64(count, coeff_contexts);
    levels += 2 * stride;
    coeff_contexts += 64;
    col -= 2;
  } while (col > 0);
}

HWY_ATTR HWY_INLINE void Get32CoeffContextsHor(const uint8_t *levels,
                                               const int width,
                                               const ptrdiff_t *const offsets,
                                               int8_t *coeff_contexts) {
  const ptrdiff_t stride = 32 + TX_PAD_HOR;
  const auto p0 = hn::Set(int8x16_tag, SIG_COEF_CONTEXTS_2D + 0);
  const auto p1 = hn::Set(int8x16_tag, SIG_COEF_CONTEXTS_2D + 5);
  Vec64 pos_to_offset = Pack4x128To64(p0, p0, p1, p1);
  const Vec64 pos_to_offset_large = Set1_64(SIG_COEF_CONTEXTS_2D + 10);
  int col = width;
  Vec64 level[5];

  do {
    LoadLevels32x2x5(levels, stride, offsets, level);
    Vec64 count = GetCoeffContextsKernel(level);
    count = Add64(count, pos_to_offset);
    StoreU64(count, coeff_contexts);
    pos_to_offset = pos_to_offset_large;
    levels += 2 * stride;
    coeff_contexts += 64;
    col -= 2;
  } while (col > 0);
}

HWY_ATTR void GetNzMapContexts(const uint8_t *const levels,
                               const int16_t *const scan, const uint16_t eob,
                               const TX_SIZE tx_size, const TX_CLASS tx_class,
                               int8_t *const coeff_contexts) {
  const int last_idx = static_cast<int>(eob) - 1;
  if (!last_idx) {
    coeff_contexts[0] = 0;
    return;
  }

  const int real_width = tx_size_wide[tx_size];
  const int real_height = tx_size_high[tx_size];
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  const ptrdiff_t stride = height + TX_PAD_HOR;
  ptrdiff_t offsets[3];

  /* coeff_contexts must be 16 byte aligned. */
  assert(!((intptr_t)coeff_contexts & 0xf));

  if (tx_class == TX_CLASS_2D) {
    offsets[0] = 0 * stride + 2;
    offsets[1] = 1 * stride + 1;
    offsets[2] = 2 * stride + 0;

    if (height == 8 && width >= 8) {
      Get8CoeffContexts2D(levels, width, offsets, coeff_contexts);
    } else if (height == 16) {
      Get16CoeffContexts2D(levels, real_width, real_height, width, offsets,
                           coeff_contexts);
    } else if (height == 32) {
      Get32CoeffContexts2D(levels, real_width, real_height, width, offsets,
                           coeff_contexts);
    } else {
      av1_get_nz_map_contexts_sse2(levels, scan, eob, tx_size, tx_class,
                                   coeff_contexts);
      return;
    }
  } else if (tx_class == TX_CLASS_HORIZ) {
    offsets[0] = 2 * stride;
    offsets[1] = 3 * stride;
    offsets[2] = 4 * stride;

    if (height == 8 && width >= 8) {
      Get8CoeffContextsHor(levels, width, offsets, coeff_contexts);
    } else if (height == 16) {
      Get16CoeffContextsHor(levels, width, offsets, coeff_contexts);
    } else if (height == 32) {
      Get32CoeffContextsHor(levels, width, offsets, coeff_contexts);
    } else {
      av1_get_nz_map_contexts_sse2(levels, scan, eob, tx_size, tx_class,
                                   coeff_contexts);
      return;
    }
  } else {
    offsets[0] = 2;
    offsets[1] = 3;
    offsets[2] = 4;

    if (height == 8 && width >= 8) {
      Get8CoeffContextsVer(levels, width, offsets, coeff_contexts);
    } else if (height == 16) {
      Get16CoeffContextsVer(levels, width, offsets, coeff_contexts);
    } else if (height == 32) {
      Get32CoeffContextsVer(levels, width, offsets, coeff_contexts);
    } else {
      av1_get_nz_map_contexts_sse2(levels, scan, eob, tx_size, tx_class,
                                   coeff_contexts);
      return;
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

// -----------------------------------------------------------------------------
// Wedge utilities
// -----------------------------------------------------------------------------

HWY_ATTR uint64_t WedgeSseFromResiduals(const int16_t *r1, const int16_t *d,
                                        const uint8_t *m, int N) {
  assert(N % 64 == 0);
  constexpr hn::ScalableTag<int16_t> int16xN_tag;
  constexpr hn::ScalableTag<int32_t> int32xN_tag;
  constexpr hn::ScalableTag<uint64_t> uint64xN_tag;
  constexpr intptr_t kStep = hn::MaxLanes(int16xN_tag);
  constexpr hn::Half<decltype(uint8xN_tag)> uint8_half_tag;
  const auto v_max_mask = hn::Set(int16xN_tag, 1 << WEDGE_WEIGHT_BITS);
  auto v_acc = hn::Zero(uint64xN_tag);

  for (int i = 0; i < N; i += static_cast<int>(kStep)) {
    const auto r1_v = hn::LoadU(int16xN_tag, r1 + i);
    const auto d_v = hn::LoadU(int16xN_tag, d + i);
    const auto m_v =
        hn::PromoteTo(int16xN_tag, hn::LoadU(uint8_half_tag, m + i));

    const auto rd_lo = hn::InterleaveLower(d_v, r1_v);
    const auto rd_hi = hn::InterleaveUpper(int16xN_tag, d_v, r1_v);
    const auto m_lo = hn::InterleaveLower(m_v, v_max_mask);
    const auto m_hi = hn::InterleaveUpper(int16xN_tag, m_v, v_max_mask);

    const auto t = hn::ReorderDemote2To(
        int16xN_tag, hn::WidenMulPairwiseAdd(int32xN_tag, rd_lo, m_lo),
        hn::WidenMulPairwiseAdd(int32xN_tag, rd_hi, m_hi));
    const auto sq = hn::BitCast(hn::ScalableTag<uint32_t>(),
                                hn::WidenMulPairwiseAdd(int32xN_tag, t, t));
    v_acc = hn::Add(v_acc, hn::Add(hn::PromoteEvenTo(uint64xN_tag, sq),
                                   hn::PromoteOddTo(uint64xN_tag, sq)));
  }

  const uint64_t csse = hn::ReduceSum(uint64xN_tag, v_acc);
  return ROUND_POWER_OF_TWO(csse, 2 * WEDGE_WEIGHT_BITS);
}

HWY_ATTR int8_t WedgeSignFromResiduals(const int16_t *ds, const uint8_t *m,
                                       int N, int64_t limit) {
  assert(N % 64 == 0);
  // 32-bit per-lane accumulators; see av1_wedge_sign_from_residuals_avx2.
  assert(N < 8192);
  constexpr hn::ScalableTag<int16_t> int16xN_tag;
  constexpr hn::ScalableTag<int32_t> int32xN_tag;
  constexpr hn::ScalableTag<int64_t> int64xN_tag;
  constexpr intptr_t kStep = hn::MaxLanes(int16xN_tag);
  constexpr hn::Half<decltype(uint8xN_tag)> uint8_half_tag;
  auto acc = hn::Zero(int32xN_tag);

  for (int i = 0; i < N; i += static_cast<int>(kStep)) {
    const auto ds_v = hn::LoadU(int16xN_tag, ds + i);
    const auto m_v =
        hn::PromoteTo(int16xN_tag, hn::LoadU(uint8_half_tag, m + i));
    acc = hn::Add(acc, hn::WidenMulPairwiseAdd(int32xN_tag, ds_v, m_v));
  }

  // Widen to 64-bit before the cross-lane reduction; the 16-lane total can
  // exceed INT32_MAX even though each lane cannot.
  const auto acc64 = hn::Add(hn::PromoteEvenTo(int64xN_tag, acc),
                             hn::PromoteOddTo(int64xN_tag, acc));
  const int64_t sign_sum = hn::ReduceSum(int64xN_tag, acc64);
  return sign_sum > limit;
}

HWY_ATTR void WedgeComputeDeltaSquares(int16_t *d, const int16_t *a,
                                       const int16_t *b, int N) {
  assert(N % 64 == 0);
  constexpr hn::ScalableTag<int16_t> int16xN_tag;
  constexpr hn::ScalableTag<int32_t> int32xN_tag;
  constexpr intptr_t kStep = hn::MaxLanes(int16xN_tag);
  const auto zero = hn::Zero(int16xN_tag);

  for (int i = 0; i < N; i += static_cast<int>(kStep)) {
    const auto a_v = hn::LoadU(int16xN_tag, a + i);
    const auto b_v = hn::LoadU(int16xN_tag, b + i);
    const auto nb_v = hn::SaturatedSub(zero, b_v);

    const auto r0 =
        hn::WidenMulPairwiseAdd(int32xN_tag, hn::InterleaveLower(a_v, b_v),
                                hn::InterleaveLower(a_v, nb_v));
    const auto r1 = hn::WidenMulPairwiseAdd(
        int32xN_tag, hn::InterleaveUpper(int16xN_tag, a_v, b_v),
        hn::InterleaveUpper(int16xN_tag, a_v, nb_v));

    const auto res = hn::ReorderDemote2To(int16xN_tag, r0, r1);
    hn::StoreU(res, int16xN_tag, d + i);
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace

HWY_AFTER_NAMESPACE();

#define MAKE_AV1_TXB_INIT_LEVELS(suffix)                                \
  extern "C" void av1_txb_init_levels_##suffix(                         \
      const tran_low_t *const coeff, const int width, const int height, \
      uint8_t *const levels);                                           \
  HWY_ATTR void av1_txb_init_levels_##suffix(                           \
      const tran_low_t *const coeff, const int width, const int height, \
      uint8_t *const levels) {                                          \
    HWY_NAMESPACE::TxbInitLevels(coeff, width, height, levels);         \
  }

#define MAKE_AV1_GET_NZ_MAP_CONTEXTS(suffix)                                  \
  extern "C" void av1_get_nz_map_contexts_##suffix(                           \
      const uint8_t *const levels, const int16_t *const scan, const int eob,  \
      const TX_SIZE tx_size, const TX_CLASS tx_class,                         \
      int8_t *const coeff_contexts);                                          \
  HWY_ATTR void av1_get_nz_map_contexts_##suffix(                             \
      const uint8_t *const levels, const int16_t *const scan, const int eob,  \
      const TX_SIZE tx_size, const TX_CLASS tx_class,                         \
      int8_t *const coeff_contexts) {                                         \
    HWY_NAMESPACE::GetNzMapContexts(levels, scan, static_cast<uint16_t>(eob), \
                                    tx_size, tx_class, coeff_contexts);       \
  }

#define MAKE_AV1_WEDGE_SSE_FROM_RESIDUALS(suffix)                     \
  extern "C" uint64_t av1_wedge_sse_from_residuals_##suffix(          \
      const int16_t *r1, const int16_t *d, const uint8_t *m, int N);  \
  HWY_ATTR uint64_t av1_wedge_sse_from_residuals_##suffix(            \
      const int16_t *r1, const int16_t *d, const uint8_t *m, int N) { \
    return HWY_NAMESPACE::WedgeSseFromResiduals(r1, d, m, N);         \
  }

#define MAKE_AV1_WEDGE_SIGN_FROM_RESIDUALS(suffix)                 \
  extern "C" int8_t av1_wedge_sign_from_residuals_##suffix(        \
      const int16_t *ds, const uint8_t *m, int N, int64_t limit);  \
  HWY_ATTR int8_t av1_wedge_sign_from_residuals_##suffix(          \
      const int16_t *ds, const uint8_t *m, int N, int64_t limit) { \
    return HWY_NAMESPACE::WedgeSignFromResiduals(ds, m, N, limit); \
  }

#define MAKE_AV1_WEDGE_COMPUTE_DELTA_SQUARES(suffix)           \
  extern "C" void av1_wedge_compute_delta_squares_##suffix(    \
      int16_t *d, const int16_t *a, const int16_t *b, int N);  \
  HWY_ATTR void av1_wedge_compute_delta_squares_##suffix(      \
      int16_t *d, const int16_t *a, const int16_t *b, int N) { \
    HWY_NAMESPACE::WedgeComputeDeltaSquares(d, a, b, N);       \
  }

#endif  // AOM_AV1_ENCODER_ENCODETXB_HWY_H_
