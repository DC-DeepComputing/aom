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

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_ports/aom_timer.h"
#include "av1/common/cdef_block.h"
#include "av1/common/enums.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"

using libaom_test::ACMRandom;

namespace {

using CdefFilterBlockFunctions = std::array<cdef_filter_block_func, 4>;

using cdef_dir_param_t =
    std::tuple<CdefFilterBlockFunctions, CdefFilterBlockFunctions, BLOCK_SIZE,
               int, int>;

class CDEFBlockTest : public ::testing::TestWithParam<cdef_dir_param_t> {
 public:
  ~CDEFBlockTest() override = default;
  void SetUp() override {
    cdef_ = GET_PARAM(0);
    ref_cdef_ = GET_PARAM(1);
    bsize_ = GET_PARAM(2);
    boundary_ = GET_PARAM(3);
    depth_ = GET_PARAM(4);
  }

 protected:
  BLOCK_SIZE bsize_;
  int boundary_;
  int depth_;
  CdefFilterBlockFunctions cdef_;
  CdefFilterBlockFunctions ref_cdef_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFBlockTest);

using CDEFBlockHighbdTest = CDEFBlockTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFBlockHighbdTest);

using CDEFSpeedTest = CDEFBlockTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFSpeedTest);

using CDEFSpeedHighbdTest = CDEFBlockTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFSpeedHighbdTest);

