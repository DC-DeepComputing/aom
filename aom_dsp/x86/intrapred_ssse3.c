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

#include <tmmintrin.h>

#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/intrapred_common.h"
#include "aom_dsp/x86/synonyms.h"

// -----------------------------------------------------------------------------
// PAETH_PRED

// Return 8 16-bit pixels in one row using precomputed dt = top - tl, pl = |dt|,
// dl = left - tl, pt = |dl|.
static AOM_FORCE_INLINE __m128i paeth_8x1_pred_pre(
    const __m128i left, const __m128i top, const __m128i topleft,
    const __m128i dt, const __m128i pl, const __m128i dl, const __m128i pt) {
  const __m128i ptl = _mm_abs_epi16(_mm_add_epi16(dt, dl));
  const __m128i min_t_tl = _mm_min_epi16(pt, ptl);
  const __m128i mask1 = _mm_cmpgt_epi16(pl, min_t_tl);
  const __m128i mask2 = _mm_cmpgt_epi16(pt, ptl);
  const __m128i sel_t_tl =
      _mm_or_si128(_mm_andnot_si128(mask2, top), _mm_and_si128(mask2, topleft));
  return _mm_or_si128(_mm_andnot_si128(mask1, left),
                      _mm_and_si128(mask1, sel_t_tl));
}

// Return 16 8-bit pixels in one row
static AOM_FORCE_INLINE __m128i paeth_16x1_pred_pre(
    const __m128i left, const __m128i top0, const __m128i top1,
    const __m128i topleft, const __m128i dt0, const __m128i pl0,
    const __m128i dt1, const __m128i pl1, const __m128i dl, const __m128i pt) {
  const __m128i p0 = paeth_8x1_pred_pre(left, top0, topleft, dt0, pl0, dl, pt);
  const __m128i p1 = paeth_8x1_pred_pre(left, top1, topleft, dt1, pl1, dl, pt);
  return _mm_packus_epi16(p0, p1);
}

