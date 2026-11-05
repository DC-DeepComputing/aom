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

#include "av1/encoder/encodetxb.h"

#include <stdint.h>

#include "aom_ports/mem.h"
#include "av1/common/blockd.h"
#include "av1/common/idct.h"
#include "av1/common/pred_common.h"
#include "av1/common/scan.h"
#include "av1/encoder/bitstream.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/encoder_utils.h"
#include "av1/encoder/hash.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/tokenize.h"

#if defined(__GNUC__) || defined(__clang__)
#define TXB_RESTRICT __restrict
#else
#define TXB_RESTRICT
#endif

#if HAVE_SSE2 && defined(__SSE2__) && !CONFIG_ENTROPY_STATS
DECLARE_ALIGNED(32, static const uint64_t,
                kCdf4Mask[4]) = { 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFF0000ULL,
                                  0xFFFFFFFF00000000ULL,
                                  0xFFFF000000000000ULL };
#endif

void av1_alloc_txb_buf(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;
  CoeffBufferPool *coeff_buf_pool = &cpi->coeff_buffer_pool;
  int mi_rows = cm->mi_params.mi_rows;
  int mi_cols = cm->mi_params.mi_cols;
  if (cpi->oxcf.frm_dim_cfg.forced_max_frame_width) {
    mi_cols = size_in_mi(cpi->oxcf.frm_dim_cfg.forced_max_frame_width);
  }
  if (cpi->oxcf.frm_dim_cfg.forced_max_frame_height) {
    mi_rows = size_in_mi(cpi->oxcf.frm_dim_cfg.forced_max_frame_height);
  }
  const int num_sb_rows =
      CEIL_POWER_OF_TWO(mi_rows, cm->seq_params->mib_size_log2);
  const int num_sb_cols =
      CEIL_POWER_OF_TWO(mi_cols, cm->seq_params->mib_size_log2);
  const int size = num_sb_rows * num_sb_cols;
  const int num_planes = av1_num_planes(cm);
  const int subsampling_x = cm->seq_params->subsampling_x;
  const int subsampling_y = cm->seq_params->subsampling_y;
  const int luma_max_sb_square =
      1 << num_pels_log2_lookup[cm->seq_params->sb_size];
  const int chroma_max_sb_square =
      luma_max_sb_square >> (subsampling_x + subsampling_y);
  const int total_max_sb_square =
      (luma_max_sb_square + (num_planes - 1) * chroma_max_sb_square);
  if ((size_t)size > SIZE_MAX / (size_t)total_max_sb_square) {
    aom_internal_error(cm->error, AOM_CODEC_ERROR,
                       "A multiplication would overflow size_t");
  }
  const size_t num_tcoeffs = (size_t)size * (size_t)total_max_sb_square;
  const int txb_unit_size = TX_SIZE_W_MIN * TX_SIZE_H_MIN;

  av1_free_txb_buf(cpi);
  // TODO(jingning): This should be further reduced.
  CHECK_MEM_ERROR(cm, cpi->coeff_buffer_base,
                  aom_malloc(sizeof(*cpi->coeff_buffer_base) * size));
  cpi->coeff_buffer_stride = num_sb_cols;
  if (sizeof(*coeff_buf_pool->tcoeff) > SIZE_MAX / num_tcoeffs) {
    aom_internal_error(cm->error, AOM_CODEC_ERROR,
                       "A multiplication would overflow size_t");
  }
  CHECK_MEM_ERROR(
      cm, coeff_buf_pool->tcoeff,
      aom_memalign(32, sizeof(*coeff_buf_pool->tcoeff) * num_tcoeffs));
  if (sizeof(*coeff_buf_pool->eobs) > SIZE_MAX / num_tcoeffs) {
    aom_internal_error(cm->error, AOM_CODEC_ERROR,
                       "A multiplication would overflow size_t");
  }
  CHECK_MEM_ERROR(
      cm, coeff_buf_pool->eobs,
      aom_malloc(sizeof(*coeff_buf_pool->eobs) * num_tcoeffs / txb_unit_size));
  if (sizeof(*coeff_buf_pool->entropy_ctx) > SIZE_MAX / num_tcoeffs) {
    aom_internal_error(cm->error, AOM_CODEC_ERROR,
                       "A multiplication would overflow size_t");
  }
  CHECK_MEM_ERROR(cm, coeff_buf_pool->entropy_ctx,
                  aom_malloc(sizeof(*coeff_buf_pool->entropy_ctx) *
                             num_tcoeffs / txb_unit_size));

  tran_low_t *tcoeff_ptr = coeff_buf_pool->tcoeff;
  uint16_t *eob_ptr = coeff_buf_pool->eobs;
  uint8_t *entropy_ctx_ptr = coeff_buf_pool->entropy_ctx;
  for (int i = 0; i < size; i++) {
    for (int plane = 0; plane < num_planes; plane++) {
      const int max_sb_square =
          (plane == AOM_PLANE_Y) ? luma_max_sb_square : chroma_max_sb_square;
      cpi->coeff_buffer_base[i].tcoeff[plane] = tcoeff_ptr;
      cpi->coeff_buffer_base[i].eobs[plane] = eob_ptr;
      cpi->coeff_buffer_base[i].entropy_ctx[plane] = entropy_ctx_ptr;
      tcoeff_ptr += max_sb_square;
      eob_ptr += max_sb_square / txb_unit_size;
      entropy_ctx_ptr += max_sb_square / txb_unit_size;
    }
  }
}

void av1_free_txb_buf(AV1_COMP *cpi) {
  CoeffBufferPool *coeff_buf_pool = &cpi->coeff_buffer_pool;
  aom_free(cpi->coeff_buffer_base);
  cpi->coeff_buffer_base = NULL;
  cpi->coeff_buffer_stride = 0;
  aom_free(coeff_buf_pool->tcoeff);
  coeff_buf_pool->tcoeff = NULL;
  aom_free(coeff_buf_pool->eobs);
  coeff_buf_pool->eobs = NULL;
  aom_free(coeff_buf_pool->entropy_ctx);
  coeff_buf_pool->entropy_ctx = NULL;
}

static void write_golomb(aom_writer *w, int level) {
  int x = level + 1;
  int i = x;
  int length = 0;

  while (i) {
    i >>= 1;
    ++length;
  }
  assert(length > 0);

  for (i = 0; i < length - 1; ++i) aom_write_bit(w, 0);

  for (i = length - 1; i >= 0; --i) aom_write_bit(w, (x >> i) & 0x01);
}

int av1_get_eob_pos_token(const int eob, int *const extra) {
  if (eob < 3) {
    *extra = 0;
    return eob;
  }
  const int d = eob - 1;
  const int k = get_msb(d);
  *extra = d - (1 << k);
  return k + 2;
}

static AOM_FORCE_INLINE void update_cdf_2_local(aom_cdf_prob *cdf, int val) {
  const int count = cdf[2];
  const int mask = val - 1;
  const int c = cdf[0];
  if (LIKELY(count == 32)) {
    cdf[0] = (aom_cdf_prob)(c - mask + (((mask | CDF_PROB_TOP) - c) >> 6));
  } else {
    const int rate = 4 + (count >> 4);
    cdf[0] = (aom_cdf_prob)(c - mask + (((mask | CDF_PROB_TOP) - c) >> rate));
    cdf[2] = (aom_cdf_prob)(count + (count < 32));
  }
}

#if HAVE_SSE2 && defined(__SSE2__) && !CONFIG_ENTROPY_STATS
static AOM_FORCE_INLINE void update_cdf_3_mask(aom_cdf_prob *cdf, uint32_t val,
                                               const uint64_t *mask_tab,
                                               __m128i top_vec) {
  const int count = cdf[3];
  int32_t cdf32;
  memcpy(&cdf32, cdf, sizeof(cdf32));
  __m128i c0 = _mm_cvtsi32_si128(cdf32);
  const __m128i m0 = _mm_loadl_epi64((const __m128i *)&mask_tab[val]);
  const __m128i diff = _mm_sub_epi16(_mm_or_si128(m0, top_vec), c0);
  __m128i delta;
  if (LIKELY(count == 32)) {
    delta = _mm_srai_epi16(diff, 6);
  } else {
    delta = _mm_sra_epi16(diff, _mm_cvtsi32_si128(4 + (count >> 4)));
    cdf[3] = (aom_cdf_prob)(count + (count < 32));
  }
  c0 = _mm_add_epi16(_mm_sub_epi16(c0, m0), delta);
  cdf32 = _mm_cvtsi128_si32(c0);
  memcpy(cdf, &cdf32, sizeof(cdf32));
}

static AOM_FORCE_INLINE void update_cdf_4_mask(aom_cdf_prob *cdf, uint32_t val,
                                               const uint64_t *mask_tab,
                                               __m128i top_vec) {
  const int count = cdf[4];
  __m128i c0 = _mm_loadl_epi64((const __m128i *)cdf);
  const __m128i m0 = _mm_loadl_epi64((const __m128i *)&mask_tab[val]);
  const __m128i diff = _mm_sub_epi16(_mm_or_si128(m0, top_vec), c0);
  __m128i delta;
  if (LIKELY(count == 32)) {
    delta = _mm_srai_epi16(diff, 7);
  } else {
    delta = _mm_sra_epi16(diff, _mm_cvtsi32_si128(5 + (count >> 4)));
    cdf[4] = (aom_cdf_prob)(count + (count < 32));
  }
  c0 = _mm_add_epi16(_mm_sub_epi16(c0, m0), delta);
  _mm_storel_epi64((__m128i *)cdf, c0);
}

static inline void update_cdf_eob_sse2(aom_cdf_prob *cdf, int8_t val,
                                       int nsyms) {
  const int count = cdf[nsyms];
  const __m128i top = _mm_set1_epi16((int16_t)CDF_PROB_TOP);
  const __m128i idx = _mm_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8);
  if (nsyms == 5) {
    __m128i c0 = _mm_loadl_epi64((const __m128i *)cdf);
    const __m128i sym_vec = _mm_shufflelo_epi16(_mm_cvtsi32_si128(val), 0);
    const __m128i m0 = _mm_cmpgt_epi16(idx, sym_vec);
    const __m128i diff = _mm_sub_epi16(_mm_or_si128(m0, top), c0);
    __m128i delta;
    if (LIKELY(count == 32)) {
      delta = _mm_srai_epi16(diff, 7);
    } else {
      delta = _mm_sra_epi16(diff, _mm_cvtsi32_si128(5 + (count >> 4)));
      cdf[5] = (aom_cdf_prob)(count + (count < 32));
    }
    c0 = _mm_add_epi16(_mm_sub_epi16(c0, m0), delta);
    _mm_storel_epi64((__m128i *)cdf, c0);
    return;
  }
  const __m128i sym_vec = _mm_set1_epi16(val);
  __m128i c0 = _mm_loadu_si128((const __m128i *)cdf);
  const __m128i m0 = _mm_cmpgt_epi16(idx, sym_vec);
  const __m128i diff = _mm_sub_epi16(_mm_or_si128(m0, top), c0);
  int rate_scalar = 7;
  __m128i delta;
  if (LIKELY(count == 32)) {
    delta = _mm_srai_epi16(diff, 7);
  } else {
    rate_scalar = 5 + (count >> 4);
    delta = _mm_sra_epi16(diff, _mm_cvtsi32_si128(rate_scalar));
  }
  c0 = _mm_add_epi16(_mm_sub_epi16(c0, m0), delta);
  if (nsyms == 6) {
    _mm_storel_epi64((__m128i *)cdf, c0);
    cdf[4] = (aom_cdf_prob)_mm_extract_epi16(c0, 4);
  } else {
    _mm_storeu_si128((__m128i *)cdf, c0);
    if (UNLIKELY(nsyms >= 10)) {
      const int c8 = cdf[8];
      const int m8 = (val - 9) >> 31;
      cdf[8] =
          (aom_cdf_prob)(c8 - m8 + (((m8 | CDF_PROB_TOP) - c8) >> rate_scalar));
      if (nsyms == 11) {
        const int c9 = cdf[9];
        const int m9 = (val - 10) >> 31;
        cdf[9] = (aom_cdf_prob)(c9 - m9 +
                                (((m9 | CDF_PROB_TOP) - c9) >> rate_scalar));
      }
    }
  }
  if (UNLIKELY(count < 32)) cdf[nsyms] = (aom_cdf_prob)(count + 1);
}
#endif

#if HAVE_SSE2 && defined(__SSE2__)
static inline void update_cdf_ext_tx_intra_sse2(aom_cdf_prob *cdf, int8_t val,
                                                int nsymbs) {
  const int count = cdf[nsymbs];
  const __m128i rate = _mm_cvtsi32_si128(5 + (count >> 4));
  const __m128i sym_vec = _mm_set1_epi16(val);
  const __m128i top = _mm_set1_epi16((int16_t)CDF_PROB_TOP);
  __m128i c0 = _mm_loadu_si128((const __m128i *)cdf);
  const __m128i m0 =
      _mm_cmpgt_epi16(_mm_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8), sym_vec);
  c0 = _mm_add_epi16(
      _mm_sub_epi16(c0, m0),
      _mm_sra_epi16(_mm_sub_epi16(_mm_or_si128(m0, top), c0), rate));
  _mm_storeu_si128((__m128i *)cdf, c0);
  cdf[nsymbs] = (aom_cdf_prob)(count + (count < 32));
}

static inline void update_cdf_ext_tx_inter_sse2(aom_cdf_prob *cdf, int8_t val,
                                                int nsymbs) {
  const int count = cdf[nsymbs];
  const __m128i rate = _mm_cvtsi32_si128(4 + (count >> 4) + (nsymbs > 3));
  const __m128i sym_vec = _mm_set1_epi16(val);
  const __m128i top = _mm_set1_epi16((int16_t)CDF_PROB_TOP);
  __m128i c0 = _mm_loadu_si128((const __m128i *)cdf);
  const __m128i m0 =
      _mm_cmpgt_epi16(_mm_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8), sym_vec);
  c0 = _mm_add_epi16(
      _mm_sub_epi16(c0, m0),
      _mm_sra_epi16(_mm_sub_epi16(_mm_or_si128(m0, top), c0), rate));
  _mm_storeu_si128((__m128i *)cdf, c0);
  __m128i c1 = _mm_loadu_si128((const __m128i *)(cdf + 8));
  const __m128i m1 =
      _mm_cmpgt_epi16(_mm_setr_epi16(9, 10, 11, 12, 13, 14, 15, 16), sym_vec);
  c1 = _mm_add_epi16(
      _mm_sub_epi16(c1, m1),
      _mm_sra_epi16(_mm_sub_epi16(_mm_or_si128(m1, top), c1), rate));
  _mm_storeu_si128((__m128i *)(cdf + 8), c1);
  cdf[nsymbs] = (aom_cdf_prob)(count + (count < 32));
}
#endif

