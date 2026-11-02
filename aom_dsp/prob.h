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

#ifndef AOM_AOM_DSP_PROB_H_
#define AOM_AOM_DSP_PROB_H_

#include <assert.h>
#include <stdio.h>

#include "config/aom_config.h"

#include "aom_dsp/aom_dsp_common.h"

#include "aom_ports/bitops.h"
#include "aom_ports/mem.h"

#if defined(__SSE2__)
#include <emmintrin.h>
#include <string.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t aom_cdf_prob;

#define CDF_SIZE(x) ((x) + 1)
#define CDF_PROB_BITS 15
#define CDF_PROB_TOP (1 << CDF_PROB_BITS)
/*The value stored in an iCDF is CDF_PROB_TOP minus the actual cumulative
  probability (an "inverse" CDF).
  This function converts from one representation to the other (and is its own
  inverse).*/
#define AOM_ICDF(x) (CDF_PROB_TOP - (x))

#define AOM_CDF2(a0) AOM_ICDF(a0), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF3(a0, a1) AOM_ICDF(a0), AOM_ICDF(a1), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF4(a0, a1, a2) \
  AOM_ICDF(a0), AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF5(a0, a1, a2, a3) \
  AOM_ICDF(a0)                   \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF6(a0, a1, a2, a3, a4)                        \
  AOM_ICDF(a0)                                              \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF7(a0, a1, a2, a3, a4, a5)                                  \
  AOM_ICDF(a0)                                                            \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5), \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF8(a0, a1, a2, a3, a4, a5, a6)                              \
  AOM_ICDF(a0)                                                            \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5), \
      AOM_ICDF(a6), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF9(a0, a1, a2, a3, a4, a5, a6, a7)                          \
  AOM_ICDF(a0)                                                            \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5), \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF10(a0, a1, a2, a3, a4, a5, a6, a7, a8)                     \
  AOM_ICDF(a0)                                                            \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5), \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF11(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9)                 \
  AOM_ICDF(a0)                                                            \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5), \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9),             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF12(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)               \
  AOM_ICDF(a0)                                                               \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5),    \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9), AOM_ICDF(a10), \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF13(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)          \
  AOM_ICDF(a0)                                                               \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5),    \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9), AOM_ICDF(a10), \
      AOM_ICDF(a11), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF14(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)     \
  AOM_ICDF(a0)                                                               \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5),    \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9), AOM_ICDF(a10), \
      AOM_ICDF(a11), AOM_ICDF(a12), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF15(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) \
  AOM_ICDF(a0)                                                                \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5),     \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9), AOM_ICDF(a10),  \
      AOM_ICDF(a11), AOM_ICDF(a12), AOM_ICDF(a13), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF16(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, \
                  a14)                                                        \
  AOM_ICDF(a0)                                                                \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5),     \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9), AOM_ICDF(a10),  \
      AOM_ICDF(a11), AOM_ICDF(a12), AOM_ICDF(a13), AOM_ICDF(a14),             \
      AOM_ICDF(CDF_PROB_TOP), 0

static inline uint8_t get_prob(unsigned int num, unsigned int den) {
  assert(den != 0);
  {
    const int p = (int)(((uint64_t)num * 256 + (den >> 1)) / den);
    // (p > 255) ? 255 : (p < 1) ? 1 : p;
    const int clipped_prob = p | ((255 - p) >> 23) | (p == 0);
    return (uint8_t)clipped_prob;
  }
}