int64_t test_cdef(BLOCK_SIZE bsize, int iterations,
                  CdefFilterBlockFunctions cdef,
                  CdefFilterBlockFunctions ref_cdef, int boundary, int depth) {
  aom_usec_timer ref_timer;
  int64_t ref_elapsed_time = 0;
  const int size = 8;
  const int ysize = size + 2 * CDEF_VBORDER;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, s[ysize * CDEF_BSTRIDE]);
  DECLARE_ALIGNED(16, static uint16_t, d[size * size]);
  DECLARE_ALIGNED(16, static uint16_t, ref_d[size * size]);
  memset(ref_d, 0, sizeof(ref_d));
  memset(d, 0, sizeof(d));

  int error = 0, pristrength = 0, secstrength, dir;
  int pridamping, secdamping, bits, level, count,
      errdepth = 0, errpristrength = 0, errsecstrength = 0, errboundary = 0,
      errpridamping = 0, errsecdamping = 0;
  unsigned int pos = 0;

  const int block_width =
      ((bsize == BLOCK_8X8) || (bsize == BLOCK_8X4)) ? 8 : 4;
  const int block_height =
      ((bsize == BLOCK_8X8) || (bsize == BLOCK_4X8)) ? 8 : 4;
  assert(depth >= 8);  // Suppress static analysis warning w/negative depth - 8.
  const unsigned int max_pos = size * size >> static_cast<int>(depth == 8);
  for (pridamping = 3 + depth - 8; pridamping < 7 - 3 * !!boundary + depth - 8;
       pridamping++) {
    for (secdamping = 3 + depth - 8;
         secdamping < 7 - 3 * !!boundary + depth - 8; secdamping++) {
      for (count = 0; count < iterations; count++) {
        for (level = 0; level < (1 << depth) && !error;
             level += (2 + 6 * !!boundary) << (depth - 8)) {
          for (bits = 1; bits <= depth && !error; bits += 1 + 3 * !!boundary) {
            for (unsigned int i = 0; i < sizeof(s) / sizeof(*s); i++)
              s[i] = clamp((rnd.Rand16() & ((1 << bits) - 1)) + level, 0,
                           (1 << depth) - 1);
            if (boundary) {
              if (boundary & 1) {  // Left
                for (int i = 0; i < ysize; i++)
                  for (int j = 0; j < CDEF_HBORDER; j++)
                    s[i * CDEF_BSTRIDE + j] = CDEF_VERY_LARGE;
              }
              if (boundary & 2) {  // Right
                for (int i = 0; i < ysize; i++)
                  for (int j = CDEF_HBORDER + size; j < CDEF_BSTRIDE; j++)
                    s[i * CDEF_BSTRIDE + j] = CDEF_VERY_LARGE;
              }
              if (boundary & 4) {  // Above
                for (int i = 0; i < CDEF_VBORDER; i++)
                  for (int j = 0; j < CDEF_BSTRIDE; j++)
                    s[i * CDEF_BSTRIDE + j] = CDEF_VERY_LARGE;
              }
              if (boundary & 8) {  // Below
                for (int i = CDEF_VBORDER + size; i < ysize; i++)
                  for (int j = 0; j < CDEF_BSTRIDE; j++)
                    s[i * CDEF_BSTRIDE + j] = CDEF_VERY_LARGE;
              }
            }
            for (dir = 0; dir < 8; dir++) {
              for (pristrength = 0; pristrength <= 19 << (depth - 8) && !error;
                   pristrength += (1 + 4 * !!boundary) << (depth - 8)) {
                if (pristrength == 16) pristrength = 19;
                for (secstrength = 0; secstrength <= 4 << (depth - 8) && !error;
                     secstrength += 1 << (depth - 8)) {
                  if (secstrength == 3 << (depth - 8)) continue;

                  const int strength_index =
                      (secstrength == 0) | ((pristrength == 0) << 1);

                  aom_usec_timer_start(&ref_timer);
                  ref_cdef[strength_index](
                      ref_d, size,
                      s + CDEF_HBORDER + CDEF_VBORDER * CDEF_BSTRIDE,
                      pristrength, secstrength, dir, pridamping, secdamping,
                      depth - 8, block_width, block_height);
                  aom_usec_timer_mark(&ref_timer);
                  ref_elapsed_time += aom_usec_timer_elapsed(&ref_timer);
                  // If cdef and ref_cdef are the same, we're just testing
                  // speed
                  if (cdef[0] != ref_cdef[0])
                    API_REGISTER_STATE_CHECK(cdef[strength_index](
                        d, size, s + CDEF_HBORDER + CDEF_VBORDER * CDEF_BSTRIDE,
                        pristrength, secstrength, dir, pridamping, secdamping,
                        depth - 8, block_width, block_height));
                  if (ref_cdef[0] != cdef[0]) {
                    for (pos = 0; pos < max_pos && !error; pos++) {
                      error = ref_d[pos] != d[pos];
                      errdepth = depth;
                      errpristrength = pristrength;
                      errsecstrength = secstrength;
                      errboundary = boundary;
                      errpridamping = pridamping;
                      errsecdamping = secdamping;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  pos--;
  EXPECT_EQ(0, error) << "Error: CDEFBlockTest, SIMD and C mismatch."
                      << std::endl
                      << "First error at " << pos % size << "," << pos / size
                      << " (" << (int16_t)ref_d[pos] << " : " << (int16_t)d[pos]
                      << ") " << std::endl
                      << "pristrength: " << errpristrength << std::endl
                      << "pridamping: " << errpridamping << std::endl
                      << "secstrength: " << errsecstrength << std::endl
                      << "secdamping: " << errsecdamping << std::endl
                      << "depth: " << errdepth << std::endl
                      << "size: " << bsize << std::endl
                      << "boundary: " << errboundary << std::endl
                      << std::endl;

  return ref_elapsed_time;
}

void test_cdef_speed(BLOCK_SIZE bsize, int iterations,
                     CdefFilterBlockFunctions cdef,
                     CdefFilterBlockFunctions ref_cdef, int boundary,
                     int depth) {
  int64_t ref_elapsed_time =
      test_cdef(bsize, iterations, ref_cdef, ref_cdef, boundary, depth);

  int64_t elapsed_time =
      test_cdef(bsize, iterations, cdef, cdef, boundary, depth);

  std::cout << "C time: " << ref_elapsed_time << " us" << std::endl
            << "SIMD time: " << elapsed_time << " us" << std::endl;

  EXPECT_GT(ref_elapsed_time, elapsed_time)
      << "Error: CDEFSpeedTest, SIMD slower than C." << std::endl
      << "C time: " << ref_elapsed_time << " us" << std::endl
      << "SIMD time: " << elapsed_time << " us" << std::endl;
}

using find_dir_t = int (*)(const uint16_t *img, int stride, int32_t *var,
                           int coeff_shift);

using find_dir_param_t = std::tuple<find_dir_t, find_dir_t>;

class CDEFFindDirTest : public ::testing::TestWithParam<find_dir_param_t> {
 public:
  ~CDEFFindDirTest() override = default;
  void SetUp() override {
    finddir_ = GET_PARAM(0);
    ref_finddir_ = GET_PARAM(1);
  }

 protected:
  find_dir_t finddir_;
  find_dir_t ref_finddir_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFFindDirTest);

using CDEFFindDirSpeedTest = CDEFFindDirTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFFindDirSpeedTest);

void test_finddir(int (*finddir)(const uint16_t *img, int stride, int32_t *var,
                                 int coeff_shift),
                  int (*ref_finddir)(const uint16_t *img, int stride,
                                     int32_t *var, int coeff_shift)) {
  const int size = 8;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, s[size * size]);

  int error = 0;
  int depth, bits, level, count, errdepth = 0;
  int ref_res = 0, res = 0;
  int32_t ref_var = 0, var = 0;

  for (depth = 8; depth <= 12 && !error; depth += 2) {
    for (count = 0; count < 512 && !error; count++) {
      for (level = 0; level < (1 << depth) && !error;
           level += 1 << (depth - 8)) {
        for (bits = 1; bits <= depth && !error; bits++) {
          for (unsigned int i = 0; i < sizeof(s) / sizeof(*s); i++)
            s[i] = clamp((rnd.Rand16() & ((1 << bits) - 1)) + level, 0,
                         (1 << depth) - 1);
          for (int c = 0; c < 1 + 9 * (finddir == ref_finddir); c++)
            ref_res = ref_finddir(s, size, &ref_var, depth - 8);
          if (finddir != ref_finddir)
            API_REGISTER_STATE_CHECK(res = finddir(s, size, &var, depth - 8));
          if (ref_finddir != finddir) {
            if (res != ref_res || var != ref_var) error = 1;
            errdepth = depth;
          }
        }
      }
    }
  }

  EXPECT_EQ(0, error) << "Error: CDEFFindDirTest, SIMD and C mismatch."
                      << std::endl
                      << "return: " << res << " : " << ref_res << std::endl
                      << "var: " << var << " : " << ref_var << std::endl
                      << "depth: " << errdepth << std::endl
                      << std::endl;
}

void test_finddir_speed(int (*finddir)(const uint16_t *img, int stride,
                                       int32_t *var, int coeff_shift),
                        int (*ref_finddir)(const uint16_t *img, int stride,
                                           int32_t *var, int coeff_shift)) {
  aom_usec_timer ref_timer;
  aom_usec_timer timer;

  aom_usec_timer_start(&ref_timer);
  test_finddir(ref_finddir, ref_finddir);
  aom_usec_timer_mark(&ref_timer);
  int64_t ref_elapsed_time = aom_usec_timer_elapsed(&ref_timer);

  aom_usec_timer_start(&timer);
  test_finddir(finddir, finddir);
  aom_usec_timer_mark(&timer);
  int64_t elapsed_time = aom_usec_timer_elapsed(&timer);

  EXPECT_GT(ref_elapsed_time, elapsed_time)
      << "Error: CDEFFindDirSpeedTest, SIMD slower than C." << std::endl
      << "C time: " << ref_elapsed_time << " us" << std::endl
      << "SIMD time: " << elapsed_time << " us" << std::endl;
}

using find_dir_dual_t = void (*)(const uint16_t *img1, const uint16_t *img2,
                                 int stride, int32_t *var1, int32_t *var2,
                                 int coeff_shift, int *out1, int *out2);

using find_dir_dual_param_t = std::tuple<find_dir_dual_t, find_dir_dual_t>;

class CDEFFindDirDualTest
    : public ::testing::TestWithParam<find_dir_dual_param_t> {
 public:
  ~CDEFFindDirDualTest() override = default;
  void SetUp() override {
    finddir_ = GET_PARAM(0);
    ref_finddir_ = GET_PARAM(1);
  }

 protected:
  find_dir_dual_t finddir_;
  find_dir_dual_t ref_finddir_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFFindDirDualTest);

using CDEFFindDirDualSpeedTest = CDEFFindDirDualTest;
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFFindDirDualSpeedTest);

void test_finddir_dual(
    void (*finddir)(const uint16_t *img1, const uint16_t *img2, int stride,
                    int32_t *var1, int32_t *var2, int coeff_shift, int *out1,
                    int *out2),
    void (*ref_finddir)(const uint16_t *img1, const uint16_t *img2, int stride,
                        int32_t *var1, int32_t *var2, int coeff_shift,
                        int *out1, int *out2)) {
  const int size_wd = 16;
  const int size_ht = 8;
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(16, uint16_t, s[size_ht * size_wd]);

  int error = 0, errdepth = 0;
  int32_t ref_var[2] = { 0 };
  int ref_dir[2] = { 0 };
  int32_t var[2] = { 0 };
  int dir[2] = { 0 };

  for (int depth = 8; depth <= 12 && !error; depth += 2) {
    for (int count = 0; count < 512 && !error; count++) {
      for (int level = 0; level < (1 << depth) && !error;
           level += 1 << (depth - 8)) {
        for (int bits = 1; bits <= depth && !error; bits++) {
          for (unsigned int i = 0; i < sizeof(s) / sizeof(*s); i++)
            s[i] = clamp((rnd.Rand16() & ((1 << bits) - 1)) + level, 0,
                         (1 << depth) - 1);
          for (int c = 0; c < 1 + 9 * (finddir == ref_finddir); c++)
            ref_finddir(s, s + 8, size_wd, &ref_var[0], &ref_var[1], depth - 8,
                        &ref_dir[0], &ref_dir[1]);
          if (finddir != ref_finddir)
            API_REGISTER_STATE_CHECK(finddir(s, s + 8, size_wd, &var[0],
                                             &var[1], depth - 8, &dir[0],
                                             &dir[1]));
          if (ref_finddir != finddir) {
            for (int j = 0; j < 2; j++) {
              if (ref_dir[j] != dir[j] || ref_var[j] != var[j]) error = 1;
            }
            errdepth = depth;
          }
        }
      }
    }
  }

  for (int j = 0; j < 2; j++) {
    EXPECT_EQ(0, error) << "Error: CDEFFindDirTest, SIMD and C mismatch."
                        << std::endl
                        << "direction: " << dir[j] << " : " << ref_dir[j]
                        << std::endl
                        << "variance: " << var[j] << " : " << ref_var[j]
                        << std::endl
                        << "depth: " << errdepth << std::endl
                        << std::endl;
  }
}

void test_finddir_dual_speed(
    void (*finddir)(const uint16_t *img1, const uint16_t *img2, int stride,
                    int32_t *var1, int32_t *var2, int coeff_shift, int *out1,
                    int *out2),
    void (*ref_finddir)(const uint16_t *img1, const uint16_t *img2, int stride,
                        int32_t *var1, int32_t *var2, int coeff_shift,
                        int *out1, int *out2)) {
  aom_usec_timer ref_timer;
  aom_usec_timer timer;

  aom_usec_timer_start(&ref_timer);
  test_finddir_dual(ref_finddir, ref_finddir);
  aom_usec_timer_mark(&ref_timer);
  const double ref_elapsed_time =
      static_cast<double>(aom_usec_timer_elapsed(&ref_timer));

  aom_usec_timer_start(&timer);
  test_finddir_dual(finddir, finddir);
  aom_usec_timer_mark(&timer);
  const double elapsed_time =
      static_cast<double>(aom_usec_timer_elapsed(&timer));

  printf(
      "ref_time=%lf \t simd_time=%lf \t "
      "gain=%lf \n",
      ref_elapsed_time, elapsed_time, ref_elapsed_time / elapsed_time);
}

#define MAX_CDEF_BLOCK 256

constexpr int kIterations = 100;

using CDEFCopyRect8To16 = void (*)(uint16_t *dst, int dstride,
                                   const uint8_t *src, int sstride, int width,
                                   int height);

using CDEFCopyRect8To16Param = std::tuple<CDEFCopyRect8To16, CDEFCopyRect8To16>;

class CDEFCopyRect8to16Test
    : public ::testing::TestWithParam<CDEFCopyRect8To16Param> {
 public:
  CDEFCopyRect8to16Test()
      : rnd_(libaom_test::ACMRandom::DeterministicSeed()),
        test_func_(GET_PARAM(0)), ref_func_(GET_PARAM(1)) {}
  ~CDEFCopyRect8to16Test() override = default;
  void SetUp() override {
    src_ = reinterpret_cast<uint8_t *>(
        aom_memalign(8, sizeof(uint8_t) * MAX_CDEF_BLOCK * MAX_CDEF_BLOCK));
    ASSERT_NE(src_, nullptr);
    ref_dst_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, sizeof(uint16_t) * MAX_CDEF_BLOCK * MAX_CDEF_BLOCK));
    ASSERT_NE(ref_dst_, nullptr);
    test_dst_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, sizeof(uint16_t) * MAX_CDEF_BLOCK * MAX_CDEF_BLOCK));
    ASSERT_NE(test_dst_, nullptr);
  }

  void TearDown() override {
    aom_free(src_);
    aom_free(ref_dst_);
    aom_free(test_dst_);
  }

  void test_copy_rect_8_to_16(CDEFCopyRect8To16 test_func,
                              CDEFCopyRect8To16 ref_func) {
    constexpr int stride = MAX_CDEF_BLOCK;
    int error = 0;
    for (int k = 0; k < kIterations && !error; k++) {
      // This function operates on values of width that are either 4 or a
      // multiple of 8. For height, generate a random value between 1 and 256,
      // making sure it is even.
      const int width = k == 0 ? 4 : (rnd_.Rand8() % 32 + 1) * 8;
      const int height = k == 0 ? 4 : (rnd_.Rand8() % 128 + 1) * 2;
      for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
          src_[i * stride + j] = rnd_.Rand8();
        }
      }

      ref_func(ref_dst_, stride, src_, stride, width, height);
      test_func(test_dst_, stride, src_, stride, width, height);

      int i, j;
      for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
          if (test_dst_[i * stride + j] != ref_dst_[i * stride + j]) {
            error = 1;
            break;
          }
        }
        if (error) {
          break;
        }
      }
      EXPECT_EQ(0, error)
          << "Error: CDEFCopyRect8to16Test, SIMD and C mismatch." << std::endl
          << "First error at " << i << "," << j << " ("
          << ref_dst_[i * stride + j] << " : " << test_dst_[i * stride + j]
          << ") " << std::endl
          << "width: " << width << std::endl
          << "height: " << height << std::endl
          << std::endl;
    }
  }

 protected:
  libaom_test::ACMRandom rnd_;
  uint8_t *src_;
  uint16_t *ref_dst_;
  uint16_t *test_dst_;
  CDEFCopyRect8To16 test_func_;
  CDEFCopyRect8To16 ref_func_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFCopyRect8to16Test);

