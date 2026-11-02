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

#include "av1/encoder/context_tree.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "aom_ports/bitops.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/rd.h"

typedef struct PmcPoolHeader {
  PICK_MODE_CONTEXT *pmc_free[2][11];
  uintptr_t pad[2];
} PmcPoolHeader;

static inline int pmc_size_idx(int num_blk) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_ctz(num_blk);
#else
  return get_msb(num_blk);
#endif
}

static inline PICK_MODE_CONTEXT *pmc_get_next_free(const void *ctx) {
  PICK_MODE_CONTEXT *next;
  memcpy(&next, ctx, sizeof(next));
  return next;
}

static inline void pmc_set_next_free(void *ctx, PICK_MODE_CONTEXT *next) {
  memcpy(ctx, &next, sizeof(next));
}

static inline void zero_tx_type_map(uint8_t *p, int num_blk) {
  memset(p, 0, num_blk);
}

static inline void zero_pmc_fast(PICK_MODE_CONTEXT *ctx) {
  memset(ctx, 0, offsetof(PICK_MODE_CONTEXT, color_index_map));
}

void av1_copy_tree_context(PICK_MODE_CONTEXT *dst_ctx,
                           PICK_MODE_CONTEXT *src_ctx) {
  dst_ctx->mic = src_ctx->mic;
  dst_ctx->mbmi_ext_best = src_ctx->mbmi_ext_best;

  dst_ctx->num_4x4_blk = src_ctx->num_4x4_blk;
  dst_ctx->skippable = src_ctx->skippable;
#if CONFIG_INTERNAL_STATS
  dst_ctx->best_mode_index = src_ctx->best_mode_index;
#endif  // CONFIG_INTERNAL_STATS

  av1_copy_tx_type_map(dst_ctx->tx_type_map, src_ctx->tx_type_map,
                       src_ctx->num_4x4_blk);

  dst_ctx->rd_stats = src_ctx->rd_stats;
  dst_ctx->rd_mode_is_ready = src_ctx->rd_mode_is_ready;
}

void av1_setup_shared_coeff_buffer(const SequenceHeader *const seq_params,
                                   PC_TREE_SHARED_BUFFERS *shared_bufs,
                                   struct aom_internal_error_info *error) {
  const int num_planes = seq_params->monochrome ? 1 : MAX_MB_PLANE;
  const int max_sb_square_y = 1 << num_pels_log2_lookup[seq_params->sb_size];
  const int max_sb_square_uv = max_sb_square_y >> (seq_params->subsampling_x +
                                                   seq_params->subsampling_y);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 1
#endif
  for (int i = 0; i < num_planes; i++) {
    const int max_num_pix =
        (i == AOM_PLANE_Y) ? max_sb_square_y : max_sb_square_uv;
    const size_t extra = (i == AOM_PLANE_Y) ? sizeof(PmcPoolHeader) : 0;
    AOM_CHECK_MEM_ERROR(
        error, shared_bufs->coeff_buf[i],
        aom_memalign(32, extra + max_num_pix * sizeof(tran_low_t)));
    if (i == AOM_PLANE_Y) {
      memset(shared_bufs->coeff_buf[0], 0, sizeof(PmcPoolHeader));
      shared_bufs->coeff_buf[0] =
          (tran_low_t *)((uint8_t *)shared_bufs->coeff_buf[0] +
                         sizeof(PmcPoolHeader));
    }
    AOM_CHECK_MEM_ERROR(error, shared_bufs->qcoeff_buf[i],
                        aom_memalign(32, max_num_pix * sizeof(tran_low_t)));
    AOM_CHECK_MEM_ERROR(error, shared_bufs->dqcoeff_buf[i],
                        aom_memalign(32, max_num_pix * sizeof(tran_low_t)));
  }
}

void av1_free_shared_coeff_buffer(PC_TREE_SHARED_BUFFERS *shared_bufs) {
  if (shared_bufs->coeff_buf[0] != NULL) {
    PmcPoolHeader *pool = (PmcPoolHeader *)shared_bufs->coeff_buf[0] - 1;
    for (int pal = 0; pal < 2; ++pal) {
      for (int idx = 0; idx < 11; ++idx) {
        PICK_MODE_CONTEXT *cur = pool->pmc_free[pal][idx];
        while (cur != NULL) {
          PICK_MODE_CONTEXT *next = pmc_get_next_free(cur);
          aom_free(cur);
          cur = next;
        }
      }
    }
    aom_free(pool);
    shared_bufs->coeff_buf[0] = NULL;
  }
  aom_free(shared_bufs->qcoeff_buf[0]);
  aom_free(shared_bufs->dqcoeff_buf[0]);
  shared_bufs->qcoeff_buf[0] = NULL;
  shared_bufs->dqcoeff_buf[0] = NULL;
  for (int i = 1; i < 3; i++) {
    aom_free(shared_bufs->coeff_buf[i]);
    aom_free(shared_bufs->qcoeff_buf[i]);
    aom_free(shared_bufs->dqcoeff_buf[i]);
    shared_bufs->coeff_buf[i] = NULL;
    shared_bufs->qcoeff_buf[i] = NULL;
    shared_bufs->dqcoeff_buf[i] = NULL;
  }
}