#if CONFIG_ENTROPY_STATS
static void update_eob_context(int cdf_idx, int eob, TX_SIZE tx_size,
                               TX_CLASS tx_class, PLANE_TYPE plane,
                               FRAME_CONTEXT *ec_ctx, FRAME_COUNTS *counts,
                               uint8_t allow_update_cdf) {
  int eob_extra;
  const int eob_pt = av1_get_eob_pos_token(eob, &eob_extra);
  TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);

  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const int eob_multi_ctx = (tx_class == TX_CLASS_2D) ? 0 : 1;

  switch (eob_multi_size) {
    case 0:
      ++counts->eob_multi16[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
      if (allow_update_cdf)
        update_cdf(ec_ctx->eob_flag_cdf16[plane][eob_multi_ctx], eob_pt - 1, 5);
      break;
    case 1:
      ++counts->eob_multi32[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
      if (allow_update_cdf)
        update_cdf(ec_ctx->eob_flag_cdf32[plane][eob_multi_ctx], eob_pt - 1, 6);
      break;
    case 2:
      ++counts->eob_multi64[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
      if (allow_update_cdf)
        update_cdf(ec_ctx->eob_flag_cdf64[plane][eob_multi_ctx], eob_pt - 1, 7);
      break;
    case 3:
      ++counts->eob_multi128[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
      if (allow_update_cdf) {
        update_cdf(ec_ctx->eob_flag_cdf128[plane][eob_multi_ctx], eob_pt - 1,
                   8);
      }
      break;
    case 4:
      ++counts->eob_multi256[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
      if (allow_update_cdf) {
        update_cdf(ec_ctx->eob_flag_cdf256[plane][eob_multi_ctx], eob_pt - 1,
                   9);
      }
      break;
    case 5:
      ++counts->eob_multi512[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
      if (allow_update_cdf) {
        update_cdf(ec_ctx->eob_flag_cdf512[plane][eob_multi_ctx], eob_pt - 1,
                   10);
      }
      break;
    case 6:
    default:
      ++counts->eob_multi1024[cdf_idx][plane][eob_multi_ctx][eob_pt - 1];
      if (allow_update_cdf) {
        update_cdf(ec_ctx->eob_flag_cdf1024[plane][eob_multi_ctx], eob_pt - 1,
                   11);
      }
      break;
  }

  if (av1_eob_offset_bits[eob_pt] > 0) {
    int eob_ctx = eob_pt - 3;
    int eob_shift = av1_eob_offset_bits[eob_pt] - 1;
    int bit = (eob_extra & (1 << eob_shift)) ? 1 : 0;
    counts->eob_extra[cdf_idx][txs_ctx][plane][eob_pt][bit]++;
    if (allow_update_cdf)
      update_cdf(ec_ctx->eob_extra_cdf[txs_ctx][plane][eob_ctx], bit, 2);
  }
}
#else
static AOM_FORCE_INLINE aom_cdf_prob *get_eob_flag_cdf(FRAME_CONTEXT *ec_ctx,
                                                       int eob_s,
                                                       PLANE_TYPE plane,
                                                       int ctx) {
  assert(offsetof(FRAME_CONTEXT, eob_flag_cdf32) ==
         offsetof(FRAME_CONTEXT, eob_flag_cdf16) + 24 * sizeof(aom_cdf_prob));
  assert(offsetof(FRAME_CONTEXT, eob_flag_cdf64) ==
         offsetof(FRAME_CONTEXT, eob_flag_cdf16) + 52 * sizeof(aom_cdf_prob));
  assert(offsetof(FRAME_CONTEXT, eob_flag_cdf128) ==
         offsetof(FRAME_CONTEXT, eob_flag_cdf16) + 84 * sizeof(aom_cdf_prob));
  assert(offsetof(FRAME_CONTEXT, eob_flag_cdf256) ==
         offsetof(FRAME_CONTEXT, eob_flag_cdf16) + 120 * sizeof(aom_cdf_prob));
  assert(offsetof(FRAME_CONTEXT, eob_flag_cdf512) ==
         offsetof(FRAME_CONTEXT, eob_flag_cdf16) + 160 * sizeof(aom_cdf_prob));
  assert(offsetof(FRAME_CONTEXT, eob_flag_cdf1024) ==
         offsetof(FRAME_CONTEXT, eob_flag_cdf16) + 204 * sizeof(aom_cdf_prob));
  const int elem_off =
      2 * eob_s * (eob_s + 11) + (2 * (int)plane + ctx) * (eob_s + 6);
  return (aom_cdf_prob *)((char *)ec_ctx +
                          offsetof(FRAME_CONTEXT, eob_flag_cdf16) +
                          (size_t)elem_off * sizeof(aom_cdf_prob));
}

static inline void update_eob_context(int eob, TX_SIZE tx_size, TX_SIZE txs_ctx,
                                      TX_CLASS tx_class, PLANE_TYPE plane,
                                      FRAME_CONTEXT *ec_ctx) {
  int eob_extra;
  const int eob_pt = av1_get_eob_pos_token(eob, &eob_extra);
  const int eob_s = AOMMIN(txsize_log2_minus4[tx_size], 6);
  const int eob_multi_ctx = (tx_class == TX_CLASS_2D) ? 0 : 1;
  aom_cdf_prob *const eob_cdf =
      get_eob_flag_cdf(ec_ctx, eob_s, plane, eob_multi_ctx);
  const int nsyms = eob_s + 5;
#if HAVE_SSE2 && defined(__SSE2__)
  update_cdf_eob_sse2(eob_cdf, (int8_t)(eob_pt - 1), nsyms);
#else
  update_cdf(eob_cdf, (int8_t)(eob_pt - 1), nsyms);
#endif
  if (eob_pt >= 3) {
    const int eob_ctx = eob_pt - 3;
    const int bit = (eob_extra >> eob_ctx) & 1;
    update_cdf_2_local(ec_ctx->eob_extra_cdf[txs_ctx][plane][eob_ctx], bit);
  }
}
#endif

static inline int get_nz_map_ctx(const uint8_t *const levels,
                                 const int coeff_idx, const int bhl,
                                 const int width, const int scan_idx,
                                 const int is_eob, const TX_SIZE tx_size,
                                 const TX_CLASS tx_class) {
  if (is_eob) {
    if (scan_idx == 0) return 0;
    if (scan_idx <= ((width << bhl) >> 3)) return 1;
    if (scan_idx <= ((width << bhl) >> 2)) return 2;
    return 3;
  }
  const int stats =
      get_nz_mag(levels + get_padded_idx(coeff_idx, bhl), bhl, tx_class);
  return get_nz_map_ctx_from_stats(stats, coeff_idx, bhl, tx_size, tx_class);
}

void av1_txb_init_levels_c(const tran_low_t *const coeff, const int width,
                           const int height, uint8_t *const levels) {
  const int stride = height + TX_PAD_HOR;
  uint8_t *ls = levels;

  memset(levels + stride * width, 0,
         sizeof(*levels) * (TX_PAD_BOTTOM * stride + TX_PAD_END));

  for (int i = 0; i < width; i++) {
    for (int j = 0; j < height; j++) {
      *ls++ = (uint8_t)clamp(abs(coeff[i * height + j]), 0, INT8_MAX);
    }
    for (int j = 0; j < TX_PAD_HOR; j++) {
      *ls++ = 0;
    }
  }
}

void av1_get_nz_map_contexts_c(const uint8_t *const levels,
                               const int16_t *const scan, const int eob,
                               const TX_SIZE tx_size, const TX_CLASS tx_class,
                               int8_t *const coeff_contexts) {
  const int bhl = get_txb_bhl(tx_size);
  const int width = get_txb_wide(tx_size);
  for (int i = 0; i < eob; ++i) {
    const int pos = scan[i];
    coeff_contexts[pos] = get_nz_map_ctx(levels, pos, bhl, width, i,
                                         i == eob - 1, tx_size, tx_class);
  }
}

static AOM_FORCE_INLINE tran_low_t pack_txb_coeff_token(tran_low_t level,
                                                        int sign, int coeff_ctx,
                                                        int br_ctx) {
  return (tran_low_t)((uint32_t)level | ((uint32_t)sign << 20) |
                      ((uint32_t)coeff_ctx << 21) | ((uint32_t)br_ctx << 27));
}

#if !CONFIG_ENTROPY_STATS && !CONFIG_BITSTREAM_DEBUG && !OD_MEASURE_EC_OVERHEAD
#define ENCODETXB_FAST_PACK 1
#else
#define ENCODETXB_FAST_PACK 0
#endif

#if ENCODETXB_FAST_PACK
#define TXB_EOB_MASK 0x07FFu
#define TXB_FAST_MULTI_FLAG 0x0800u
#define TXB_HAS_GOLOMB_FLAG 0x1000u
#define TXB_EOB1_LVL_SHIFT 12
#define TXB_ENT_EOB_MULTI_SHIFT 6
#define TXB_ENT_DC_SIGN_SHIFT 7
#define TXB_RET_FAST 0x40000000u
#define TXB_RET_GOLOMB 0x80000000u
#define TXB_RET_SUM_MASK 0x3FFFFFFFu

// Compact 16-bit forward stream stored at (uint16_t *)tcoeff:
//   tp[0 .. eob-1]: base+BR token in coding order (c = eob-1 down to 0):
//     bits  0.. 1: symb (0..2 for EOB, 0..3 otherwise)
//     bits  2.. 7: coeff_ctx (0..41)
//     bits  8..12: br_ctx (0..20 when has_br == 1)
//     bits 13..14: first br symbol k0 (0..2 inline for lvl 3..5; 3 for lvl>=6)
//     bit  15    : has_br (1 iff level > NUM_BASE_LEVELS, i.e. (int16_t)tok <
//     0)
//   tp[eob .. eob+n_ge6-1]: uint16_t AOMMIN(lvl - 3, 12) for each lvl >= 6
//   followed by AC sign bitpack (when !TXB_HAS_GOLOMB_FLAG) or DC/AC Golomb.
static AOM_FORCE_INLINE uint16_t pack_fast_br_tok16(uint32_t base_tok,
                                                    uint32_t level, int br_ctx,
                                                    uint16_t **hi_out,
                                                    uint32_t *max_lvl) {
  const uint32_t br = level - (1u + NUM_BASE_LEVELS);
  uint32_t k0 = br;
  if (UNLIKELY(br >= 3u)) {
    k0 = 3u;
    *(*hi_out)++ = (uint16_t)AOMMIN(br, (uint32_t)COEFF_BASE_RANGE);
    if (UNLIKELY(level > *max_lvl)) *max_lvl = level;
  }
  return (uint16_t)(base_tok | 0x8000u | ((uint32_t)br_ctx << 8) | (k0 << 13));
}
#endif  // ENCODETXB_FAST_PACK

#if !CONFIG_BITSTREAM_DEBUG && !OD_MEASURE_EC_OVERHEAD
#define EC_NORMALIZE_LOCAL(enc_ptr, low_var, rng_var, cnt_var)               \
  do {                                                                       \
    const int d_ = 15 ^ get_msb(rng_var);                                    \
    const int s_ = (cnt_var) + d_;                                           \
    if (UNLIKELY(s_ >= 40)) {                                                \
      od_ec_enc_normalize_flush(enc_ptr, low_var, rng_var, d_, cnt_var, s_); \
      (low_var) = (enc_ptr)->low;                                            \
      (rng_var) = (enc_ptr)->rng;                                            \
      (cnt_var) = (enc_ptr)->cnt;                                            \
    } else {                                                                 \
      (low_var) <<= d_;                                                      \
      (rng_var) <<= d_;                                                      \
      (cnt_var) = s_;                                                        \
    }                                                                        \
  } while (0)

#define EC_ENCODE_CDF_LOCAL(enc_ptr, low_var, rng_var, cnt_var, symb_val, \
                            cdf_ptr, nsymbs_val)                          \
  do {                                                                    \
    const int s_val_ = (symb_val);                                        \
    const aom_cdf_prob *const icdf_ = (cdf_ptr);                          \
    const int nsyms_ = (nsymbs_val);                                      \
    const int gt0_ = (s_val_ > 0);                                        \
    const unsigned r_shifted_ = (rng_var) >> 8;                           \
    const unsigned fh_ = icdf_[s_val_];                                   \
    const unsigned fl_ = icdf_[s_val_ - gt0_];                            \
    assert(!gt0_ || fl_ < CDF_PROB_TOP);                                  \
    const unsigned min_prob_v_ =                                          \
        (unsigned)(EC_MIN_PROB * (nsyms_ - 1 - s_val_));                  \
    const unsigned v_ = (r_shifted_ * (uint32_t)(fh_ >> EC_PROB_SHIFT) >> \
                         (7 - EC_PROB_SHIFT)) +                           \
                        min_prob_v_;                                      \
    const unsigned u_ = (r_shifted_ * (uint32_t)(fl_ >> EC_PROB_SHIFT) >> \
                         (7 - EC_PROB_SHIFT)) +                           \
                        min_prob_v_ + EC_MIN_PROB;                        \
    const unsigned u_sel_ = gt0_ ? u_ : (rng_var);                        \
    (low_var) += (rng_var) - u_sel_;                                      \
    (rng_var) = u_sel_ - v_;                                              \
    EC_NORMALIZE_LOCAL(enc_ptr, low_var, rng_var, cnt_var);               \
  } while (0)

#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#define EC_ENCODE_CDF4_LOCAL(enc_ptr, low_var, rng_var, cnt_var, symb_val, \
                             cdf_ptr)                                      \
  do {                                                                     \
    const uint32_t s4_ = (uint32_t)(symb_val);                             \
    const aom_cdf_prob *const c4_ = (cdf_ptr);                             \
    assert(s4_ < 4u && c4_[3] == 0);                                       \
    uint64_t c64_;                                                         \
    memcpy(&c64_, c4_, sizeof(c64_));                                      \
    const uint64_t pair_ = (c64_ << 16) >> (s4_ << 4);                     \
    const unsigned r_sh_ = (rng_var) >> 8;                                 \
    const unsigned mp_ = (unsigned)(EC_MIN_PROB * (3u - s4_));             \
    const unsigned fl_q_ = (uint32_t)((uint16_t)pair_) >> EC_PROB_SHIFT;   \
    const unsigned fh_q_ =                                                 \
        ((uint32_t)(pair_ >> (16 + EC_PROB_SHIFT))) & 0x3FFu;              \
    const unsigned v_ = ((r_sh_ * fh_q_) >> (7 - EC_PROB_SHIFT)) + mp_;    \
    unsigned u_ =                                                          \
        ((r_sh_ * fl_q_) >> (7 - EC_PROB_SHIFT)) + mp_ + EC_MIN_PROB;      \
    const unsigned nz_m_ = s4_ ? ~0u : 0u;                                 \
    const unsigned diff_ = (rng_var) - u_;                                 \
    (low_var) += (od_ec_enc_window)(diff_ & nz_m_);                        \
    (rng_var) = (s4_ ? u_ : (rng_var)) - v_;                               \
    EC_NORMALIZE_LOCAL(enc_ptr, low_var, rng_var, cnt_var);                \
  } while (0)
#else
#define EC_ENCODE_CDF4_LOCAL(enc_ptr, low_var, rng_var, cnt_var, symb_val, \
                             cdf_ptr)                                      \
  EC_ENCODE_CDF_LOCAL(enc_ptr, low_var, rng_var, cnt_var, symb_val, cdf_ptr, 4)
#endif

#define EC_ENCODE_EOB_BASE_LOCAL(enc_ptr, low_var, rng_var, cnt_var, symb_val, \
                                 cdf_ptr)                                      \
  do {                                                                         \
    const int se_ = (symb_val);                                                \
    const aom_cdf_prob *const ecdf_ = (cdf_ptr);                               \
    if (LIKELY(se_ == 0)) {                                                    \
      const unsigned v_ =                                                      \
          ((((rng_var) >> 8) * (uint32_t)(ecdf_[0] >> EC_PROB_SHIFT)) >>       \
           (7 - EC_PROB_SHIFT)) +                                              \
          (EC_MIN_PROB * 2);                                                   \
      (rng_var) -= v_;                                                         \
    } else {                                                                   \
      const unsigned r_sh_ = (rng_var) >> 8;                                   \
      const unsigned fl_ = ecdf_[se_ - 1];                                     \
      const unsigned fh_ = ecdf_[se_];                                         \
      const unsigned mp_ = (unsigned)(EC_MIN_PROB * (2 - se_));                \
      const unsigned v_ = ((r_sh_ * (uint32_t)(fh_ >> EC_PROB_SHIFT)) >>       \
                           (7 - EC_PROB_SHIFT)) +                              \
                          mp_;                                                 \
      const unsigned u_ = ((r_sh_ * (uint32_t)(fl_ >> EC_PROB_SHIFT)) >>       \
                           (7 - EC_PROB_SHIFT)) +                              \
                          mp_ + EC_MIN_PROB;                                   \
      (low_var) += (rng_var) - u_;                                             \
      (rng_var) = u_ - v_;                                                     \
    }                                                                          \
    EC_NORMALIZE_LOCAL(enc_ptr, low_var, rng_var, cnt_var);                    \
  } while (0)

#define EC_ENCODE_BIN_LOCAL(enc_ptr, low_var, rng_var, cnt_var, bit_val, \
                            cdf_ptr)                                     \
  do {                                                                   \
    const unsigned bit_ = (unsigned)(bit_val);                           \
    const unsigned fh0_ = (cdf_ptr)[0];                                  \
    assert(fh0_ < CDF_PROB_TOP && (cdf_ptr)[1] == 0);                    \
    const unsigned u_ =                                                  \
        (((rng_var) >> 8) * (uint32_t)(fh0_ >> EC_PROB_SHIFT) >>         \
         (7 - EC_PROB_SHIFT)) +                                          \
        EC_MIN_PROB;                                                     \
    const unsigned diff_ = (rng_var) - u_;                               \
    (low_var) += (od_ec_enc_window)((bit_ ? diff_ : 0u));                \
    (rng_var) = bit_ ? u_ : diff_;                                       \
    EC_NORMALIZE_LOCAL(enc_ptr, low_var, rng_var, cnt_var);              \
  } while (0)