#if CONFIG_AV1_HIGHBITDEPTH
using CDEFCopyRect16To16 = void (*)(uint16_t *dst, int dstride,
                                    const uint16_t *src, int sstride, int width,
                                    int height);

using CDEFCopyRect16To16Param =
    std::tuple<CDEFCopyRect16To16, CDEFCopyRect16To16>;

class CDEFCopyRect16to16Test
    : public ::testing::TestWithParam<CDEFCopyRect16To16Param> {
 public:
  CDEFCopyRect16to16Test()
      : rnd_(libaom_test::ACMRandom::DeterministicSeed()),
        test_func_(GET_PARAM(0)), ref_func_(GET_PARAM(1)) {}
  ~CDEFCopyRect16to16Test() override = default;
  void SetUp() override {
    src_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, sizeof(uint16_t) * MAX_CDEF_BLOCK * MAX_CDEF_BLOCK));
    ASSERT_NE(src_, nullptr);
    ref_dst_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, sizeof(uint16_t) * MAX_CDEF_BLOCK * MAX_CDEF_BLOCK));
    ASSERT_NE(ref_dst_, nullptr);
    test_dst_ = reinterpret_cast<uint16_t *>(
        aom_memalign(16, sizeof(uint16_t) * MAX_CDEF_BLOCK * MAX_CDEF_BLOCK));
    ASSERT_NE(test_dst_, nullptr);
  }

  void TearDown() override {
    aom_free(src_);
    aom_free(ref_dst_);
    aom_free(test_dst_);
  }

  void test_copy_rect_16_to_16(CDEFCopyRect16To16 test_func,
                               CDEFCopyRect16To16 ref_func) {
    constexpr int stride = MAX_CDEF_BLOCK;
    int error = 0;
    for (int k = 0; k < kIterations && !error; k++) {
      // This function operates on values of width that are either 4 or a
      // multiple of 8. For height, generate a random value between 1 and 256,
      // making sure it is even.
      const int width = k == 0 ? 4 : (rnd_.Rand8() % 32 + 1) * 8;
      const int height = k == 0 ? 4 : (rnd_.Rand8() % 128 + 1) * 2;
      for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
          src_[i * stride + j] = rnd_.Rand16();
        }
      }

      ref_func(ref_dst_, stride, src_, stride, width, height);
      test_func(test_dst_, stride, src_, stride, width, height);

      int i, j;
      for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
          if (test_dst_[i * stride + j] != ref_dst_[i * stride + j]) {
            error = 1;
            break;
          }
        }
        if (error) {
          break;
        }
      }
      EXPECT_EQ(0, error)
          << "Error: CDEFCopyRect16to16Test, SIMD and C mismatch." << std::endl
          << "First error at " << i << "," << j << " ("
          << ref_dst_[i * stride + j] << " : " << test_dst_[i * stride + j]
          << ") " << std::endl
          << "width: " << width << std::endl
          << "height: " << height << std::endl
          << std::endl;
    }
  }

 protected:
  libaom_test::ACMRandom rnd_;
  uint16_t *src_;
  uint16_t *ref_dst_;
  uint16_t *test_dst_;
  CDEFCopyRect16To16 test_func_;
  CDEFCopyRect16To16 ref_func_;
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFCopyRect16to16Test);
#endif  // CONFIG_AV1_HIGHBITDEPTH

TEST_P(CDEFBlockTest, TestSIMDNoMismatch) {
  test_cdef(bsize_, 1, cdef_, ref_cdef_, boundary_, depth_);
}

TEST_P(CDEFBlockHighbdTest, TestSIMDHighbdNoMismatch) {
  test_cdef(bsize_, 1, cdef_, ref_cdef_, boundary_, depth_);
}

TEST_P(CDEFSpeedTest, DISABLED_TestSpeed) {
  test_cdef_speed(bsize_, 4, cdef_, ref_cdef_, boundary_, depth_);
}

TEST_P(CDEFSpeedHighbdTest, DISABLED_TestSpeed) {
  test_cdef_speed(bsize_, 4, cdef_, ref_cdef_, boundary_, depth_);
}

