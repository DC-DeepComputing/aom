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
#include <stdlib.h>

#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem.h"

#include "av1/encoder/aq_variance.h"
#include "av1/common/seg_common.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/dwt.h"
#include "config/aom_config.h"
#if HAVE_AVX2 && (defined(__GNUC__) || defined(__clang__)) && \
    (defined(__x86_64__) || defined(__i386__))
#include <immintrin.h>
#endif

#if !CONFIG_REALTIME_ONLY
static const double rate_ratio[MAX_SEGMENTS] = { 2.2, 1.7, 1.3, 1.0,
                                                 0.9, .8,  .7,  .6 };

static const double deltaq_rate_ratio[MAX_SEGMENTS] = { 2.5,  2.0, 1.5, 1.0,
                                                        0.75, 1.0, 1.0, 1.0 };
#define ENERGY_MIN (-4)
#define ENERGY_MAX (1)
#define ENERGY_SPAN (ENERGY_MAX - ENERGY_MIN + 1)
#define ENERGY_IN_BOUNDS(energy) \
  assert((energy) >= ENERGY_MIN && (energy) <= ENERGY_MAX)

static const int segment_id[ENERGY_SPAN] = { 0, 1, 1, 2, 3, 4 };

#define SEGMENT_ID(i) segment_id[(i) - ENERGY_MIN]

void av1_vaq_frame_setup(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  const RefreshFrameInfo *const refresh_frame = &cpi->refresh_frame;
  const int base_qindex = cm->quant_params.base_qindex;
  struct segmentation *seg = &cm->seg;
  int i;

  int resolution_change =
      cm->prev_frame && (cm->width != cm->prev_frame->width ||
                         cm->height != cm->prev_frame->height);
  int avg_energy = (int)(cpi->twopass_frame.mb_av_energy - 2);
  double avg_ratio;
  if (avg_energy > 7) avg_energy = 7;
  if (avg_energy < 0) avg_energy = 0;
  avg_ratio = rate_ratio[avg_energy];

  if (resolution_change) {
    memset(cpi->enc_seg.map, 0, cm->mi_params.mi_rows * cm->mi_params.mi_cols);
    av1_clearall_segfeatures(seg);
    av1_disable_segmentation(seg);
    return;
  }
  if (frame_is_intra_only(cm) || cm->features.error_resilient_mode ||
      refresh_frame->alt_ref_frame ||
      (refresh_frame->golden_frame && !cpi->rc.is_src_frame_alt_ref)) {
    cpi->vaq_refresh = 1;

    av1_enable_segmentation(seg);
    av1_clearall_segfeatures(seg);

    for (i = 0; i < MAX_SEGMENTS; ++i) {
      // Set up avg segment id to be 1.0 and adjust the other segments around
      // it.
      int qindex_delta =
          av1_compute_qdelta_by_rate(cpi, cm->current_frame.frame_type,
                                     base_qindex, rate_ratio[i] / avg_ratio);

      // We don't allow qindex 0 in a segment if the base value is not 0.
      // Q index 0 (lossless) implies 4x4 encoding only and in AQ mode a segment
      // Q delta is sometimes applied without going back around the rd loop.
      // This could lead to an illegal combination of partition size and q.
      if ((base_qindex != 0) && ((base_qindex + qindex_delta) == 0)) {
        qindex_delta = -base_qindex + 1;
      }

      av1_set_segdata(seg, i, SEG_LVL_ALT_Q, qindex_delta);
      av1_enable_segfeature(seg, i, SEG_LVL_ALT_Q);
    }
  }
}