void aom_paeth_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  const __m128i l = xx_loadl_32(left);
  const __m128i t = xx_loadl_32(above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(_mm_unpacklo_epi32(t, t), zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt = _mm_sub_epi16(t16, tl16);
  const __m128i pl = _mm_abs_epi16(dt);
  const __m128i l16_all = _mm_unpacklo_epi8(l, zero);

  const __m128i l01 = _mm_shuffle_epi8(
      l16_all, _mm_set_epi32(0x03020302, 0x03020302, 0x01000100, 0x01000100));
  const __m128i dl01 = _mm_sub_epi16(l01, tl16);
  const __m128i pt01 = _mm_abs_epi16(dl01);
  const __m128i row01 = paeth_8x1_pred_pre(l01, t16, tl16, dt, pl, dl01, pt01);
  const __m128i packed01 = _mm_packus_epi16(row01, row01);
  xx_storel_32(dst, packed01);
  xx_storel_32(dst + stride, _mm_srli_si128(packed01, 4));
  dst += stride << 1;

  const __m128i l23 = _mm_shuffle_epi8(
      l16_all, _mm_set_epi32(0x07060706, 0x07060706, 0x05040504, 0x05040504));
  const __m128i dl23 = _mm_sub_epi16(l23, tl16);
  const __m128i pt23 = _mm_abs_epi16(dl23);
  const __m128i row23 = paeth_8x1_pred_pre(l23, t16, tl16, dt, pl, dl23, pt23);
  const __m128i packed23 = _mm_packus_epi16(row23, row23);
  xx_storel_32(dst, packed23);
  xx_storel_32(dst + stride, _mm_srli_si128(packed23, 4));
}

void aom_paeth_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  const __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = xx_loadl_32(above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(_mm_unpacklo_epi32(t, t), zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt = _mm_sub_epi16(t16, tl16);
  const __m128i pl = _mm_abs_epi16(dt);
  __m128i rep = _mm_set_epi32((int)0x80018001, (int)0x80018001, (int)0x80008000,
                              (int)0x80008000);
  const __m128i two = _mm_set1_epi16(2);

  for (int i = 0; i < 4; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i dl = _mm_sub_epi16(l16, tl16);
    const __m128i pt = _mm_abs_epi16(dl);
    const __m128i row = paeth_8x1_pred_pre(l16, t16, tl16, dt, pl, dl, pt);
    const __m128i packed = _mm_packus_epi16(row, row);

    xx_storel_32(dst, packed);
    xx_storel_32(dst + stride, _mm_srli_si128(packed, 4));
    dst += stride << 1;
    rep = _mm_add_epi16(rep, two);
  }
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_paeth_predictor_4x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = xx_loadl_32(above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(_mm_unpacklo_epi32(t, t), zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt = _mm_sub_epi16(t16, tl16);
  const __m128i pl = _mm_abs_epi16(dt);
  __m128i rep = _mm_set_epi32((int)0x80018001, (int)0x80018001, (int)0x80008000,
                              (int)0x80008000);
  const __m128i two = _mm_set1_epi16(2);

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 1
#endif
  for (int i = 0; i < 8; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i dl = _mm_sub_epi16(l16, tl16);
    const __m128i pt = _mm_abs_epi16(dl);
    const __m128i row = paeth_8x1_pred_pre(l16, t16, tl16, dt, pl, dl, pt);
    const __m128i packed = _mm_packus_epi16(row, row);

    xx_storel_32(dst, packed);
    xx_storel_32(dst + stride, _mm_srli_si128(packed, 4));
    dst += stride << 1;
    rep = _mm_add_epi16(rep, two);
  }
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_paeth_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  const __m128i l = xx_loadl_32(left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt = _mm_sub_epi16(t16, tl16);
  const __m128i pl = _mm_abs_epi16(dt);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  for (int i = 0; i < 4; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i dl = _mm_sub_epi16(l16, tl16);
    const __m128i pt = _mm_abs_epi16(dl);
    const __m128i row = paeth_8x1_pred_pre(l16, t16, tl16, dt, pl, dl, pt);

    _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  const __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt = _mm_sub_epi16(t16, tl16);
  const __m128i pl = _mm_abs_epi16(dt);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  for (int i = 0; i < 8; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i dl = _mm_sub_epi16(l16, tl16);
    const __m128i pt = _mm_abs_epi16(dl);
    const __m128i row = paeth_8x1_pred_pre(l16, t16, tl16, dt, pl, dl, pt);

    _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt = _mm_sub_epi16(t16, tl16);
  const __m128i pl = _mm_abs_epi16(dt);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  for (int i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i dl = _mm_sub_epi16(l16, tl16);
    const __m128i pt = _mm_abs_epi16(dl);
    const __m128i row = paeth_8x1_pred_pre(l16, t16, tl16, dt, pl, dl, pt);

    _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_paeth_predictor_8x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt = _mm_sub_epi16(t16, tl16);
  const __m128i pl = _mm_abs_epi16(dt);
  const __m128i one = _mm_set1_epi16(1);

  for (int j = 0; j < 2; ++j) {
    const __m128i l = _mm_load_si128((const __m128i *)(left + j * 16));
    __m128i rep = _mm_set1_epi16((short)0x8000);
    for (int i = 0; i < 16; ++i) {
      const __m128i l16 = _mm_shuffle_epi8(l, rep);
      const __m128i dl = _mm_sub_epi16(l16, tl16);
      const __m128i pt = _mm_abs_epi16(dl);
      const __m128i row = paeth_8x1_pred_pre(l16, t16, tl16, dt, pl, dl, pt);

      _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
      dst += stride;
      rep = _mm_add_epi16(rep, one);
    }
  }
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_paeth_predictor_16x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m128i l = xx_loadl_32(left);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt0 = _mm_sub_epi16(top0, tl16);
  const __m128i pl0 = _mm_abs_epi16(dt0);
  const __m128i dt1 = _mm_sub_epi16(top1, tl16);
  const __m128i pl1 = _mm_abs_epi16(dt1);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  for (int i = 0; i < 4; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i dl = _mm_sub_epi16(l16, tl16);
    const __m128i pt = _mm_abs_epi16(dl);
    const __m128i row =
        paeth_16x1_pred_pre(l16, top0, top1, tl16, dt0, pl0, dt1, pl1, dl, pt);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_paeth_predictor_16x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt0 = _mm_sub_epi16(top0, tl16);
  const __m128i pl0 = _mm_abs_epi16(dt0);
  const __m128i dt1 = _mm_sub_epi16(top1, tl16);
  const __m128i pl1 = _mm_abs_epi16(dt1);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 1
#endif
  for (int i = 0; i < 8; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i dl = _mm_sub_epi16(l16, tl16);
    const __m128i pt = _mm_abs_epi16(dl);
    const __m128i row =
        paeth_16x1_pred_pre(l16, top0, top1, tl16, dt0, pl0, dt1, pl1, dl, pt);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt0 = _mm_sub_epi16(top0, tl16);
  const __m128i pl0 = _mm_abs_epi16(dt0);
  const __m128i dt1 = _mm_sub_epi16(top1, tl16);
  const __m128i pl1 = _mm_abs_epi16(dt1);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  for (int i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i dl = _mm_sub_epi16(l16, tl16);
    const __m128i pt = _mm_abs_epi16(dl);
    const __m128i row =
        paeth_16x1_pred_pre(l16, top0, top1, tl16, dt0, pl0, dt1, pl1, dl, pt);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_16x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt0 = _mm_sub_epi16(top0, tl16);
  const __m128i pl0 = _mm_abs_epi16(dt0);
  const __m128i dt1 = _mm_sub_epi16(top1, tl16);
  const __m128i pl1 = _mm_abs_epi16(dt1);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  for (int i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i dl = _mm_sub_epi16(l16, tl16);
    const __m128i pt = _mm_abs_epi16(dl);
    const __m128i row =
        paeth_16x1_pred_pre(l16, top0, top1, tl16, dt0, pl0, dt1, pl1, dl, pt);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }

  l = _mm_load_si128((const __m128i *)(left + 16));
  rep = _mm_set1_epi16((short)0x8000);
  for (int i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i dl = _mm_sub_epi16(l16, tl16);
    const __m128i pt = _mm_abs_epi16(dl);
    const __m128i row =
        paeth_16x1_pred_pre(l16, top0, top1, tl16, dt0, pl0, dt1, pl1, dl, pt);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_paeth_predictor_16x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt0 = _mm_sub_epi16(top0, tl16);
  const __m128i pl0 = _mm_abs_epi16(dt0);
  const __m128i dt1 = _mm_sub_epi16(top1, tl16);
  const __m128i pl1 = _mm_abs_epi16(dt1);
  const __m128i one = _mm_set1_epi16(1);

  for (int j = 0; j < 4; ++j) {
    const __m128i l = _mm_load_si128((const __m128i *)(left + j * 16));
    __m128i rep = _mm_set1_epi16((short)0x8000);
    for (int i = 0; i < 16; ++i) {
      const __m128i l16 = _mm_shuffle_epi8(l, rep);
      const __m128i dl = _mm_sub_epi16(l16, tl16);
      const __m128i pt = _mm_abs_epi16(dl);
      const __m128i row = paeth_16x1_pred_pre(l16, top0, top1, tl16, dt0, pl0,
                                              dt1, pl1, dl, pt);
      _mm_store_si128((__m128i *)dst, row);
      dst += stride;
      rep = _mm_add_epi16(rep, one);
    }
  }
}

void aom_paeth_predictor_32x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);

  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt_al = _mm_sub_epi16(al, tl16);
  const __m128i pl_al = _mm_abs_epi16(dt_al);
  const __m128i dt_ah = _mm_sub_epi16(ah, tl16);
  const __m128i pl_ah = _mm_abs_epi16(dt_ah);
  const __m128i dt_bl = _mm_sub_epi16(bl, tl16);
  const __m128i pl_bl = _mm_abs_epi16(dt_bl);
  const __m128i dt_bh = _mm_sub_epi16(bh, tl16);
  const __m128i pl_bh = _mm_abs_epi16(dt_bh);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);
  const __m128i l = _mm_loadl_epi64((const __m128i *)left);

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 1
#endif
  for (int i = 0; i < 8; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i dl = _mm_sub_epi16(l16, tl16);
    const __m128i pt = _mm_abs_epi16(dl);
    const __m128i r32l = paeth_16x1_pred_pre(l16, al, ah, tl16, dt_al, pl_al,
                                             dt_ah, pl_ah, dl, pt);
    const __m128i r32h = paeth_16x1_pred_pre(l16, bl, bh, tl16, dt_bl, pl_bl,
                                             dt_bh, pl_bh, dl, pt);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_paeth_predictor_32x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);

  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt_al = _mm_sub_epi16(al, tl16);
  const __m128i pl_al = _mm_abs_epi16(dt_al);
  const __m128i dt_ah = _mm_sub_epi16(ah, tl16);
  const __m128i pl_ah = _mm_abs_epi16(dt_ah);
  const __m128i dt_bl = _mm_sub_epi16(bl, tl16);
  const __m128i pl_bl = _mm_abs_epi16(dt_bl);
  const __m128i dt_bh = _mm_sub_epi16(bh, tl16);
  const __m128i pl_bh = _mm_abs_epi16(dt_bh);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);
  const __m128i l = _mm_load_si128((const __m128i *)left);

  for (int i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i dl = _mm_sub_epi16(l16, tl16);
    const __m128i pt = _mm_abs_epi16(dl);
    const __m128i r32l = paeth_16x1_pred_pre(l16, al, ah, tl16, dt_al, pl_al,
                                             dt_ah, pl_ah, dl, pt);
    const __m128i r32h = paeth_16x1_pred_pre(l16, bl, bh, tl16, dt_bl, pl_bl,
                                             dt_bh, pl_bh, dl, pt);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);

  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt_al = _mm_sub_epi16(al, tl16);
  const __m128i pl_al = _mm_abs_epi16(dt_al);
  const __m128i dt_ah = _mm_sub_epi16(ah, tl16);
  const __m128i pl_ah = _mm_abs_epi16(dt_ah);
  const __m128i dt_bl = _mm_sub_epi16(bl, tl16);
  const __m128i pl_bl = _mm_abs_epi16(dt_bl);
  const __m128i dt_bh = _mm_sub_epi16(bh, tl16);
  const __m128i pl_bh = _mm_abs_epi16(dt_bh);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l = _mm_load_si128((const __m128i *)left);

  for (int i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i dl = _mm_sub_epi16(l16, tl16);
    const __m128i pt = _mm_abs_epi16(dl);
    const __m128i r32l = paeth_16x1_pred_pre(l16, al, ah, tl16, dt_al, pl_al,
                                             dt_ah, pl_ah, dl, pt);
    const __m128i r32h = paeth_16x1_pred_pre(l16, bl, bh, tl16, dt_bl, pl_bl,
                                             dt_bh, pl_bh, dl, pt);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }

  rep = _mm_set1_epi16((short)0x8000);
  l = _mm_load_si128((const __m128i *)(left + 16));
  for (int i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i dl = _mm_sub_epi16(l16, tl16);
    const __m128i pt = _mm_abs_epi16(dl);
    const __m128i r32l = paeth_16x1_pred_pre(l16, al, ah, tl16, dt_al, pl_al,
                                             dt_ah, pl_ah, dl, pt);
    const __m128i r32h = paeth_16x1_pred_pre(l16, bl, bh, tl16, dt_bl, pl_bl,
                                             dt_bh, pl_bh, dl, pt);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_32x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);

  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt_al = _mm_sub_epi16(al, tl16);
  const __m128i pl_al = _mm_abs_epi16(dt_al);
  const __m128i dt_ah = _mm_sub_epi16(ah, tl16);
  const __m128i pl_ah = _mm_abs_epi16(dt_ah);
  const __m128i dt_bl = _mm_sub_epi16(bl, tl16);
  const __m128i pl_bl = _mm_abs_epi16(dt_bl);
  const __m128i dt_bh = _mm_sub_epi16(bh, tl16);
  const __m128i pl_bh = _mm_abs_epi16(dt_bh);
  const __m128i one = _mm_set1_epi16(1);

  for (int j = 0; j < 4; ++j) {
    const __m128i l = _mm_load_si128((const __m128i *)(left + j * 16));
    __m128i rep = _mm_set1_epi16((short)0x8000);
    for (int i = 0; i < 16; ++i) {
      const __m128i l16 = _mm_shuffle_epi8(l, rep);
      const __m128i dl = _mm_sub_epi16(l16, tl16);
      const __m128i pt = _mm_abs_epi16(dl);
      const __m128i r32l = paeth_16x1_pred_pre(l16, al, ah, tl16, dt_al, pl_al,
                                               dt_ah, pl_ah, dl, pt);
      const __m128i r32h = paeth_16x1_pred_pre(l16, bl, bh, tl16, dt_bl, pl_bl,
                                               dt_bh, pl_bh, dl, pt);

      _mm_store_si128((__m128i *)dst, r32l);
      _mm_store_si128((__m128i *)(dst + 16), r32h);
      dst += stride;
      rep = _mm_add_epi16(rep, one);
    }
  }
}

void aom_paeth_predictor_64x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i c = _mm_load_si128((const __m128i *)(above + 32));
  const __m128i d = _mm_load_si128((const __m128i *)(above + 48));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);
  const __m128i cl = _mm_unpacklo_epi8(c, zero);
  const __m128i ch = _mm_unpackhi_epi8(c, zero);
  const __m128i dl = _mm_unpacklo_epi8(d, zero);
  const __m128i dh = _mm_unpackhi_epi8(d, zero);

  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt_al = _mm_sub_epi16(al, tl16);
  const __m128i pl_al = _mm_abs_epi16(dt_al);
  const __m128i dt_ah = _mm_sub_epi16(ah, tl16);
  const __m128i pl_ah = _mm_abs_epi16(dt_ah);
  const __m128i dt_bl = _mm_sub_epi16(bl, tl16);
  const __m128i pl_bl = _mm_abs_epi16(dt_bl);
  const __m128i dt_bh = _mm_sub_epi16(bh, tl16);
  const __m128i pl_bh = _mm_abs_epi16(dt_bh);
  const __m128i dt_cl = _mm_sub_epi16(cl, tl16);
  const __m128i pl_cl = _mm_abs_epi16(dt_cl);
  const __m128i dt_ch = _mm_sub_epi16(ch, tl16);
  const __m128i pl_ch = _mm_abs_epi16(dt_ch);
  const __m128i dt_dl = _mm_sub_epi16(dl, tl16);
  const __m128i pl_dl = _mm_abs_epi16(dt_dl);
  const __m128i dt_dh = _mm_sub_epi16(dh, tl16);
  const __m128i pl_dh = _mm_abs_epi16(dt_dh);
  const __m128i one = _mm_set1_epi16(1);

  for (int j = 0; j < 2; ++j) {
    const __m128i l = _mm_load_si128((const __m128i *)(left + j * 16));
    __m128i rep = _mm_set1_epi16((short)0x8000);
    for (int i = 0; i < 16; ++i) {
      const __m128i l16 = _mm_shuffle_epi8(l, rep);
      const __m128i dl_row = _mm_sub_epi16(l16, tl16);
      const __m128i pt = _mm_abs_epi16(dl_row);
      const __m128i r0 = paeth_16x1_pred_pre(l16, al, ah, tl16, dt_al, pl_al,
                                             dt_ah, pl_ah, dl_row, pt);
      const __m128i r1 = paeth_16x1_pred_pre(l16, bl, bh, tl16, dt_bl, pl_bl,
                                             dt_bh, pl_bh, dl_row, pt);
      const __m128i r2 = paeth_16x1_pred_pre(l16, cl, ch, tl16, dt_cl, pl_cl,
                                             dt_ch, pl_ch, dl_row, pt);
      const __m128i r3 = paeth_16x1_pred_pre(l16, dl, dh, tl16, dt_dl, pl_dl,
                                             dt_dh, pl_dh, dl_row, pt);

      _mm_store_si128((__m128i *)dst, r0);
      _mm_store_si128((__m128i *)(dst + 16), r1);
      _mm_store_si128((__m128i *)(dst + 32), r2);
      _mm_store_si128((__m128i *)(dst + 48), r3);
      dst += stride;
      rep = _mm_add_epi16(rep, one);
    }
  }
}

void aom_paeth_predictor_64x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i c = _mm_load_si128((const __m128i *)(above + 32));
  const __m128i d = _mm_load_si128((const __m128i *)(above + 48));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);
  const __m128i cl = _mm_unpacklo_epi8(c, zero);
  const __m128i ch = _mm_unpackhi_epi8(c, zero);
  const __m128i dl = _mm_unpacklo_epi8(d, zero);
  const __m128i dh = _mm_unpackhi_epi8(d, zero);

  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt_al = _mm_sub_epi16(al, tl16);
  const __m128i pl_al = _mm_abs_epi16(dt_al);
  const __m128i dt_ah = _mm_sub_epi16(ah, tl16);
  const __m128i pl_ah = _mm_abs_epi16(dt_ah);
  const __m128i dt_bl = _mm_sub_epi16(bl, tl16);
  const __m128i pl_bl = _mm_abs_epi16(dt_bl);
  const __m128i dt_bh = _mm_sub_epi16(bh, tl16);
  const __m128i pl_bh = _mm_abs_epi16(dt_bh);
  const __m128i dt_cl = _mm_sub_epi16(cl, tl16);
  const __m128i pl_cl = _mm_abs_epi16(dt_cl);
  const __m128i dt_ch = _mm_sub_epi16(ch, tl16);
  const __m128i pl_ch = _mm_abs_epi16(dt_ch);
  const __m128i dt_dl = _mm_sub_epi16(dl, tl16);
  const __m128i pl_dl = _mm_abs_epi16(dt_dl);
  const __m128i dt_dh = _mm_sub_epi16(dh, tl16);
  const __m128i pl_dh = _mm_abs_epi16(dt_dh);
  const __m128i one = _mm_set1_epi16(1);

  for (int j = 0; j < 4; ++j) {
    const __m128i l = _mm_load_si128((const __m128i *)(left + j * 16));
    __m128i rep = _mm_set1_epi16((short)0x8000);
    for (int i = 0; i < 16; ++i) {
      const __m128i l16 = _mm_shuffle_epi8(l, rep);
      const __m128i dl_row = _mm_sub_epi16(l16, tl16);
      const __m128i pt = _mm_abs_epi16(dl_row);
      const __m128i r0 = paeth_16x1_pred_pre(l16, al, ah, tl16, dt_al, pl_al,
                                             dt_ah, pl_ah, dl_row, pt);
      const __m128i r1 = paeth_16x1_pred_pre(l16, bl, bh, tl16, dt_bl, pl_bl,
                                             dt_bh, pl_bh, dl_row, pt);
      const __m128i r2 = paeth_16x1_pred_pre(l16, cl, ch, tl16, dt_cl, pl_cl,
                                             dt_ch, pl_ch, dl_row, pt);
      const __m128i r3 = paeth_16x1_pred_pre(l16, dl, dh, tl16, dt_dl, pl_dl,
                                             dt_dh, pl_dh, dl_row, pt);

      _mm_store_si128((__m128i *)dst, r0);
      _mm_store_si128((__m128i *)(dst + 16), r1);
      _mm_store_si128((__m128i *)(dst + 32), r2);
      _mm_store_si128((__m128i *)(dst + 48), r3);
      dst += stride;
      rep = _mm_add_epi16(rep, one);
    }
  }
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_paeth_predictor_64x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i c = _mm_load_si128((const __m128i *)(above + 32));
  const __m128i d = _mm_load_si128((const __m128i *)(above + 48));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);
  const __m128i cl = _mm_unpacklo_epi8(c, zero);
  const __m128i ch = _mm_unpackhi_epi8(c, zero);
  const __m128i dl = _mm_unpacklo_epi8(d, zero);
  const __m128i dh = _mm_unpackhi_epi8(d, zero);

  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i dt_al = _mm_sub_epi16(al, tl16);
  const __m128i pl_al = _mm_abs_epi16(dt_al);
  const __m128i dt_ah = _mm_sub_epi16(ah, tl16);
  const __m128i pl_ah = _mm_abs_epi16(dt_ah);
  const __m128i dt_bl = _mm_sub_epi16(bl, tl16);
  const __m128i pl_bl = _mm_abs_epi16(dt_bl);
  const __m128i dt_bh = _mm_sub_epi16(bh, tl16);
  const __m128i pl_bh = _mm_abs_epi16(dt_bh);
  const __m128i dt_cl = _mm_sub_epi16(cl, tl16);
  const __m128i pl_cl = _mm_abs_epi16(dt_cl);
  const __m128i dt_ch = _mm_sub_epi16(ch, tl16);
  const __m128i pl_ch = _mm_abs_epi16(dt_ch);
  const __m128i dt_dl = _mm_sub_epi16(dl, tl16);
  const __m128i pl_dl = _mm_abs_epi16(dt_dl);
  const __m128i dt_dh = _mm_sub_epi16(dh, tl16);
  const __m128i pl_dh = _mm_abs_epi16(dt_dh);
  const __m128i one = _mm_set1_epi16(1);

  const __m128i l = _mm_load_si128((const __m128i *)left);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  for (int i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i dl_row = _mm_sub_epi16(l16, tl16);
    const __m128i pt = _mm_abs_epi16(dl_row);
    const __m128i r0 = paeth_16x1_pred_pre(l16, al, ah, tl16, dt_al, pl_al,
                                           dt_ah, pl_ah, dl_row, pt);
    const __m128i r1 = paeth_16x1_pred_pre(l16, bl, bh, tl16, dt_bl, pl_bl,
                                           dt_bh, pl_bh, dl_row, pt);
    const __m128i r2 = paeth_16x1_pred_pre(l16, cl, ch, tl16, dt_cl, pl_cl,
                                           dt_ch, pl_ch, dl_row, pt);
    const __m128i r3 = paeth_16x1_pred_pre(l16, dl, dh, tl16, dt_dl, pl_dl,
                                           dt_dh, pl_dh, dl_row, pt);

    _mm_store_si128((__m128i *)dst, r0);
    _mm_store_si128((__m128i *)(dst + 16), r1);
    _mm_store_si128((__m128i *)(dst + 32), r2);
    _mm_store_si128((__m128i *)(dst + 48), r3);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

// -----------------------------------------------------------------------------
// SMOOTH_PRED

// TODO(slavarnway): Visual Studio only supports restrict when /std:c11
// (available in 2019+) or greater is specified; __restrict can be used in that
// case. This should be moved to rtcd and used consistently between the
// function declarations and definitions to avoid warnings in Visual Studio
// when defining LIBAOM_RESTRICT to restrict or __restrict.
#if defined(_MSC_VER)
#define LIBAOM_RESTRICT
#else
#define LIBAOM_RESTRICT restrict
#endif

static AOM_FORCE_INLINE __m128i Load4(const void *src) {
  int val;
  memcpy(&val, src, sizeof(val));
  return _mm_cvtsi32_si128(val);
}

static AOM_FORCE_INLINE __m128i LoadLo8(const void *a) {
  return _mm_loadl_epi64((const __m128i *)(a));
}

static AOM_FORCE_INLINE __m128i LoadUnaligned16(const void *a) {
  return _mm_loadu_si128((const __m128i *)(a));
}

static AOM_FORCE_INLINE void Store4(void *dst, const __m128i x) {
  const int val = _mm_cvtsi128_si32(x);
  memcpy(dst, &val, sizeof(val));
}

static AOM_FORCE_INLINE void StoreLo8(void *a, const __m128i v) {
  _mm_storel_epi64((__m128i *)(a), v);
}

static AOM_FORCE_INLINE void StoreUnaligned16(void *a, const __m128i v) {
  _mm_storeu_si128((__m128i *)(a), v);
}

static AOM_FORCE_INLINE __m128i cvtepu8_epi16(__m128i x) {
  return _mm_unpacklo_epi8((x), _mm_setzero_si128());
}

static AOM_FORCE_INLINE void smooth_pred_4xh_2rows(
    const __m128i tdiff, const __m128i wx, const __m128i bl_base,
    const __m128i tr_base, const __m128i wh16, const __m128i ldiff16,
    int h_pairs, uint8_t *dst, ptrdiff_t stride) {
  __m128i rep = _mm_set_epi32(0x03020302, 0x03020302, 0x01000100, 0x01000100);
  const __m128i inc = _mm_set1_epi16(0x0404);
  for (int i = 0; i < h_pairs; ++i) {
    const __m128i wy = _mm_shuffle_epi8(wh16, rep);
    const __m128i ld = _mm_shuffle_epi8(ldiff16, rep);
    const __m128i v = _mm_add_epi16(_mm_mullo_epi16(tdiff, wy), bl_base);
    const __m128i h = _mm_add_epi16(_mm_mullo_epi16(wx, ld), tr_base);
    const __m128i p = _mm_srli_epi16(_mm_avg_epu16(v, h), 8);
    const __m128i packed = _mm_packus_epi16(p, p);
    Store4(dst, packed);
    Store4(dst + stride, _mm_srli_si128(packed, 4));
    dst += stride << 1;
    rep = _mm_add_epi16(rep, inc);
  }
}

void aom_smooth_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left[3];
  const int tr = above[3];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 127));
  const __m128i t4 = Load4(above);
  const __m128i tdiff =
      _mm_sub_epi16(_mm_unpacklo_epi8(_mm_unpacklo_epi32(t4, t4), zero), bl16);
  const __m128i w4 = Load4(smooth_weights);
  const __m128i wx = _mm_unpacklo_epi8(_mm_unpacklo_epi32(w4, w4), zero);
  const __m128i ldiff16 =
      _mm_sub_epi16(_mm_unpacklo_epi8(Load4(left), zero), tr16);
  smooth_pred_4xh_2rows(tdiff, wx, bl_base, tr_base, wx, ldiff16, 2, dst,
                        stride);
}

void aom_smooth_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left[7];
  const int tr = above[3];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 127));
  const __m128i t4 = Load4(above);
  const __m128i tdiff =
      _mm_sub_epi16(_mm_unpacklo_epi8(_mm_unpacklo_epi32(t4, t4), zero), bl16);
  const __m128i w4 = Load4(smooth_weights);
  const __m128i wx = _mm_unpacklo_epi8(_mm_unpacklo_epi32(w4, w4), zero);
  const __m128i wh16 = _mm_unpacklo_epi8(LoadLo8(smooth_weights + 4), zero);
  const __m128i ldiff16 =
      _mm_sub_epi16(_mm_unpacklo_epi8(LoadLo8(left), zero), tr16);
  smooth_pred_4xh_2rows(tdiff, wx, bl_base, tr_base, wh16, ldiff16, 4, dst,
                        stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_smooth_predictor_4x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left[15];
  const int tr = above[3];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 127));
  const __m128i t4 = Load4(above);
  const __m128i tdiff =
      _mm_sub_epi16(_mm_unpacklo_epi8(_mm_unpacklo_epi32(t4, t4), zero), bl16);
  const __m128i w4 = Load4(smooth_weights);
  const __m128i wx = _mm_unpacklo_epi8(_mm_unpacklo_epi32(w4, w4), zero);
  const __m128i wh = LoadUnaligned16(smooth_weights + 12);
  const __m128i l = LoadUnaligned16(left);
  smooth_pred_4xh_2rows(
      tdiff, wx, bl_base, tr_base, _mm_unpacklo_epi8(wh, zero),
      _mm_sub_epi16(_mm_unpacklo_epi8(l, zero), tr16), 4, dst, stride);
  dst += stride << 3;
  smooth_pred_4xh_2rows(
      tdiff, wx, bl_base, tr_base, _mm_unpackhi_epi8(wh, zero),
      _mm_sub_epi16(_mm_unpackhi_epi8(l, zero), tr16), 4, dst, stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

static AOM_FORCE_INLINE void smooth_pred_8xh_2rows(
    const __m128i tdiff, const __m128i wx, const __m128i bl_base,
    const __m128i tr_base, const __m128i wh16, const __m128i ldiff16,
    int h_pairs, uint8_t *dst, ptrdiff_t stride) {
  __m128i rep0 = _mm_set1_epi16(0x0100);
  __m128i rep1 = _mm_set1_epi16(0x0302);
  const __m128i inc = _mm_set1_epi16(0x0404);
  for (int i = 0; i < h_pairs; ++i) {
    const __m128i wy0 = _mm_shuffle_epi8(wh16, rep0);
    const __m128i ld0 = _mm_shuffle_epi8(ldiff16, rep0);
    const __m128i v0 = _mm_add_epi16(_mm_mullo_epi16(tdiff, wy0), bl_base);
    const __m128i h0 = _mm_add_epi16(_mm_mullo_epi16(wx, ld0), tr_base);
    const __m128i p0 = _mm_srli_epi16(_mm_avg_epu16(v0, h0), 8);

    const __m128i wy1 = _mm_shuffle_epi8(wh16, rep1);
    const __m128i ld1 = _mm_shuffle_epi8(ldiff16, rep1);
    const __m128i v1 = _mm_add_epi16(_mm_mullo_epi16(tdiff, wy1), bl_base);
    const __m128i h1 = _mm_add_epi16(_mm_mullo_epi16(wx, ld1), tr_base);
    const __m128i p1 = _mm_srli_epi16(_mm_avg_epu16(v1, h1), 8);

    const __m128i packed = _mm_packus_epi16(p0, p1);
    StoreLo8(dst, packed);
    StoreLo8(dst + stride, _mm_srli_si128(packed, 8));
    dst += stride << 1;
    rep0 = _mm_add_epi16(rep0, inc);
    rep1 = _mm_add_epi16(rep1, inc);
  }
}

void aom_smooth_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left[3];
  const int tr = above[7];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 127));
  const __m128i tdiff =
      _mm_sub_epi16(_mm_unpacklo_epi8(LoadLo8(above), zero), bl16);
  const __m128i wx = _mm_unpacklo_epi8(LoadLo8(smooth_weights + 4), zero);
  const __m128i wh16 = _mm_unpacklo_epi8(Load4(smooth_weights), zero);
  const __m128i ldiff16 =
      _mm_sub_epi16(_mm_unpacklo_epi8(Load4(left), zero), tr16);
  smooth_pred_8xh_2rows(tdiff, wx, bl_base, tr_base, wh16, ldiff16, 2, dst,
                        stride);
}