#define EC_WRITE_BIT_LOCAL(enc_ptr, low_var, rng_var, cnt_var, bit_val)  \
  do {                                                                   \
    const unsigned bit_ = (unsigned)(bit_val);                           \
    assert((rng_var) >= 32768u && (rng_var) <= 65535u);                  \
    const unsigned v_ = (((rng_var) & 0xFF00u) >> 1) + EC_MIN_PROB;      \
    const unsigned diff_ = (rng_var) - v_;                               \
    const unsigned nr_ = bit_ ? v_ : diff_;                              \
    const int d_ = 2 - (int)(nr_ >> 14);                                 \
    const int s_ = (cnt_var) + d_;                                       \
    (low_var) += (od_ec_enc_window)((bit_ ? diff_ : 0u));                \
    if (UNLIKELY(s_ >= 40)) {                                            \
      od_ec_enc_normalize_flush(enc_ptr, low_var, nr_, d_, cnt_var, s_); \
      (low_var) = (enc_ptr)->low;                                        \
      (rng_var) = (enc_ptr)->rng;                                        \
      (cnt_var) = (enc_ptr)->cnt;                                        \
    } else {                                                             \
      (low_var) <<= d_;                                                  \
      (rng_var) = nr_ << d_;                                             \
      (cnt_var) = s_;                                                    \
    }                                                                    \
  } while (0)

static void write_sym_slow(aom_writer *w, int symb, aom_cdf_prob *cdf,
                           int nsymbs) {
  aom_write_symbol(w, symb, cdf, nsymbs);
}

static void write_coeffs_txb_multi_slow(aom_writer *w, FRAME_CONTEXT *ec_ctx,
                                        const tran_low_t *tcoeff,
                                        int eob_and_flags, int plane_and_ctx,
                                        TX_SIZE txs_ctx) {
  const PLANE_TYPE plane_type = (PLANE_TYPE)(plane_and_ctx & 0xFF);
  const TX_SIZE br_txs_ctx = AOMMIN(txs_ctx, TX_32X32);
  aom_cdf_prob(*const base_cdfs)[CDF_SIZE(4)] =
      ec_ctx->coeff_base_cdf[txs_ctx][plane_type];
  aom_cdf_prob(*const br_cdfs)[CDF_SIZE(BR_CDF_SIZE)] =
      ec_ctx->coeff_br_cdf[br_txs_ctx][plane_type];
#if ENCODETXB_FAST_PACK
  const int eob = eob_and_flags & (int)TXB_EOB_MASK;
  if (eob_and_flags & (int)TXB_FAST_MULTI_FLAG) {
    const uint16_t *const tp = (const uint16_t *)tcoeff;
    const uint16_t *hi_ptr = tp + eob;
    for (int i = 0; i < eob; ++i) {
      const uint16_t tok = tp[i];
      const int symb = (int)(tok & 3u);
      const uint32_t coeff_ctx = (tok >> 2) & 0x3Fu;
      if (i == 0) {
        write_sym_slow(
            w, symb, ec_ctx->coeff_base_eob_cdf[txs_ctx][plane_type][coeff_ctx],
            3);
      } else {
        write_sym_slow(w, symb, base_cdfs[coeff_ctx], 4);
      }
      if ((int16_t)tok < 0) {
        aom_cdf_prob *const cdf = br_cdfs[(tok >> 8) & 0x1Fu];
        const int k0 = (int)((tok >> 13) & 3u);
        write_sym_slow(w, k0, cdf, BR_CDF_SIZE);
        if (k0 == 3) {
          const int br = (int)(*hi_ptr++);
          for (int idx = 3; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
            const int k = AOMMIN(br - idx, BR_CDF_SIZE - 1);
            write_sym_slow(w, k, cdf, BR_CDF_SIZE);
            if (k < BR_CDF_SIZE - 1) break;
          }
        }
      }
    }
    const uint16_t tok0 = tp[eob - 1];
    if ((tok0 & 3u) != 0) {
      const int sign0 = (plane_and_ctx >> (8 + TXB_ENT_DC_SIGN_SHIFT)) & 1;
      const int dc_sign_ctx =
          (plane_and_ctx >> (8 + DC_SIGN_CTX_SHIFT)) & DC_SIGN_CTX_MASK;
      write_sym_slow(w, sign0, ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx], 2);
      if (eob_and_flags & (int)TXB_HAS_GOLOMB_FLAG) {
        const uint32_t dc_g = *hi_ptr++;
        if (dc_g != 0) write_golomb(w, (int)dc_g - 1);
      }
    }
    uint32_t n_ac = *hi_ptr++;
    if (!(eob_and_flags & (int)TXB_HAS_GOLOMB_FLAG)) {
      do {
        uint32_t sbits = *hi_ptr++;
        uint32_t chunk = (n_ac < 16u) ? n_ac : 16u;
        n_ac -= chunk;
        do {
          aom_write_bit(w, (int)(sbits & 1u));
          sbits >>= 1;
        } while (--chunk != 0);
      } while (n_ac != 0);
    } else {
      do {
        const uint32_t entry = *hi_ptr++;
        aom_write_bit(w, (int)(entry & 1u));
        if (entry >= 2u) write_golomb(w, (int)(entry >> 1) - 1);
      } while (--n_ac != 0);
    }
    return;
  }
#else
  const int eob = eob_and_flags;
#endif
  for (int c = eob - 1; c >= 0; --c) {
    const uint32_t tok = (uint32_t)tcoeff[c];
    const tran_low_t level = (tran_low_t)(tok & 0xFFFFFu);
    const uint32_t coeff_ctx = (tok >> 21) & 0x3Fu;
    if (c == eob - 1) {
      write_sym_slow(w, AOMMIN(level, 3) - 1,
                     ec_ctx->coeff_base_eob_cdf[txs_ctx][plane_type][coeff_ctx],
                     3);
    } else {
      write_sym_slow(w, AOMMIN(level, 3), base_cdfs[coeff_ctx], 4);
    }
    if (UNLIKELY(level > NUM_BASE_LEVELS)) {
      const int base_range = level - 1 - NUM_BASE_LEVELS;
      aom_cdf_prob *const cdf = br_cdfs[tok >> 27];
      for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        const int k = AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
        write_sym_slow(w, k, cdf, BR_CDF_SIZE);
        if (k < BR_CDF_SIZE - 1) break;
      }
    }
  }
  for (int c = 0; c < eob; ++c) {
    const uint32_t tok = (uint32_t)tcoeff[c];
    const tran_low_t level = (tran_low_t)(tok & 0xFFFFFu);
    if (level) {
      const int sign = (int)((tok >> 20) & 1u);
      if (c == 0) {
        const int dc_sign_ctx =
            (plane_and_ctx >> (8 + DC_SIGN_CTX_SHIFT)) & DC_SIGN_CTX_MASK;
        write_sym_slow(w, sign, ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx],
                       2);
      } else {
        aom_write_bit(w, sign);
      }
      if (UNLIKELY(level > COEFF_BASE_RANGE + NUM_BASE_LEVELS))
        write_golomb(w, level - COEFF_BASE_RANGE - 1 - NUM_BASE_LEVELS);
    }
  }
}
#endif

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, aligned(64)))
#endif
static void write_coeffs_txb_multi(aom_writer *TXB_RESTRICT w,
                                   FRAME_CONTEXT *TXB_RESTRICT ec_ctx,
                                   const tran_low_t *TXB_RESTRICT tcoeff,
                                   int eob_and_flags, int plane_and_ctx,
                                   TX_SIZE txs_ctx) {
  const PLANE_TYPE plane_type = (PLANE_TYPE)(plane_and_ctx & 0xFF);
  const TX_SIZE br_txs_ctx = AOMMIN(txs_ctx, TX_32X32);
#if ENCODETXB_FAST_PACK
  if (UNLIKELY(!w->allow_update_cdf ||
               !(eob_and_flags & (int)TXB_FAST_MULTI_FLAG))) {
    write_coeffs_txb_multi_slow(w, ec_ctx, tcoeff, eob_and_flags, plane_and_ctx,
                                txs_ctx);
    return;
  }
  const intptr_t eob = (intptr_t)(eob_and_flags & (int)TXB_EOB_MASK);
  const uint16_t *TXB_RESTRICT const tp = (const uint16_t *)tcoeff;
  const uint16_t *TXB_RESTRICT hi_ptr = tp + eob;
  od_ec_enc *TXB_RESTRICT const enc = &w->ec;
  od_ec_enc_window low = enc->low;
  unsigned rng = enc->rng;
  int cnt = enc->cnt;
  aom_cdf_prob(*base_cdfs)[CDF_SIZE(4)] =
      ec_ctx->coeff_base_cdf[txs_ctx][plane_type];
  aom_cdf_prob(*const br_cdfs)[CDF_SIZE(BR_CDF_SIZE)] =
      ec_ctx->coeff_br_cdf[br_txs_ctx][plane_type];
#if HAVE_SSE2 && defined(__SSE2__)
  const uint64_t *const k_mask = kCdf4Mask;
  const __m128i top_vec = _mm_set1_epi16((int16_t)CDF_PROB_TOP);
#endif
  {
    const uint16_t tok = tp[0];
    const uint32_t symb = tok & 3u;
    const uint32_t coeff_ctx = (tok >> 2) & 0x3Fu;
    aom_cdf_prob *const eob_cdf =
        ec_ctx->coeff_base_eob_cdf[txs_ctx][plane_type][coeff_ctx];
    EC_ENCODE_EOB_BASE_LOCAL(enc, low, rng, cnt, (int)symb, eob_cdf);
#if HAVE_SSE2 && defined(__SSE2__)
    update_cdf_3_mask(eob_cdf, symb, k_mask, top_vec);
#else
    update_cdf(eob_cdf, (int8_t)symb, 3);
#endif
    if (UNLIKELY((int16_t)tok < 0)) {
      aom_cdf_prob *const cdf = br_cdfs[(tok >> 8) & 0x1Fu];
      const uint32_t k0 = (tok >> 13) & 3u;
      EC_ENCODE_CDF4_LOCAL(enc, low, rng, cnt, k0, cdf);
#if HAVE_SSE2 && defined(__SSE2__)
      update_cdf_4_mask(cdf, k0, k_mask, top_vec);
#else
      update_cdf(cdf, (int8_t)k0, BR_CDF_SIZE);
#endif
      if (UNLIKELY(k0 == 3u)) {
        const int br = (int)(*hi_ptr++);
        for (int idx = 3; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
          const uint32_t k = (uint32_t)AOMMIN(br - idx, BR_CDF_SIZE - 1);
          EC_ENCODE_CDF4_LOCAL(enc, low, rng, cnt, k, cdf);
#if HAVE_SSE2 && defined(__SSE2__)
          update_cdf_4_mask(cdf, k, k_mask, top_vec);
#else
          update_cdf(cdf, (int8_t)k, BR_CDF_SIZE);
#endif
          if (k < (uint32_t)(BR_CDF_SIZE - 1)) break;
        }
      }
    }
  }
  const uint16_t *TXB_RESTRICT cur = tp + 1;
  const uint16_t *const end = tp + eob;
  uint16_t last_tok;
  do {
    const uint16_t tok = *cur++;
    last_tok = tok;
    const uint32_t symb = tok & 3u;
    aom_cdf_prob *TXB_RESTRICT const base_cdf = base_cdfs[(tok >> 2) & 0x3Fu];
    EC_ENCODE_CDF4_LOCAL(enc, low, rng, cnt, symb, base_cdf);
#if HAVE_SSE2 && defined(__SSE2__)
    update_cdf_4_mask(base_cdf, symb, k_mask, top_vec);
#else
    update_cdf(base_cdf, (int8_t)symb, 4);
#endif
    if (UNLIKELY((int16_t)tok < 0)) {
      aom_cdf_prob *TXB_RESTRICT const cdf = br_cdfs[(tok >> 8) & 0x1Fu];
      const uint32_t k0 = (tok >> 13) & 3u;
      EC_ENCODE_CDF4_LOCAL(enc, low, rng, cnt, k0, cdf);
#if HAVE_SSE2 && defined(__SSE2__)
      update_cdf_4_mask(cdf, k0, k_mask, top_vec);
#else
      update_cdf(cdf, (int8_t)k0, BR_CDF_SIZE);
#endif
      if (UNLIKELY(k0 == 3u)) {
        const int br = (int)(*hi_ptr++);
        for (int idx = 3; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
          const uint32_t k = (uint32_t)AOMMIN(br - idx, BR_CDF_SIZE - 1);
          EC_ENCODE_CDF4_LOCAL(enc, low, rng, cnt, k, cdf);
#if HAVE_SSE2 && defined(__SSE2__)
          update_cdf_4_mask(cdf, k, k_mask, top_vec);
#else
          update_cdf(cdf, (int8_t)k, BR_CDF_SIZE);
#endif
          if (k < (uint32_t)(BR_CDF_SIZE - 1)) break;
        }
      }
    }
  } while (cur != end);

  if ((last_tok & 3u) != 0) {
    const int sign0 = (plane_and_ctx >> (8 + TXB_ENT_DC_SIGN_SHIFT)) & 1;
    const int dc_sign_ctx =
        (plane_and_ctx >> (8 + DC_SIGN_CTX_SHIFT)) & DC_SIGN_CTX_MASK;
    aom_cdf_prob *const dc_cdf = ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx];
    EC_ENCODE_BIN_LOCAL(enc, low, rng, cnt, sign0, dc_cdf);
    update_cdf_2_local(dc_cdf, sign0);
    if (UNLIKELY(eob_and_flags & (int)TXB_HAS_GOLOMB_FLAG)) {
      const uint32_t dc_g = *hi_ptr++;
      if (dc_g != 0) {
        enc->low = low;
        enc->rng = (uint16_t)rng;
        enc->cnt = (int16_t)cnt;
        write_golomb(w, (int)dc_g - 1);
        low = enc->low;
        rng = enc->rng;
        cnt = enc->cnt;
      }
    }
  }

  uint32_t n_ac = *hi_ptr++;
  if (LIKELY(!(eob_and_flags & (int)TXB_HAS_GOLOMB_FLAG))) {
    do {
      uint32_t sbits = *hi_ptr++;
      uint32_t chunk = (n_ac < 16u) ? n_ac : 16u;
      n_ac -= chunk;
      if (LIKELY(cnt + (int)(chunk * 2u) < 40)) {
        do {
          const unsigned bit = sbits & 1u;
          const unsigned v = (((rng & 0xFF00u) >> 1) + EC_MIN_PROB);
          const unsigned diff = rng - v;
          const unsigned nr = bit ? v : diff;
          const int d = 2 - (int)(nr >> 14);
          low = (low + (od_ec_enc_window)((bit ? diff : 0u))) << d;
          rng = nr << d;
          cnt += d;
          sbits >>= 1;
        } while (--chunk != 0);
      } else {
        do {
          EC_WRITE_BIT_LOCAL(enc, low, rng, cnt, sbits & 1u);
          sbits >>= 1;
        } while (--chunk != 0);
      }
    } while (UNLIKELY(n_ac != 0));
  } else {
    do {
      const uint32_t entry = *hi_ptr++;
      EC_WRITE_BIT_LOCAL(enc, low, rng, cnt, entry & 1u);
      if (UNLIKELY(entry >= 2u)) {
        enc->low = low;
        enc->rng = (uint16_t)rng;
        enc->cnt = (int16_t)cnt;
        write_golomb(w, (int)(entry >> 1) - 1);
        low = enc->low;
        rng = enc->rng;
        cnt = enc->cnt;
      }
    } while (--n_ac != 0);
  }

  enc->low = low;
  enc->rng = (uint16_t)rng;
  enc->cnt = (int16_t)cnt;
#elif !CONFIG_BITSTREAM_DEBUG && !OD_MEASURE_EC_OVERHEAD
  (void)plane_type;
  (void)br_txs_ctx;
  write_coeffs_txb_multi_slow(w, ec_ctx, tcoeff, eob_and_flags, plane_and_ctx,
                              txs_ctx);