int av1_log_block_avg(const AV1_COMP *cpi, const MACROBLOCK *x, BLOCK_SIZE bs,
                      int mi_row, int mi_col) {
  // This functions returns the block average of luma block
  unsigned int sum, avg, num_pix;
  int r, c;
  const int pic_w = cpi->common.width;
  const int pic_h = cpi->common.height;
  const int bw = MI_SIZE * mi_size_wide[bs];
  const int bh = MI_SIZE * mi_size_high[bs];
  const uint16_t *x16 = CONVERT_TO_SHORTPTR(x->plane[0].src.buf);

  sum = 0;
  num_pix = 0;
  avg = 0;
  int row = mi_row << MI_SIZE_LOG2;
  int col = mi_col << MI_SIZE_LOG2;
  for (r = row; (r < (row + bh)) && (r < pic_h); r++) {
    for (c = col; (c < (col + bw)) && (c < pic_w); c++) {
      sum += *(x16 + r * x->plane[0].src.stride + c);
      num_pix++;
    }
  }
  if (num_pix != 0) {
    avg = sum / num_pix;
  }
  return avg;
}

#define DEFAULT_E_MIDPOINT 10.0

static unsigned int haar_ac_energy(const MACROBLOCK *x, BLOCK_SIZE bs) {
  const MACROBLOCKD *xd = &x->e_mbd;
  int stride = x->plane[0].src.stride;
  const uint8_t *buf = x->plane[0].src.buf;
  const int num_8x8_cols = block_size_wide[bs] / 8;
  const int num_8x8_rows = block_size_high[bs] / 8;
  const int hbd = is_cur_buf_hbd(xd);

  int64_t var = av1_haar_ac_sad_mxn_uint8_input(buf, stride, hbd, num_8x8_rows,
                                                num_8x8_cols);

  return (unsigned int)((uint64_t)var * 256) >> num_pels_log2_lookup[bs];
}

static double log_block_wavelet_energy(const MACROBLOCK *x, BLOCK_SIZE bs) {
  unsigned int haar_sad = haar_ac_energy(x, bs);
  return log1p(haar_sad);
}

int av1_block_wavelet_energy_level(const AV1_COMP *cpi, const MACROBLOCK *x,
                                   BLOCK_SIZE bs) {
  double energy, energy_midpoint;
  energy_midpoint = (is_stat_consumption_stage_twopass(cpi))
                        ? cpi->twopass_frame.frame_avg_haar_energy
                        : DEFAULT_E_MIDPOINT;
  energy = log_block_wavelet_energy(x, bs) - energy_midpoint;
  return clamp((int)round(energy), ENERGY_MIN, ENERGY_MAX);
}

int av1_compute_q_from_energy_level_deltaq_mode(const AV1_COMP *const cpi,
                                                int block_var_level) {
  int rate_level;
  const AV1_COMMON *const cm = &cpi->common;

  if (DELTA_Q_PERCEPTUAL_MODULATION == 1) {
    ENERGY_IN_BOUNDS(block_var_level);
    rate_level = SEGMENT_ID(block_var_level);
  } else {
    rate_level = block_var_level;
  }
  const int base_qindex = cm->quant_params.base_qindex;
  int qindex_delta =
      av1_compute_qdelta_by_rate(cpi, cm->current_frame.frame_type, base_qindex,
                                 deltaq_rate_ratio[rate_level]);

  if ((base_qindex != 0) && ((base_qindex + qindex_delta) == 0)) {
    qindex_delta = -base_qindex + 1;
  }
  return base_qindex + qindex_delta;
}

#define SUPERBLOCK_SIZE 64
#define SUBBLOCK_SIZE 8
#define SUBBLOCKS_IN_SB_DIM (SUPERBLOCK_SIZE / SUBBLOCK_SIZE)
#define SUBBLOCKS_IN_SB (SUBBLOCKS_IN_SB_DIM * SUBBLOCKS_IN_SB_DIM)
#define SUBBLOCKS_IN_OCTILE (SUBBLOCKS_IN_SB / 8)

