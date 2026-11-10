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

#include <assert.h>
#include <smmintrin.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

void av1_filter_intra_edge_sse4_1(uint8_t *p, int sz, int strength) {
  if (!strength) return;

  DECLARE_ALIGNED(16, static const int8_t, shuf_pairs[4][16]) = {
    { 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8 },
    { 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9 },
    { 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10 },
    { 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11 },
  };

  // Extend the first and last samples to simplify the loop for the 5-tap case
  __m128i last = _mm_set1_epi8((char)p[sz - 1]);
  _mm_storeu_si128((__m128i *)&p[sz], last);

  // Avoid modifying first sample
  uint8_t *out = p + 1;
  int len = sz - 1;

  const __m128i eight = _mm_set1_epi16(8);
  const __m128i shuf01 = _mm_lddqu_si128((const __m128i *)shuf_pairs[0]);
  __m128i in0 = _mm_lddqu_si128((__m128i *)p);

  if (strength < 3) {
    uint8_t *in = p;
    const __m128i shuf12 = _mm_lddqu_si128((const __m128i *)shuf_pairs[1]);
    const __m128i coef01 = _mm_set1_epi16((strength == 1) ? 0x0404 : 0x0305);
    const __m128i coef12 = _mm_set1_epi16((strength == 1) ? 0x0404 : 0x0503);
    for (;;) {
      __m128i d0 = _mm_maddubs_epi16(_mm_shuffle_epi8(in0, shuf01), coef01);
      __m128i d1 = _mm_maddubs_epi16(_mm_shuffle_epi8(in0, shuf12), coef12);
      d0 = _mm_add_epi16(d0, _mm_add_epi16(d1, eight));
      d0 = _mm_srai_epi16(d0, 4);
      d0 = _mm_packus_epi16(d0, d0);
      _mm_storel_epi64((__m128i *)out, d0);
      len -= 8;
      if (len <= 0) break;
      __m128i in1 = _mm_lddqu_si128((__m128i *)(in + 16));
      in0 = _mm_alignr_epi8(in1, in0, 8);
      in += 8;
      out += 8;
    }
  } else {  // 5-tap filter: [2, 4, 4, 4, 2]
    uint8_t *in = p - 1;
    in0 = _mm_alignr_epi8(in0, _mm_slli_si128(in0, 15), 15);
    const __m128i shuf23 = _mm_lddqu_si128((const __m128i *)shuf_pairs[2]);
    const __m128i shuf34 = _mm_lddqu_si128((const __m128i *)shuf_pairs[3]);
    const __m128i coef01 = _mm_set1_epi16(0x0402);
    const __m128i coef23 = _mm_set1_epi16(0x0204);
    const __m128i coef34 = _mm_set1_epi16(0x0202);
    for (;;) {
      __m128i d0 = _mm_maddubs_epi16(_mm_shuffle_epi8(in0, shuf01), coef01);
      __m128i d1 = _mm_maddubs_epi16(_mm_shuffle_epi8(in0, shuf23), coef23);
      __m128i d2 = _mm_maddubs_epi16(_mm_shuffle_epi8(in0, shuf34), coef34);
      d0 = _mm_add_epi16(_mm_add_epi16(d0, d1), _mm_add_epi16(d2, eight));
      d0 = _mm_srai_epi16(d0, 4);
      d0 = _mm_packus_epi16(d0, d0);
      _mm_storel_epi64((__m128i *)out, d0);
      len -= 8;
      if (len <= 0) break;
      __m128i in1 = _mm_lddqu_si128((__m128i *)(in + 16));
      in0 = _mm_alignr_epi8(in1, in0, 8);
      in += 8;
      out += 8;
    }
  }
  _mm_storeu_si128((__m128i *)&p[sz], last);
}