void aom_smooth_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left[7];
  const int tr = above[7];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 127));
  const __m128i tdiff =
      _mm_sub_epi16(_mm_unpacklo_epi8(LoadLo8(above), zero), bl16);
  const __m128i wx = _mm_unpacklo_epi8(LoadLo8(smooth_weights + 4), zero);
  const __m128i ldiff16 =
      _mm_sub_epi16(_mm_unpacklo_epi8(LoadLo8(left), zero), tr16);
  smooth_pred_8xh_2rows(tdiff, wx, bl_base, tr_base, wx, ldiff16, 4, dst,
                        stride);
}

void aom_smooth_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left[15];
  const int tr = above[7];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 127));
  const __m128i tdiff =
      _mm_sub_epi16(_mm_unpacklo_epi8(LoadLo8(above), zero), bl16);
  const __m128i wx = _mm_unpacklo_epi8(LoadLo8(smooth_weights + 4), zero);
  const __m128i wh = LoadUnaligned16(smooth_weights + 12);
  const __m128i l = LoadUnaligned16(left);
  smooth_pred_8xh_2rows(
      tdiff, wx, bl_base, tr_base, _mm_unpacklo_epi8(wh, zero),
      _mm_sub_epi16(_mm_unpacklo_epi8(l, zero), tr16), 4, dst, stride);
  dst += stride << 3;
  smooth_pred_8xh_2rows(
      tdiff, wx, bl_base, tr_base, _mm_unpackhi_epi8(wh, zero),
      _mm_sub_epi16(_mm_unpackhi_epi8(l, zero), tr16), 4, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_smooth_predictor_8x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left[31];
  const int tr = above[7];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 127));
  const __m128i tdiff =
      _mm_sub_epi16(_mm_unpacklo_epi8(LoadLo8(above), zero), bl16);
  const __m128i wx = _mm_unpacklo_epi8(LoadLo8(smooth_weights + 4), zero);
  for (int j = 0; j < 2; ++j) {
    const __m128i wh = LoadUnaligned16(smooth_weights + 28 + j * 16);
    const __m128i l = LoadUnaligned16(left + j * 16);
    smooth_pred_8xh_2rows(
        tdiff, wx, bl_base, tr_base, _mm_unpacklo_epi8(wh, zero),
        _mm_sub_epi16(_mm_unpacklo_epi8(l, zero), tr16), 4, dst, stride);
    dst += stride << 3;
    smooth_pred_8xh_2rows(
        tdiff, wx, bl_base, tr_base, _mm_unpackhi_epi8(wh, zero),
        _mm_sub_epi16(_mm_unpackhi_epi8(l, zero), tr16), 4, dst, stride);
    dst += stride << 3;
  }
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