#else
  const int eob = eob_and_flags;
  {
    const uint32_t tok = (uint32_t)tcoeff[eob - 1];
    const tran_low_t level = (tran_low_t)(tok & 0xFFFFFu);
    const int coeff_ctx = (int)((tok >> 21) & 0x3Fu);
    aom_write_symbol(w, AOMMIN(level, 3) - 1,
                     ec_ctx->coeff_base_eob_cdf[txs_ctx][plane_type][coeff_ctx],
                     3);
    if (UNLIKELY(level > NUM_BASE_LEVELS)) {
      const int base_range = level - 1 - NUM_BASE_LEVELS;
      const int br_ctx = (int)(tok >> 27);
      aom_cdf_prob *cdf = ec_ctx->coeff_br_cdf[br_txs_ctx][plane_type][br_ctx];
      for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        const int k = AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
        aom_write_symbol(w, k, cdf, BR_CDF_SIZE);
        if (k < BR_CDF_SIZE - 1) break;
      }
    }
  }
  for (int c = eob - 2; c >= 0; --c) {
    const uint32_t tok = (uint32_t)tcoeff[c];
    const tran_low_t level = (tran_low_t)(tok & 0xFFFFFu);
    const int coeff_ctx = (int)((tok >> 21) & 0x3Fu);
    aom_write_symbol(w, AOMMIN(level, 3),
                     ec_ctx->coeff_base_cdf[txs_ctx][plane_type][coeff_ctx], 4);
    if (UNLIKELY(level > NUM_BASE_LEVELS)) {
      const int base_range = level - 1 - NUM_BASE_LEVELS;
      const int br_ctx = (int)(tok >> 27);
      aom_cdf_prob *cdf = ec_ctx->coeff_br_cdf[br_txs_ctx][plane_type][br_ctx];
      for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        const int k = AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
        aom_write_symbol(w, k, cdf, BR_CDF_SIZE);
        if (k < BR_CDF_SIZE - 1) break;
      }
    }
  }
  for (int c = 0; c < eob; ++c) {
    const uint32_t tok = (uint32_t)tcoeff[c];
    const tran_low_t level = (tran_low_t)(tok & 0xFFFFFu);
    if (level) {
      const int sign = (int)((tok >> 20) & 1u);
      if (c == 0) {
        const int dc_sign_ctx =
            (plane_and_ctx >> (8 + DC_SIGN_CTX_SHIFT)) & DC_SIGN_CTX_MASK;
        aom_write_symbol(w, sign, ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx],
                         2);
      } else {
        aom_write_bit(w, sign);
      }
      if (UNLIKELY(level > COEFF_BASE_RANGE + NUM_BASE_LEVELS))
        write_golomb(w, level - COEFF_BASE_RANGE - 1 - NUM_BASE_LEVELS);
    }
  }
#endif
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((aligned(64)))
#endif
void av1_write_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCK *const x,
                          aom_writer *w, int blk_row, int blk_col, int plane,
                          int block, TX_SIZE tx_size) {
  MACROBLOCKD *const xd = &x->e_mbd;
  const CB_COEFF_BUFFER *const cb_coef_buff = x->cb_coef_buff;
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const int cb_offset = x->mbmi_ext_frame->cb_offset[plane_type];
  const int txb_idx = (cb_offset >> 4) + block;
  const uint16_t eob_raw = cb_coef_buff->eobs[plane][txb_idx];
  const uint8_t ent_ctx = cb_coef_buff->entropy_ctx[plane][txb_idx];
  const int txb_skip_ctx = ent_ctx & TXB_SKIP_CTX_MASK;
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  FRAME_CONTEXT *const ec_ctx = xd->tile_ctx;
  aom_cdf_prob *const skip_cdf = ec_ctx->txb_skip_cdf[txs_ctx][txb_skip_ctx];

#if ENCODETXB_FAST_PACK
  od_ec_enc *const enc = &w->ec;
  od_ec_enc_window low = enc->low;
  unsigned rng = enc->rng;
  int cnt = enc->cnt;
  const uint8_t allow_update_cdf = w->allow_update_cdf;
  if (eob_raw == 0) {
    const unsigned fh0 = skip_cdf[0];
    const unsigned u = (((rng >> 8) * (uint32_t)(fh0 >> EC_PROB_SHIFT)) >>
                        (7 - EC_PROB_SHIFT)) +
                       EC_MIN_PROB;
    low += rng - u;
    rng = u;
    EC_NORMALIZE_LOCAL(enc, low, rng, cnt);
    if (LIKELY(allow_update_cdf)) update_cdf_2_local(skip_cdf, 1);
    enc->low = low;
    enc->rng = (uint16_t)rng;
    enc->cnt = (int16_t)cnt;
    return;
  }

  const int eob = (int)(eob_raw & TXB_EOB_MASK);
  const tran_low_t *const tcoeff =
      cb_coef_buff->tcoeff[plane] + cb_offset + BLOCK_OFFSET(block);
#if defined(__GNUC__) || defined(__clang__)
  if (eob > 1) __builtin_prefetch(tcoeff, 0, 3);
#endif
  TX_TYPE y_tx_type = DCT_DCT;
  if (plane == 0) {
    y_tx_type = av1_get_tx_type(xd, plane_type, blk_row, blk_col, tx_size,
                                cm->features.reduced_tx_set_used);
  }

  {
    const unsigned fh0 = skip_cdf[0];
    const unsigned v = (((rng >> 8) * (uint32_t)(fh0 >> EC_PROB_SHIFT)) >>
                        (7 - EC_PROB_SHIFT)) +
                       EC_MIN_PROB;
    rng -= v;
    EC_NORMALIZE_LOCAL(enc, low, rng, cnt);
    if (LIKELY(allow_update_cdf)) update_cdf_2_local(skip_cdf, 0);
  }

  // Only y plane's tx_type is transmitted; eob_multi_ctx is packed in ent_ctx.
  if (plane == 0) {
    enc->low = low;
    enc->rng = (uint16_t)rng;
    enc->cnt = (int16_t)cnt;
    av1_write_tx_type(cm, xd, y_tx_type, tx_size, w);
    low = enc->low;
    rng = enc->rng;
    cnt = enc->cnt;
  }

  const int eob_s = AOMMIN(txsize_log2_minus4[tx_size], 6);
  const int eob_multi_ctx = (ent_ctx >> TXB_ENT_EOB_MULTI_SHIFT) & 1;
  aom_cdf_prob *const eob_cdf =
      get_eob_flag_cdf(ec_ctx, eob_s, plane_type, eob_multi_ctx);
  const int eob_nsyms = eob_s + 5;

  if (eob == 1) {
    // Code eob_pt - 1 == 0 directly without av1_get_eob_pos_token or tcoeff.
    {
      const unsigned v =
          (((rng >> 8) * (uint32_t)(eob_cdf[0] >> EC_PROB_SHIFT)) >>
           (7 - EC_PROB_SHIFT)) +
          (unsigned)(EC_MIN_PROB * (eob_nsyms - 1));
      rng -= v;
      EC_NORMALIZE_LOCAL(enc, low, rng, cnt);
      if (LIKELY(allow_update_cdf)) {
#if HAVE_SSE2 && defined(__SSE2__)
        update_cdf_eob_sse2(eob_cdf, 0, eob_nsyms);
#else
        update_cdf(eob_cdf, 0, eob_nsyms);
#endif
      }
    }
    uint32_t level = (uint32_t)(eob_raw >> TXB_EOB1_LVL_SHIFT);
    if (UNLIKELY(level == 15u)) {
      level = (uint32_t)tcoeff[0];
    }
    const uint32_t symb = AOMMIN(level, 3u) - 1u;
    aom_cdf_prob *const base_eob_cdf =
        ec_ctx->coeff_base_eob_cdf[txs_ctx][plane_type][0];
    EC_ENCODE_EOB_BASE_LOCAL(enc, low, rng, cnt, (int)symb, base_eob_cdf);
#if HAVE_SSE2 && defined(__SSE2__)
    const uint64_t *const k_mask = kCdf4Mask;
    const __m128i top_vec = _mm_set1_epi16((int16_t)CDF_PROB_TOP);
    if (LIKELY(allow_update_cdf)) {
      update_cdf_3_mask(base_eob_cdf, symb, k_mask, top_vec);
    }
#else
    if (LIKELY(allow_update_cdf)) update_cdf(base_eob_cdf, (int8_t)symb, 3);
#endif
    if (UNLIKELY(level > NUM_BASE_LEVELS)) {
      const int base_range = (int)level - 1 - NUM_BASE_LEVELS;
      aom_cdf_prob *const cdf =
          ec_ctx->coeff_br_cdf[AOMMIN(txs_ctx, TX_32X32)][plane_type][0];
      for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        const uint32_t k = (uint32_t)AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
        EC_ENCODE_CDF4_LOCAL(enc, low, rng, cnt, k, cdf);
        if (LIKELY(allow_update_cdf)) {
#if HAVE_SSE2 && defined(__SSE2__)
          update_cdf_4_mask(cdf, k, k_mask, top_vec);
#else
          update_cdf(cdf, (int8_t)k, BR_CDF_SIZE);
#endif
        }
        if (k < (uint32_t)(BR_CDF_SIZE - 1)) break;
      }
    }
    const int sign = (ent_ctx >> TXB_ENT_DC_SIGN_SHIFT) & 1;
    const int dc_sign_ctx = (ent_ctx >> DC_SIGN_CTX_SHIFT) & DC_SIGN_CTX_MASK;
    aom_cdf_prob *const dc_cdf = ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx];
    EC_ENCODE_BIN_LOCAL(enc, low, rng, cnt, sign, dc_cdf);
    if (LIKELY(allow_update_cdf)) update_cdf_2_local(dc_cdf, sign);
    enc->low = low;
    enc->rng = (uint16_t)rng;
    enc->cnt = (int16_t)cnt;
    if (UNLIKELY(level > COEFF_BASE_RANGE + NUM_BASE_LEVELS))
      write_golomb(w, (int)level - COEFF_BASE_RANGE - 1 - NUM_BASE_LEVELS);
    return;
  }

  int eob_extra;
  const int eob_pt = av1_get_eob_pos_token(eob, &eob_extra);
  EC_ENCODE_CDF_LOCAL(enc, low, rng, cnt, eob_pt - 1, eob_cdf, eob_nsyms);
  if (LIKELY(allow_update_cdf)) {
#if HAVE_SSE2 && defined(__SSE2__)
    update_cdf_eob_sse2(eob_cdf, (int8_t)(eob_pt - 1), eob_nsyms);
#else
    update_cdf(eob_cdf, (int8_t)(eob_pt - 1), eob_nsyms);
#endif
  }

  if (eob_pt >= 3) {
    const int eob_ctx = eob_pt - 3;
    const int bit0 = (eob_extra >> eob_ctx) & 1;
    aom_cdf_prob *const extra_cdf =
        ec_ctx->eob_extra_cdf[txs_ctx][plane_type][eob_ctx];
    EC_ENCODE_BIN_LOCAL(enc, low, rng, cnt, bit0, extra_cdf);
    if (LIKELY(allow_update_cdf)) update_cdf_2_local(extra_cdf, bit0);
    for (int s = eob_ctx - 1; s >= 0; --s) {
      EC_WRITE_BIT_LOCAL(enc, low, rng, cnt, (eob_extra >> s) & 1);
    }
  }

  enc->low = low;
  enc->rng = (uint16_t)rng;
  enc->cnt = (int16_t)cnt;
  write_coeffs_txb_multi(w, ec_ctx, tcoeff, (int)eob_raw,
                         (int)plane_type | ((int)ent_ctx << 8), txs_ctx);
#else
  const uint16_t eob = eob_raw;
  aom_write_symbol(w, eob == 0, skip_cdf, 2);
  if (eob == 0) return;

  const TX_TYPE tx_type =
      av1_get_tx_type(xd, plane_type, blk_row, blk_col, tx_size,
                      cm->features.reduced_tx_set_used);
  if (plane == 0) {
    av1_write_tx_type(cm, xd, tx_type, tx_size, w);
  }

  int eob_extra;
  const int eob_pt = av1_get_eob_pos_token(eob, &eob_extra);
  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const TX_CLASS tx_class = tx_type_to_class[tx_type];
  const int eob_multi_ctx = (tx_class == TX_CLASS_2D) ? 0 : 1;
  switch (eob_multi_size) {
    case 0:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf16[plane_type][eob_multi_ctx], 5);
      break;
    case 1:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf32[plane_type][eob_multi_ctx], 6);
      break;
    case 2:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf64[plane_type][eob_multi_ctx], 7);
      break;
    case 3:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf128[plane_type][eob_multi_ctx], 8);
      break;
    case 4:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf256[plane_type][eob_multi_ctx], 9);
      break;
    case 5:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf512[plane_type][eob_multi_ctx], 10);
      break;
    default:
      aom_write_symbol(w, eob_pt - 1,
                       ec_ctx->eob_flag_cdf1024[plane_type][eob_multi_ctx], 11);
      break;
  }

  const int eob_offset_bits = av1_eob_offset_bits[eob_pt];
  if (eob_offset_bits > 0) {
    const int eob_ctx = eob_pt - 3;
    int eob_shift = eob_offset_bits - 1;
    int bit = (eob_extra & (1 << eob_shift)) ? 1 : 0;
    aom_write_symbol(w, bit,
                     ec_ctx->eob_extra_cdf[txs_ctx][plane_type][eob_ctx], 2);
    for (int i = 1; i < eob_offset_bits; i++) {
      eob_shift = eob_offset_bits - 1 - i;
      bit = (eob_extra & (1 << eob_shift)) ? 1 : 0;
      aom_write_bit(w, bit);
    }
  }

  const tran_low_t *const tcoeff =
      cb_coef_buff->tcoeff[plane] + cb_offset + BLOCK_OFFSET(block);
  if (eob == 1) {
    const tran_low_t v = tcoeff[0];
    const tran_low_t level = abs(v);
    aom_write_symbol(w, AOMMIN(level, 3) - 1,
                     ec_ctx->coeff_base_eob_cdf[txs_ctx][plane_type][0], 3);
    if (UNLIKELY(level > NUM_BASE_LEVELS)) {
      const int base_range = level - 1 - NUM_BASE_LEVELS;
      aom_cdf_prob *cdf =
          ec_ctx->coeff_br_cdf[AOMMIN(txs_ctx, TX_32X32)][plane_type][0];
      for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        const int k = AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
        aom_write_symbol(w, k, cdf, BR_CDF_SIZE);
        if (k < BR_CDF_SIZE - 1) break;
      }
    }
    const int sign = (v < 0) ? 1 : 0;
    const int dc_sign_ctx = (ent_ctx >> DC_SIGN_CTX_SHIFT) & DC_SIGN_CTX_MASK;
    aom_write_symbol(w, sign, ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx], 2);
    if (UNLIKELY(level > COEFF_BASE_RANGE + NUM_BASE_LEVELS))
      write_golomb(w, level - COEFF_BASE_RANGE - 1 - NUM_BASE_LEVELS);
    return;
  }

  write_coeffs_txb_multi(w, ec_ctx, tcoeff, eob,
                         (int)plane_type | ((int)ent_ctx << 8), txs_ctx);
#endif
}

void av1_write_intra_coeffs_mb(const AV1_COMMON *const cm, MACROBLOCK *x,
                               aom_writer *w, BLOCK_SIZE bsize) {
  MACROBLOCKD *xd = &x->e_mbd;
  const int num_planes = av1_num_planes(cm);
  int block[MAX_MB_PLANE] = { 0 };
  int row, col;
  assert(bsize == get_plane_block_size(bsize, xd->plane[0].subsampling_x,
                                       xd->plane[0].subsampling_y));
  const int max_blocks_wide = max_block_wide(xd, bsize, 0);
  const int max_blocks_high = max_block_high(xd, bsize, 0);
  const BLOCK_SIZE max_unit_bsize = BLOCK_64X64;
  const int mu_blocks_wide =
      AOMMIN(max_blocks_wide, mi_size_wide[max_unit_bsize]);
  const int mu_blocks_high =
      AOMMIN(max_blocks_high, mi_size_high[max_unit_bsize]);

  const int active_planes =
      (num_planes > 1 && xd->is_chroma_ref) ? MAX_MB_PLANE : 1;
  const TX_SIZE y_tx = av1_get_tx_size(0, xd);
  const TX_SIZE uv_tx = (active_planes > 1) ? av1_get_tx_size(1, xd) : y_tx;
#if defined(__GNUC__) || defined(__clang__)
  if (active_planes > 1) {
    const CB_COEFF_BUFFER *const cb_buf = x->cb_coef_buff;
    const int uv_idx = x->mbmi_ext_frame->cb_offset[PLANE_TYPE_UV] >> 4;
    __builtin_prefetch(cb_buf->eobs[1] + uv_idx, 0, 1);
    __builtin_prefetch(cb_buf->entropy_ctx[1] + uv_idx, 0, 1);
    __builtin_prefetch(cb_buf->eobs[2] + uv_idx, 0, 1);
    __builtin_prefetch(cb_buf->entropy_ctx[2] + uv_idx, 0, 1);
  }
#endif

  for (row = 0; row < max_blocks_high; row += mu_blocks_high) {
    const int y_unit_h = AOMMIN(mu_blocks_high + row, max_blocks_high);
    for (col = 0; col < max_blocks_wide; col += mu_blocks_wide) {
      const int y_unit_w = AOMMIN(mu_blocks_wide + col, max_blocks_wide);
      for (int plane = 0; plane < active_planes; ++plane) {
        const TX_SIZE tx_size = (plane == 0) ? y_tx : uv_tx;
        const int stepr = tx_size_high_unit[tx_size];
        const int stepc = tx_size_wide_unit[tx_size];
        const int step = stepr * stepc;
        const struct macroblockd_plane *const pd = &xd->plane[plane];
        const int unit_height = ROUND_POWER_OF_TWO(y_unit_h, pd->subsampling_y);
        const int unit_width = ROUND_POWER_OF_TWO(y_unit_w, pd->subsampling_x);
        int p_blk = block[plane];
        for (int blk_row = row >> pd->subsampling_y; blk_row < unit_height;
             blk_row += stepr) {
          for (int blk_col = col >> pd->subsampling_x; blk_col < unit_width;
               blk_col += stepc) {
            av1_write_coeffs_txb(cm, x, w, blk_row, blk_col, plane, p_blk,
                                 tx_size);
            p_blk += step;
          }
        }
        block[plane] = p_blk;
      }
    }
  }
}