TEST_P(CDEFFindDirTest, TestSIMDNoMismatch) {
  test_finddir(finddir_, ref_finddir_);
}

TEST_P(CDEFFindDirSpeedTest, DISABLED_TestSpeed) {
  test_finddir_speed(finddir_, ref_finddir_);
}

TEST_P(CDEFFindDirDualTest, TestSIMDNoMismatch) {
  test_finddir_dual(finddir_, ref_finddir_);
}

TEST_P(CDEFFindDirDualTest, TestCdefBStride) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  DECLARE_ALIGNED(32, uint16_t, inbuf[CDEF_INBUF_SIZE]);
  struct BlockPair {
    int r1, c1, r2, c2;
  };
  static const BlockPair kPairs[] = {
    { CDEF_VBORDER, CDEF_HBORDER, CDEF_VBORDER, CDEF_HBORDER + 16 },
    { CDEF_VBORDER, CDEF_HBORDER + 1, CDEF_VBORDER + 16, CDEF_HBORDER + 25 },
    { CDEF_VBORDER + 8, CDEF_HBORDER + 56, CDEF_VBORDER + 24,
      CDEF_HBORDER + 8 },
  };
  for (int shift = 0; shift <= 4; shift += 2) {
    const int depth = 8 + shift;
    const uint16_t max_val = static_cast<uint16_t>((1 << depth) - 1);
    for (size_t pair = 0; pair < sizeof(kPairs) / sizeof(kPairs[0]); ++pair) {
      const BlockPair &bp = kPairs[pair];
      for (int pat = 0; pat < 10; ++pat) {
        for (size_t idx = 0; idx < static_cast<size_t>(CDEF_INBUF_SIZE);
             ++idx) {
          inbuf[idx] = CDEF_VERY_LARGE;
        }
        uint16_t *const img1 = inbuf + bp.r1 * CDEF_BSTRIDE + bp.c1;
        uint16_t *const img2 = inbuf + bp.r2 * CDEF_BSTRIDE + bp.c2;
        for (int r = 0; r < 8; ++r) {
          for (int c = 0; c < 8; ++c) {
            uint16_t v1 = 0;
            uint16_t v2 = 0;
            if (pat == 0) {
              v1 = 0;
              v2 = max_val;
            } else if (pat == 1) {
              v1 = max_val;
              v2 = 0;
            } else if (pat == 2) {
              v1 = ((r ^ c) & 1) ? max_val : 0;
              v2 = ((r ^ c) & 1) ? 0 : max_val;
            } else if (pat == 3) {
              const uint16_t lo = static_cast<uint16_t>((1 << shift) - 1);
              v1 = (c < 4) ? lo : static_cast<uint16_t>(max_val - lo);
              v2 = (r < 4) ? static_cast<uint16_t>(max_val - lo) : lo;
            } else if (pat == 4) {
              v1 = static_cast<uint16_t>(((r * 31 + c * 3) << shift) & max_val);
              v2 = static_cast<uint16_t>(((c * 29 + r * 5) << shift) & max_val);
            } else {
              v1 = static_cast<uint16_t>(rnd.Rand16() & max_val);
              v2 = static_cast<uint16_t>(rnd.Rand16() & max_val);
            }
            img1[r * CDEF_BSTRIDE + c] = v1;
            img2[r * CDEF_BSTRIDE + c] = v2;
          }
        }
        int32_t ref_var[2] = { -1, -1 };
        int32_t tst_var[2] = { -2, -2 };
        int ref_dir[2] = { -1, -1 };
        int tst_dir[2] = { -2, -2 };
        ref_finddir_(img1, img2, CDEF_BSTRIDE, &ref_var[0], &ref_var[1], shift,
                     &ref_dir[0], &ref_dir[1]);
        API_REGISTER_STATE_CHECK(finddir_(img1, img2, CDEF_BSTRIDE, &tst_var[0],
                                          &tst_var[1], shift, &tst_dir[0],
                                          &tst_dir[1]));
        for (int j = 0; j < 2; ++j) {
          ASSERT_EQ(ref_dir[j], tst_dir[j])
              << "shift=" << shift << " pair=" << pair << " pat=" << pat
              << " blk=" << j;
          ASSERT_EQ(ref_var[j], tst_var[j])
              << "shift=" << shift << " pair=" << pair << " pat=" << pat
              << " blk=" << j;
        }
      }
    }
  }
}

TEST_P(CDEFFindDirDualSpeedTest, DISABLED_TestSpeed) {
  test_finddir_dual_speed(finddir_, ref_finddir_);
}

TEST_P(CDEFCopyRect8to16Test, TestSIMDNoMismatch) {
  test_copy_rect_8_to_16(test_func_, ref_func_);
}

#if CONFIG_AV1_HIGHBITDEPTH
TEST_P(CDEFCopyRect16to16Test, TestSIMDNoMismatch) {
  test_copy_rect_16_to_16(test_func_, ref_func_);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

#if CONFIG_AV1_ENCODER && HAVE_AVX2
class CDEFMseCalcBlockLvl4Test : public ::testing::TestWithParam<int> {};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CDEFMseCalcBlockLvl4Test);

int AdjustStrengthRef(int strength, int32_t var) {
  const int i = (var >> 6) ? AOMMIN(get_msb(var >> 6), 12) : 0;
  return var ? (strength * (4 + i) + 8) >> 4 : 0;
}

void CalcMseLvl4Ref(const uint16_t *in, const uint8_t *ref_buf, int ref_stride,
                    int row, int col, const cdef_list *dlist, int cdef_count,
                    int dir[CDEF_NBLOCKS][CDEF_NBLOCKS],
                    int var[CDEF_NBLOCKS][CDEF_NBLOCKS], int damping, int pli,
                    int mi_wide_l2, uint64_t mse[4]) {
  static constexpr int kPriConvLvl4[2] = { 0, 11 };
  static constexpr int kSecConvLvl3[2] = { 0, 2 };
  static const cdef_filter_block_func kFilterC[4] = { &cdef_filter_8_0_c,
                                                      &cdef_filter_8_1_c,
                                                      &cdef_filter_8_2_c,
                                                      &cdef_filter_8_3_c };
  if (pli == 0) {
    for (int bi = 0; bi < cdef_count; ++bi) {
      const int by = dlist[bi].by;
      const int bx = dlist[bi].bx;
      dir[by][bx] = cdef_find_dir_c(&in[8 * by * CDEF_BSTRIDE + 8 * bx],
                                    CDEF_BSTRIDE, &var[by][bx], 0);
    }
  }
  const int shift = mi_wide_l2 + 1;
  const int bsize = 1 << shift;
  const int damp = damping - (pli != AOM_PLANE_Y);
  for (int gi = 0; gi < 4; ++gi) {
    const int pri = kPriConvLvl4[gi / 2];
    const int sec = kSecConvLvl3[gi % 2];
    uint64_t sse = 0;
    DECLARE_ALIGNED(16, uint8_t, dst8[8 * 8]);
    for (int bi = 0; bi < cdef_count; ++bi) {
      const int by = dlist[bi].by;
      const int bx = dlist[bi].bx;
      const int t =
          (pli != AOM_PLANE_Y) ? pri : AdjustStrengthRef(pri, var[by][bx]);
      const int sidx = (sec == 0) | ((t == 0) << 1);
      kFilterC[sidx](dst8, 8, &in[(by * CDEF_BSTRIDE << shift) + (bx << shift)],
                     t, sec, pri ? dir[by][bx] : 0, damp, damp, 0, bsize,
                     bsize);
      const uint8_t *rb =
          &ref_buf[(row + (by << shift)) * ref_stride + col + (bx << shift)];
      for (int r = 0; r < bsize; ++r) {
        for (int c = 0; c < bsize; ++c) {
          const int diff = dst8[r * 8 + c] - rb[r * ref_stride + c];
          sse += static_cast<uint64_t>(diff * diff);
        }
      }
    }
    if (pli < 2) {
      mse[gi] = sse;
    } else {
      mse[gi] += sse;
    }
  }
}