static AOM_FORCE_INLINE void smooth_predictor_16xh(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column, int height) {
  const uint8_t *const sm_weights_h = smooth_weights + height - 4;
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[height - 1];
  const int tr = top_row[15];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 127));
  const __m128i top = LoadUnaligned16(top_row);
  const __m128i tdiff0 = _mm_sub_epi16(_mm_unpacklo_epi8(top, zero), bl16);
  const __m128i tdiff1 = _mm_sub_epi16(_mm_unpackhi_epi8(top, zero), bl16);
  const __m128i w = LoadUnaligned16(smooth_weights + 12);
  const __m128i wx0 = _mm_unpacklo_epi8(w, zero);
  const __m128i wx1 = _mm_unpackhi_epi8(w, zero);

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 1
#endif
  for (int y = 0; y < height; ++y) {
    const __m128i wy = _mm_set1_epi16((int16_t)sm_weights_h[y]);
    const __m128i ld = _mm_set1_epi16((int16_t)((int)left_column[y] - tr));
    const __m128i v0 = _mm_add_epi16(_mm_mullo_epi16(tdiff0, wy), bl_base);
    const __m128i v1 = _mm_add_epi16(_mm_mullo_epi16(tdiff1, wy), bl_base);
    const __m128i h0 = _mm_add_epi16(_mm_mullo_epi16(wx0, ld), tr_base);
    const __m128i h1 = _mm_add_epi16(_mm_mullo_epi16(wx1, ld), tr_base);
    const __m128i p0 = _mm_srli_epi16(_mm_avg_epu16(v0, h0), 8);
    const __m128i p1 = _mm_srli_epi16(_mm_avg_epu16(v1, h1), 8);
    StoreUnaligned16(dst, _mm_packus_epi16(p0, p1));
    dst += stride;
  }
}

static AOM_FORCE_INLINE void smooth_predictor_32xh_strip(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_strip,
    const uint8_t *LIBAOM_RESTRICT wx_strip,
    const uint8_t *LIBAOM_RESTRICT sm_weights_h,
    const uint8_t *LIBAOM_RESTRICT left_column, int bl, int tr, int height) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 127));
  const __m128i top_lo = LoadUnaligned16(top_strip);
  const __m128i top_hi = LoadUnaligned16(top_strip + 16);
  const __m128i tdiff0 = _mm_sub_epi16(_mm_unpacklo_epi8(top_lo, zero), bl16);
  const __m128i tdiff1 = _mm_sub_epi16(_mm_unpackhi_epi8(top_lo, zero), bl16);
  const __m128i tdiff2 = _mm_sub_epi16(_mm_unpacklo_epi8(top_hi, zero), bl16);
  const __m128i tdiff3 = _mm_sub_epi16(_mm_unpackhi_epi8(top_hi, zero), bl16);
  const __m128i w_lo = LoadUnaligned16(wx_strip);
  const __m128i w_hi = LoadUnaligned16(wx_strip + 16);
  const __m128i wx0 = _mm_unpacklo_epi8(w_lo, zero);
  const __m128i wx1 = _mm_unpackhi_epi8(w_lo, zero);
  const __m128i wx2 = _mm_unpacklo_epi8(w_hi, zero);
  const __m128i wx3 = _mm_unpackhi_epi8(w_hi, zero);

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 1
#endif
  for (int y = 0; y < height; ++y) {
    const __m128i wy = _mm_set1_epi16((int16_t)sm_weights_h[y]);
    const __m128i ld = _mm_set1_epi16((int16_t)((int)left_column[y] - tr));
    const __m128i v0 = _mm_add_epi16(_mm_mullo_epi16(tdiff0, wy), bl_base);
    const __m128i v1 = _mm_add_epi16(_mm_mullo_epi16(tdiff1, wy), bl_base);
    const __m128i h0 = _mm_add_epi16(_mm_mullo_epi16(wx0, ld), tr_base);
    const __m128i h1 = _mm_add_epi16(_mm_mullo_epi16(wx1, ld), tr_base);
    const __m128i p0 = _mm_srli_epi16(_mm_avg_epu16(v0, h0), 8);
    const __m128i p1 = _mm_srli_epi16(_mm_avg_epu16(v1, h1), 8);
    StoreUnaligned16(dst, _mm_packus_epi16(p0, p1));

    const __m128i v2 = _mm_add_epi16(_mm_mullo_epi16(tdiff2, wy), bl_base);
    const __m128i v3 = _mm_add_epi16(_mm_mullo_epi16(tdiff3, wy), bl_base);
    const __m128i h2 = _mm_add_epi16(_mm_mullo_epi16(wx2, ld), tr_base);
    const __m128i h3 = _mm_add_epi16(_mm_mullo_epi16(wx3, ld), tr_base);
    const __m128i p2 = _mm_srli_epi16(_mm_avg_epu16(v2, h2), 8);
    const __m128i p3 = _mm_srli_epi16(_mm_avg_epu16(v3, h3), 8);
    StoreUnaligned16(dst + 16, _mm_packus_epi16(p2, p3));
    dst += stride;
  }
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_smooth_predictor_16x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  smooth_predictor_16xh(dst, stride, above, left, 4);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_smooth_predictor_16x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  smooth_predictor_16xh(dst, stride, above, left, 8);
}

void aom_smooth_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_16xh(dst, stride, above, left, 16);
}

void aom_smooth_predictor_16x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_16xh(dst, stride, above, left, 32);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_smooth_predictor_16x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_16xh(dst, stride, above, left, 64);
}

void aom_smooth_predictor_32x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  smooth_predictor_32xh_strip(dst, stride, above, smooth_weights + 28,
                              smooth_weights + 4, left, left[7], above[31], 8);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_smooth_predictor_32x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_32xh_strip(dst, stride, above, smooth_weights + 28,
                              smooth_weights + 12, left, left[15], above[31],
                              16);
}

void aom_smooth_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_32xh_strip(dst, stride, above, smooth_weights + 28,
                              smooth_weights + 28, left, left[31], above[31],
                              32);
}

void aom_smooth_predictor_32x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_32xh_strip(dst, stride, above, smooth_weights + 28,
                              smooth_weights + 60, left, left[63], above[31],
                              64);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_smooth_predictor_64x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_32xh_strip(dst, stride, above, smooth_weights + 60,
                              smooth_weights + 12, left, left[15], above[63],
                              16);
  smooth_predictor_32xh_strip(dst + 32, stride, above + 32, smooth_weights + 92,
                              smooth_weights + 12, left, left[15], above[63],
                              16);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_smooth_predictor_64x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_32xh_strip(dst, stride, above, smooth_weights + 60,
                              smooth_weights + 28, left, left[31], above[63],
                              32);
  smooth_predictor_32xh_strip(dst + 32, stride, above + 32, smooth_weights + 92,
                              smooth_weights + 28, left, left[31], above[63],
                              32);
}