static inline void free_pmc_inline(PICK_MODE_CONTEXT *ctx) {
  PmcPoolHeader *pool = (PmcPoolHeader *)ctx->coeff[0] - 1;
  const int use_pal = (ctx->color_index_map[0] != NULL);
  const int idx = pmc_size_idx(ctx->num_4x4_blk);
  pmc_set_next_free(ctx, pool->pmc_free[use_pal][idx]);
  pool->pmc_free[use_pal][idx] = ctx;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static PICK_MODE_CONTEXT *av1_alloc_pmc_slow(
    const struct AV1_COMP *const cpi, int num_pix, int num_blk,
    bool use_palette, PC_TREE_SHARED_BUFFERS *shared_bufs) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const size_t ctx_sz = (sizeof(PICK_MODE_CONTEXT) + 31) & ~(size_t)31;
  const size_t eob_sz = (num_blk * sizeof(uint16_t) + 31) & ~(size_t)31;
  const size_t txb_ctx_sz =
      (num_blk * sizeof(ENTROPY_CONTEXT) + 31) & ~(size_t)31;
  const size_t pal_sz =
      use_palette ? ((num_pix * sizeof(uint8_t) + 31) & ~(size_t)31) : 0;
  const size_t tx_type_sz = num_blk * sizeof(uint8_t);

  const size_t total_sz = ctx_sz + (size_t)num_planes * (eob_sz + txb_ctx_sz) +
                          2 * pal_sz + tx_type_sz;
  uint8_t *raw = (uint8_t *)aom_memalign(32, total_sz);
  if (!raw) {
    return NULL;
  }

  PICK_MODE_CONTEXT *ctx = (PICK_MODE_CONTEXT *)raw;
  memset(ctx, 0, offsetof(PICK_MODE_CONTEXT, color_index_map));
  ctx->num_4x4_blk = num_blk;

  uint8_t *ptr = raw + ctx_sz;
  for (int i = 0; i < num_planes; ++i) {
    ctx->coeff[i] = shared_bufs->coeff_buf[i];
    ctx->qcoeff[i] = shared_bufs->qcoeff_buf[i];
    ctx->dqcoeff[i] = shared_bufs->dqcoeff_buf[i];
    ctx->eobs[i] = (uint16_t *)ptr;
    ptr += eob_sz;
    ctx->txb_entropy_ctx[i] = ptr;
    ptr += txb_ctx_sz;
  }

  if (use_palette) {
    ctx->color_index_map[0] = ptr;
    ptr += pal_sz;
    ctx->color_index_map[1] = ptr;
    ptr += pal_sz;
  } else {
    ctx->color_index_map[0] = NULL;
    ctx->color_index_map[1] = NULL;
  }

  ctx->tx_type_map = ptr;
  ctx->skippable = 0;
#if CONFIG_INTERNAL_STATS
  ctx->best_mode_index = 0;
#endif
  ctx->rd_mode_is_ready = 0;
  zero_tx_type_map(ctx->tx_type_map, num_blk);
  av1_invalid_rd_stats(&ctx->rd_stats);
  return ctx;
}

PICK_MODE_CONTEXT *av1_alloc_pmc(const struct AV1_COMP *const cpi,
                                 BLOCK_SIZE bsize,
                                 PC_TREE_SHARED_BUFFERS *shared_bufs) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_pix = block_size_wide[bsize] * block_size_high[bsize];
  const int num_blk = num_pix / 16;
  const bool use_palette = (num_pix <= MAX_PALETTE_SQUARE) &&
                           cm->features.allow_screen_content_tools;
  PmcPoolHeader *const pool = (PmcPoolHeader *)shared_bufs->coeff_buf[0] - 1;
  const int idx = pmc_size_idx(num_blk);
  PICK_MODE_CONTEXT *ctx = pool->pmc_free[use_palette][idx];
  if (LIKELY(ctx != NULL)) {
    pool->pmc_free[use_palette][idx] = pmc_get_next_free(ctx);
    zero_pmc_fast(ctx);
    ctx->skippable = 0;
#if CONFIG_INTERNAL_STATS
    ctx->best_mode_index = 0;
#endif
    ctx->rd_mode_is_ready = 0;
    zero_tx_type_map(ctx->tx_type_map, num_blk);
    av1_invalid_rd_stats(&ctx->rd_stats);
    return ctx;
  }
  return av1_alloc_pmc_slow(cpi, num_pix, num_blk, use_palette, shared_bufs);
}

