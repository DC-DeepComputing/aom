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

#ifndef AOM_AV1_ENCODER_COST_H_
#define AOM_AV1_ENCODER_COST_H_

#include "aom_dsp/entcode.h"
#include "aom_dsp/prob.h"
#include "aom/aom_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const uint16_t av1_prob_cost[130];

// The factor to scale from cost in bits to cost in av1_prob_cost units.
#define AV1_PROB_COST_SHIFT 9

// Cost of coding an n bit literal, using 128 (i.e. 50%) probability
// for each bit.
#define av1_cost_literal(n) ((n) * (1 << AV1_PROB_COST_SHIFT))

static inline int av1_cost_symbol_clamped(int p15) {
  assert(0 < p15 && p15 < CDF_PROB_TOP);
#if defined(__LZCNT__) && (defined(__GNUC__) || defined(__clang__))
  const unsigned int shift =
      (unsigned int)(__builtin_clz((unsigned int)p15) - 17);
#else
  const unsigned int shift = (unsigned int)(CDF_PROB_BITS - 1 - get_msb(p15));
#endif
  const unsigned int prob_idx = ((unsigned int)(p15 << shift) - 16320U) >> 7;
  return av1_prob_cost[prob_idx] + (int)av1_cost_literal(shift);
}

// Calculate the cost of a symbol with probability p15 / 2^15
static inline int av1_cost_symbol(aom_cdf_prob p15) {
  // p15 can be out of range [1, CDF_PROB_TOP - 1]. Clamping it, so that the
  // following cost calculation works correctly. Otherwise, if p15 =
  // CDF_PROB_TOP, shift would be -1, and "p15 << shift" would be wrong.
  const int clamped = clamp((int)p15, 1, CDF_PROB_TOP - 1);
  return av1_cost_symbol_clamped(clamped);
}

static inline void av1_cost_tokens_from_cdf(int *costs, const aom_cdf_prob *cdf,
                                            const int *inv_map) {
  int prev_icdf = CDF_PROB_TOP;
  if (inv_map) {
    for (int i = 0;; ++i) {
      const int cur_icdf = cdf[i];
      const int p15 =
          clamp(prev_icdf - cur_icdf, EC_MIN_PROB, CDF_PROB_TOP - 1);
      prev_icdf = cur_icdf;
      costs[inv_map[i]] = av1_cost_symbol_clamped(p15);
      if (cur_icdf == 0) break;
    }
  } else {
    for (int i = 0;; ++i) {
      const int cur_icdf = cdf[i];
      const int p15 =
          clamp(prev_icdf - cur_icdf, EC_MIN_PROB, CDF_PROB_TOP - 1);
      prev_icdf = cur_icdf;
      costs[i] = av1_cost_symbol_clamped(p15);
      if (cur_icdf == 0) break;
    }
  }
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_COST_H_