static inline void update_cdf(aom_cdf_prob *cdf, int8_t val, int nsymbs) {
  assert(nsymbs < 17);
  const int count = cdf[nsymbs];

  // rate is computed in the spec as:
  //  3 + ( cdf[N] > 15 ) + ( cdf[N] > 31 ) + Min(FloorLog2(N), 2)
  // In this case cdf[N] is |count|.
  // Min(FloorLog2(N), 2) is 1 for nsymbs == {2, 3} and 2 for all
  // nsymbs > 3. So the equation becomes:
  //  4 + (count > 15) + (count > 31) + (nsymbs > 3).
  // Note that the largest value for count is 32 (it is not incremented beyond
  // 32). So using that information:
  //  count >> 4 is 0 for count from 0 to 15.
  //  count >> 4 is 1 for count from 16 to 31.
  //  count >> 4 is 2 for count == 31.
  // Now, the equation becomes:
  //  4 + (count >> 4) + (nsymbs > 3).
  const int rate = 4 + (count >> 4) + (nsymbs > 3);

#if defined(__GNUC__) || defined(__clang__)
  if (__builtin_constant_p(nsymbs)) {
    if (nsymbs == 2) {
      const int mask = val - 1;
      const int c = cdf[0];
      cdf[0] = (aom_cdf_prob)(c - mask + (((mask | CDF_PROB_TOP) - c) >> rate));
      cdf[2] = (aom_cdf_prob)(count + (count < 32));
      return;
    }
#if defined(__SSE2__)
    if (nsymbs == 3) {
      int32_t cdf32;
      memcpy(&cdf32, cdf, sizeof(cdf32));
      __m128i cdf_vec = _mm_cvtsi32_si128(cdf32);
      const __m128i index = _mm_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8);
      const __m128i sym_vec = _mm_set1_epi16(val);
      const __m128i mask = _mm_cmpgt_epi16(index, sym_vec);
      const __m128i a =
          _mm_or_si128(mask, _mm_set1_epi16((int16_t)CDF_PROB_TOP));
      const __m128i diff = _mm_sub_epi16(a, cdf_vec);
      const __m128i cdf_offset = _mm_sub_epi16(cdf_vec, mask);
      const __m128i delta = _mm_sra_epi16(diff, _mm_cvtsi32_si128(rate));
      cdf_vec = _mm_add_epi16(cdf_offset, delta);
      cdf32 = _mm_cvtsi128_si32(cdf_vec);
      memcpy(cdf, &cdf32, sizeof(cdf32));
      cdf[3] = (aom_cdf_prob)(count + (count < 32));
      return;
    }
    if (nsymbs == 4 || nsymbs == 5) {
      __m128i cdf_vec = _mm_loadl_epi64((const __m128i *)cdf);
      const __m128i index = _mm_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8);
      const __m128i sym_vec = _mm_set1_epi16(val);
      const __m128i mask = _mm_cmpgt_epi16(index, sym_vec);
      const __m128i a =
          _mm_or_si128(mask, _mm_set1_epi16((int16_t)CDF_PROB_TOP));
      const __m128i diff = _mm_sub_epi16(a, cdf_vec);
      const __m128i cdf_offset = _mm_sub_epi16(cdf_vec, mask);
      const __m128i delta = _mm_sra_epi16(diff, _mm_cvtsi32_si128(rate));
      cdf_vec = _mm_add_epi16(cdf_offset, delta);
      _mm_storel_epi64((__m128i *)cdf, cdf_vec);
      cdf[nsymbs] = (aom_cdf_prob)(count + (count < 32));
      return;
    }
    if (nsymbs >= 7 && nsymbs <= 9) {
      __m128i cdf_vec = _mm_loadu_si128((const __m128i *)cdf);
      const __m128i index = _mm_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8);
      const __m128i sym_vec = _mm_set1_epi16(val);
      const __m128i mask = _mm_cmpgt_epi16(index, sym_vec);
      const __m128i a =
          _mm_or_si128(mask, _mm_set1_epi16((int16_t)CDF_PROB_TOP));
      const __m128i diff = _mm_sub_epi16(a, cdf_vec);
      const __m128i cdf_offset = _mm_sub_epi16(cdf_vec, mask);
      const __m128i delta = _mm_sra_epi16(diff, _mm_cvtsi32_si128(rate));
      cdf_vec = _mm_add_epi16(cdf_offset, delta);
      _mm_storeu_si128((__m128i *)cdf, cdf_vec);
      cdf[nsymbs] = (aom_cdf_prob)(count + (count < 32));
      return;
    }
#endif
  }
#endif

  int i = 0;
  do {
    const int c = cdf[i];
    const int mask = (val - 1 - i) >> 31;
    cdf[i] = (aom_cdf_prob)(c - mask + (((mask | CDF_PROB_TOP) - c) >> rate));
  } while (++i < nsymbs - 1);
  cdf[nsymbs] = (aom_cdf_prob)(count + (count < 32));
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOM_DSP_PROB_H_