static unsigned int get_variance_boost_fallback(const AV1_COMP *cpi,
                                                const MACROBLOCK *x,
                                                const uint8_t *all_zeros) {
  unsigned int sse;
  unsigned int variances[SUBBLOCKS_IN_SB];
  aom_variance_fn_t vf = cpi->ppi->fn_ptr[BLOCK_8X8].vf;
  for (int subb_i = 0; subb_i < SUBBLOCKS_IN_SB_DIM; subb_i++) {
    int i = subb_i * SUBBLOCK_SIZE;
    for (int subb_j = 0; subb_j < SUBBLOCKS_IN_SB_DIM; subb_j++) {
      int j = subb_j * SUBBLOCK_SIZE;
      variances[subb_i * SUBBLOCKS_IN_SB_DIM + subb_j] =
          vf(x->plane[0].src.buf + i * x->plane[0].src.stride + j,
             x->plane[0].src.stride, all_zeros, 0, &sse) /
          64;
    }
  }
  for (int i = 1; i < SUBBLOCKS_IN_SB; i++) {
    unsigned int v = variances[i];
    int j = i;
    while (j > 0 && variances[j - 1] > v) {
      variances[j] = variances[j - 1];
      j--;
    }
    variances[j] = v;
  }
  return (variances[31] + (variances[39] * 2) + variances[47] + 2) / 4;
}

#if HAVE_AVX2 && (defined(__GNUC__) || defined(__clang__)) && \
    (defined(__x86_64__) || defined(__i386__))
__attribute__((target("avx2"))) static unsigned int get_variance_boost_avx2(
    const MACROBLOCK *x) {
  unsigned int variances[SUBBLOCKS_IN_SB];
  const int stride = x->plane[0].src.stride;
  const uint8_t *src = x->plane[0].src.buf;
  const __m256i zero = _mm256_setzero_si256();

  for (int idx = 0; idx < SUBBLOCKS_IN_SB; idx += 4) {
    const uint8_t *p = src + (idx & ~7) * stride + ((idx & 7) << 3);
    __m256i vsum = zero;
    __m256i sse0 = zero;
    __m256i sse1 = zero;
    for (int r = 0; r < 8; r++, p += stride) {
      const __m256i row = _mm256_loadu_si256((const __m256i *)p);
      vsum = _mm256_add_epi32(vsum, _mm256_sad_epu8(row, zero));
      const __m256i lo = _mm256_unpacklo_epi8(row, zero);
      const __m256i hi = _mm256_unpackhi_epi8(row, zero);
      sse0 = _mm256_add_epi32(sse0, _mm256_madd_epi16(lo, lo));
      sse1 = _mm256_add_epi32(sse1, _mm256_madd_epi16(hi, hi));
    }
    __m256i sse_pair = _mm256_add_epi32(_mm256_unpacklo_epi64(sse0, sse1),
                                        _mm256_unpackhi_epi64(sse0, sse1));
    sse_pair = _mm256_add_epi32(sse_pair, _mm256_srli_epi64(sse_pair, 32));
    const __m256i sq = _mm256_srli_epi64(_mm256_mul_epu32(vsum, vsum), 6);
    const __m256i vp = _mm256_srli_epi32(_mm256_sub_epi32(sse_pair, sq), 6);
    const __m128i lo128 = _mm256_castsi256_si128(vp);
    const __m128i hi128 = _mm256_extracti128_si256(vp, 1);
    const __m128i v4 = _mm_castps_si128(
        _mm_shuffle_ps(_mm_castsi128_ps(lo128), _mm_castsi128_ps(hi128),
                       _MM_SHUFFLE(2, 0, 2, 0)));
    _mm_storeu_si128((__m128i *)&variances[idx], v4);
  }

  for (int gap = 32; gap >= 8; gap >>= 1) {
    for (int i = 0; i < SUBBLOCKS_IN_SB - gap; i += 8) {
      const __m256i a = _mm256_loadu_si256((const __m256i *)&variances[i]);
      const __m256i b =
          _mm256_loadu_si256((const __m256i *)&variances[i + gap]);
      _mm256_storeu_si256((__m256i *)&variances[i], _mm256_min_epu32(a, b));
      _mm256_storeu_si256((__m256i *)&variances[i + gap],
                          _mm256_max_epu32(a, b));
    }
  }

  for (int i = 1; i < SUBBLOCKS_IN_SB; i++) {
    unsigned int v = variances[i];
    int j = i;
    while (j > 0 && variances[j - 1] > v) {
      variances[j] = variances[j - 1];
      j--;
    }
    variances[j] = v;
  }

  return (variances[31] + (variances[39] * 2) + variances[47] + 2) / 4;
}
#endif