TEST_P(CDEFMseCalcBlockLvl4Test, TestSIMDNoMismatch) {
  const int damping = GetParam();
  ACMRandom rnd(ACMRandom::DeterministicSeed() + damping);
  DECLARE_ALIGNED(32, uint16_t, inbuf[CDEF_INBUF_SIZE]);
  uint16_t *const in = inbuf + CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER;
  constexpr int kRefStride = 197;
  constexpr int kRefRows = 160;
  std::vector<uint8_t> ref_y(kRefStride * kRefRows);
  std::vector<uint8_t> ref_u(kRefStride * kRefRows);
  std::vector<uint8_t> ref_v(kRefStride * kRefRows);
  cdef_list dlist[MI_SIZE_128X128 * MI_SIZE_128X128];
  const int kCounts[] = { 0, 1, 2, 3, 8, 15, 64 };

  for (int mode = 0; mode < 5; ++mode) {
    for (int boundary = 0; boundary < 16; ++boundary) {
      for (size_t i = 0; i < CDEF_INBUF_SIZE; ++i) {
        if (mode == 0) {
          inbuf[i] = 128;  // Flat block: forces var == 0, t == 0, dir == 0.
        } else if (mode == 1) {
          inbuf[i] = (rnd.Rand8() & 1) ? 255 : 0;  // Extreme contrast/clamping.
        } else if (mode == 2) {
          inbuf[i] =
              120 + (rnd.Rand8() & 3);  // Very low contrast: var==0, dir!=0.
        } else if (mode == 3) {
          inbuf[i] = 120 + (rnd.Rand8() & 15);  // Low/mid variance.
        } else {
          inbuf[i] = rnd.Rand8();
        }
      }
      if (boundary & 1) {
        for (int r = 0; r < (1 << MAX_SB_SIZE_LOG2) + 2 * CDEF_VBORDER; ++r)
          for (int c = 0; c < CDEF_HBORDER; ++c)
            inbuf[r * CDEF_BSTRIDE + c] = CDEF_VERY_LARGE;
      }
      if (boundary & 2) {
        for (int r = 0; r < (1 << MAX_SB_SIZE_LOG2) + 2 * CDEF_VBORDER; ++r)
          for (int c = CDEF_HBORDER + 64; c < CDEF_BSTRIDE; ++c)
            inbuf[r * CDEF_BSTRIDE + c] = CDEF_VERY_LARGE;
      }
      if (boundary & 4) {
        for (int r = 0; r < CDEF_VBORDER; ++r)
          for (int c = 0; c < CDEF_BSTRIDE; ++c)
            inbuf[r * CDEF_BSTRIDE + c] = CDEF_VERY_LARGE;
      }
      if (boundary & 8) {
        for (int r = CDEF_VBORDER + 64;
             r < (1 << MAX_SB_SIZE_LOG2) + 2 * CDEF_VBORDER; ++r)
          for (int c = 0; c < CDEF_BSTRIDE; ++c)
            inbuf[r * CDEF_BSTRIDE + c] = CDEF_VERY_LARGE;
      }
      for (size_t i = 0; i < ref_y.size(); ++i) {
        ref_y[i] = rnd.Rand8();
        ref_u[i] = rnd.Rand8();
        ref_v[i] = rnd.Rand8();
      }

      for (int cdef_count : kCounts) {
        for (int bi = 0; bi < cdef_count; ++bi) {
          dlist[bi].by = static_cast<uint8_t>((bi / 8) & 7);
          dlist[bi].bx = static_cast<uint8_t>(bi & 7);
        }
        const int row = (boundary & 3) * 4;
        const int col = ((boundary >> 2) & 3) * 4;

        int dir_ref[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
        int var_ref[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
        int dir_tst[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
        int var_tst[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
        uint64_t mse_ref[4] = { 99, 99, 99, 99 };
        uint64_t mse_tst[4] = { 77, 77, 77, 77 };

        CalcMseLvl4Ref(in, ref_y.data(), kRefStride, row, col, dlist,
                       cdef_count, dir_ref, var_ref, damping, 0, 2, mse_ref);
        API_REGISTER_STATE_CHECK(av1_cdef_mse_calc_block_lvl4_avx2(
            in, ref_y.data(), kRefStride, row, col, dlist, cdef_count, dir_tst,
            var_tst, damping, 0, 2, mse_tst));
        for (int gi = 0; gi < 4; ++gi) {
          ASSERT_EQ(mse_ref[gi], mse_tst[gi])
              << "Luma gi=" << gi << " damp=" << damping
              << " cnt=" << cdef_count << " bnd=" << boundary
              << " mode=" << mode;
        }
        for (int bi = 0; bi < cdef_count; ++bi) {
          const int by = dlist[bi].by;
          const int bx = dlist[bi].bx;
          ASSERT_EQ(dir_ref[by][bx], dir_tst[by][bx]);
          ASSERT_EQ(var_ref[by][bx], var_tst[by][bx]);
        }

        // Test both 4:2:0 (mi_wide_l2=1, 4x4) and 4:4:4 (mi_wide_l2=2, 8x8)
        // chroma across pli=1 (overwrite) + pli=2 (accumulate), cycling all
        // 8 directions so every (d & 3) parity branch is covered.
        for (int mi_wide_l2 : { 1, 2 }) {
          for (int bi = 0; bi < cdef_count; ++bi) {
            dir_ref[dlist[bi].by][dlist[bi].bx] = (bi + boundary + mode) & 7;
            dir_tst[dlist[bi].by][dlist[bi].bx] = (bi + boundary + mode) & 7;
          }
          uint64_t uv_ref[4] = { 123, 456, 789, 1011 };
          uint64_t uv_tst[4] = { 999, 888, 777, 666 };
          CalcMseLvl4Ref(in, ref_u.data(), kRefStride, row, col, dlist,
                         cdef_count, dir_ref, var_ref, damping, 1, mi_wide_l2,
                         uv_ref);
          API_REGISTER_STATE_CHECK(av1_cdef_mse_calc_block_lvl4_avx2(
              in, ref_u.data(), kRefStride, row, col, dlist, cdef_count,
              dir_tst, var_tst, damping, 1, mi_wide_l2, uv_tst));
          for (int gi = 0; gi < 4; ++gi) {
            ASSERT_EQ(uv_ref[gi], uv_tst[gi])
                << "Chroma U mi_wide_l2=" << mi_wide_l2 << " gi=" << gi;
          }
          CalcMseLvl4Ref(in, ref_v.data(), kRefStride, row, col, dlist,
                         cdef_count, dir_ref, var_ref, damping, 2, mi_wide_l2,
                         uv_ref);
          API_REGISTER_STATE_CHECK(av1_cdef_mse_calc_block_lvl4_avx2(
              in, ref_v.data(), kRefStride, row, col, dlist, cdef_count,
              dir_tst, var_tst, damping, 2, mi_wide_l2, uv_tst));
          for (int gi = 0; gi < 4; ++gi) {
            ASSERT_EQ(uv_ref[gi], uv_tst[gi])
                << "Chroma U+V mi_wide_l2=" << mi_wide_l2 << " gi=" << gi;
          }
        }
      }
    }
  }
}

TEST_P(CDEFMseCalcBlockLvl4Test, TestCoverageAndAlignments) {
  const int damping = GetParam();
  ACMRandom rnd(ACMRandom::DeterministicSeed() + 77 + damping);
  DECLARE_ALIGNED(32, uint16_t, inbuf[CDEF_INBUF_SIZE]);
  uint16_t *const in = inbuf + CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER;
  constexpr int kRefStride = 197;
  constexpr int kRefRows = 160;
  constexpr size_t kRefSize = static_cast<size_t>(kRefStride) * kRefRows;
  std::vector<uint8_t> ref_raw(kRefSize + 16);
  cdef_list dlist[MI_SIZE_128X128 * MI_SIZE_128X128];

  // 1. Chroma 4x4 (mi_wide_l2=1) with CDEF_VERY_LARGE boundaries at +32,
  // cycling all 8 directions and unaligned ref_buffer (+1, +2, +3 bytes).
  for (int boundary = 0; boundary < 16; ++boundary) {
    for (int align_off = 1; align_off <= 3; ++align_off) {
      for (size_t idx = 0; idx < static_cast<size_t>(CDEF_INBUF_SIZE); ++idx) {
        inbuf[idx] = rnd.Rand8();
      }
      if (boundary & 1) {
        for (int r = 0; r < 32 + 2 * CDEF_VBORDER; ++r)
          for (int c = 0; c < CDEF_HBORDER; ++c)
            inbuf[r * CDEF_BSTRIDE + c] = CDEF_VERY_LARGE;
      }
      if (boundary & 2) {
        for (int r = 0; r < 32 + 2 * CDEF_VBORDER; ++r)
          for (int c = CDEF_HBORDER + 32; c < CDEF_BSTRIDE; ++c)
            inbuf[r * CDEF_BSTRIDE + c] = CDEF_VERY_LARGE;
      }
      if (boundary & 4) {
        for (int r = 0; r < CDEF_VBORDER; ++r)
          for (int c = 0; c < CDEF_BSTRIDE; ++c)
            inbuf[r * CDEF_BSTRIDE + c] = CDEF_VERY_LARGE;
      }
      if (boundary & 8) {
        for (int r = CDEF_VBORDER + 32; r < 32 + 2 * CDEF_VBORDER; ++r)
          for (int c = 0; c < CDEF_BSTRIDE; ++c)
            inbuf[r * CDEF_BSTRIDE + c] = CDEF_VERY_LARGE;
      }
      uint8_t *const ref_ptr = ref_raw.data() + align_off;
      for (size_t idx = 0; idx < kRefSize; ++idx) {
        ref_ptr[idx] = rnd.Rand8();
      }
      const int cdef_count = 64;
      int dir_ref[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
      int var_ref[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
      int dir_tst[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
      int var_tst[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
      for (int bi = 0; bi < cdef_count; ++bi) {
        dlist[bi].by = static_cast<uint8_t>((bi / 8) & 7);
        dlist[bi].bx = static_cast<uint8_t>(bi & 7);
        dir_ref[dlist[bi].by][dlist[bi].bx] = (bi + boundary + align_off) & 7;
        dir_tst[dlist[bi].by][dlist[bi].bx] = (bi + boundary + align_off) & 7;
      }
      uint64_t mse_ref[4] = { 17, 29, 43, 61 };
      uint64_t mse_tst[4] = { 17, 29, 43, 61 };
      const int row = align_off * 3;
      const int col = align_off * 5;
      for (int pli = 1; pli <= 2; ++pli) {
        CalcMseLvl4Ref(in, ref_ptr, kRefStride, row, col, dlist, cdef_count,
                       dir_ref, var_ref, damping, pli, 1, mse_ref);
        API_REGISTER_STATE_CHECK(av1_cdef_mse_calc_block_lvl4_avx2(
            in, ref_ptr, kRefStride, row, col, dlist, cdef_count, dir_tst,
            var_tst, damping, pli, 1, mse_tst));
        for (int gi = 0; gi < 4; ++gi) {
          ASSERT_EQ(mse_ref[gi], mse_tst[gi])
              << "Chroma4x4+32 damp=" << damping << " bnd=" << boundary
              << " align=" << align_off << " pli=" << pli << " gi=" << gi;
        }
      }
    }
  }

  // 2. Explicit patterns triggering luma t == 0 with (d & 3) == 0 (d = 0, 4)
  // and contrast levels spanning all t/shift thresholds across damping 3..6,
  // tested with unaligned ref_buffer (+1, +2, +3 bytes).
  for (size_t idx = 0; idx < static_cast<size_t>(CDEF_INBUF_SIZE); ++idx) {
    inbuf[idx] = 128;
  }
  const int cdef_count = 16;
  for (int bi = 0; bi < cdef_count; ++bi) {
    const int by = bi / 8;
    const int bx = bi & 7;
    dlist[bi].by = static_cast<uint8_t>(by);
    dlist[bi].bx = static_cast<uint8_t>(bx);
    uint16_t *const blk = &in[8 * by * CDEF_BSTRIDE + 8 * bx];
    if (bi == 0) {
      // Non-flat pattern giving dir == 0 and var == 0 -> t == 0, (d & 3) == 0.
      blk[2 * CDEF_BSTRIDE + 5] = 129;
      blk[3 * CDEF_BSTRIDE + 4] = 129;
      blk[4 * CDEF_BSTRIDE + 3] = 129;
    } else if (bi == 1) {
      // Non-flat pattern giving dir == 4 and var == 0 -> t == 0, (d & 3) == 0.
      blk[2 * CDEF_BSTRIDE + 2] = 129;
      blk[3 * CDEF_BSTRIDE + 3] = 129;
      blk[4 * CDEF_BSTRIDE + 4] = 129;
    } else {
      static const int kAmps[14] = { 1,  2,  4,  8,  12,  16,  24,
                                     32, 48, 64, 96, 128, 192, 255 };
      const int amp = kAmps[bi - 2];
      const uint16_t lo = static_cast<uint16_t>((255 - amp) / 2);
      const uint16_t hi = static_cast<uint16_t>(lo + amp);
      for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
          const int diag = ((bi & 1) ? c : r) < 4;
          blk[r * CDEF_BSTRIDE + c] = diag ? lo : hi;
        }
      }
    }
  }
  for (int align_off = 1; align_off <= 3; ++align_off) {
    uint8_t *const ref_ptr = ref_raw.data() + align_off;
    for (size_t idx = 0; idx < kRefSize; ++idx) {
      ref_ptr[idx] = rnd.Rand8();
    }
    int dir_ref[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
    int var_ref[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
    int dir_tst[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
    int var_tst[CDEF_NBLOCKS][CDEF_NBLOCKS] = { { 0 } };
    uint64_t mse_ref[4] = { 0 };
    uint64_t mse_tst[4] = { 0 };
    CalcMseLvl4Ref(in, ref_ptr, kRefStride, 3, 5, dlist, cdef_count, dir_ref,
                   var_ref, damping, 0, 2, mse_ref);
    API_REGISTER_STATE_CHECK(av1_cdef_mse_calc_block_lvl4_avx2(
        in, ref_ptr, kRefStride, 3, 5, dlist, cdef_count, dir_tst, var_tst,
        damping, 0, 2, mse_tst));
    ASSERT_EQ(0, dir_ref[0][0]);
    ASSERT_EQ(0, var_ref[0][0]);
    ASSERT_EQ(4, dir_ref[0][1]);
    ASSERT_EQ(0, var_ref[0][1]);
    for (int gi = 0; gi < 4; ++gi) {
      ASSERT_EQ(mse_ref[gi], mse_tst[gi])
          << "Luma explicit t/shift damp=" << damping << " align=" << align_off
          << " gi=" << gi;
    }
    for (int bi = 0; bi < cdef_count; ++bi) {
      const int by = dlist[bi].by;
      const int bx = dlist[bi].bx;
      ASSERT_EQ(dir_ref[by][bx], dir_tst[by][bx]);
      ASSERT_EQ(var_ref[by][bx], var_tst[by][bx]);
    }
    for (int pli = 1; pli <= 2; ++pli) {
      CalcMseLvl4Ref(in, ref_ptr, kRefStride, 3, 5, dlist, cdef_count, dir_ref,
                     var_ref, damping, pli, 2, mse_ref);
      API_REGISTER_STATE_CHECK(av1_cdef_mse_calc_block_lvl4_avx2(
          in, ref_ptr, kRefStride, 3, 5, dlist, cdef_count, dir_tst, var_tst,
          damping, pli, 2, mse_tst));
      for (int gi = 0; gi < 4; ++gi) {
        ASSERT_EQ(mse_ref[gi], mse_tst[gi])
            << "Chroma8x8 explicit align=" << align_off << " pli=" << pli
            << " gi=" << gi;
      }
    }
  }
}
#endif  // CONFIG_AV1_ENCODER && HAVE_AVX2

using std::make_tuple;

#if ((AOM_ARCH_X86 && HAVE_SSSE3) || HAVE_SSE4_1 || HAVE_AVX2 || HAVE_NEON || \
     HAVE_RVV)
static const CdefFilterBlockFunctions kCdefFilterFuncC[] = {
  { &cdef_filter_8_0_c, &cdef_filter_8_1_c, &cdef_filter_8_2_c,
    &cdef_filter_8_3_c }
};

static const CdefFilterBlockFunctions kCdefFilterHighbdFuncC[] = {
  { &cdef_filter_16_0_c, &cdef_filter_16_0_c, &cdef_filter_16_0_c,
    &cdef_filter_16_0_c }
};
#endif

#if AOM_ARCH_X86 && HAVE_SSSE3
static const CdefFilterBlockFunctions kCdefFilterFuncSsse3[] = {
  { &cdef_filter_8_0_ssse3, &cdef_filter_8_1_ssse3, &cdef_filter_8_2_ssse3,
    &cdef_filter_8_3_ssse3 }
};

static const CdefFilterBlockFunctions kCdefFilterHighbdFuncSsse3[] = {
  { &cdef_filter_16_0_ssse3, &cdef_filter_16_1_ssse3, &cdef_filter_16_2_ssse3,
    &cdef_filter_16_3_ssse3 }
};

INSTANTIATE_TEST_SUITE_P(
    SSSE3, CDEFBlockTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncSsse3),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    SSSE3, CDEFBlockHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncSsse3),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(10, 13, 2)));
INSTANTIATE_TEST_SUITE_P(SSSE3, CDEFFindDirTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_ssse3,
                                                      &cdef_find_dir_c)));
INSTANTIATE_TEST_SUITE_P(SSSE3, CDEFFindDirDualTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_dual_ssse3,
                                                      &cdef_find_dir_dual_c)));

INSTANTIATE_TEST_SUITE_P(
    SSSE3, CDEFCopyRect8to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_8bit_to_16bit_c,
                                 &cdef_copy_rect8_8bit_to_16bit_ssse3)));