void av1_reset_pmc(PICK_MODE_CONTEXT *ctx) {
  zero_tx_type_map(ctx->tx_type_map, ctx->num_4x4_blk);
  av1_invalid_rd_stats(&ctx->rd_stats);
}

void av1_free_pmc(PICK_MODE_CONTEXT *ctx, int num_planes) {
  (void)num_planes;
  if (ctx == NULL) return;
  aom_free(ctx);
}

PC_TREE *av1_alloc_pc_tree_node(BLOCK_SIZE bsize) {
  PC_TREE *pc_tree = aom_calloc(1, sizeof(*pc_tree));
  if (pc_tree == NULL) return NULL;

  pc_tree->partitioning = PARTITION_NONE;
  pc_tree->block_size = bsize;

  return pc_tree;
}

#define FREE_PMC_NODE(CTX)  \
  do {                      \
    if (CTX != NULL) {      \
      free_pmc_inline(CTX); \
      CTX = NULL;           \
    }                       \
  } while (0)

void av1_free_pc_tree_recursive(PC_TREE *pc_tree, int num_planes, int keep_best,
                                int keep_none,
                                PARTITION_SEARCH_TYPE partition_search_type) {
  (void)num_planes;
  if (pc_tree == NULL) return;

  const BLOCK_SIZE bsize = pc_tree->block_size;
  if (LIKELY(!keep_best && !keep_none)) {
    if (pc_tree->none != NULL) free_pmc_inline(pc_tree->none);
    if (bsize != BLOCK_4X4) {
      for (int i = 0; i < 2; ++i) {
        if (pc_tree->horizontal[i] != NULL)
          free_pmc_inline(pc_tree->horizontal[i]);
        if (pc_tree->vertical[i] != NULL) free_pmc_inline(pc_tree->vertical[i]);
      }
#if !CONFIG_REALTIME_ONLY
      if (bsize > BLOCK_8X8 && partition_search_type != VAR_BASED_PARTITION) {
        if (pc_tree->horizontala[0] != NULL) {
          for (int i = 0; i < 3; ++i)
            if (pc_tree->horizontala[i] != NULL)
              free_pmc_inline(pc_tree->horizontala[i]);
        }
        if (pc_tree->horizontalb[0] != NULL) {
          for (int i = 0; i < 3; ++i)
            if (pc_tree->horizontalb[i] != NULL)
              free_pmc_inline(pc_tree->horizontalb[i]);
        }
        if (pc_tree->verticala[0] != NULL) {
          for (int i = 0; i < 3; ++i)
            if (pc_tree->verticala[i] != NULL)
              free_pmc_inline(pc_tree->verticala[i]);
        }
        if (pc_tree->verticalb[0] != NULL) {
          for (int i = 0; i < 3; ++i)
            if (pc_tree->verticalb[i] != NULL)
              free_pmc_inline(pc_tree->verticalb[i]);
        }
        if (pc_tree->horizontal4[0] != NULL) {
          for (int i = 0; i < 4; ++i)
            if (pc_tree->horizontal4[i] != NULL)
              free_pmc_inline(pc_tree->horizontal4[i]);
        }
        if (pc_tree->vertical4[0] != NULL) {
          for (int i = 0; i < 4; ++i)
            if (pc_tree->vertical4[i] != NULL)
              free_pmc_inline(pc_tree->vertical4[i]);
        }
      }
#endif
      for (int i = 0; i < 4; ++i) {
        if (pc_tree->split[i] != NULL) {
          av1_free_pc_tree_recursive(pc_tree->split[i], 0, 0, 0,
                                     partition_search_type);
        }
      }
    }
    aom_free(pc_tree);
    return;
  }

  if (bsize == BLOCK_4X4) {
    if (!keep_none &&
        (!keep_best || (pc_tree->partitioning != PARTITION_NONE))) {
      FREE_PMC_NODE(pc_tree->none);
    }
    return;
  }

  const PARTITION_TYPE partition = pc_tree->partitioning;
  if (!keep_none && (!keep_best || (partition != PARTITION_NONE)))
    FREE_PMC_NODE(pc_tree->none);

  for (int i = 0; i < 2; ++i) {
    if (!keep_best || (partition != PARTITION_HORZ))
      FREE_PMC_NODE(pc_tree->horizontal[i]);
    if (!keep_best || (partition != PARTITION_VERT))
      FREE_PMC_NODE(pc_tree->vertical[i]);
  }
#if !CONFIG_REALTIME_ONLY
  if (bsize > BLOCK_8X8) {
    if (pc_tree->horizontala[0] != NULL &&
        (!keep_best || (partition != PARTITION_HORZ_A))) {
      for (int i = 0; i < 3; ++i) FREE_PMC_NODE(pc_tree->horizontala[i]);
    }
    if (pc_tree->horizontalb[0] != NULL &&
        (!keep_best || (partition != PARTITION_HORZ_B))) {
      for (int i = 0; i < 3; ++i) FREE_PMC_NODE(pc_tree->horizontalb[i]);
    }
    if (pc_tree->verticala[0] != NULL &&
        (!keep_best || (partition != PARTITION_VERT_A))) {
      for (int i = 0; i < 3; ++i) FREE_PMC_NODE(pc_tree->verticala[i]);
    }
    if (pc_tree->verticalb[0] != NULL &&
        (!keep_best || (partition != PARTITION_VERT_B))) {
      for (int i = 0; i < 3; ++i) FREE_PMC_NODE(pc_tree->verticalb[i]);
    }
    if (pc_tree->horizontal4[0] != NULL &&
        (!keep_best || (partition != PARTITION_HORZ_4))) {
      for (int i = 0; i < 4; ++i) FREE_PMC_NODE(pc_tree->horizontal4[i]);
    }
    if (pc_tree->vertical4[0] != NULL &&
        (!keep_best || (partition != PARTITION_VERT_4))) {
      for (int i = 0; i < 4; ++i) FREE_PMC_NODE(pc_tree->vertical4[i]);
    }
  }
#endif
  if (!keep_best || (partition != PARTITION_SPLIT)) {
    for (int i = 0; i < 4; ++i) {
      if (pc_tree->split[i] != NULL) {
        av1_free_pc_tree_recursive(pc_tree->split[i], 0, 0, 0,
                                   partition_search_type);
        pc_tree->split[i] = NULL;
      }
    }
  }
}
#undef FREE_PMC_NODE