void aom_smooth_predictor_64x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_32xh_strip(dst, stride, above, smooth_weights + 60,
                              smooth_weights + 60, left, left[63], above[63],
                              64);
  smooth_predictor_32xh_strip(dst + 32, stride, above + 32, smooth_weights + 92,
                              smooth_weights + 60, left, left[63], above[63],
                              64);
}

// -----------------------------------------------------------------------------
// Smooth horizontal/vertical helper functions.

static AOM_FORCE_INLINE void write_smooth_directional_diff16(
    uint8_t *LIBAOM_RESTRICT dst, const __m128i diff1, const __m128i diff2,
    const __m128i weights1, const __m128i weights2, const __m128i base) {
  const __m128i pred1 =
      _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(diff1, weights1), base), 8);
  const __m128i pred2 =
      _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(diff2, weights2), base), 8);
  StoreUnaligned16(dst, _mm_packus_epi16(pred1, pred2));
}

// -----------------------------------------------------------------------------
// SMOOTH_V_PRED

static AOM_FORCE_INLINE void smooth_v_4xh_2rows(const __m128i tdiff,
                                                const __m128i bl_base,
                                                const __m128i wh16, int h_pairs,
                                                uint8_t *LIBAOM_RESTRICT dst,
                                                ptrdiff_t stride) {
  __m128i rep = _mm_set_epi32(0x03020302, 0x03020302, 0x01000100, 0x01000100);
  const __m128i inc = _mm_set1_epi16(0x0404);
  for (int i = 0; i < h_pairs; ++i) {
    const __m128i wy = _mm_shuffle_epi8(wh16, rep);
    const __m128i p =
        _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(tdiff, wy), bl_base), 8);
    const __m128i packed = _mm_packus_epi16(p, p);
    Store4(dst, packed);
    Store4(dst + stride, _mm_srli_si128(packed, 4));
    dst += stride << 1;
    rep = _mm_add_epi16(rep, inc);
  }
}

void aom_smooth_v_predictor_4x4_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[3];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i t4 = Load4(top_row);
  const __m128i tdiff =
      _mm_sub_epi16(_mm_unpacklo_epi8(_mm_unpacklo_epi32(t4, t4), zero), bl16);
  const __m128i wh16 = _mm_unpacklo_epi8(Load4(smooth_weights), zero);
  smooth_v_4xh_2rows(tdiff, bl_base, wh16, 2, dst, stride);
}

void aom_smooth_v_predictor_4x8_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[7];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i t4 = Load4(top_row);
  const __m128i tdiff =
      _mm_sub_epi16(_mm_unpacklo_epi8(_mm_unpacklo_epi32(t4, t4), zero), bl16);
  const __m128i wh16 = _mm_unpacklo_epi8(LoadLo8(smooth_weights + 4), zero);
  smooth_v_4xh_2rows(tdiff, bl_base, wh16, 4, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_smooth_v_predictor_4x16_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[15];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i t4 = Load4(top_row);
  const __m128i tdiff =
      _mm_sub_epi16(_mm_unpacklo_epi8(_mm_unpacklo_epi32(t4, t4), zero), bl16);
  const __m128i wh = LoadUnaligned16(smooth_weights + 12);
  smooth_v_4xh_2rows(tdiff, bl_base, _mm_unpacklo_epi8(wh, zero), 4, dst,
                     stride);
  dst += stride << 3;
  smooth_v_4xh_2rows(tdiff, bl_base, _mm_unpackhi_epi8(wh, zero), 4, dst,
                     stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

static AOM_FORCE_INLINE void smooth_v_8xh_2rows(const __m128i tdiff,
                                                const __m128i bl_base,
                                                const __m128i wh16, int h_pairs,
                                                uint8_t *LIBAOM_RESTRICT dst,
                                                ptrdiff_t stride) {
  __m128i rep0 = _mm_set1_epi16(0x0100);
  __m128i rep1 = _mm_set1_epi16(0x0302);
  const __m128i inc = _mm_set1_epi16(0x0404);
  for (int i = 0; i < h_pairs; ++i) {
    const __m128i wy0 = _mm_shuffle_epi8(wh16, rep0);
    const __m128i p0 =
        _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(tdiff, wy0), bl_base), 8);
    const __m128i wy1 = _mm_shuffle_epi8(wh16, rep1);
    const __m128i p1 =
        _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(tdiff, wy1), bl_base), 8);
    const __m128i packed = _mm_packus_epi16(p0, p1);
    StoreLo8(dst, packed);
    StoreLo8(dst + stride, _mm_srli_si128(packed, 8));
    dst += stride << 1;
    rep0 = _mm_add_epi16(rep0, inc);
    rep1 = _mm_add_epi16(rep1, inc);
  }
}

void aom_smooth_v_predictor_8x4_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const int bl = left_column[3];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i tdiff = _mm_sub_epi16(cvtepu8_epi16(LoadLo8(top_row)), bl16);
  const __m128i wh16 = cvtepu8_epi16(Load4(smooth_weights));
  smooth_v_8xh_2rows(tdiff, bl_base, wh16, 2, dst, stride);
}

void aom_smooth_v_predictor_8x8_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const int bl = left_column[7];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i tdiff = _mm_sub_epi16(cvtepu8_epi16(LoadLo8(top_row)), bl16);
  const __m128i wh16 = cvtepu8_epi16(LoadLo8(smooth_weights + 4));
  smooth_v_8xh_2rows(tdiff, bl_base, wh16, 4, dst, stride);
}

void aom_smooth_v_predictor_8x16_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[15];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i tdiff = _mm_sub_epi16(cvtepu8_epi16(LoadLo8(top_row)), bl16);
  const __m128i weights = LoadUnaligned16(smooth_weights + 12);
  smooth_v_8xh_2rows(tdiff, bl_base, _mm_unpacklo_epi8(weights, zero), 4, dst,
                     stride);
  dst += stride << 3;
  smooth_v_8xh_2rows(tdiff, bl_base, _mm_unpackhi_epi8(weights, zero), 4, dst,
                     stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_smooth_v_predictor_8x32_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[31];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i tdiff = _mm_sub_epi16(cvtepu8_epi16(LoadLo8(top_row)), bl16);
  for (int j = 0; j < 2; ++j) {
    const __m128i weights = LoadUnaligned16(smooth_weights + 28 + j * 16);
    smooth_v_8xh_2rows(tdiff, bl_base, _mm_unpacklo_epi8(weights, zero), 4, dst,
                       stride);
    dst += stride << 3;
    smooth_v_8xh_2rows(tdiff, bl_base, _mm_unpackhi_epi8(weights, zero), 4, dst,
                       stride);
    dst += stride << 3;
  }
}

void aom_smooth_v_predictor_16x4_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[3];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i weights = cvtepu8_epi16(Load4(smooth_weights));
  const __m128i top = LoadUnaligned16(top_row);
  const __m128i tdiff_lo = _mm_sub_epi16(_mm_unpacklo_epi8(top, zero), bl16);
  const __m128i tdiff_hi = _mm_sub_epi16(_mm_unpackhi_epi8(top, zero), bl16);

  for (int y_mask = 0x01000100; y_mask < 0x08000000; y_mask += 0x02020202) {
    const __m128i weights_y = _mm_shuffle_epi8(weights, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, tdiff_lo, tdiff_hi, weights_y,
                                    weights_y, bl_base);
    dst += stride;
  }
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_smooth_v_predictor_16x8_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[7];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i weights = cvtepu8_epi16(LoadLo8(smooth_weights + 4));
  const __m128i top = LoadUnaligned16(top_row);
  const __m128i tdiff_lo = _mm_sub_epi16(_mm_unpacklo_epi8(top, zero), bl16);
  const __m128i tdiff_hi = _mm_sub_epi16(_mm_unpackhi_epi8(top, zero), bl16);
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i weights_y = _mm_shuffle_epi8(weights, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, tdiff_lo, tdiff_hi, weights_y,
                                    weights_y, bl_base);
    dst += stride;
  }
}

void aom_smooth_v_predictor_16x16_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[15];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i weights = LoadUnaligned16(smooth_weights + 12);
  const __m128i weights_lo = cvtepu8_epi16(weights);
  const __m128i weights_hi = _mm_unpackhi_epi8(weights, zero);
  const __m128i top = LoadUnaligned16(top_row);
  const __m128i tdiff_lo = _mm_sub_epi16(_mm_unpacklo_epi8(top, zero), bl16);
  const __m128i tdiff_hi = _mm_sub_epi16(_mm_unpackhi_epi8(top, zero), bl16);
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i weights_y =
        _mm_shuffle_epi8(weights_lo, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, tdiff_lo, tdiff_hi, weights_y,
                                    weights_y, bl_base);
    dst += stride;
  }
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i weights_y =
        _mm_shuffle_epi8(weights_hi, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, tdiff_lo, tdiff_hi, weights_y,
                                    weights_y, bl_base);
    dst += stride;
  }
}

void aom_smooth_v_predictor_16x32_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[31];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i top = LoadUnaligned16(top_row);
  const __m128i tdiff_lo = _mm_sub_epi16(_mm_unpacklo_epi8(top, zero), bl16);
  const __m128i tdiff_hi = _mm_sub_epi16(_mm_unpackhi_epi8(top, zero), bl16);
  for (int j = 0; j < 2; ++j) {
    const __m128i weights = LoadUnaligned16(smooth_weights + 28 + j * 16);
    const __m128i weights_lo = _mm_unpacklo_epi8(weights, zero);
    const __m128i weights_hi = _mm_unpackhi_epi8(weights, zero);
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i weights_y =
          _mm_shuffle_epi8(weights_lo, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, tdiff_lo, tdiff_hi, weights_y,
                                      weights_y, bl_base);
      dst += stride;
    }
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i weights_y =
          _mm_shuffle_epi8(weights_hi, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, tdiff_lo, tdiff_hi, weights_y,
                                      weights_y, bl_base);
      dst += stride;
    }
  }
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_smooth_v_predictor_16x64_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[63];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i top = LoadUnaligned16(top_row);
  const __m128i tdiff_lo = _mm_sub_epi16(_mm_unpacklo_epi8(top, zero), bl16);
  const __m128i tdiff_hi = _mm_sub_epi16(_mm_unpackhi_epi8(top, zero), bl16);
  const uint8_t *weights_base_ptr = smooth_weights + 60;
  for (int left_offset = 0; left_offset < 64; left_offset += 16) {
    const __m128i weights = LoadUnaligned16(weights_base_ptr + left_offset);
    const __m128i weights_lo = cvtepu8_epi16(weights);
    const __m128i weights_hi = _mm_unpackhi_epi8(weights, zero);
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i weights_y =
          _mm_shuffle_epi8(weights_lo, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, tdiff_lo, tdiff_hi, weights_y,
                                      weights_y, bl_base);
      dst += stride;
    }
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i weights_y =
          _mm_shuffle_epi8(weights_hi, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, tdiff_lo, tdiff_hi, weights_y,
                                      weights_y, bl_base);
      dst += stride;
    }
  }
}