#if CONFIG_AV1_HIGHBITDEPTH
INSTANTIATE_TEST_SUITE_P(
    SSSE3, CDEFCopyRect16to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_16bit_to_16bit_c,
                                 &cdef_copy_rect8_16bit_to_16bit_ssse3)));
#endif  // CONFIG_AV1_HIGHBITDEPTH
#endif

#if HAVE_SSE4_1
static const CdefFilterBlockFunctions kCdefFilterFuncSse4_1[] = {
  { &cdef_filter_8_0_sse4_1, &cdef_filter_8_1_sse4_1, &cdef_filter_8_2_sse4_1,
    &cdef_filter_8_3_sse4_1 }
};

static const CdefFilterBlockFunctions kCdefFilterHighbdFuncSse4_1[] = {
  { &cdef_filter_16_0_sse4_1, &cdef_filter_16_1_sse4_1,
    &cdef_filter_16_2_sse4_1, &cdef_filter_16_3_sse4_1 }
};

INSTANTIATE_TEST_SUITE_P(
    SSE4_1, CDEFBlockTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncSse4_1),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, CDEFBlockHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncSse4_1),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(10, 13, 2)));
INSTANTIATE_TEST_SUITE_P(SSE4_1, CDEFFindDirTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_sse4_1,
                                                      &cdef_find_dir_c)));
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, CDEFFindDirDualTest,
    ::testing::Values(make_tuple(&cdef_find_dir_dual_sse4_1,
                                 &cdef_find_dir_dual_c)));