unsigned int av1_get_variance_boost_block_variance(const AV1_COMP *cpi,
                                                   const MACROBLOCK *x) {
  DECLARE_ALIGNED(16, static const uint16_t,
                  av1_highbd_all_zeros[SUBBLOCK_SIZE]) = { 0 };
  DECLARE_ALIGNED(16, static const uint8_t,
                  av1_all_zeros[SUBBLOCK_SIZE]) = { 0 };

  const MACROBLOCKD *xd = &x->e_mbd;
#if HAVE_AVX2 && (defined(__GNUC__) || defined(__clang__)) && \
    (defined(__x86_64__) || defined(__i386__))
  if (LIKELY(!is_cur_buf_hbd(xd) &&
             cpi->ppi->fn_ptr[BLOCK_16X16].vf == aom_variance16x16_avx2)) {
    (void)av1_all_zeros;
    (void)av1_highbd_all_zeros;
    return get_variance_boost_avx2(x);
  }
#endif
  const uint8_t *all_zeros = is_cur_buf_hbd(xd)
                                 ? CONVERT_TO_BYTEPTR(av1_highbd_all_zeros)
                                 : av1_all_zeros;
  return get_variance_boost_fallback(cpi, x, all_zeros);
}
#endif  // !CONFIG_REALTIME_ONLY

int av1_log_block_var(const AV1_COMP *cpi, const MACROBLOCK *x, BLOCK_SIZE bs) {
  DECLARE_ALIGNED(16, static const uint16_t,
                  av1_highbd_all_zeros[MAX_SB_SIZE]) = { 0 };
  DECLARE_ALIGNED(16, static const uint8_t, av1_all_zeros[MAX_SB_SIZE]) = { 0 };

  // This function returns a score for the blocks local variance as calculated
  // by: sum of the log of the (4x4 variances) of each subblock to the current
  // block (x,bs)
  // * 32 / number of pixels in the block_size.
  // This is used for segmentation because to avoid situations in which a large
  // block with a gentle gradient gets marked high variance even though each
  // subblock has a low variance.   This allows us to assign the same segment
  // number for the same sorts of area regardless of how the partitioning goes.

  const MACROBLOCKD *xd = &x->e_mbd;
  double var = 0;
  unsigned int sse;
  int i, j;

  int right_overflow =
      (xd->mb_to_right_edge < 0) ? ((-xd->mb_to_right_edge) >> 3) : 0;
  int bottom_overflow =
      (xd->mb_to_bottom_edge < 0) ? ((-xd->mb_to_bottom_edge) >> 3) : 0;

  const int bw = MI_SIZE * mi_size_wide[bs] - right_overflow;
  const int bh = MI_SIZE * mi_size_high[bs] - bottom_overflow;

  aom_variance_fn_t vf = cpi->ppi->fn_ptr[BLOCK_4X4].vf;
  for (i = 0; i < bh; i += 4) {
    for (j = 0; j < bw; j += 4) {
      if (is_cur_buf_hbd(xd)) {
        var += log1p(vf(x->plane[0].src.buf + i * x->plane[0].src.stride + j,
                        x->plane[0].src.stride,
                        CONVERT_TO_BYTEPTR(av1_highbd_all_zeros), 0, &sse) /
                     16.0);
      } else {
        var += log1p(vf(x->plane[0].src.buf + i * x->plane[0].src.stride + j,
                        x->plane[0].src.stride, av1_all_zeros, 0, &sse) /
                     16.0);
      }
    }
  }
  // Use average of 4x4 log variance. The range for 8 bit 0 - 9.704121561.
  var /= (bw / 4 * bh / 4);
  if (var > 7) var = 7;

  return (int)(var);
}