int av1_setup_sms_tree(AV1_COMP *const cpi, ThreadData *td) {
  // The structure 'sms_tree' is used to store the simple motion search data for
  // partition pruning in inter frames. Hence, the memory allocations and
  // initializations related to it are avoided for allintra encoding mode.
  if (cpi->oxcf.kf_cfg.key_freq_max == 0) return 0;

  AV1_COMMON *const cm = &cpi->common;
  const int stat_generation_stage = is_stat_generation_stage(cpi);
  const int is_sb_size_128 = cm->seq_params->sb_size == BLOCK_128X128;
  const int tree_nodes =
      av1_get_pc_tree_nodes(is_sb_size_128, stat_generation_stage);
  int sms_tree_index = 0;
  SIMPLE_MOTION_DATA_TREE *this_sms;
  int square_index = 1;
  int nodes;

  aom_free(td->sms_tree);
  td->sms_tree =
      (SIMPLE_MOTION_DATA_TREE *)aom_calloc(tree_nodes, sizeof(*td->sms_tree));
  if (!td->sms_tree) return -1;
  this_sms = &td->sms_tree[0];

  if (!stat_generation_stage) {
    const int leaf_factor = is_sb_size_128 ? 4 : 1;
    const int leaf_nodes = 256 * leaf_factor;

    // Sets up all the leaf nodes in the tree.
    for (sms_tree_index = 0; sms_tree_index < leaf_nodes; ++sms_tree_index) {
      SIMPLE_MOTION_DATA_TREE *const tree = &td->sms_tree[sms_tree_index];
      tree->block_size = square[0];
    }

    // Each node has 4 leaf nodes, fill each block_size level of the tree
    // from leafs to the root.
    for (nodes = leaf_nodes >> 2; nodes > 0; nodes >>= 2) {
      for (int i = 0; i < nodes; ++i) {
        SIMPLE_MOTION_DATA_TREE *const tree = &td->sms_tree[sms_tree_index];
        tree->block_size = square[square_index];
        for (int j = 0; j < 4; j++) tree->split[j] = this_sms++;
        ++sms_tree_index;
      }
      ++square_index;
    }
  } else {
    // Allocation for firstpass/LAP stage
    // TODO(Mufaddal): refactor square_index to use a common block_size macro
    // from firstpass.c
    SIMPLE_MOTION_DATA_TREE *const tree = &td->sms_tree[sms_tree_index];
    square_index = 2;
    tree->block_size = square[square_index];
  }

  // Set up the root node for the largest superblock size
  td->sms_root = &td->sms_tree[tree_nodes - 1];
  return 0;
}

void av1_free_sms_tree(ThreadData *td) {
  aom_free(td->sms_tree);
  td->sms_tree = NULL;
}