static void update_tx_type_count(const AV1_COMP *cpi, const AV1_COMMON *cm,
                                 MACROBLOCKD *xd, int plane, TX_SIZE tx_size,
                                 TX_TYPE tx_type, FRAME_COUNTS *counts,
                                 uint8_t allow_update_cdf) {
  // Only y plane's tx_type is updated
  if (plane > 0) return;
  MB_MODE_INFO *mbmi = xd->mi[0];
  int is_inter = is_inter_block(mbmi);
  const int reduced_tx_set_used = cm->features.reduced_tx_set_used;
  FRAME_CONTEXT *fc = xd->tile_ctx;
#if !CONFIG_ENTROPY_STATS
  (void)counts;
#endif  // !CONFIG_ENTROPY_STATS
  (void)cpi;
#ifndef NDEBUG
  if (is_inter) {
    if (cpi->oxcf.txfm_cfg.use_inter_dct_only) {
      assert(tx_type == DCT_DCT);
    }
  } else {
    if (cpi->oxcf.txfm_cfg.use_intra_dct_only) {
      assert(tx_type == DCT_DCT);
    } else if (cpi->oxcf.txfm_cfg.use_intra_default_tx_only) {
      const TX_TYPE default_type = get_default_tx_type(
          PLANE_TYPE_Y, xd, tx_size, cpi->use_screen_content_tools);
      (void)default_type;
      assert(tx_type == default_type || cpi->sf.rt_sf.use_nonrd_pick_mode ||
             cpi->oxcf.mode == REALTIME);
    }
  }
#endif

  if (get_ext_tx_types(tx_size, is_inter, reduced_tx_set_used) > 1 &&
      cm->quant_params.base_qindex > 0 && !mbmi->skip_txfm &&
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
    const int eset = get_ext_tx_set(tx_size, is_inter, reduced_tx_set_used);
    if (eset > 0) {
      const TxSetType tx_set_type =
          av1_get_ext_tx_set_type(tx_size, is_inter, reduced_tx_set_used);
      if (is_inter) {
        if (allow_update_cdf) {
#if HAVE_SSE2 && defined(__SSE2__)
          update_cdf_ext_tx_inter_sse2(
              fc->inter_ext_tx_cdf[eset][txsize_sqr_map[tx_size]],
              av1_ext_tx_ind[tx_set_type][tx_type],
              av1_num_ext_tx_set[tx_set_type]);
#else
          update_cdf(fc->inter_ext_tx_cdf[eset][txsize_sqr_map[tx_size]],
                     av1_ext_tx_ind[tx_set_type][tx_type],
                     av1_num_ext_tx_set[tx_set_type]);
#endif
        }
#if CONFIG_ENTROPY_STATS
        ++counts->inter_ext_tx[eset][txsize_sqr_map[tx_size]]
                              [av1_ext_tx_ind[tx_set_type][tx_type]];
#endif  // CONFIG_ENTROPY_STATS
      } else {
        PREDICTION_MODE intra_dir;
        if (mbmi->filter_intra_mode_info.use_filter_intra)
          intra_dir = fimode_to_intradir[mbmi->filter_intra_mode_info
                                             .filter_intra_mode];
        else
          intra_dir = mbmi->mode;
#if CONFIG_ENTROPY_STATS
        ++counts->intra_ext_tx[eset][txsize_sqr_map[tx_size]][intra_dir]
                              [av1_ext_tx_ind[tx_set_type][tx_type]];
#endif  // CONFIG_ENTROPY_STATS
        if (allow_update_cdf) {
#if HAVE_SSE2 && defined(__SSE2__)
          update_cdf_ext_tx_intra_sse2(
              fc->intra_ext_tx_cdf[eset][txsize_sqr_map[tx_size]][intra_dir],
              av1_ext_tx_ind[tx_set_type][tx_type],
              av1_num_ext_tx_set[tx_set_type]);
#else
          update_cdf(
              fc->intra_ext_tx_cdf[eset][txsize_sqr_map[tx_size]][intra_dir],
              av1_ext_tx_ind[tx_set_type][tx_type],
              av1_num_ext_tx_set[tx_set_type]);
#endif
        }
      }
    }
  }
}

#if !CONFIG_ENTROPY_STATS
static AOM_FORCE_INLINE void update_br_cdf_loop(aom_cdf_prob *br_cdf,
                                                int base_range
#if HAVE_SSE2 && defined(__SSE2__)
                                                ,
                                                const uint64_t *mask_tab,
                                                __m128i top_vec
#endif
) {
  for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
    const uint32_t k = (uint32_t)AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
#if HAVE_SSE2 && defined(__SSE2__)
    update_cdf_4_mask(br_cdf, k, mask_tab, top_vec);
#else
    update_cdf(br_cdf, (int8_t)k, BR_CDF_SIZE);
#endif
    if (k < (uint32_t)(BR_CDF_SIZE - 1)) break;
  }
}