void aom_smooth_v_predictor_32x8_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[7];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i top_lo = LoadUnaligned16(top_row);
  const __m128i top_hi = LoadUnaligned16(top_row + 16);
  const __m128i tdiff1 = _mm_sub_epi16(cvtepu8_epi16(top_lo), bl16);
  const __m128i tdiff2 = _mm_sub_epi16(_mm_unpackhi_epi8(top_lo, zero), bl16);
  const __m128i tdiff3 = _mm_sub_epi16(cvtepu8_epi16(top_hi), bl16);
  const __m128i tdiff4 = _mm_sub_epi16(_mm_unpackhi_epi8(top_hi, zero), bl16);
  const __m128i weights = cvtepu8_epi16(LoadLo8(smooth_weights + 4));
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i weights_y = _mm_shuffle_epi8(weights, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, tdiff1, tdiff2, weights_y, weights_y,
                                    bl_base);
    write_smooth_directional_diff16(dst + 16, tdiff3, tdiff4, weights_y,
                                    weights_y, bl_base);
    dst += stride;
  }
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_smooth_v_predictor_32x16_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[15];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i top_lo = LoadUnaligned16(top_row);
  const __m128i top_hi = LoadUnaligned16(top_row + 16);
  const __m128i tdiff1 = _mm_sub_epi16(cvtepu8_epi16(top_lo), bl16);
  const __m128i tdiff2 = _mm_sub_epi16(_mm_unpackhi_epi8(top_lo, zero), bl16);
  const __m128i tdiff3 = _mm_sub_epi16(cvtepu8_epi16(top_hi), bl16);
  const __m128i tdiff4 = _mm_sub_epi16(_mm_unpackhi_epi8(top_hi, zero), bl16);
  const __m128i weights = LoadUnaligned16(smooth_weights + 12);
  const __m128i weights1 = cvtepu8_epi16(weights);
  const __m128i weights2 = _mm_unpackhi_epi8(weights, zero);
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i weights_y =
        _mm_shuffle_epi8(weights1, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, tdiff1, tdiff2, weights_y, weights_y,
                                    bl_base);
    write_smooth_directional_diff16(dst + 16, tdiff3, tdiff4, weights_y,
                                    weights_y, bl_base);
    dst += stride;
  }
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i weights_y =
        _mm_shuffle_epi8(weights2, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, tdiff1, tdiff2, weights_y, weights_y,
                                    bl_base);
    write_smooth_directional_diff16(dst + 16, tdiff3, tdiff4, weights_y,
                                    weights_y, bl_base);
    dst += stride;
  }
}

void aom_smooth_v_predictor_32x32_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[31];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i top_lo = LoadUnaligned16(top_row);
  const __m128i top_hi = LoadUnaligned16(top_row + 16);
  const __m128i tdiff1 = _mm_sub_epi16(cvtepu8_epi16(top_lo), bl16);
  const __m128i tdiff2 = _mm_sub_epi16(_mm_unpackhi_epi8(top_lo, zero), bl16);
  const __m128i tdiff3 = _mm_sub_epi16(cvtepu8_epi16(top_hi), bl16);
  const __m128i tdiff4 = _mm_sub_epi16(_mm_unpackhi_epi8(top_hi, zero), bl16);
  for (int j = 0; j < 2; ++j) {
    const __m128i weights = LoadUnaligned16(smooth_weights + 28 + j * 16);
    const __m128i weights_lo = cvtepu8_epi16(weights);
    const __m128i weights_hi = _mm_unpackhi_epi8(weights, zero);
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i weights_y =
          _mm_shuffle_epi8(weights_lo, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, tdiff1, tdiff2, weights_y, weights_y,
                                      bl_base);
      write_smooth_directional_diff16(dst + 16, tdiff3, tdiff4, weights_y,
                                      weights_y, bl_base);
      dst += stride;
    }
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i weights_y =
          _mm_shuffle_epi8(weights_hi, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, tdiff1, tdiff2, weights_y, weights_y,
                                      bl_base);
      write_smooth_directional_diff16(dst + 16, tdiff3, tdiff4, weights_y,
                                      weights_y, bl_base);
      dst += stride;
    }
  }
}

void aom_smooth_v_predictor_32x64_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[63];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i top_lo = LoadUnaligned16(top_row);
  const __m128i top_hi = LoadUnaligned16(top_row + 16);
  const __m128i tdiff1 = _mm_sub_epi16(cvtepu8_epi16(top_lo), bl16);
  const __m128i tdiff2 = _mm_sub_epi16(_mm_unpackhi_epi8(top_lo, zero), bl16);
  const __m128i tdiff3 = _mm_sub_epi16(cvtepu8_epi16(top_hi), bl16);
  const __m128i tdiff4 = _mm_sub_epi16(_mm_unpackhi_epi8(top_hi, zero), bl16);
  const uint8_t *weights_base_ptr = smooth_weights + 60;
  for (int left_offset = 0; left_offset < 64; left_offset += 16) {
    const __m128i weights = LoadUnaligned16(weights_base_ptr + left_offset);
    const __m128i weights_lo = cvtepu8_epi16(weights);
    const __m128i weights_hi = _mm_unpackhi_epi8(weights, zero);
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i weights_y =
          _mm_shuffle_epi8(weights_lo, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, tdiff1, tdiff2, weights_y, weights_y,
                                      bl_base);
      write_smooth_directional_diff16(dst + 16, tdiff3, tdiff4, weights_y,
                                      weights_y, bl_base);
      dst += stride;
    }
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i weights_y =
          _mm_shuffle_epi8(weights_hi, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, tdiff1, tdiff2, weights_y, weights_y,
                                      bl_base);
      write_smooth_directional_diff16(dst + 16, tdiff3, tdiff4, weights_y,
                                      weights_y, bl_base);
      dst += stride;
    }
  }
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_smooth_v_predictor_64x16_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[15];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i top_lolo = LoadUnaligned16(top_row);
  const __m128i top_lohi = LoadUnaligned16(top_row + 16);
  const __m128i tdiff1 = _mm_sub_epi16(cvtepu8_epi16(top_lolo), bl16);
  const __m128i tdiff2 = _mm_sub_epi16(_mm_unpackhi_epi8(top_lolo, zero), bl16);
  const __m128i tdiff3 = _mm_sub_epi16(cvtepu8_epi16(top_lohi), bl16);
  const __m128i tdiff4 = _mm_sub_epi16(_mm_unpackhi_epi8(top_lohi, zero), bl16);
  const __m128i top_hilo = LoadUnaligned16(top_row + 32);
  const __m128i top_hihi = LoadUnaligned16(top_row + 48);
  const __m128i tdiff5 = _mm_sub_epi16(cvtepu8_epi16(top_hilo), bl16);
  const __m128i tdiff6 = _mm_sub_epi16(_mm_unpackhi_epi8(top_hilo, zero), bl16);
  const __m128i tdiff7 = _mm_sub_epi16(cvtepu8_epi16(top_hihi), bl16);
  const __m128i tdiff8 = _mm_sub_epi16(_mm_unpackhi_epi8(top_hihi, zero), bl16);
  const __m128i weights = LoadUnaligned16(smooth_weights + 12);
  const __m128i weights1 = cvtepu8_epi16(weights);
  const __m128i weights2 = _mm_unpackhi_epi8(weights, zero);
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i weights_y =
        _mm_shuffle_epi8(weights1, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, tdiff1, tdiff2, weights_y, weights_y,
                                    bl_base);
    write_smooth_directional_diff16(dst + 16, tdiff3, tdiff4, weights_y,
                                    weights_y, bl_base);
    write_smooth_directional_diff16(dst + 32, tdiff5, tdiff6, weights_y,
                                    weights_y, bl_base);
    write_smooth_directional_diff16(dst + 48, tdiff7, tdiff8, weights_y,
                                    weights_y, bl_base);
    dst += stride;
  }
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i weights_y =
        _mm_shuffle_epi8(weights2, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, tdiff1, tdiff2, weights_y, weights_y,
                                    bl_base);
    write_smooth_directional_diff16(dst + 16, tdiff3, tdiff4, weights_y,
                                    weights_y, bl_base);
    write_smooth_directional_diff16(dst + 32, tdiff5, tdiff6, weights_y,
                                    weights_y, bl_base);
    write_smooth_directional_diff16(dst + 48, tdiff7, tdiff8, weights_y,
                                    weights_y, bl_base);
    dst += stride;
  }
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_smooth_v_predictor_64x32_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[31];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i top_lolo = LoadUnaligned16(top_row);
  const __m128i top_lohi = LoadUnaligned16(top_row + 16);
  const __m128i tdiff1 = _mm_sub_epi16(cvtepu8_epi16(top_lolo), bl16);
  const __m128i tdiff2 = _mm_sub_epi16(_mm_unpackhi_epi8(top_lolo, zero), bl16);
  const __m128i tdiff3 = _mm_sub_epi16(cvtepu8_epi16(top_lohi), bl16);
  const __m128i tdiff4 = _mm_sub_epi16(_mm_unpackhi_epi8(top_lohi, zero), bl16);
  const __m128i top_hilo = LoadUnaligned16(top_row + 32);
  const __m128i top_hihi = LoadUnaligned16(top_row + 48);
  const __m128i tdiff5 = _mm_sub_epi16(cvtepu8_epi16(top_hilo), bl16);
  const __m128i tdiff6 = _mm_sub_epi16(_mm_unpackhi_epi8(top_hilo, zero), bl16);
  const __m128i tdiff7 = _mm_sub_epi16(cvtepu8_epi16(top_hihi), bl16);
  const __m128i tdiff8 = _mm_sub_epi16(_mm_unpackhi_epi8(top_hihi, zero), bl16);
  for (int j = 0; j < 2; ++j) {
    const __m128i weights = LoadUnaligned16(smooth_weights + 28 + j * 16);
    const __m128i weights_lo = cvtepu8_epi16(weights);
    const __m128i weights_hi = _mm_unpackhi_epi8(weights, zero);
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i weights_y =
          _mm_shuffle_epi8(weights_lo, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, tdiff1, tdiff2, weights_y, weights_y,
                                      bl_base);
      write_smooth_directional_diff16(dst + 16, tdiff3, tdiff4, weights_y,
                                      weights_y, bl_base);
      write_smooth_directional_diff16(dst + 32, tdiff5, tdiff6, weights_y,
                                      weights_y, bl_base);
      write_smooth_directional_diff16(dst + 48, tdiff7, tdiff8, weights_y,
                                      weights_y, bl_base);
      dst += stride;
    }
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i weights_y =
          _mm_shuffle_epi8(weights_hi, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, tdiff1, tdiff2, weights_y, weights_y,
                                      bl_base);
      write_smooth_directional_diff16(dst + 16, tdiff3, tdiff4, weights_y,
                                      weights_y, bl_base);
      write_smooth_directional_diff16(dst + 32, tdiff5, tdiff6, weights_y,
                                      weights_y, bl_base);
      write_smooth_directional_diff16(dst + 48, tdiff7, tdiff8, weights_y,
                                      weights_y, bl_base);
      dst += stride;
    }
  }
}