void av1_upsample_intra_edge_sse4_1(uint8_t *p, int sz) {
  // interpolate half-sample positions
  assert(sz <= 24);

  DECLARE_ALIGNED(16, static const int8_t, shuf_pairs[2][16]) = {
    { 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8 },
    { 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10 }
  };

  // Extend first/last samples (upper-left p[-1], last p[sz-1])
  // to support 4-tap filter
  p[-2] = p[-1];
  p[sz] = p[sz - 1];

  uint8_t *in = &p[-2];
  uint8_t *out = &p[-2];

  int n = sz + 1;  // Input length including upper-left sample

  __m128i in0 = _mm_lddqu_si128((__m128i *)&in[0]);
  __m128i in16 = _mm_lddqu_si128((__m128i *)&in[16]);

  const __m128i coef01 = _mm_set1_epi16(0x09ff);
  const __m128i coef23 = _mm_set1_epi16((short)0xff09);
  const __m128i shuf01 = _mm_lddqu_si128((const __m128i *)shuf_pairs[0]);
  const __m128i shuf23 = _mm_lddqu_si128((const __m128i *)shuf_pairs[1]);
  const __m128i eight = _mm_set1_epi16(8);

  while (n > 0) {
    __m128i in8 = _mm_alignr_epi8(in16, in0, 8);
    __m128i d0 = _mm_maddubs_epi16(_mm_shuffle_epi8(in0, shuf01), coef01);
    __m128i d1 = _mm_maddubs_epi16(_mm_shuffle_epi8(in0, shuf23), coef23);
    __m128i d2 = _mm_maddubs_epi16(_mm_shuffle_epi8(in8, shuf01), coef01);
    __m128i d3 = _mm_maddubs_epi16(_mm_shuffle_epi8(in8, shuf23), coef23);
    d0 = _mm_add_epi16(d0, _mm_add_epi16(d1, eight));
    d2 = _mm_add_epi16(d2, _mm_add_epi16(d3, eight));
    d0 = _mm_srai_epi16(d0, 4);
    d2 = _mm_srai_epi16(d2, 4);
    d0 = _mm_packus_epi16(d0, d2);
    __m128i in1 = _mm_alignr_epi8(in16, in0, 1);
    __m128i out0 = _mm_unpacklo_epi8(in1, d0);
    __m128i out1 = _mm_unpackhi_epi8(in1, d0);
    _mm_storeu_si128((__m128i *)&out[0], out0);
    _mm_storeu_si128((__m128i *)&out[16], out1);
    in0 = in16;
    in16 = _mm_setzero_si128();
    out += 32;
    n -= 16;
  }
}

#if CONFIG_AV1_HIGHBITDEPTH

void av1_highbd_filter_intra_edge_sse4_1(uint16_t *p, int sz, int strength) {
  if (!strength) return;

  DECLARE_ALIGNED(16, static const int16_t, kern[3][8]) = {
    { 4, 8, 4, 8, 4, 8, 4, 8 },  // strength 1: 4,8,4
    { 5, 6, 5, 6, 5, 6, 5, 6 },  // strength 2: 5,6,5
    { 2, 4, 2, 4, 2, 4, 2, 4 }   // strength 3: 2,4,4,4,2
  };

  DECLARE_ALIGNED(16, static const int16_t,
                  v_const[1][8]) = { { 0, 1, 2, 3, 4, 5, 6, 7 } };

  // Extend the first and last samples to simplify the loop for the 5-tap case
  p[-1] = p[0];
  __m128i last = _mm_set1_epi16(p[sz - 1]);
  _mm_storeu_si128((__m128i *)&p[sz], last);

  // Adjust input pointer for filter support area
  uint16_t *in = (strength == 3) ? p - 1 : p;

  // Avoid modifying first sample
  uint16_t *out = p + 1;
  int len = sz - 1;

  const int use_3tap_filter = (strength < 3);
  __m128i coef0 = _mm_lddqu_si128((__m128i const *)kern[strength - 1]);
  __m128i iden = _mm_lddqu_si128((__m128i *)v_const[0]);
  __m128i in0 = _mm_lddqu_si128((__m128i *)&in[0]);
  __m128i in8 = _mm_lddqu_si128((__m128i *)&in[8]);
  while (len > 0) {
    int n_out = (len < 8) ? len : 8;
    __m128i in1 = _mm_alignr_epi8(in8, in0, 2);
    __m128i in2 = _mm_alignr_epi8(in8, in0, 4);
    __m128i s0, s1;
    if (use_3tap_filter) {
      s0 = _mm_add_epi16(in0, in2);
      s1 = in1;
    } else {
      __m128i in3 = _mm_alignr_epi8(in8, in0, 6);
      __m128i in4 = _mm_alignr_epi8(in8, in0, 8);
      s0 = _mm_add_epi16(in0, in4);
      s1 = _mm_add_epi16(_mm_add_epi16(in1, in2), in3);
    }
    __m128i d0 = _mm_unpacklo_epi16(s0, s1);
    __m128i d1 = _mm_unpackhi_epi16(s0, s1);
    d0 = _mm_mullo_epi16(d0, coef0);
    d1 = _mm_mullo_epi16(d1, coef0);
    d0 = _mm_hadd_epi16(d0, d1);
    __m128i eight = _mm_set1_epi16(8);
    d0 = _mm_add_epi16(d0, eight);
    d0 = _mm_srli_epi16(d0, 4);
    __m128i out0 = _mm_lddqu_si128((__m128i *)out);
    __m128i n0 = _mm_set1_epi16(n_out);
    __m128i mask = _mm_cmpgt_epi16(n0, iden);
    out0 = _mm_blendv_epi8(out0, d0, mask);
    _mm_storeu_si128((__m128i *)out, out0);
    in += 8;
    in0 = in8;
    in8 = _mm_lddqu_si128((__m128i *)&in[8]);
    out += 8;
    len -= n_out;
  }
}