static uint32_t update_and_record_txb_context_multi(
    FRAME_CONTEXT *TXB_RESTRICT ec_ctx, const tran_low_t *TXB_RESTRICT qcoeff,
    tran_low_t *TXB_RESTRICT tcoeff, int eob,
    const int16_t *TXB_RESTRICT const scan, uint32_t packed_tx);

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, aligned(64)))
#endif
static uint32_t update_and_record_txb_context_2d_small(
    FRAME_CONTEXT *TXB_RESTRICT ec_ctx, const tran_low_t *TXB_RESTRICT qcoeff,
    tran_low_t *TXB_RESTRICT tcoeff, int eob,
    const int16_t *TXB_RESTRICT const scan, uint32_t packed_tx) {
  const PLANE_TYPE plane_type = (PLANE_TYPE)(packed_tx & 0xFu);
  const TX_SIZE tx_size = (TX_SIZE)((packed_tx >> 4) & 0xFFu);
  const TX_SIZE txsize_ctx = (TX_SIZE)((packed_tx >> 12) & 0xFFu);
#if HAVE_SSE2 && defined(__SSE2__)
  const uint64_t *const k_mask = kCdf4Mask;
  const __m128i top_vec = _mm_set1_epi16((int16_t)CDF_PROB_TOP);
#endif
  if (eob == 2) {
    const int pos1 = scan[1];
    const tran_low_t v1 = qcoeff[pos1];
    const tran_low_t v0 = qcoeff[0];
    if (UNLIKELY(((uint32_t)(v1 + NUM_BASE_LEVELS) > 2u * NUM_BASE_LEVELS) |
                 ((uint32_t)(v0 + NUM_BASE_LEVELS) > 2u * NUM_BASE_LEVELS))) {
      return update_and_record_txb_context_multi(ec_ctx, qcoeff, tcoeff, eob,
                                                 scan, packed_tx);
    }
    const uint32_t level1 = (uint32_t)abs(v1);
    const uint32_t level0 = (uint32_t)abs(v0);
    const uint32_t symb1 = level1 - 1u;
#if HAVE_SSE2 && defined(__SSE2__)
    update_cdf_3_mask(ec_ctx->coeff_base_eob_cdf[txsize_ctx][plane_type][1],
                      symb1, k_mask, top_vec);
    update_cdf_4_mask(ec_ctx->coeff_base_cdf[txsize_ctx][plane_type][0], level0,
                      k_mask, top_vec);
#else
    update_cdf(ec_ctx->coeff_base_eob_cdf[txsize_ctx][plane_type][1],
               (int8_t)symb1, 3);
    update_cdf(ec_ctx->coeff_base_cdf[txsize_ctx][plane_type][0],
               (int8_t)level0, 4);
#endif
#if ENCODETXB_FAST_PACK
    uint16_t *TXB_RESTRICT const out = (uint16_t *)tcoeff;
    out[0] = (uint16_t)(symb1 | (1u << 2));
    out[1] = (uint16_t)level0;
    out[2] = 1u;
    out[3] = (uint16_t)((uint32_t)v1 >> 31);
    return (level0 + level1) | TXB_RET_FAST;
#else
    tcoeff[1] = (tran_low_t)(level1 | ((uint32_t)(v1 < 0) << 20) | (1u << 21));
    tcoeff[0] = (tran_low_t)(level0 | ((uint32_t)(v0 < 0) << 20));
    return level0 + level1;
#endif
  }

  if (eob == 3) {
    const int pos2 = scan[2];
    const int pos1 = scan[1];
    const tran_low_t v2 = qcoeff[pos2];
    const tran_low_t v1 = qcoeff[pos1];
    const tran_low_t v0 = qcoeff[0];
    if (UNLIKELY(((uint32_t)(v2 + NUM_BASE_LEVELS) > 2u * NUM_BASE_LEVELS) |
                 ((uint32_t)(v1 + NUM_BASE_LEVELS) > 2u * NUM_BASE_LEVELS) |
                 ((uint32_t)(v0 + NUM_BASE_LEVELS) > 2u * NUM_BASE_LEVELS))) {
      return update_and_record_txb_context_multi(ec_ctx, qcoeff, tcoeff, eob,
                                                 scan, packed_tx);
    }
    const int bhl = get_txb_bhl(tx_size);
    const int row_mask = (1 << bhl) - 1;
    const int dc = (pos2 >> bhl) - (pos1 >> bhl);
    const int dr = (pos2 & row_mask) - (pos1 & row_mask);
    const uint32_t ctx1 =
        (uint32_t)((((dc | dr) >= 0 && dc + dr <= 2) ? 1 : 0) +
                   av1_nz_map_ctx_offset[tx_size][pos1]);
    const uint32_t level2 = (uint32_t)abs(v2);
    const uint32_t level1 = (uint32_t)abs(v1);
    const uint32_t level0 = (uint32_t)abs(v0);
    const uint32_t symb2 = level2 - 1u;
    aom_cdf_prob(*const base_cdfs)[CDF_SIZE(4)] =
        ec_ctx->coeff_base_cdf[txsize_ctx][plane_type];
#if HAVE_SSE2 && defined(__SSE2__)
    update_cdf_3_mask(ec_ctx->coeff_base_eob_cdf[txsize_ctx][plane_type][1],
                      symb2, k_mask, top_vec);
    update_cdf_4_mask(base_cdfs[ctx1], level1, k_mask, top_vec);
    update_cdf_4_mask(base_cdfs[0], level0, k_mask, top_vec);
#else
    update_cdf(ec_ctx->coeff_base_eob_cdf[txsize_ctx][plane_type][1],
               (int8_t)symb2, 3);
    update_cdf(base_cdfs[ctx1], (int8_t)level1, 4);
    update_cdf(base_cdfs[0], (int8_t)level0, 4);
#endif
#if ENCODETXB_FAST_PACK
    uint16_t *TXB_RESTRICT const out = (uint16_t *)tcoeff;
    const uint32_t nz1 = (v1 != 0);
    const uint32_t s2 = (uint32_t)v2 >> 31;
    out[0] = (uint16_t)(symb2 | (1u << 2));
    out[1] = (uint16_t)(level1 | (ctx1 << 2));
    out[2] = (uint16_t)level0;
    out[3] = (uint16_t)(1u + nz1);
    out[4] = (uint16_t)((s2 << nz1) | ((uint32_t)v1 >> 31));
    return (level0 + level1 + level2) | TXB_RET_FAST;
#else
    tcoeff[2] = (tran_low_t)(level2 | ((uint32_t)(v2 < 0) << 20) | (1u << 21));
    tcoeff[1] =
        (tran_low_t)(level1 | ((uint32_t)(v1 < 0) << 20) | (ctx1 << 21));
    tcoeff[0] = (tran_low_t)(level0 | ((uint32_t)(v0 < 0) << 20));
    return level0 + level1 + level2;
#endif
  }

  // eob == 4 && tx_class == TX_CLASS_2D
  const int pos3 = scan[3];
  const int pos2 = scan[2];
  const int pos1 = scan[1];
  const tran_low_t v3 = qcoeff[pos3];
  const tran_low_t v2 = qcoeff[pos2];
  const tran_low_t v1 = qcoeff[pos1];
  const tran_low_t v0 = qcoeff[0];
  if (UNLIKELY(((uint32_t)(v3 + NUM_BASE_LEVELS) > 2u * NUM_BASE_LEVELS) |
               ((uint32_t)(v2 + NUM_BASE_LEVELS) > 2u * NUM_BASE_LEVELS) |
               ((uint32_t)(v1 + NUM_BASE_LEVELS) > 2u * NUM_BASE_LEVELS) |
               ((uint32_t)(v0 + NUM_BASE_LEVELS) > 2u * NUM_BASE_LEVELS))) {
    return update_and_record_txb_context_multi(ec_ctx, qcoeff, tcoeff, eob,
                                               scan, packed_tx);
  }
  const int bhl = get_txb_bhl(tx_size);
  const int row_mask = (1 << bhl) - 1;
  const int c3 = pos3 >> bhl, r3 = pos3 & row_mask;
  const int c2 = pos2 >> bhl, r2 = pos2 & row_mask;
  const int c1 = pos1 >> bhl, r1 = pos1 & row_mask;
  const uint32_t level3 = (uint32_t)abs(v3);
  const uint32_t level2 = (uint32_t)abs(v2);
  const uint32_t level1 = (uint32_t)abs(v1);
  const uint32_t level0 = (uint32_t)abs(v0);
  const int8_t *const nz_off = av1_nz_map_ctx_offset[tx_size];
  const uint32_t ctx3 = 1u + (uint32_t)(tx_size == TX_4X4);
  const int dc32 = c3 - c2, dr32 = r3 - r2;
  const uint32_t st2 = (((dc32 | dr32) >= 0 && dc32 + dr32 <= 2) ? level3 : 0u);
  const uint32_t ctx2 = ((st2 + 1u) >> 1) + (uint32_t)nz_off[pos2];
  const int dc21 = c2 - c1, dr21 = r2 - r1;
  const int dc31 = c3 - c1, dr31 = r3 - r1;
  const uint32_t st1 =
      (((dc21 | dr21) >= 0 && dc21 + dr21 <= 2) ? level2 : 0u) +
      (((dc31 | dr31) >= 0 && dc31 + dr31 <= 2) ? level3 : 0u);
  const uint32_t ctx1 = ((st1 + 1u) >> 1) + (uint32_t)nz_off[pos1];
  const uint32_t symb3 = level3 - 1u;
  aom_cdf_prob(*const base_cdfs)[CDF_SIZE(4)] =
      ec_ctx->coeff_base_cdf[txsize_ctx][plane_type];
#if HAVE_SSE2 && defined(__SSE2__)
  update_cdf_3_mask(ec_ctx->coeff_base_eob_cdf[txsize_ctx][plane_type][ctx3],
                    symb3, k_mask, top_vec);
  update_cdf_4_mask(base_cdfs[ctx2], level2, k_mask, top_vec);
  update_cdf_4_mask(base_cdfs[ctx1], level1, k_mask, top_vec);
  update_cdf_4_mask(base_cdfs[0], level0, k_mask, top_vec);
#else
  update_cdf(ec_ctx->coeff_base_eob_cdf[txsize_ctx][plane_type][ctx3],
             (int8_t)symb3, 3);
  update_cdf(base_cdfs[ctx2], (int8_t)level2, 4);
  update_cdf(base_cdfs[ctx1], (int8_t)level1, 4);
  update_cdf(base_cdfs[0], (int8_t)level0, 4);
#endif
#if ENCODETXB_FAST_PACK
  uint16_t *TXB_RESTRICT const out = (uint16_t *)tcoeff;
  const uint32_t nz2 = (v2 != 0);
  const uint32_t nz1 = (v1 != 0);
  uint32_t sbits = ((uint32_t)v3 >> 31);
  sbits = (sbits << nz2) | ((uint32_t)v2 >> 31);
  sbits = (sbits << nz1) | ((uint32_t)v1 >> 31);
  out[0] = (uint16_t)(symb3 | (ctx3 << 2));
  out[1] = (uint16_t)(level2 | (ctx2 << 2));
  out[2] = (uint16_t)(level1 | (ctx1 << 2));
  out[3] = (uint16_t)level0;
  out[4] = (uint16_t)(1u + nz2 + nz1);
  out[5] = (uint16_t)sbits;
  return (level0 + level1 + level2 + level3) | TXB_RET_FAST;
#else
  tcoeff[3] = (tran_low_t)(level3 | ((uint32_t)(v3 < 0) << 20) | (ctx3 << 21));
  tcoeff[2] = (tran_low_t)(level2 | ((uint32_t)(v2 < 0) << 20) | (ctx2 << 21));
  tcoeff[1] = (tran_low_t)(level1 | ((uint32_t)(v1 < 0) << 20) | (ctx1 << 21));
  tcoeff[0] = (tran_low_t)(level0 | ((uint32_t)(v0 < 0) << 20));
  return level0 + level1 + level2 + level3;
#endif
}
#endif

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, aligned(64)))
#endif
#if CONFIG_ENTROPY_STATS
static int update_and_record_txb_context_multi(
    const AV1_COMP *cpi, ThreadData *td, FRAME_CONTEXT *ec_ctx,
    const tran_low_t *qcoeff, tran_low_t *tcoeff, int eob,
    const int16_t *const scan, uint32_t packed_tx, uint8_t allow_update_cdf) {
  const int cdf_idx = cpi->common.coef_cdf_category;
  const PLANE_TYPE plane_type = (PLANE_TYPE)(packed_tx & 0xFu);
  const TX_SIZE tx_size = (TX_SIZE)((packed_tx >> 4) & 0xFFu);
  const TX_SIZE txsize_ctx = (TX_SIZE)((packed_tx >> 12) & 0xFFu);
  const TX_CLASS tx_class = (TX_CLASS)(packed_tx >> 20);
  const int bhl = get_txb_bhl(tx_size);
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, height);
  DECLARE_ALIGNED(16, int8_t, coeff_contexts[1024]);
  av1_txb_init_levels(qcoeff, width, height, levels);
  av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class, coeff_contexts);

  const intptr_t stride = height + TX_PAD_HOR;
  int abs_sum = 0;
  {
    const int c = eob - 1;
    const int pos = scan[c];
    const int coeff_ctx = coeff_contexts[pos];
    const tran_low_t v = qcoeff[pos];
    const tran_low_t level = abs(v);
    int br_ctx = 0;
    abs_sum += level;
    assert(coeff_ctx < 4);
    if (allow_update_cdf) {
      update_cdf(ec_ctx->coeff_base_eob_cdf[txsize_ctx][plane_type][coeff_ctx],
                 AOMMIN(level, 3) - 1, 3);
    }
    ++td->counts->coeff_base_eob_multi[cdf_idx][txsize_ctx][plane_type]
                                      [coeff_ctx][AOMMIN(level, 3) - 1];
    if (UNLIKELY(level > NUM_BASE_LEVELS)) {
      const int base_range = level - 1 - NUM_BASE_LEVELS;
      br_ctx = get_br_ctx_eob(pos, bhl, tx_class);
      aom_cdf_prob *const br_cdf =
          ec_ctx
              ->coeff_br_cdf[AOMMIN(txsize_ctx, TX_32X32)][plane_type][br_ctx];
      for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        const int k = AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
        if (allow_update_cdf) {
          update_cdf(br_cdf, k, BR_CDF_SIZE);
        }
        for (int lps = 0; lps < BR_CDF_SIZE - 1; lps++) {
          ++td->counts->coeff_lps[AOMMIN(txsize_ctx, TX_32X32)][plane_type][lps]
                                 [br_ctx][lps == k];
          if (lps == k) break;
        }
        ++td->counts->coeff_lps_multi[cdf_idx][AOMMIN(txsize_ctx, TX_32X32)]
                                     [plane_type][br_ctx][k];
        if (k < BR_CDF_SIZE - 1) break;
      }
    }
    tcoeff[c] = pack_txb_coeff_token(level, v < 0, coeff_ctx, br_ctx);
  }
  for (int c = eob - 2; c >= 1; --c) {
    const int pos = scan[c];
    const int coeff_ctx = coeff_contexts[pos];
    const tran_low_t v = qcoeff[pos];
    const tran_low_t level = abs(v);
    int br_ctx = 0;
    abs_sum += level;
    if (allow_update_cdf) {
      update_cdf(ec_ctx->coeff_base_cdf[txsize_ctx][plane_type][coeff_ctx],
                 AOMMIN(level, 3), 4);
    }
    ++td->counts->coeff_base_multi[cdf_idx][txsize_ctx][plane_type][coeff_ctx]
                                  [AOMMIN(level, 3)];
    if (UNLIKELY(level > NUM_BASE_LEVELS)) {
      const int base_range = level - 1 - NUM_BASE_LEVELS;
      br_ctx = get_br_ctx_simple_stride(levels, pos, bhl, stride, tx_class);
      aom_cdf_prob *const br_cdf =
          ec_ctx
              ->coeff_br_cdf[AOMMIN(txsize_ctx, TX_32X32)][plane_type][br_ctx];
      for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        const int k = AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
        if (allow_update_cdf) {
          update_cdf(br_cdf, k, BR_CDF_SIZE);
        }
        for (int lps = 0; lps < BR_CDF_SIZE - 1; lps++) {
          ++td->counts->coeff_lps[AOMMIN(txsize_ctx, TX_32X32)][plane_type][lps]
                                 [br_ctx][lps == k];
          if (lps == k) break;
        }
        ++td->counts->coeff_lps_multi[cdf_idx][AOMMIN(txsize_ctx, TX_32X32)]
                                     [plane_type][br_ctx][k];
        if (k < BR_CDF_SIZE - 1) break;
      }
    }
    tcoeff[c] = pack_txb_coeff_token(level, v < 0, coeff_ctx, br_ctx);
  }
  {
    const int coeff_ctx = coeff_contexts[0];
    const tran_low_t v = qcoeff[0];
    const tran_low_t level = abs(v);
    int br_ctx = 0;
    abs_sum += level;
    if (allow_update_cdf) {
      update_cdf(ec_ctx->coeff_base_cdf[txsize_ctx][plane_type][coeff_ctx],
                 AOMMIN(level, 3), 4);
    }
    ++td->counts->coeff_base_multi[cdf_idx][txsize_ctx][plane_type][coeff_ctx]
                                  [AOMMIN(level, 3)];
    if (UNLIKELY(level > NUM_BASE_LEVELS)) {
      const int base_range = level - 1 - NUM_BASE_LEVELS;
      br_ctx = get_br_ctx_stride(levels, 0, bhl, stride, tx_class);
      aom_cdf_prob *const br_cdf =
          ec_ctx
              ->coeff_br_cdf[AOMMIN(txsize_ctx, TX_32X32)][plane_type][br_ctx];
      for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        const int k = AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
        if (allow_update_cdf) {
          update_cdf(br_cdf, k, BR_CDF_SIZE);
        }
        for (int lps = 0; lps < BR_CDF_SIZE - 1; lps++) {
          ++td->counts->coeff_lps[AOMMIN(txsize_ctx, TX_32X32)][plane_type][lps]
                                 [br_ctx][lps == k];
          if (lps == k) break;
        }
        ++td->counts->coeff_lps_multi[cdf_idx][AOMMIN(txsize_ctx, TX_32X32)]
                                     [plane_type][br_ctx][k];
        if (k < BR_CDF_SIZE - 1) break;
      }
    }
    tcoeff[0] = pack_txb_coeff_token(level, v < 0, coeff_ctx, br_ctx);
  }
  return abs_sum;
}
#else
#if ENCODETXB_FAST_PACK
static uint32_t finish_txb_tail(
    const tran_low_t *qcoeff, tran_low_t *tcoeff, const int16_t *scan,
    const uint8_t *levels, const int8_t *coeff_contexts, uint16_t *hi_out,
    const uint64_t *sign_chunks, const uint8_t *chunk_lens, uint64_t sign_acc,
    int eob, int n_chunks, uint32_t n_ac, uint32_t max_lvl, int cap_u16,
    int bhl, intptr_t stride, TX_CLASS tx_class, uint32_t abs_sum) {
  uint16_t *const cap_end = (uint16_t *)tcoeff + cap_u16;
  uint32_t total_ac = n_ac;
  for (int chk = 0; chk < n_chunks; ++chk) total_ac += chunk_lens[chk];
  if (max_lvl <= (uint32_t)(COEFF_BASE_RANGE + NUM_BASE_LEVELS)) {
    const int sign_u16 = (int)((total_ac + 15u) >> 4);
    if (hi_out + 1 + sign_u16 <= cap_end) {
      *hi_out++ = (uint16_t)total_ac;
      uint32_t bitbuf = 0;
      uint32_t nbits = 0;
      for (uint32_t b = 0; b < n_ac; ++b) {
        bitbuf |= (uint32_t)((sign_acc >> b) & 1u) << nbits;
        if (++nbits == 16u) {
          *hi_out++ = (uint16_t)bitbuf;
          bitbuf = 0;
          nbits = 0;
        }
      }
      for (int chk = n_chunks - 1; chk >= 0; --chk) {
        const uint64_t cw = sign_chunks[chk];
        const uint32_t clen = chunk_lens[chk];
        for (uint32_t b = 0; b < clen; ++b) {
          bitbuf |= (uint32_t)((cw >> b) & 1u) << nbits;
          if (++nbits == 16u) {
            *hi_out++ = (uint16_t)bitbuf;
            bitbuf = 0;
            nbits = 0;
          }
        }
      }
      if (nbits > 0) *hi_out++ = (uint16_t)bitbuf;
      return abs_sum | TXB_RET_FAST;
    }
  } else if (max_lvl <= 32767u) {
    const int has_dc = (qcoeff[0] != 0) ? 1 : 0;
    if (hi_out + has_dc + 1 + (int)total_ac <= cap_end) {
      if (has_dc) {
        const uint32_t lvl0 = (uint32_t)abs(qcoeff[0]);
        *hi_out++ = (uint16_t)((lvl0 > 14u) ? (lvl0 - 14u) : 0u);
      }
      *hi_out++ = (uint16_t)total_ac;
      for (int idx = 1; idx < eob; ++idx) {
        const tran_low_t val = qcoeff[scan[idx]];
        if (val != 0) {
          const uint32_t lvl = (uint32_t)abs(val);
          const uint32_t g = (lvl > 14u) ? (lvl - 14u) : 0u;
          *hi_out++ = (uint16_t)((g << 1) | ((uint32_t)val >> 31));
        }
      }
      return abs_sum | TXB_RET_FAST | TXB_RET_GOLOMB;
    }
  }
  for (int idx = eob - 1; idx >= 0; --idx) {
    const int pos = scan[idx];
    const tran_low_t val = qcoeff[pos];
    const tran_low_t lvl = abs(val);
    const int br_ctx =
        (lvl > NUM_BASE_LEVELS)
            ? ((idx == eob - 1)
                   ? get_br_ctx_eob(pos, bhl, tx_class)
                   : get_br_ctx_stride(levels, pos, bhl, stride, tx_class))
            : 0;
    tcoeff[idx] = pack_txb_coeff_token(lvl, val < 0,
                                       (uint8_t)coeff_contexts[pos], br_ctx);
  }
  return abs_sum;
}
#endif

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline, aligned(64)))
#endif
static uint32_t update_and_record_txb_context_multi(
    FRAME_CONTEXT *TXB_RESTRICT ec_ctx, const tran_low_t *TXB_RESTRICT qcoeff,
    tran_low_t *TXB_RESTRICT tcoeff, int eob,
    const int16_t *TXB_RESTRICT const scan, uint32_t packed_tx) {
  const PLANE_TYPE plane_type = (PLANE_TYPE)(packed_tx & 0xFu);
  const TX_SIZE tx_size = (TX_SIZE)((packed_tx >> 4) & 0xFFu);
  const TX_SIZE txsize_ctx = (TX_SIZE)((packed_tx >> 12) & 0xFFu);
  const TX_CLASS tx_class = (TX_CLASS)(packed_tx >> 20);
  const int bhl = get_txb_bhl(tx_size);
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  int init_width = width;
  if (LIKELY(tx_class == TX_CLASS_2D)) {
    if (eob <= 10)
      init_width = 4;
    else if (eob <= 36 && height >= 8 && width > 8)
      init_width = 8;
    else if (eob <= 136 && height >= 16 && width > 16)
      init_width = 16;
  }
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, height);
  DECLARE_ALIGNED(16, int8_t, coeff_contexts[1024]);
  av1_txb_init_levels(qcoeff, init_width, height, levels);
  av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class, coeff_contexts);

  aom_cdf_prob(*TXB_RESTRICT const base_cdfs)[CDF_SIZE(4)] =
      ec_ctx->coeff_base_cdf[txsize_ctx][plane_type];
  aom_cdf_prob(*TXB_RESTRICT const br_cdfs)[CDF_SIZE(BR_CDF_SIZE)] =
      ec_ctx->coeff_br_cdf[AOMMIN(txsize_ctx, TX_32X32)][plane_type];
#if HAVE_SSE2 && defined(__SSE2__)
  const uint64_t *const k_mask = kCdf4Mask;
  const __m128i top_vec = _mm_set1_epi16((int16_t)CDF_PROB_TOP);
#endif
  const intptr_t stride = height + TX_PAD_HOR;
  uint32_t abs_sum;
#if ENCODETXB_FAST_PACK
  uint16_t *TXB_RESTRICT out = (uint16_t *)tcoeff;
  uint16_t *hi_out = out + eob;
  uint64_t sign_chunks[17];
  uint8_t chunk_lens[17];
  int n_chunks = 0;
  uint32_t max_lvl = 0;
  uint64_t sign_acc;
  uint32_t n_ac = 1u;
#endif
  {
    const intptr_t c = (intptr_t)eob - 1;
    const int pos = scan[c];
    const uint32_t coeff_ctx = (uint8_t)coeff_contexts[pos];
    const tran_low_t v = qcoeff[pos];
    const uint32_t level = (uint32_t)abs(v);
    const uint32_t symb = AOMMIN(level, 3u) - 1u;
    abs_sum = level;
    assert(coeff_ctx < 4);
#if HAVE_SSE2 && defined(__SSE2__)
    update_cdf_3_mask(
        ec_ctx->coeff_base_eob_cdf[txsize_ctx][plane_type][coeff_ctx], symb,
        k_mask, top_vec);
#else
    update_cdf(ec_ctx->coeff_base_eob_cdf[txsize_ctx][plane_type][coeff_ctx],
               (int8_t)symb, 3);
#endif
#if ENCODETXB_FAST_PACK
    sign_acc = (uint32_t)v >> 31;
    const uint32_t base_tok = symb | (coeff_ctx << 2);
    if (UNLIKELY(level > NUM_BASE_LEVELS)) {
      const int br_ctx = get_br_ctx_eob(pos, bhl, tx_class);
      aom_cdf_prob *const br_cdf = br_cdfs[br_ctx];
#if HAVE_SSE2 && defined(__SSE2__)
      update_br_cdf_loop(br_cdf, (int)level - 1 - NUM_BASE_LEVELS, k_mask,
                         top_vec);
#else
      update_br_cdf_loop(br_cdf, (int)level - 1 - NUM_BASE_LEVELS);
#endif
      *out++ = pack_fast_br_tok16(base_tok, level, br_ctx, &hi_out, &max_lvl);
    } else {
      *out++ = (uint16_t)base_tok;
    }
#else
    uint32_t tok = level | ((uint32_t)(v < 0) << 20) | (coeff_ctx << 21);
    if (UNLIKELY(level > NUM_BASE_LEVELS)) {
      const int br_ctx = get_br_ctx_eob(pos, bhl, tx_class);
      aom_cdf_prob *const br_cdf = br_cdfs[br_ctx];
#if HAVE_SSE2 && defined(__SSE2__)
      update_br_cdf_loop(br_cdf, (int)level - 1 - NUM_BASE_LEVELS, k_mask,
                         top_vec);
#else
      update_br_cdf_loop(br_cdf, (int)level - 1 - NUM_BASE_LEVELS);
#endif
      tok |= (uint32_t)br_ctx << 27;
    }
    tcoeff[c] = (tran_low_t)tok;
#endif
  }