INSTANTIATE_TEST_SUITE_P(
    SSE4_1, CDEFCopyRect8to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_8bit_to_16bit_c,
                                 &cdef_copy_rect8_8bit_to_16bit_sse4_1)));

#if CONFIG_AV1_HIGHBITDEPTH
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, CDEFCopyRect16to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_16bit_to_16bit_c,
                                 &cdef_copy_rect8_16bit_to_16bit_sse4_1)));
#endif  // CONFIG_AV1_HIGHBITDEPTH
#endif

#if HAVE_AVX2
static const CdefFilterBlockFunctions kCdefFilterFuncAvx2[] = {
  { &cdef_filter_8_0_avx2, &cdef_filter_8_1_avx2, &cdef_filter_8_2_avx2,
    &cdef_filter_8_3_avx2 }
};

static const CdefFilterBlockFunctions kCdefFilterHighbdFuncAvx2[] = {
  { &cdef_filter_16_0_avx2, &cdef_filter_16_1_avx2, &cdef_filter_16_2_avx2,
    &cdef_filter_16_3_avx2 }
};

INSTANTIATE_TEST_SUITE_P(
    AVX2, CDEFBlockTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncAvx2),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    AVX2, CDEFBlockHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncAvx2),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(10, 13, 2)));
INSTANTIATE_TEST_SUITE_P(AVX2, CDEFFindDirTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_avx2,
                                                      &cdef_find_dir_c)));
INSTANTIATE_TEST_SUITE_P(AVX2, CDEFFindDirDualTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_dual_avx2,
                                                      &cdef_find_dir_dual_c)));

INSTANTIATE_TEST_SUITE_P(
    AVX2, CDEFCopyRect8to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_8bit_to_16bit_c,
                                 &cdef_copy_rect8_8bit_to_16bit_avx2)));