void av1_highbd_upsample_intra_edge_sse4_1(uint16_t *p, int sz, int bd) {
  // interpolate half-sample positions
  assert(sz <= 24);

  DECLARE_ALIGNED(16, static const int16_t,
                  kernel[1][8]) = { { -1, 9, -1, 9, -1, 9, -1, 9 } };

  // Extend first/last samples (upper-left p[-1], last p[sz-1])
  // to support 4-tap filter
  p[-2] = p[-1];
  p[sz] = p[sz - 1];

  uint16_t *in = &p[-2];
  uint16_t *out = in;
  int n = sz + 1;

  __m128i in0 = _mm_lddqu_si128((__m128i *)&in[0]);
  __m128i in8 = _mm_lddqu_si128((__m128i *)&in[8]);
  __m128i in16 = _mm_lddqu_si128((__m128i *)&in[16]);
  __m128i in24 = _mm_lddqu_si128((__m128i *)&in[24]);

  while (n > 0) {
    __m128i in1 = _mm_alignr_epi8(in8, in0, 2);
    __m128i in2 = _mm_alignr_epi8(in8, in0, 4);
    __m128i in3 = _mm_alignr_epi8(in8, in0, 6);
    __m128i sum0 = _mm_add_epi16(in0, in3);
    __m128i sum1 = _mm_add_epi16(in1, in2);
    __m128i d0 = _mm_unpacklo_epi16(sum0, sum1);
    __m128i d1 = _mm_unpackhi_epi16(sum0, sum1);
    __m128i coef0 = _mm_lddqu_si128((__m128i *)kernel[0]);
    d0 = _mm_madd_epi16(d0, coef0);
    d1 = _mm_madd_epi16(d1, coef0);
    __m128i eight = _mm_set1_epi32(8);
    d0 = _mm_add_epi32(d0, eight);
    d1 = _mm_add_epi32(d1, eight);
    d0 = _mm_srai_epi32(d0, 4);
    d1 = _mm_srai_epi32(d1, 4);
    d0 = _mm_packus_epi32(d0, d1);
    __m128i max0 = _mm_set1_epi16((1 << bd) - 1);
    d0 = _mm_min_epi16(d0, max0);
    __m128i out0 = _mm_unpacklo_epi16(in1, d0);
    __m128i out1 = _mm_unpackhi_epi16(in1, d0);
    _mm_storeu_si128((__m128i *)&out[0], out0);
    _mm_storeu_si128((__m128i *)&out[8], out1);
    in0 = in8;
    in8 = in16;
    in16 = in24;
    in24 = _mm_setzero_si128();
    out += 16;
    n -= 8;
  }
}

#endif  // CONFIG_AV1_HIGHBITDEPTH