#if ENCODETXB_FAST_PACK
  for (intptr_t c = (intptr_t)eob - 2; c >= 1;) {
    const intptr_t stop = UNLIKELY(c > 63) ? (c - 62) : 1;
    for (; c >= stop; --c) {
      const int pos = scan[c];
      const uint32_t coeff_ctx = (uint8_t)coeff_contexts[pos];
      const tran_low_t v = qcoeff[pos];
      const uint32_t level = (uint32_t)abs(v);
      const uint32_t symb = AOMMIN(level, 3u);
      abs_sum += level;
#if HAVE_SSE2 && defined(__SSE2__)
      update_cdf_4_mask(base_cdfs[coeff_ctx], symb, k_mask, top_vec);
#else
      update_cdf(base_cdfs[coeff_ctx], (int8_t)symb, 4);
#endif
      const uint32_t nz = (v != 0);
      sign_acc = (sign_acc << nz) | ((uint32_t)v >> 31);
      n_ac += nz;
      const uint32_t base_tok = symb | (coeff_ctx << 2);
      if (UNLIKELY(level > NUM_BASE_LEVELS)) {
        const int br_ctx =
            get_br_ctx_simple_stride(levels, pos, bhl, stride, tx_class);
#if HAVE_SSE2 && defined(__SSE2__)
        update_br_cdf_loop(br_cdfs[br_ctx], (int)level - 1 - NUM_BASE_LEVELS,
                           k_mask, top_vec);
#else
        update_br_cdf_loop(br_cdfs[br_ctx], (int)level - 1 - NUM_BASE_LEVELS);
#endif
        *out++ = pack_fast_br_tok16(base_tok, level, br_ctx, &hi_out, &max_lvl);
      } else {
        *out++ = (uint16_t)base_tok;
      }
    }
    if (UNLIKELY(c >= 1)) {
      sign_chunks[n_chunks] = sign_acc;
      chunk_lens[n_chunks++] = (uint8_t)n_ac;
      sign_acc = 0;
      n_ac = 0;
    }
  }
#else
  for (intptr_t c = (intptr_t)eob - 2; c >= 1; --c) {
    const int pos = scan[c];
    const uint32_t coeff_ctx = (uint8_t)coeff_contexts[pos];
    const tran_low_t v = qcoeff[pos];
    const uint32_t level = (uint32_t)abs(v);
    const uint32_t symb = AOMMIN(level, 3u);
    abs_sum += level;
#if HAVE_SSE2 && defined(__SSE2__)
    update_cdf_4_mask(base_cdfs[coeff_ctx], symb, k_mask, top_vec);
#else
    update_cdf(base_cdfs[coeff_ctx], (int8_t)symb, 4);
#endif
    uint32_t tok = level | ((uint32_t)(v < 0) << 20) | (coeff_ctx << 21);
    if (UNLIKELY(level > NUM_BASE_LEVELS)) {
      const int br_ctx =
          get_br_ctx_simple_stride(levels, pos, bhl, stride, tx_class);
#if HAVE_SSE2 && defined(__SSE2__)
      update_br_cdf_loop(br_cdfs[br_ctx], (int)level - 1 - NUM_BASE_LEVELS,
                         k_mask, top_vec);
#else
      update_br_cdf_loop(br_cdfs[br_ctx], (int)level - 1 - NUM_BASE_LEVELS);
#endif
      tok |= (uint32_t)br_ctx << 27;
    }
    tcoeff[c] = (tran_low_t)tok;
  }
#endif
  {
    const uint32_t coeff_ctx = (uint8_t)coeff_contexts[0];
    const tran_low_t v = qcoeff[0];
    const uint32_t level = (uint32_t)abs(v);
    const uint32_t symb = AOMMIN(level, 3u);
    abs_sum += level;
#if HAVE_SSE2 && defined(__SSE2__)
    update_cdf_4_mask(base_cdfs[coeff_ctx], symb, k_mask, top_vec);
#else
    update_cdf(base_cdfs[coeff_ctx], (int8_t)symb, 4);
#endif
#if ENCODETXB_FAST_PACK
    const uint32_t base_tok = symb | (coeff_ctx << 2);
    if (UNLIKELY(level > NUM_BASE_LEVELS)) {
      const int br_ctx = get_br_ctx_stride(levels, 0, bhl, stride, tx_class);
#if HAVE_SSE2 && defined(__SSE2__)
      update_br_cdf_loop(br_cdfs[br_ctx], (int)level - 1 - NUM_BASE_LEVELS,
                         k_mask, top_vec);
#else
      update_br_cdf_loop(br_cdfs[br_ctx], (int)level - 1 - NUM_BASE_LEVELS);
#endif
      *out++ = pack_fast_br_tok16(base_tok, level, br_ctx, &hi_out, &max_lvl);
    } else {
      *out++ = (uint16_t)base_tok;
    }
#else
    uint32_t tok = level | ((uint32_t)(v < 0) << 20) | (coeff_ctx << 21);
    if (UNLIKELY(level > NUM_BASE_LEVELS)) {
      const int br_ctx = get_br_ctx_stride(levels, 0, bhl, stride, tx_class);
#if HAVE_SSE2 && defined(__SSE2__)
      update_br_cdf_loop(br_cdfs[br_ctx], (int)level - 1 - NUM_BASE_LEVELS,
                         k_mask, top_vec);
#else
      update_br_cdf_loop(br_cdfs[br_ctx], (int)level - 1 - NUM_BASE_LEVELS);
#endif
      tok |= (uint32_t)br_ctx << 27;
    }
    tcoeff[0] = (tran_low_t)tok;
#endif
  }
#if ENCODETXB_FAST_PACK
  const int cap_u16 = width << (bhl + 1);
  if (LIKELY(max_lvl <= (uint32_t)(COEFF_BASE_RANGE + NUM_BASE_LEVELS) &&
             n_chunks == 0)) {
    const int sign_u16 = (int)((n_ac + 15u) >> 4);
    if (LIKELY(hi_out + 1 + sign_u16 <= (uint16_t *)tcoeff + cap_u16)) {
      *hi_out++ = (uint16_t)n_ac;
      *hi_out++ = (uint16_t)sign_acc;
      for (uint32_t rem = n_ac; UNLIKELY(rem > 16u);) {
        rem -= 16u;
        sign_acc >>= 16;
        *hi_out++ = (uint16_t)sign_acc;
      }
      return abs_sum | TXB_RET_FAST;
    }
  }
  return finish_txb_tail(qcoeff, tcoeff, scan, levels, coeff_contexts, hi_out,
                         sign_chunks, chunk_lens, sign_acc, eob, n_chunks, n_ac,
                         max_lvl, cap_u16, bhl, stride, tx_class, abs_sum);
#else
  return abs_sum;
#endif
}
#endif

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void update_and_record_txb_context_output(
    int plane, int block, int blk_row, int blk_col, BLOCK_SIZE plane_bsize,
    TX_SIZE tx_size, struct tokenize_b_args *const args) {
  const AV1_COMP *cpi = args->cpi;
  const AV1_COMMON *cm = &cpi->common;
  ThreadData *const td = args->td;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  const int eob = p->eobs[block];
  const PLANE_TYPE plane_type = pd->plane_type;
  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, pd->above_entropy_context + blk_col,
              pd->left_entropy_context + blk_row, &txb_ctx);
#if CONFIG_ENTROPY_STATS
  const uint8_t allow_update_cdf = args->allow_update_cdf;
#else
  const uint8_t allow_update_cdf = 1;
#endif
  const TX_SIZE txsize_ctx = get_txsize_entropy_ctx(tx_size);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
#if CONFIG_ENTROPY_STATS
  int cdf_idx = cm->coef_cdf_category;
  ++td->counts->txb_skip[cdf_idx][txsize_ctx][txb_ctx.txb_skip_ctx][eob == 0];
  if (allow_update_cdf) {
    update_cdf_2_local(ec_ctx->txb_skip_cdf[txsize_ctx][txb_ctx.txb_skip_ctx],
                       eob == 0);
  }
#else
  update_cdf_2_local(ec_ctx->txb_skip_cdf[txsize_ctx][txb_ctx.txb_skip_ctx],
                     eob == 0);
#endif

  CB_COEFF_BUFFER *cb_coef_buff = x->cb_coef_buff;
  const int cb_offset = x->mbmi_ext_frame->cb_offset[plane_type];
  const int txb_idx = (cb_offset >> 4) + block;

  if (eob == 0) {
    cb_coef_buff->entropy_ctx[plane][txb_idx] = (uint8_t)txb_ctx.txb_skip_ctx;
    cb_coef_buff->eobs[plane][txb_idx] = 0;
    av1_set_entropy_contexts(xd, pd, plane, plane_bsize, tx_size, 0, blk_col,
                             blk_row);
    return;
  }
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *qcoeff = p->qcoeff + block_offset;
  const TX_TYPE tx_type =
      av1_get_tx_type(xd, plane_type, blk_row, blk_col, tx_size,
                      cm->features.reduced_tx_set_used);
  const TX_CLASS tx_class = tx_type_to_class[tx_type];

  if (plane == 0) {
    update_tx_type_count(cpi, cm, xd, plane, tx_size, tx_type, td->counts,
                         allow_update_cdf);
  }
  td->rd_counts.tx_type_used[tx_size][tx_type]++;

#if CONFIG_ENTROPY_STATS
  update_eob_context(cdf_idx, eob, tx_size, tx_class, plane_type, ec_ctx,
                     td->counts, allow_update_cdf);
#else
  update_eob_context(eob, tx_size, txsize_ctx, tx_class, plane_type, ec_ctx);
#endif

  const tran_low_t dc_val = qcoeff[0];
#if ENCODETXB_FAST_PACK
  uint8_t ent_byte =
      (uint8_t)(txb_ctx.txb_skip_ctx |
                ((tx_class != TX_CLASS_2D) << TXB_ENT_EOB_MULTI_SHIFT));
#else
  uint8_t ent_byte = (uint8_t)txb_ctx.txb_skip_ctx;
#endif
  if (dc_val != 0) {
    const int dc_sign = (dc_val < 0) ? 1 : 0;
    const int dc_sign_ctx = txb_ctx.dc_sign_ctx;
#if CONFIG_ENTROPY_STATS
    ++td->counts->dc_sign[plane_type][dc_sign_ctx][dc_sign];
    if (allow_update_cdf)
      update_cdf_2_local(ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx], dc_sign);
#else
    update_cdf_2_local(ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx], dc_sign);
#endif
#if ENCODETXB_FAST_PACK
    ent_byte |= (uint8_t)((dc_sign_ctx << DC_SIGN_CTX_SHIFT) |
                          (dc_sign << TXB_ENT_DC_SIGN_SHIFT));
#else
    ent_byte |= (uint8_t)(dc_sign_ctx << DC_SIGN_CTX_SHIFT);
#endif
  }
  cb_coef_buff->entropy_ctx[plane][txb_idx] = ent_byte;

  const uint8_t cul_level = p->txb_entropy_ctx[block];
  av1_set_entropy_contexts(xd, pd, plane, plane_bsize, tx_size, cul_level,
                           blk_col, blk_row);

  if (eob == 1) {
    const tran_low_t level = abs(dc_val);
#if ENCODETXB_FAST_PACK
    uint32_t code_lvl = (uint32_t)level;
    if (UNLIKELY(code_lvl > COEFF_BASE_RANGE + NUM_BASE_LEVELS)) {
      code_lvl = 15u;
      cb_coef_buff->tcoeff[plane][cb_offset + block_offset] = level;
    }
    cb_coef_buff->eobs[plane][txb_idx] =
        (uint16_t)(1u | (code_lvl << TXB_EOB1_LVL_SHIFT));
#else
    cb_coef_buff->eobs[plane][txb_idx] = 1;
    cb_coef_buff->tcoeff[plane][cb_offset + block_offset] = dc_val;
#endif
#if CONFIG_ENTROPY_STATS
    if (allow_update_cdf) {
      update_cdf(ec_ctx->coeff_base_eob_cdf[txsize_ctx][plane_type][0],
                 AOMMIN(level, 3) - 1, 3);
    }
    ++td->counts->coeff_base_eob_multi[cdf_idx][txsize_ctx][plane_type][0]
                                      [AOMMIN(level, 3) - 1];
    if (UNLIKELY(level > NUM_BASE_LEVELS)) {
      const int base_range = level - 1 - NUM_BASE_LEVELS;
      aom_cdf_prob *const br_cdf =
          ec_ctx->coeff_br_cdf[AOMMIN(txsize_ctx, TX_32X32)][plane_type][0];
      for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        const int k = AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
        if (allow_update_cdf) update_cdf(br_cdf, k, BR_CDF_SIZE);
        for (int lps = 0; lps < BR_CDF_SIZE - 1; lps++) {
          ++td->counts->coeff_lps[AOMMIN(txsize_ctx, TX_32X32)][plane_type][lps]
                                 [0][lps == k];
          if (lps == k) break;
        }
        ++td->counts->coeff_lps_multi[cdf_idx][AOMMIN(txsize_ctx, TX_32X32)]
                                     [plane_type][0][k];
        if (k < BR_CDF_SIZE - 1) break;
      }
    }
#else
    const uint32_t symb = AOMMIN((uint32_t)level, 3u) - 1u;
#if HAVE_SSE2 && defined(__SSE2__)
    const uint64_t *const k_mask = kCdf4Mask;
    const __m128i top_vec = _mm_set1_epi16((int16_t)CDF_PROB_TOP);
    update_cdf_3_mask(ec_ctx->coeff_base_eob_cdf[txsize_ctx][plane_type][0],
                      symb, k_mask, top_vec);
#else
    update_cdf(ec_ctx->coeff_base_eob_cdf[txsize_ctx][plane_type][0],
               (int8_t)symb, 3);
#endif
    if (UNLIKELY(level > NUM_BASE_LEVELS)) {
#if HAVE_SSE2 && defined(__SSE2__)
      update_br_cdf_loop(
          ec_ctx->coeff_br_cdf[AOMMIN(txsize_ctx, TX_32X32)][plane_type][0],
          level - 1 - NUM_BASE_LEVELS, k_mask, top_vec);
#else
      update_br_cdf_loop(
          ec_ctx->coeff_br_cdf[AOMMIN(txsize_ctx, TX_32X32)][plane_type][0],
          level - 1 - NUM_BASE_LEVELS);
#endif
    }
#endif
    if (UNLIKELY(cpi->mt_info.pack_bs_mt_enabled)) td->abs_sum_level += level;
  } else {
    tran_low_t *const tcoeff =
        cb_coef_buff->tcoeff[plane] + cb_offset + block_offset;
    const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
    const uint32_t packed_tx = (uint32_t)plane_type | ((uint32_t)tx_size << 4) |
                               ((uint32_t)txsize_ctx << 12) |
                               ((uint32_t)tx_class << 20);
#if CONFIG_ENTROPY_STATS
    cb_coef_buff->eobs[plane][txb_idx] = (uint16_t)eob;
    const int abs_sum = update_and_record_txb_context_multi(
        cpi, td, ec_ctx, qcoeff, tcoeff, eob, scan_order->scan, packed_tx,
        allow_update_cdf);
#else
    const uint32_t ret =
        (eob <= 4 && tx_class == TX_CLASS_2D)
            ? update_and_record_txb_context_2d_small(
                  ec_ctx, qcoeff, tcoeff, eob, scan_order->scan, packed_tx)
            : update_and_record_txb_context_multi(ec_ctx, qcoeff, tcoeff, eob,
                                                  scan_order->scan, packed_tx);
#if ENCODETXB_FAST_PACK
    cb_coef_buff->eobs[plane][txb_idx] =
        (uint16_t)((uint32_t)eob |
                   ((ret >> 19) & (TXB_FAST_MULTI_FLAG | TXB_HAS_GOLOMB_FLAG)));
    const int abs_sum = (int)(ret & TXB_RET_SUM_MASK);
#else
    cb_coef_buff->eobs[plane][txb_idx] = (uint16_t)eob;
    const int abs_sum = (int)ret;
#endif
#endif
    if (UNLIKELY(cpi->mt_info.pack_bs_mt_enabled)) td->abs_sum_level += abs_sum;
  }
}

#if !CONFIG_ENTROPY_STATS
static uint32_t record_txb_context_multi(const tran_low_t *qcoeff,
                                         tran_low_t *tcoeff, int eob,
                                         const int16_t *const scan,
                                         uint32_t packed_tx) {
  const TX_SIZE tx_size = (TX_SIZE)(packed_tx & 0xFFu);
  const TX_CLASS tx_class = (TX_CLASS)(packed_tx >> 8);
  const int bhl = get_txb_bhl(tx_size);
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  int init_width = width;
  if (tx_class == TX_CLASS_2D) {
    if (eob <= 10)
      init_width = 4;
    else if (eob <= 36 && height >= 8 && width > 8)
      init_width = 8;
  }
  const intptr_t stride = height + TX_PAD_HOR;
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, height);
  DECLARE_ALIGNED(16, int8_t, coeff_contexts[1024]);
  av1_txb_init_levels(qcoeff, init_width, height, levels);
  av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class, coeff_contexts);
  uint32_t abs_sum = 0;
  for (int c = eob - 1; c >= 0; --c) {
    const int pos = scan[c];
    const tran_low_t v = qcoeff[pos];
    const tran_low_t level = abs(v);
    const int coeff_ctx = (uint8_t)coeff_contexts[pos];
    const int br_ctx =
        (level > NUM_BASE_LEVELS)
            ? ((c == eob - 1)
                   ? get_br_ctx_eob(pos, bhl, tx_class)
                   : get_br_ctx_stride(levels, pos, bhl, stride, tx_class))
            : 0;
    abs_sum += (uint32_t)level;
    tcoeff[c] = pack_txb_coeff_token(level, v < 0, coeff_ctx, br_ctx);
  }
  return abs_sum;
}
#endif