void aom_smooth_v_predictor_64x64_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int bl = left_column[63];
  const __m128i bl16 = _mm_set1_epi16((int16_t)bl);
  const __m128i bl_base = _mm_set1_epi16((int16_t)((bl << 8) + 128));
  const __m128i top_lolo = LoadUnaligned16(top_row);
  const __m128i top_lohi = LoadUnaligned16(top_row + 16);
  const __m128i tdiff1 = _mm_sub_epi16(cvtepu8_epi16(top_lolo), bl16);
  const __m128i tdiff2 = _mm_sub_epi16(_mm_unpackhi_epi8(top_lolo, zero), bl16);
  const __m128i tdiff3 = _mm_sub_epi16(cvtepu8_epi16(top_lohi), bl16);
  const __m128i tdiff4 = _mm_sub_epi16(_mm_unpackhi_epi8(top_lohi, zero), bl16);
  const __m128i top_hilo = LoadUnaligned16(top_row + 32);
  const __m128i top_hihi = LoadUnaligned16(top_row + 48);
  const __m128i tdiff5 = _mm_sub_epi16(cvtepu8_epi16(top_hilo), bl16);
  const __m128i tdiff6 = _mm_sub_epi16(_mm_unpackhi_epi8(top_hilo, zero), bl16);
  const __m128i tdiff7 = _mm_sub_epi16(cvtepu8_epi16(top_hihi), bl16);
  const __m128i tdiff8 = _mm_sub_epi16(_mm_unpackhi_epi8(top_hihi, zero), bl16);
  const uint8_t *weights_base_ptr = smooth_weights + 60;
  for (int left_offset = 0; left_offset < 64; left_offset += 16) {
    const __m128i weights = LoadUnaligned16(weights_base_ptr + left_offset);
    const __m128i weights_lo = cvtepu8_epi16(weights);
    const __m128i weights_hi = _mm_unpackhi_epi8(weights, zero);
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i weights_y =
          _mm_shuffle_epi8(weights_lo, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, tdiff1, tdiff2, weights_y, weights_y,
                                      bl_base);
      write_smooth_directional_diff16(dst + 16, tdiff3, tdiff4, weights_y,
                                      weights_y, bl_base);
      write_smooth_directional_diff16(dst + 32, tdiff5, tdiff6, weights_y,
                                      weights_y, bl_base);
      write_smooth_directional_diff16(dst + 48, tdiff7, tdiff8, weights_y,
                                      weights_y, bl_base);
      dst += stride;
    }
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i weights_y =
          _mm_shuffle_epi8(weights_hi, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, tdiff1, tdiff2, weights_y, weights_y,
                                      bl_base);
      write_smooth_directional_diff16(dst + 16, tdiff3, tdiff4, weights_y,
                                      weights_y, bl_base);
      write_smooth_directional_diff16(dst + 32, tdiff5, tdiff6, weights_y,
                                      weights_y, bl_base);
      write_smooth_directional_diff16(dst + 48, tdiff7, tdiff8, weights_y,
                                      weights_y, bl_base);
      dst += stride;
    }
  }
}

// -----------------------------------------------------------------------------
// SMOOTH_H_PRED

void aom_smooth_h_predictor_4x4_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[3];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i w4 = Load4(smooth_weights);
  const __m128i wx = _mm_unpacklo_epi8(_mm_unpacklo_epi32(w4, w4), zero);
  const __m128i ldiff16 =
      _mm_sub_epi16(_mm_unpacklo_epi8(Load4(left_column), zero), tr16);
  smooth_v_4xh_2rows(wx, tr_base, ldiff16, 2, dst, stride);
}

void aom_smooth_h_predictor_4x8_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[3];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i w4 = Load4(smooth_weights);
  const __m128i wx = _mm_unpacklo_epi8(_mm_unpacklo_epi32(w4, w4), zero);
  const __m128i ldiff16 =
      _mm_sub_epi16(_mm_unpacklo_epi8(LoadLo8(left_column), zero), tr16);
  smooth_v_4xh_2rows(wx, tr_base, ldiff16, 4, dst, stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_smooth_h_predictor_4x16_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[3];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i w4 = Load4(smooth_weights);
  const __m128i wx = _mm_unpacklo_epi8(_mm_unpacklo_epi32(w4, w4), zero);
  const __m128i l = LoadUnaligned16(left_column);
  smooth_v_4xh_2rows(wx, tr_base,
                     _mm_sub_epi16(_mm_unpacklo_epi8(l, zero), tr16), 4, dst,
                     stride);
  dst += stride << 3;
  smooth_v_4xh_2rows(wx, tr_base,
                     _mm_sub_epi16(_mm_unpackhi_epi8(l, zero), tr16), 4, dst,
                     stride);
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_smooth_h_predictor_8x4_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const int tr = top_row[7];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i wx = cvtepu8_epi16(LoadLo8(smooth_weights + 4));
  const __m128i ldiff16 =
      _mm_sub_epi16(cvtepu8_epi16(Load4(left_column)), tr16);
  smooth_v_8xh_2rows(wx, tr_base, ldiff16, 2, dst, stride);
}

void aom_smooth_h_predictor_8x8_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const int tr = top_row[7];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i wx = cvtepu8_epi16(LoadLo8(smooth_weights + 4));
  const __m128i ldiff16 =
      _mm_sub_epi16(cvtepu8_epi16(LoadLo8(left_column)), tr16);
  smooth_v_8xh_2rows(wx, tr_base, ldiff16, 4, dst, stride);
}

void aom_smooth_h_predictor_8x16_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[7];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i wx = cvtepu8_epi16(LoadLo8(smooth_weights + 4));
  const __m128i l = LoadUnaligned16(left_column);
  smooth_v_8xh_2rows(wx, tr_base,
                     _mm_sub_epi16(_mm_unpacklo_epi8(l, zero), tr16), 4, dst,
                     stride);
  dst += stride << 3;
  smooth_v_8xh_2rows(wx, tr_base,
                     _mm_sub_epi16(_mm_unpackhi_epi8(l, zero), tr16), 4, dst,
                     stride);
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_smooth_h_predictor_8x32_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[7];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i wx = cvtepu8_epi16(LoadLo8(smooth_weights + 4));
  for (int j = 0; j < 2; ++j) {
    const __m128i l = LoadUnaligned16(left_column + j * 16);
    smooth_v_8xh_2rows(wx, tr_base,
                       _mm_sub_epi16(_mm_unpacklo_epi8(l, zero), tr16), 4, dst,
                       stride);
    dst += stride << 3;
    smooth_v_8xh_2rows(wx, tr_base,
                       _mm_sub_epi16(_mm_unpackhi_epi8(l, zero), tr16), 4, dst,
                       stride);
    dst += stride << 3;
  }
}

void aom_smooth_h_predictor_16x4_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[15];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i ldiff = _mm_sub_epi16(cvtepu8_epi16(Load4(left_column)), tr16);
  const __m128i weights = LoadUnaligned16(smooth_weights + 12);
  const __m128i weights1 = cvtepu8_epi16(weights);
  const __m128i weights2 = _mm_unpackhi_epi8(weights, zero);
  for (int y_mask = 0x01000100; y_mask < 0x08000000; y_mask += 0x02020202) {
    const __m128i ldiff_y = _mm_shuffle_epi8(ldiff, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, ldiff_y, ldiff_y, weights1, weights2,
                                    tr_base);
    dst += stride;
  }
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_smooth_h_predictor_16x8_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[15];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i ldiff =
      _mm_sub_epi16(cvtepu8_epi16(LoadLo8(left_column)), tr16);
  const __m128i weights = LoadUnaligned16(smooth_weights + 12);
  const __m128i weights1 = cvtepu8_epi16(weights);
  const __m128i weights2 = _mm_unpackhi_epi8(weights, zero);
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i ldiff_y = _mm_shuffle_epi8(ldiff, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, ldiff_y, ldiff_y, weights1, weights2,
                                    tr_base);
    dst += stride;
  }
}

void aom_smooth_h_predictor_16x16_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[15];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i weights = LoadUnaligned16(smooth_weights + 12);
  const __m128i weights1 = cvtepu8_epi16(weights);
  const __m128i weights2 = _mm_unpackhi_epi8(weights, zero);
  const __m128i l = LoadUnaligned16(left_column);
  const __m128i ldiff1 = _mm_sub_epi16(cvtepu8_epi16(l), tr16);
  const __m128i ldiff2 = _mm_sub_epi16(_mm_unpackhi_epi8(l, zero), tr16);
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i ldiff_y = _mm_shuffle_epi8(ldiff1, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, ldiff_y, ldiff_y, weights1, weights2,
                                    tr_base);
    dst += stride;
  }
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i ldiff_y = _mm_shuffle_epi8(ldiff2, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, ldiff_y, ldiff_y, weights1, weights2,
                                    tr_base);
    dst += stride;
  }
}

void aom_smooth_h_predictor_16x32_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[15];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i weights = LoadUnaligned16(smooth_weights + 12);
  const __m128i weights1 = cvtepu8_epi16(weights);
  const __m128i weights2 = _mm_unpackhi_epi8(weights, zero);
  for (int left_offset = 0; left_offset < 32; left_offset += 8) {
    const __m128i ldiff =
        _mm_sub_epi16(cvtepu8_epi16(LoadLo8(left_column + left_offset)), tr16);
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i ldiff_y = _mm_shuffle_epi8(ldiff, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, ldiff_y, ldiff_y, weights1, weights2,
                                      tr_base);
      dst += stride;
    }
  }
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_smooth_h_predictor_16x64_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[15];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i weights = LoadUnaligned16(smooth_weights + 12);
  const __m128i weights1 = cvtepu8_epi16(weights);
  const __m128i weights2 = _mm_unpackhi_epi8(weights, zero);
  for (int left_offset = 0; left_offset < 64; left_offset += 8) {
    const __m128i ldiff =
        _mm_sub_epi16(cvtepu8_epi16(LoadLo8(left_column + left_offset)), tr16);
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i ldiff_y = _mm_shuffle_epi8(ldiff, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, ldiff_y, ldiff_y, weights1, weights2,
                                      tr_base);
      dst += stride;
    }
  }
}

