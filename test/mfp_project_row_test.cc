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

#include <cstdio>
#include <cstring>
#include <vector>

#include "gtest/gtest.h"

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_ports/aom_timer.h"
#include "aom_ports/mem.h"
#include "av1/common/enums.h"
#include "av1/common/mv.h"
#include "av1/common/mvref_common.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"

using libaom_test::ACMRandom;

namespace {

using ProjectFn = void (*)(const MV_REF *, TPL_MV_REF *, int, int, int, int,
                           int, const int *, int, int);

// Kernels are invoked directly, which is what AOM_SIMD_CAPS_MASK selects at
// process start. The dispatcher is checked as well.
void ExpectMatch(ProjectFn test_fn, const MV_REF *src, int mvs_cols,
                 int blk_row, int mi_rows_half, int mi_cols_half,
                 int tpl_stride, const int *ref_offset, int cur_offset,
                 int dir) {
  const int tpl_rows = mi_rows_half + 16;
  const int n = tpl_stride * tpl_rows;
  std::vector<TPL_MV_REF> ref(n);
  std::vector<TPL_MV_REF> got(n);
  memset(ref.data(), 0, sizeof(TPL_MV_REF) * n);
  memset(got.data(), 0, sizeof(TPL_MV_REF) * n);
  av1_mfp_project_row_c(src, ref.data(), tpl_stride, blk_row, mvs_cols,
                        mi_rows_half, mi_cols_half, ref_offset, cur_offset,
                        dir);
  API_REGISTER_STATE_CHECK(test_fn(src, got.data(), tpl_stride, blk_row,
                                   mvs_cols, mi_rows_half, mi_cols_half,
                                   ref_offset, cur_offset, dir));
  ASSERT_EQ(0, memcmp(ref.data(), got.data(), sizeof(TPL_MV_REF) * n));
}

// av1_copy_frame_mvs() does not store motion vectors beyond REFMVS_LIMIT, and
// av1_get_mv_projection() relies on that bound to stay within int.
int16_t RandStoredMv(ACMRandom *rng) {
  return (int16_t)(rng->PseudoUniform(2 * REFMVS_LIMIT + 1) - REFMVS_LIMIT);
}

void FillRow(ACMRandom *rng, MV_REF *src, int mvs_cols, int mode) {
  const int8_t ref = (int8_t)(1 + rng->PseudoUniform(7));
  const MV mv = { RandStoredMv(rng), RandStoredMv(rng) };
  for (int i = 0; i < mvs_cols; ++i) {
    if (mode == 0) {
      src[i].mv.as_mv.row = RandStoredMv(rng);
      src[i].mv.as_mv.col = RandStoredMv(rng);
      const int pick = rng->PseudoUniform(10);
      if (pick == 0) {
        src[i].ref_frame = NONE_FRAME;
      } else if (pick == 1) {
        src[i].ref_frame = INTRA_FRAME;
      } else {
        src[i].ref_frame = (int8_t)(1 + rng->PseudoUniform(7));
      }
    } else if (mode == 1) {
      src[i].mv.as_mv = mv;
      src[i].ref_frame = ref;
    } else {
      src[i].mv.as_mv.row = (int16_t)rng->PseudoUniform(257) - 128;
      src[i].mv.as_mv.col = (int16_t)rng->PseudoUniform(257) - 128;
      src[i].ref_frame = (int8_t)(1 + rng->PseudoUniform(7));
    }
  }
}

void FillOffsets(ACMRandom *rng, int ref_offset[REF_FRAMES], int mode) {
  for (int rf = 0; rf < REF_FRAMES; ++rf) {
    if (mode == 0) {
      ref_offset[rf] = rng->PseudoUniform(48) - 8;
    } else {
      ref_offset[rf] = 1 + rng->PseudoUniform(MAX_FRAME_DISTANCE);
    }
  }
  ref_offset[INTRA_FRAME] = 0;
  ref_offset[0] = 0;
}

// Frame geometry for a row of mvs_cols source MVs, as motion_field_projection
// derives it: mvs_cols = (mi_cols + 1) >> 1 and tpl_stride = mi_stride >> 1.
struct Geometry {
  int mi_cols_half;
  int tpl_stride;
};

Geometry GeometryFor(int mvs_cols, bool odd_mi_cols) {
  const int mi_cols = 2 * mvs_cols - (odd_mi_cols ? 1 : 0);
  return { mi_cols >> 1, ALIGN_POWER_OF_TWO(mi_cols, 5) >> 1 };
}

// Every column in a group of eight projects to one destination column, so the
// kernel must keep the last writer, as the C loop over blk_col does.
// cur_offset == ref_offset == 1 makes the projection the identity.
void FillCollidingRow(ACMRandom *rng, MV_REF *src, int mvs_cols,
                      int mi_cols_half, int sign_bias) {
  for (int base = 0; base < mvs_cols; base += 8) {
    const int lo = AOMMAX(base - 8, 0);
    const int hi = AOMMIN(base + 16, mi_cols_half);
    const int target = lo + rng->PseudoUniform(hi - lo);
    for (int i = base; i < AOMMIN(base + 8, mvs_cols); ++i) {
      const int k = sign_bias ? i - target : target - i;
      const int frac = rng->PseudoUniform(64);
      src[i].mv.as_mv.row = (int16_t)(rng->PseudoUniform(127) - 63);
      src[i].mv.as_mv.col = (int16_t)(k * 64 + (k >= 0 ? frac : -frac));
      src[i].ref_frame = rng->PseudoUniform(8)
                             ? (int8_t)(1 + rng->PseudoUniform(7))
                             : (int8_t)INTRA_FRAME;
    }
  }
}

void RunCollisions(ProjectFn fn, int mi_rows_half, std::vector<MV_REF> *src) {
  ACMRandom rng(0xc011);
  int ref_offset[REF_FRAMES];
  for (int rf = 0; rf < REF_FRAMES; ++rf) ref_offset[rf] = 1;
  ref_offset[INTRA_FRAME] = 0;
  const int cols_list[] = { 4, 7, 8, 12, 16, 23, 64, 96 };
  for (int cols : cols_list) {
    for (int dir = 0; dir < 3; ++dir) {
      for (int n = 0; n < 50; ++n) {
        const Geometry g = GeometryFor(cols, n & 1);
        FillCollidingRow(&rng, src->data(), cols, g.mi_cols_half, dir >> 1);
        ExpectMatch(fn, src->data(), cols, rng.PseudoUniform(mi_rows_half + 1),
                    mi_rows_half, g.mi_cols_half, g.tpl_stride, ref_offset, 1,
                    dir);
      }
    }
  }
}

void RunSuite(ProjectFn fn) {
  ACMRandom rng(0xbaba);
  // Not a multiple of 8, so mi_rows_half clips the last 8-row band.
  // blk_row == mi_rows_half is the extra MV row of an odd mi_rows.
  const int mi_rows_half = 45;
  const int cols_list[] = { 1, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 64, 480 };
  const int rows[] = { 0, 1, 7, 8, 15, 16, 24, 40, 44, 45 };
  const int dirs[] = { 0, 1, 2 };
  const int curs[] = { -32, -31, -8, -1, 0, 1, 8, 31, 32 };
  std::vector<MV_REF> src(480);

  for (int cols : cols_list) {
    for (int odd = 0; odd < 2; ++odd) {
      const Geometry g = GeometryFor(cols, odd);
      for (int blk_row : rows) {
        for (int dir : dirs) {
          for (int cur : curs) {
            for (int mode = 0; mode < 3; ++mode) {
              int ref_offset[REF_FRAMES];
              FillOffsets(&rng, ref_offset, mode & 1);
              FillRow(&rng, src.data(), cols, mode);
              ExpectMatch(fn, src.data(), cols, blk_row, mi_rows_half,
                          g.mi_cols_half, g.tpl_stride, ref_offset, cur, dir);
            }
          }
        }
      }
    }
  }

  for (int n = 0; n < 40; ++n) {
    const int cols = 1 + rng.PseudoUniform(128);
    const Geometry g = GeometryFor(cols, rng.PseudoUniform(2));
    int ref_offset[REF_FRAMES];
    FillOffsets(&rng, ref_offset, 0);
    FillRow(&rng, src.data(), cols, 0);
    ExpectMatch(fn, src.data(), cols, rng.PseudoUniform(mi_rows_half + 1),
                mi_rows_half, g.mi_cols_half, g.tpl_stride, ref_offset,
                rng.PseudoUniform(80) - 40, rng.PseudoUniform(3));
  }

  RunCollisions(fn, mi_rows_half, &src);
}

#if HAVE_SSE4_1 || HAVE_AVX2
// A 1080p frame: 240 8x8 columns and 135 rows, all projected each iteration.
void RunSpeed(ProjectFn fn, const char *name) {
  ACMRandom rng(0x5eed);
  const int mvs_cols = 240;
  const int mvs_rows = 135;
  const int tpl_stride = mvs_cols;
  std::vector<MV_REF> src(mvs_cols * mvs_rows);
  for (int r = 0; r < mvs_rows; ++r) {
    FillRow(&rng, &src[r * mvs_cols], mvs_cols, 2);
  }
  int ref_offset[REF_FRAMES];
  FillOffsets(&rng, ref_offset, 1);
  std::vector<TPL_MV_REF> tpl(tpl_stride * (mvs_rows + 16));
  const int kIterations = 2000;
  const ProjectFn fns[2] = { av1_mfp_project_row_c, fn };
  double elapsed[2];
  for (int f = 0; f < 2; ++f) {
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int it = 0; it < kIterations; ++it) {
      for (int r = 0; r < mvs_rows; ++r) {
        fns[f](&src[r * mvs_cols], tpl.data(), tpl_stride, r, mvs_cols,
               mvs_rows, mvs_cols, ref_offset, 2, 0);
      }
    }
    aom_usec_timer_mark(&timer);
    elapsed[f] = static_cast<double>(aom_usec_timer_elapsed(&timer));
  }
  printf("av1_mfp_project_row c: %.0f us, %s: %.0f us, speedup %.2fx\n",
         elapsed[0], name, elapsed[1], elapsed[0] / elapsed[1]);
}
#endif  // HAVE_SSE4_1 || HAVE_AVX2

TEST(SSE4_1, MfpProjectRowMatchesC) {
#if HAVE_SSE4_1
  RunSuite(av1_mfp_project_row_sse4_1);
#else
  GTEST_SKIP();
#endif
}

TEST(AVX2, MfpProjectRowMatchesC) {
#if HAVE_AVX2
  RunSuite(av1_mfp_project_row_avx2);
#else
  GTEST_SKIP();
#endif
}

TEST(SSE4_1, DISABLED_MfpProjectRowSpeed) {
#if HAVE_SSE4_1
  RunSpeed(av1_mfp_project_row_sse4_1, "sse4_1");
#else
  GTEST_SKIP();
#endif
}

TEST(AVX2, DISABLED_MfpProjectRowSpeed) {
#if HAVE_AVX2
  RunSpeed(av1_mfp_project_row_avx2, "avx2");
#else
  GTEST_SKIP();
#endif
}

TEST(MfpProjectRowTest, DispatcherMatchesC) { RunSuite(av1_mfp_project_row); }

}  // namespace