static void record_txb_context_output(int plane, int block, int blk_row,
                                      int blk_col, BLOCK_SIZE plane_bsize,
                                      TX_SIZE tx_size,
                                      struct tokenize_b_args *const args) {
  const AV1_COMP *cpi = args->cpi;
  const AV1_COMMON *cm = &cpi->common;
  ThreadData *const td = args->td;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  const int eob = p->eobs[block];
  const PLANE_TYPE plane_type = pd->plane_type;
  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, pd->above_entropy_context + blk_col,
              pd->left_entropy_context + blk_row, &txb_ctx);
#if CONFIG_ENTROPY_STATS
  const TX_SIZE txsize_ctx = get_txsize_entropy_ctx(tx_size);
  int cdf_idx = cm->coef_cdf_category;
  ++td->counts->txb_skip[cdf_idx][txsize_ctx][txb_ctx.txb_skip_ctx][eob == 0];
#endif  // CONFIG_ENTROPY_STATS

  CB_COEFF_BUFFER *cb_coef_buff = x->cb_coef_buff;
  const int cb_offset = x->mbmi_ext_frame->cb_offset[plane_type];
  const int txb_offset = cb_offset >> 4;
  uint16_t *eob_txb = cb_coef_buff->eobs[plane] + txb_offset;
  uint8_t *const entropy_ctx = cb_coef_buff->entropy_ctx[plane] + txb_offset;

  if (eob == 0) {
    entropy_ctx[block] = (uint8_t)txb_ctx.txb_skip_ctx;
    eob_txb[block] = 0;
    av1_set_entropy_contexts(xd, pd, plane, plane_bsize, tx_size, 0, blk_col,
                             blk_row);
    return;
  }
  const int block_offset = BLOCK_OFFSET(block);
  tran_low_t *qcoeff = p->qcoeff + block_offset;
  const TX_TYPE tx_type =
      av1_get_tx_type(xd, plane_type, blk_row, blk_col, tx_size,
                      cm->features.reduced_tx_set_used);
  const TX_CLASS tx_class = tx_type_to_class[tx_type];

#if !CONFIG_ENTROPY_STATS
  td->rd_counts.tx_type_used[tx_size][tx_type]++;
  const tran_low_t dc_val = qcoeff[0];
#if ENCODETXB_FAST_PACK
  uint8_t ent_byte =
      (uint8_t)(txb_ctx.txb_skip_ctx |
                ((tx_class != TX_CLASS_2D) << TXB_ENT_EOB_MULTI_SHIFT));
  if (dc_val != 0) {
    ent_byte |= (uint8_t)((txb_ctx.dc_sign_ctx << DC_SIGN_CTX_SHIFT) |
                          ((dc_val < 0) << TXB_ENT_DC_SIGN_SHIFT));
  }
  entropy_ctx[block] = ent_byte;
#else
  entropy_ctx[block] =
      (uint8_t)(txb_ctx.txb_skip_ctx |
                ((dc_val != 0) ? (txb_ctx.dc_sign_ctx << DC_SIGN_CTX_SHIFT)
                               : 0));
#endif
  const uint8_t cul_level = p->txb_entropy_ctx[block];
  av1_set_entropy_contexts(xd, pd, plane, plane_bsize, tx_size, cul_level,
                           blk_col, blk_row);
  if (eob == 1) {
    const tran_low_t level = abs(dc_val);
#if ENCODETXB_FAST_PACK
    uint32_t code_lvl = (uint32_t)level;
    if (UNLIKELY(code_lvl > COEFF_BASE_RANGE + NUM_BASE_LEVELS)) {
      code_lvl = 15u;
      cb_coef_buff->tcoeff[plane][cb_offset + block_offset] = level;
    }
    eob_txb[block] = (uint16_t)(1u | (code_lvl << TXB_EOB1_LVL_SHIFT));
#else
    eob_txb[block] = 1;
    cb_coef_buff->tcoeff[plane][cb_offset + block_offset] = dc_val;
#endif
    if (cpi->mt_info.pack_bs_mt_enabled) td->abs_sum_level += level;
  } else {
    tran_low_t *const tcoeff =
        cb_coef_buff->tcoeff[plane] + cb_offset + block_offset;
    const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
    const uint32_t ret =
        record_txb_context_multi(qcoeff, tcoeff, eob, scan_order->scan,
                                 (uint32_t)tx_size | ((uint32_t)tx_class << 8));
    eob_txb[block] = (uint16_t)eob;
    if (cpi->mt_info.pack_bs_mt_enabled) td->abs_sum_level += (int)ret;
  }
#else
  entropy_ctx[block] = txb_ctx.txb_skip_ctx;
  eob_txb[block] = eob;
  tran_low_t *tcoeff = cb_coef_buff->tcoeff[plane] + cb_offset + block_offset;
  const SCAN_ORDER *const scan_order = get_scan(tx_size, tx_type);
  const int16_t *const scan = scan_order->scan;
  const int bhl = get_txb_bhl(tx_size);
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  const intptr_t stride = height + TX_PAD_HOR;
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, height);
  DECLARE_ALIGNED(16, int8_t, coeff_contexts[1024]);

  if (eob == 1) {
    tcoeff[0] = qcoeff[0];
  } else {
    av1_txb_init_levels(qcoeff, width, height, levels);
    av1_get_nz_map_contexts(levels, scan, eob, tx_size, tx_class,
                            coeff_contexts);
    for (int c = eob - 1; c >= 0; --c) {
      const int pos = scan[c];
      const tran_low_t v = qcoeff[pos];
      const tran_low_t level = abs(v);
      const int coeff_ctx = coeff_contexts[pos];
      const int br_ctx =
          (level > NUM_BASE_LEVELS)
              ? ((c == eob - 1)
                     ? get_br_ctx_eob(pos, bhl, tx_class)
                     : get_br_ctx_stride(levels, pos, bhl, stride, tx_class))
              : 0;
      tcoeff[c] = pack_txb_coeff_token(level, v < 0, coeff_ctx, br_ctx);
    }
  }

  update_tx_type_count(cpi, cm, xd, plane, tx_size, tx_type, td->counts,
                       0 /*allow_update_cdf*/);
  td->rd_counts.tx_type_used[tx_size][tx_type]++;

  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  update_eob_context(cdf_idx, eob, tx_size, tx_class, plane_type, ec_ctx,
                     td->counts, 0 /*allow_update_cdf*/);

  for (int c = eob - 1; c >= 0; --c) {
    const int pos = scan[c];
    const tran_low_t v = qcoeff[pos];
    const tran_low_t level = abs(v);
    if (cpi->mt_info.pack_bs_mt_enabled) td->abs_sum_level += level;

    const int coeff_ctx = coeff_contexts[pos];
    if (c == eob - 1) {
      assert(coeff_ctx < 4);
      ++td->counts->coeff_base_eob_multi[cdf_idx][txsize_ctx][plane_type]
                                        [coeff_ctx][AOMMIN(level, 3) - 1];
    } else {
      ++td->counts->coeff_base_multi[cdf_idx][txsize_ctx][plane_type][coeff_ctx]
                                    [AOMMIN(level, 3)];
    }
    if (level > NUM_BASE_LEVELS) {
      const int base_range = level - 1 - NUM_BASE_LEVELS;
      const int br_ctx = get_br_ctx(levels, pos, bhl, tx_class);
      for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        const int k = AOMMIN(base_range - idx, BR_CDF_SIZE - 1);
        for (int lps = 0; lps < BR_CDF_SIZE - 1; lps++) {
          ++td->counts->coeff_lps[AOMMIN(txsize_ctx, TX_32X32)][plane_type][lps]
                                 [br_ctx][lps == k];
          if (lps == k) break;
        }
        ++td->counts->coeff_lps_multi[cdf_idx][AOMMIN(txsize_ctx, TX_32X32)]
                                     [plane_type][br_ctx][k];
        if (k < BR_CDF_SIZE - 1) break;
      }
    }
  }
  if (qcoeff[0] != 0) {
    const int dc_sign_ctx = txb_ctx.dc_sign_ctx;
    const int dc_sign = (qcoeff[0] < 0) ? 1 : 0;
    ++td->counts->dc_sign[plane_type][dc_sign_ctx][dc_sign];
    entropy_ctx[block] |= dc_sign_ctx << DC_SIGN_CTX_SHIFT;
  }
  const uint8_t cul_level = p->txb_entropy_ctx[block];
  av1_set_entropy_contexts(xd, pd, plane, plane_bsize, tx_size, cul_level,
                           blk_col, blk_row);
#endif
}

void av1_update_and_record_txb_context(int plane, int block, int blk_row,
                                       int blk_col, BLOCK_SIZE plane_bsize,
                                       TX_SIZE tx_size, void *arg) {
  struct tokenize_b_args *const args = arg;
  if (args->dry_run == OUTPUT_ENABLED) {
#if !CONFIG_ENTROPY_STATS
    if (!args->allow_update_cdf) {
      record_txb_context_output(plane, block, blk_row, blk_col, plane_bsize,
                                tx_size, args);
      return;
    }
#endif
    update_and_record_txb_context_output(plane, block, blk_row, blk_col,
                                         plane_bsize, tx_size, args);
    return;
  }
  assert(args->dry_run != DRY_RUN_COSTCOEFFS);
  MACROBLOCK *const x = &args->td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const uint8_t cul_level = p->txb_entropy_ctx[block];
  av1_set_entropy_contexts(xd, pd, plane, plane_bsize, tx_size, cul_level,
                           blk_col, blk_row);
}

void av1_record_txb_context(int plane, int block, int blk_row, int blk_col,
                            BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                            void *arg) {
  struct tokenize_b_args *const args = arg;
  if (args->dry_run == OUTPUT_ENABLED) {
    record_txb_context_output(plane, block, blk_row, blk_col, plane_bsize,
                              tx_size, args);
    return;
  }
  assert(args->dry_run != DRY_RUN_COSTCOEFFS);
  MACROBLOCK *const x = &args->td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const struct macroblock_plane *const p = &x->plane[plane];
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const uint8_t cul_level = p->txb_entropy_ctx[block];
  av1_set_entropy_contexts(xd, pd, plane, plane_bsize, tx_size, cul_level,
                           blk_col, blk_row);
}

void av1_update_intra_mb_txb_context(const AV1_COMP *cpi, ThreadData *td,
                                     RUN_TYPE dry_run, BLOCK_SIZE bsize,
                                     uint8_t allow_update_cdf) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  if (mbmi->skip_txfm) {
    av1_reset_entropy_context(xd, bsize, num_planes);
    return;
  }
  assert(bsize == get_plane_block_size(bsize, xd->plane[0].subsampling_x,
                                       xd->plane[0].subsampling_y));
  const int active_planes =
      (num_planes > 1 && xd->is_chroma_ref) ? MAX_MB_PLANE : 1;
  BLOCK_SIZE uv_bs = bsize;
  const TX_SIZE y_tx = av1_get_tx_size(0, xd);
  TX_SIZE uv_tx = y_tx;
  if (active_planes > 1) {
    const struct macroblockd_plane *const pd1 = &xd->plane[1];
    uv_bs = get_plane_block_size(bsize, pd1->subsampling_x, pd1->subsampling_y);
    uv_tx = av1_get_tx_size(1, xd);
  }
  if (dry_run != OUTPUT_ENABLED) {
    assert(dry_run != DRY_RUN_COSTCOEFFS);
    for (int plane = 0; plane < active_planes; ++plane) {
      const struct macroblock_plane *const p = &x->plane[plane];
      struct macroblockd_plane *const pd = &xd->plane[plane];
      const BLOCK_SIZE plane_bsize = (plane == 0) ? bsize : uv_bs;
      const TX_SIZE tx_size = (plane == 0) ? y_tx : uv_tx;
      if (plane_bsize == txsize_to_bsize[tx_size]) {
        av1_set_entropy_contexts(xd, pd, plane, plane_bsize, tx_size,
                                 p->txb_entropy_ctx[0], 0, 0);
        continue;
      }
      const int ss_x = pd->subsampling_x;
      const int ss_y = pd->subsampling_y;
      const int mi_width = mi_size_wide[plane_bsize];
      const int mi_height = mi_size_high[plane_bsize];
      const int step_w = tx_size_wide_unit[tx_size];
      const int step_h = tx_size_high_unit[tx_size];
      const int step = step_w * step_h;
      const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
      const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
      const BLOCK_SIZE max_unit_bsize =
          get_plane_block_size(BLOCK_64X64, ss_x, ss_y);
      const int unit_height = AOMMIN(mi_size_high[max_unit_bsize], mi_height);
      const int unit_width = AOMMIN(mi_size_wide[max_unit_bsize], mi_width);
      const uint8_t *const ent_ctx = p->txb_entropy_ctx;
      int block = 0;
      for (int r = 0; r < max_blocks_high; r += unit_height) {
        const int unit_height_r = AOMMIN(r + unit_height, max_blocks_high);
        for (int c = 0; c < max_blocks_wide; c += unit_width) {
          const int unit_width_c = AOMMIN(c + unit_width, max_blocks_wide);
          for (int blk_row = r; blk_row < unit_height_r; blk_row += step_h) {
            for (int blk_col = c; blk_col < unit_width_c; blk_col += step_w) {
              av1_set_entropy_contexts(xd, pd, plane, plane_bsize, tx_size,
                                       ent_ctx[block], blk_col, blk_row);
              block += step;
            }
          }
        }
      }
    }
    return;
  }

  struct tokenize_b_args arg = { cpi, td, 0, allow_update_cdf, dry_run };
#if CONFIG_ENTROPY_STATS
  const foreach_transformed_block_visitor visit =
      allow_update_cdf ? av1_update_and_record_txb_context
                       : av1_record_txb_context;
#endif
  for (int plane = 0; plane < active_planes; ++plane) {
    const BLOCK_SIZE plane_bsize = (plane == 0) ? bsize : uv_bs;
    const TX_SIZE tx_size = (plane == 0) ? y_tx : uv_tx;
    if (plane_bsize == txsize_to_bsize[tx_size]) {
#if !CONFIG_ENTROPY_STATS
      if (LIKELY(allow_update_cdf)) {
        update_and_record_txb_context_output(plane, 0, 0, 0, plane_bsize,
                                             tx_size, &arg);
      } else {
        record_txb_context_output(plane, 0, 0, 0, plane_bsize, tx_size, &arg);
      }
#else
      visit(plane, 0, 0, 0, plane_bsize, tx_size, &arg);
#endif
    } else {
#if !CONFIG_ENTROPY_STATS
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      const int step_w = tx_size_wide_unit[tx_size];
      const int step_h = tx_size_high_unit[tx_size];
      const int step = step_w * step_h;
      const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
      const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
      const BLOCK_SIZE max_unit_bsize = get_plane_block_size(
          BLOCK_64X64, pd->subsampling_x, pd->subsampling_y);
      const int mu_blocks_wide =
          AOMMIN(mi_size_wide[max_unit_bsize], max_blocks_wide);
      const int mu_blocks_high =
          AOMMIN(mi_size_high[max_unit_bsize], max_blocks_high);
      int block = 0;
      for (int r = 0; r < max_blocks_high; r += mu_blocks_high) {
        const int unit_height_r = AOMMIN(r + mu_blocks_high, max_blocks_high);
        for (int c = 0; c < max_blocks_wide; c += mu_blocks_wide) {
          const int unit_width_c = AOMMIN(c + mu_blocks_wide, max_blocks_wide);
          for (int blk_row = r; blk_row < unit_height_r; blk_row += step_h) {
            for (int blk_col = c; blk_col < unit_width_c; blk_col += step_w) {
              if (LIKELY(allow_update_cdf)) {
                update_and_record_txb_context_output(
                    plane, block, blk_row, blk_col, plane_bsize, tx_size, &arg);
              } else {
                record_txb_context_output(plane, block, blk_row, blk_col,
                                          plane_bsize, tx_size, &arg);
              }
              block += step;
            }
          }
        }
      }
#else
      av1_foreach_transformed_block_in_plane(xd, plane_bsize, plane, visit,
                                             &arg);
#endif
    }
  }
}

CB_COEFF_BUFFER *av1_get_cb_coeff_buffer(const struct AV1_COMP *cpi, int mi_row,
                                         int mi_col) {
  const AV1_COMMON *const cm = &cpi->common;
  const int mib_size_log2 = cm->seq_params->mib_size_log2;
  const int offset = (mi_row >> mib_size_log2) * cpi->coeff_buffer_stride +
                     (mi_col >> mib_size_log2);
  return cpi->coeff_buffer_base + offset;
}