void aom_smooth_h_predictor_32x8_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[31];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i ldiff =
      _mm_sub_epi16(cvtepu8_epi16(LoadLo8(left_column)), tr16);
  const __m128i weights_lo = LoadUnaligned16(smooth_weights + 28);
  const __m128i weights_hi = LoadUnaligned16(smooth_weights + 44);
  const __m128i weights1 = cvtepu8_epi16(weights_lo);
  const __m128i weights2 = _mm_unpackhi_epi8(weights_lo, zero);
  const __m128i weights3 = cvtepu8_epi16(weights_hi);
  const __m128i weights4 = _mm_unpackhi_epi8(weights_hi, zero);
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i ldiff_y = _mm_shuffle_epi8(ldiff, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, ldiff_y, ldiff_y, weights1, weights2,
                                    tr_base);
    write_smooth_directional_diff16(dst + 16, ldiff_y, ldiff_y, weights3,
                                    weights4, tr_base);
    dst += stride;
  }
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_smooth_h_predictor_32x16_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[31];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i weights_lo = LoadUnaligned16(smooth_weights + 28);
  const __m128i weights_hi = LoadUnaligned16(smooth_weights + 44);
  const __m128i weights1 = cvtepu8_epi16(weights_lo);
  const __m128i weights2 = _mm_unpackhi_epi8(weights_lo, zero);
  const __m128i weights3 = cvtepu8_epi16(weights_hi);
  const __m128i weights4 = _mm_unpackhi_epi8(weights_hi, zero);
  const __m128i l = LoadUnaligned16(left_column);
  const __m128i ldiff1 = _mm_sub_epi16(cvtepu8_epi16(l), tr16);
  const __m128i ldiff2 = _mm_sub_epi16(_mm_unpackhi_epi8(l, zero), tr16);
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i ldiff_y = _mm_shuffle_epi8(ldiff1, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, ldiff_y, ldiff_y, weights1, weights2,
                                    tr_base);
    write_smooth_directional_diff16(dst + 16, ldiff_y, ldiff_y, weights3,
                                    weights4, tr_base);
    dst += stride;
  }
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i ldiff_y = _mm_shuffle_epi8(ldiff2, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, ldiff_y, ldiff_y, weights1, weights2,
                                    tr_base);
    write_smooth_directional_diff16(dst + 16, ldiff_y, ldiff_y, weights3,
                                    weights4, tr_base);
    dst += stride;
  }
}

void aom_smooth_h_predictor_32x32_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[31];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i weights_lo = LoadUnaligned16(smooth_weights + 28);
  const __m128i weights_hi = LoadUnaligned16(smooth_weights + 44);
  const __m128i weights1 = cvtepu8_epi16(weights_lo);
  const __m128i weights2 = _mm_unpackhi_epi8(weights_lo, zero);
  const __m128i weights3 = cvtepu8_epi16(weights_hi);
  const __m128i weights4 = _mm_unpackhi_epi8(weights_hi, zero);
  for (int left_offset = 0; left_offset < 32; left_offset += 8) {
    const __m128i ldiff =
        _mm_sub_epi16(cvtepu8_epi16(LoadLo8(left_column + left_offset)), tr16);
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i ldiff_y = _mm_shuffle_epi8(ldiff, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, ldiff_y, ldiff_y, weights1, weights2,
                                      tr_base);
      write_smooth_directional_diff16(dst + 16, ldiff_y, ldiff_y, weights3,
                                      weights4, tr_base);
      dst += stride;
    }
  }
}

void aom_smooth_h_predictor_32x64_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[31];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i weights_lo = LoadUnaligned16(smooth_weights + 28);
  const __m128i weights_hi = LoadUnaligned16(smooth_weights + 44);
  const __m128i weights1 = cvtepu8_epi16(weights_lo);
  const __m128i weights2 = _mm_unpackhi_epi8(weights_lo, zero);
  const __m128i weights3 = cvtepu8_epi16(weights_hi);
  const __m128i weights4 = _mm_unpackhi_epi8(weights_hi, zero);
  for (int left_offset = 0; left_offset < 64; left_offset += 8) {
    const __m128i ldiff =
        _mm_sub_epi16(cvtepu8_epi16(LoadLo8(left_column + left_offset)), tr16);
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i ldiff_y = _mm_shuffle_epi8(ldiff, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, ldiff_y, ldiff_y, weights1, weights2,
                                      tr_base);
      write_smooth_directional_diff16(dst + 16, ldiff_y, ldiff_y, weights3,
                                      weights4, tr_base);
      dst += stride;
    }
  }
}

#if !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER
void aom_smooth_h_predictor_64x16_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[63];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i weights_lolo = LoadUnaligned16(smooth_weights + 60);
  const __m128i weights_lohi = LoadUnaligned16(smooth_weights + 76);
  const __m128i weights1 = cvtepu8_epi16(weights_lolo);
  const __m128i weights2 = _mm_unpackhi_epi8(weights_lolo, zero);
  const __m128i weights3 = cvtepu8_epi16(weights_lohi);
  const __m128i weights4 = _mm_unpackhi_epi8(weights_lohi, zero);
  const __m128i weights_hilo = LoadUnaligned16(smooth_weights + 92);
  const __m128i weights_hihi = LoadUnaligned16(smooth_weights + 108);
  const __m128i weights5 = cvtepu8_epi16(weights_hilo);
  const __m128i weights6 = _mm_unpackhi_epi8(weights_hilo, zero);
  const __m128i weights7 = cvtepu8_epi16(weights_hihi);
  const __m128i weights8 = _mm_unpackhi_epi8(weights_hihi, zero);
  const __m128i l = LoadUnaligned16(left_column);
  const __m128i ldiff1 = _mm_sub_epi16(cvtepu8_epi16(l), tr16);
  const __m128i ldiff2 = _mm_sub_epi16(_mm_unpackhi_epi8(l, zero), tr16);
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i ldiff_y = _mm_shuffle_epi8(ldiff1, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, ldiff_y, ldiff_y, weights1, weights2,
                                    tr_base);
    write_smooth_directional_diff16(dst + 16, ldiff_y, ldiff_y, weights3,
                                    weights4, tr_base);
    write_smooth_directional_diff16(dst + 32, ldiff_y, ldiff_y, weights5,
                                    weights6, tr_base);
    write_smooth_directional_diff16(dst + 48, ldiff_y, ldiff_y, weights7,
                                    weights8, tr_base);
    dst += stride;
  }
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i ldiff_y = _mm_shuffle_epi8(ldiff2, _mm_set1_epi32(y_mask));
    write_smooth_directional_diff16(dst, ldiff_y, ldiff_y, weights1, weights2,
                                    tr_base);
    write_smooth_directional_diff16(dst + 16, ldiff_y, ldiff_y, weights3,
                                    weights4, tr_base);
    write_smooth_directional_diff16(dst + 32, ldiff_y, ldiff_y, weights5,
                                    weights6, tr_base);
    write_smooth_directional_diff16(dst + 48, ldiff_y, ldiff_y, weights7,
                                    weights8, tr_base);
    dst += stride;
  }
}
#endif  // !CONFIG_REALTIME_ONLY || CONFIG_AV1_DECODER

void aom_smooth_h_predictor_64x32_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[63];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i weights_lolo = LoadUnaligned16(smooth_weights + 60);
  const __m128i weights_lohi = LoadUnaligned16(smooth_weights + 76);
  const __m128i weights1 = cvtepu8_epi16(weights_lolo);
  const __m128i weights2 = _mm_unpackhi_epi8(weights_lolo, zero);
  const __m128i weights3 = cvtepu8_epi16(weights_lohi);
  const __m128i weights4 = _mm_unpackhi_epi8(weights_lohi, zero);
  const __m128i weights_hilo = LoadUnaligned16(smooth_weights + 92);
  const __m128i weights_hihi = LoadUnaligned16(smooth_weights + 108);
  const __m128i weights5 = cvtepu8_epi16(weights_hilo);
  const __m128i weights6 = _mm_unpackhi_epi8(weights_hilo, zero);
  const __m128i weights7 = cvtepu8_epi16(weights_hihi);
  const __m128i weights8 = _mm_unpackhi_epi8(weights_hihi, zero);
  for (int left_offset = 0; left_offset < 32; left_offset += 8) {
    const __m128i ldiff =
        _mm_sub_epi16(cvtepu8_epi16(LoadLo8(left_column + left_offset)), tr16);
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i ldiff_y = _mm_shuffle_epi8(ldiff, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, ldiff_y, ldiff_y, weights1, weights2,
                                      tr_base);
      write_smooth_directional_diff16(dst + 16, ldiff_y, ldiff_y, weights3,
                                      weights4, tr_base);
      write_smooth_directional_diff16(dst + 32, ldiff_y, ldiff_y, weights5,
                                      weights6, tr_base);
      write_smooth_directional_diff16(dst + 48, ldiff_y, ldiff_y, weights7,
                                      weights8, tr_base);
      dst += stride;
    }
  }
}

void aom_smooth_h_predictor_64x64_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i zero = _mm_setzero_si128();
  const int tr = top_row[63];
  const __m128i tr16 = _mm_set1_epi16((int16_t)tr);
  const __m128i tr_base = _mm_set1_epi16((int16_t)((tr << 8) + 128));
  const __m128i weights_lolo = LoadUnaligned16(smooth_weights + 60);
  const __m128i weights_lohi = LoadUnaligned16(smooth_weights + 76);
  const __m128i weights1 = cvtepu8_epi16(weights_lolo);
  const __m128i weights2 = _mm_unpackhi_epi8(weights_lolo, zero);
  const __m128i weights3 = cvtepu8_epi16(weights_lohi);
  const __m128i weights4 = _mm_unpackhi_epi8(weights_lohi, zero);
  const __m128i weights_hilo = LoadUnaligned16(smooth_weights + 92);
  const __m128i weights_hihi = LoadUnaligned16(smooth_weights + 108);
  const __m128i weights5 = cvtepu8_epi16(weights_hilo);
  const __m128i weights6 = _mm_unpackhi_epi8(weights_hilo, zero);
  const __m128i weights7 = cvtepu8_epi16(weights_hihi);
  const __m128i weights8 = _mm_unpackhi_epi8(weights_hihi, zero);
  for (int left_offset = 0; left_offset < 64; left_offset += 8) {
    const __m128i ldiff =
        _mm_sub_epi16(cvtepu8_epi16(LoadLo8(left_column + left_offset)), tr16);
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i ldiff_y = _mm_shuffle_epi8(ldiff, _mm_set1_epi32(y_mask));
      write_smooth_directional_diff16(dst, ldiff_y, ldiff_y, weights1, weights2,
                                      tr_base);
      write_smooth_directional_diff16(dst + 16, ldiff_y, ldiff_y, weights3,
                                      weights4, tr_base);
      write_smooth_directional_diff16(dst + 32, ldiff_y, ldiff_y, weights5,
                                      weights6, tr_base);
      write_smooth_directional_diff16(dst + 48, ldiff_y, ldiff_y, weights7,
                                      weights8, tr_base);
      dst += stride;
    }
  }
}