TEST(AVX2, CDEFCopyRect8to16WidthsAndAlignments) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  static const int kWidths[] = { 1,  2,  3,  4,  5,  6,  7,  8,   12, 15,
                                 16, 31, 32, 63, 64, 79, 95, 127, 135 };
  static const int kHeights[] = { 2, 4, 6, 8 };
  constexpr int kSrcStride = 157;
  constexpr int kDstStride = 163;
  constexpr int kMaxRows = 12;
  constexpr uint16_t kCanary = 0x5aa5;
  std::vector<uint8_t> src_buf(static_cast<size_t>(kSrcStride) * kMaxRows + 32);
  std::vector<uint16_t> ref_buf(static_cast<size_t>(kDstStride) * kMaxRows +
                                32);
  std::vector<uint16_t> tst_buf(static_cast<size_t>(kDstStride) * kMaxRows +
                                32);

  for (int src_off : { 0, 1, 3, 7 }) {
    for (int dst_off : { 0, 1, 3 }) {
      for (int height : kHeights) {
        for (int width : kWidths) {
          for (size_t idx = 0; idx < src_buf.size(); ++idx) {
            src_buf[idx] = rnd.Rand8();
          }
          std::fill(ref_buf.begin(), ref_buf.end(), kCanary);
          std::fill(tst_buf.begin(), tst_buf.end(), kCanary);
          const uint8_t *const src_ptr = src_buf.data() + src_off;
          uint16_t *const ref_ptr = ref_buf.data() + dst_off;
          uint16_t *const tst_ptr = tst_buf.data() + dst_off;
          cdef_copy_rect8_8bit_to_16bit_c(ref_ptr, kDstStride, src_ptr,
                                          kSrcStride, width, height);
          API_REGISTER_STATE_CHECK(cdef_copy_rect8_8bit_to_16bit_avx2(
              tst_ptr, kDstStride, src_ptr, kSrcStride, width, height));
          for (size_t idx = 0; idx < ref_buf.size(); ++idx) {
            const bool in_dst = idx >= static_cast<size_t>(dst_off);
            const size_t rel = in_dst ? idx - static_cast<size_t>(dst_off) : 0;
            const size_t r = rel / static_cast<size_t>(kDstStride);
            const size_t c = rel % static_cast<size_t>(kDstStride);
            const bool in_rect = in_dst && r < static_cast<size_t>(height) &&
                                 c < static_cast<size_t>(width);
            ASSERT_EQ(ref_buf[idx], tst_buf[idx])
                << (in_rect ? "mismatch" : "canary overwrite") << " idx=" << idx
                << " r=" << r << " c=" << c << " width=" << width
                << " height=" << height << " src_off=" << src_off
                << " dst_off=" << dst_off;
          }
        }
      }
    }
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
INSTANTIATE_TEST_SUITE_P(
    AVX2, CDEFCopyRect16to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_16bit_to_16bit_c,
                                 &cdef_copy_rect8_16bit_to_16bit_avx2)));
#endif  // CONFIG_AV1_HIGHBITDEPTH
#if CONFIG_AV1_ENCODER
INSTANTIATE_TEST_SUITE_P(AVX2, CDEFMseCalcBlockLvl4Test,
                         ::testing::Range(3, 7));
#endif  // CONFIG_AV1_ENCODER
#endif

#if HAVE_NEON
static const CdefFilterBlockFunctions kCdefFilterFuncNeon[] = {
  { &cdef_filter_8_0_neon, &cdef_filter_8_1_neon, &cdef_filter_8_2_neon,
    &cdef_filter_8_3_neon }
};

static const CdefFilterBlockFunctions kCdefFilterHighbdFuncNeon[] = {
  { &cdef_filter_16_0_neon, &cdef_filter_16_1_neon, &cdef_filter_16_2_neon,
    &cdef_filter_16_3_neon }
};

INSTANTIATE_TEST_SUITE_P(
    NEON, CDEFBlockTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncNeon),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    NEON, CDEFBlockHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncNeon),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(10, 13, 2)));
INSTANTIATE_TEST_SUITE_P(NEON, CDEFFindDirTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_neon,
                                                      &cdef_find_dir_c)));
INSTANTIATE_TEST_SUITE_P(NEON, CDEFFindDirDualTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_dual_neon,
                                                      &cdef_find_dir_dual_c)));

INSTANTIATE_TEST_SUITE_P(
    NEON, CDEFCopyRect8to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_8bit_to_16bit_c,
                                 &cdef_copy_rect8_8bit_to_16bit_neon)));

#if CONFIG_AV1_HIGHBITDEPTH
INSTANTIATE_TEST_SUITE_P(
    NEON, CDEFCopyRect16to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_16bit_to_16bit_c,
                                 &cdef_copy_rect8_16bit_to_16bit_neon)));
#endif  // CONFIG_AV1_HIGHBITDEPTH
#endif

#if HAVE_RVV
static const CdefFilterBlockFunctions kCdefFilterFuncRvv[] = {
  { &cdef_filter_8_0_rvv, &cdef_filter_8_1_rvv, &cdef_filter_8_2_rvv,
    &cdef_filter_8_3_rvv }
};

static const CdefFilterBlockFunctions kCdefFilterHighbdFuncRvv[] = {
  { &cdef_filter_16_0_rvv, &cdef_filter_16_1_rvv, &cdef_filter_16_2_rvv,
    &cdef_filter_16_3_rvv }
};

INSTANTIATE_TEST_SUITE_P(
    RVV, CDEFBlockTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncRvv),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    RVV, CDEFBlockHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncRvv),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Range(10, 13, 2)));
INSTANTIATE_TEST_SUITE_P(RVV, CDEFFindDirTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_rvv,
                                                      &cdef_find_dir_c)));

INSTANTIATE_TEST_SUITE_P(
    RVV, CDEFCopyRect8to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_8bit_to_16bit_c,
                                 &cdef_copy_rect8_8bit_to_16bit_rvv)));

INSTANTIATE_TEST_SUITE_P(
    RVV, CDEFCopyRect16to16Test,
    ::testing::Values(make_tuple(&cdef_copy_rect8_16bit_to_16bit_c,
                                 &cdef_copy_rect8_16bit_to_16bit_rvv)));
#endif

// Test speed for all supported architectures
#if AOM_ARCH_X86 && HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(
    SSSE3, CDEFSpeedTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncSsse3),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    SSSE3, CDEFSpeedHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncSsse3),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(10)));
INSTANTIATE_TEST_SUITE_P(SSSE3, CDEFFindDirSpeedTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_ssse3,
                                                      &cdef_find_dir_c)));
INSTANTIATE_TEST_SUITE_P(SSSE3, CDEFFindDirDualSpeedTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_dual_ssse3,
                                                      &cdef_find_dir_dual_c)));
#endif

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, CDEFSpeedTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncSse4_1),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, CDEFSpeedHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncSse4_1),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(10)));
INSTANTIATE_TEST_SUITE_P(SSE4_1, CDEFFindDirSpeedTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_sse4_1,
                                                      &cdef_find_dir_c)));
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, CDEFFindDirDualSpeedTest,
    ::testing::Values(make_tuple(&cdef_find_dir_dual_sse4_1,
                                 &cdef_find_dir_dual_c)));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, CDEFSpeedTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncAvx2),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    AVX2, CDEFSpeedHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncAvx2),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(10)));
INSTANTIATE_TEST_SUITE_P(AVX2, CDEFFindDirSpeedTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_avx2,
                                                      &cdef_find_dir_c)));
INSTANTIATE_TEST_SUITE_P(AVX2, CDEFFindDirDualSpeedTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_dual_avx2,
                                                      &cdef_find_dir_dual_c)));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, CDEFSpeedTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncNeon),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    NEON, CDEFSpeedHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncNeon),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(10)));
INSTANTIATE_TEST_SUITE_P(NEON, CDEFFindDirSpeedTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_neon,
                                                      &cdef_find_dir_c)));
INSTANTIATE_TEST_SUITE_P(NEON, CDEFFindDirDualSpeedTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_dual_neon,
                                                      &cdef_find_dir_dual_c)));
#endif

#if HAVE_RVV
INSTANTIATE_TEST_SUITE_P(
    RVV, CDEFSpeedTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterFuncRvv),
                       ::testing::ValuesIn(kCdefFilterFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(8)));
INSTANTIATE_TEST_SUITE_P(
    RVV, CDEFSpeedHighbdTest,
    ::testing::Combine(::testing::ValuesIn(kCdefFilterHighbdFuncRvv),
                       ::testing::ValuesIn(kCdefFilterHighbdFuncC),
                       ::testing::Values(BLOCK_4X4, BLOCK_4X8, BLOCK_8X4,
                                         BLOCK_8X8),
                       ::testing::Range(0, 16), ::testing::Values(10)));
INSTANTIATE_TEST_SUITE_P(RVV, CDEFFindDirSpeedTest,
                         ::testing::Values(make_tuple(&cdef_find_dir_rvv,
                                                      &cdef_find_dir_c)));
#endif

}  // namespace
