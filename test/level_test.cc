/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "config/aom_config.h"

#include <memory>
#include <string>

#include "gtest/gtest.h"

#if CONFIG_AV1_DECODER
#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#endif
#include "common/av1_config.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "test/yuv_video_source.h"

namespace {
const int kLevelMin = 0;
const int kLevelMax = 31;
const int kLevelKeepStats = 32;
// Speed settings tested
static const int kCpuUsedVectors[] = {
  1,
  2,
  3,
  4,
};

class LevelTest
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int>,
      public ::libaom_test::EncoderTest {
 protected:
  LevelTest()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        cpu_used_(GET_PARAM(2)), target_level_(31) {}

  ~LevelTest() override = default;

  void SetUp() override {
    InitializeConfig(encoding_mode_);
    if (encoding_mode_ != ::libaom_test::kRealTime) {
      cfg_.g_lag_in_frames = 5;
    } else {
      cfg_.rc_buf_sz = 1000;
      cfg_.rc_buf_initial_sz = 500;
      cfg_.rc_buf_optimal_sz = 600;
    }
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AV1E_SET_TARGET_SEQ_LEVEL_IDX, target_level_);
      if (encoding_mode_ != ::libaom_test::kRealTime) {
        encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
        encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
        encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
      }
    }

    int num_operating_points;
    encoder->Control(AV1E_GET_NUM_OPERATING_POINTS, &num_operating_points);
    ASSERT_EQ(num_operating_points, 1);
    encoder->Control(AV1E_GET_SEQ_LEVEL_IDX, level_);
    ASSERT_LE(level_[0], kLevelMax);
    ASSERT_GE(level_[0], kLevelMin);
  }

  libaom_test::TestMode encoding_mode_;
  int cpu_used_;
  int target_level_;
  int level_[32];
};

TEST(LevelTest, TestTargetLevelApi) {
  aom_codec_iface_t *codec = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(codec, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, codec, &cfg, 0));
  for (int operating_point = 0; operating_point <= 32; ++operating_point) {
    for (int level = 0; level <= 32; ++level) {
      const int target_level = operating_point * 100 + level;
      if (operating_point <= 31 &&
          ((level < (CONFIG_CWG_C013 ? 28 : 20) && level != 2 && level != 3 &&
            level != 6 && level != 7 && level != 10 && level != 11) ||
           level == kLevelMax || level == kLevelKeepStats)) {
        EXPECT_EQ(AOM_CODEC_OK,
                  AOM_CODEC_CONTROL_TYPECHECKED(
                      &enc, AV1E_SET_TARGET_SEQ_LEVEL_IDX, target_level));
      } else {
        EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
                  AOM_CODEC_CONTROL_TYPECHECKED(
                      &enc, AV1E_SET_TARGET_SEQ_LEVEL_IDX, target_level));
      }
    }
  }
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(LevelTest, InvalidOperatingPointIndexErrorDetail) {
  aom_codec_iface_t *codec = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(aom_codec_enc_config_default(codec, &cfg, 0), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_enc_init(&enc, codec, &cfg, 0), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_TARGET_SEQ_LEVEL_IDX, 3219),
            AOM_CODEC_INVALID_PARAM);
  EXPECT_EQ(aom_codec_error_detail(&enc),
            std::string("Invalid operating point index: 32"));
  EXPECT_EQ(aom_codec_set_option(&enc, "target-seq-level-idx", "3319"),
            AOM_CODEC_INVALID_PARAM);
  EXPECT_EQ(aom_codec_error_detail(&enc),
            std::string("Invalid operating point index: 33"));
  EXPECT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

void EncodeOneFrameAndDecode(aom_codec_ctx_t *enc, int width, int height) {
  aom_image_t *img = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, width, height, 1);
  ASSERT_NE(img, nullptr);
  memset(img->planes[0], 128, img->stride[0] * height);
  memset(img->planes[1], 128, img->stride[1] * ((height + 1) / 2));
  memset(img->planes[2], 128, img->stride[2] * ((height + 1) / 2));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(enc, img, 0, 1, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(enc, nullptr, 0, 0, 0));

#if CONFIG_AV1_DECODER
  aom_codec_ctx_t dec;
  aom_codec_iface_t *dec_iface = aom_codec_av1_dx();
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_dec_init(&dec, dec_iface, nullptr, 0));
#endif

  const aom_codec_cx_pkt_t *pkt;
  aom_codec_iter_t iter = nullptr;
  while ((pkt = aom_codec_get_cx_data(enc, &iter)) != nullptr) {
#if CONFIG_AV1_DECODER
    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      EXPECT_EQ(AOM_CODEC_OK,
                aom_codec_decode(
                    &dec, static_cast<const uint8_t *>(pkt->data.frame.buf),
                    pkt->data.frame.sz, nullptr));
    }
#endif
  }

#if CONFIG_AV1_DECODER
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&dec));
#endif
  aom_img_free(img);
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(enc));
}

TEST(LevelTest, TileLevelConformanceAutoLevel) {
  aom_codec_iface_t *codec = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(codec, &cfg, 0));
  cfg.g_w = 192;
  cfg.g_h = 288;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = 30;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, codec, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AV1E_SET_SUPERBLOCK_SIZE,
                                            AOM_SUPERBLOCK_SIZE_64X64));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AV1E_SET_TILE_COLUMNS, 2));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AV1E_SET_TILE_ROWS, 2));

  aom_fixed_buf_t *glob_headers = aom_codec_get_global_headers(&enc);
  ASSERT_NE(glob_headers, nullptr);
  ASSERT_NE(glob_headers->buf, nullptr);
  Av1Config av1_config;
  EXPECT_EQ(
      0, get_av1config_from_obu(static_cast<const uint8_t *>(glob_headers->buf),
                                glob_headers->sz, false, &av1_config));
  // Small resolution 192x288 @ 30fps would fit in Level 2.0 (seq_level_idx = 0)
  // based on resolution and framerate alone. However, with sb-size=64 and
  // tile-columns=2, tile-rows=2, 9 tiles (3x3) are produced. Level 2.0 allows
  // at most 8 tiles, so the encoder must promote the level to Level 3.0
  // (seq_level_idx = 4) to ensure conformance.
  EXPECT_EQ(av1_config.seq_level_idx_0, 4);
  free(glob_headers->buf);
  free(glob_headers);

  // Encode a frame to ensure the encoder runs and produces conforming output.
  ASSERT_NO_FATAL_FAILURE(EncodeOneFrameAndDecode(&enc, cfg.g_w, cfg.g_h));
}

TEST(LevelTest, TileLevelConformanceTargetLevel) {
  aom_codec_iface_t *codec = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(codec, &cfg, 0));
  cfg.g_w = 192;
  cfg.g_h = 288;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = 30;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, codec, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AV1E_SET_SUPERBLOCK_SIZE,
                                            AOM_SUPERBLOCK_SIZE_64X64));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AV1E_SET_TILE_COLUMNS, 2));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AV1E_SET_TILE_ROWS, 2));
  // Explicitly request Level 2.0 (seq_level_idx = 0).
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_TARGET_SEQ_LEVEL_IDX, 0));

  aom_fixed_buf_t *glob_headers = aom_codec_get_global_headers(&enc);
  ASSERT_NE(glob_headers, nullptr);
  ASSERT_NE(glob_headers->buf, nullptr);
  Av1Config av1_config;
  EXPECT_EQ(
      0, get_av1config_from_obu(static_cast<const uint8_t *>(glob_headers->buf),
                                glob_headers->sz, false, &av1_config));
  // When target-seq-level-idx is specified as Level 2.0, the signaled level is
  // Level 2.0, and tile configuration is clamped to satisfy Level 2.0 limits
  // (<= 8 tiles).
  EXPECT_EQ(av1_config.seq_level_idx_0, 0);
  free(glob_headers->buf);
  free(glob_headers);

  // Encode a frame to ensure tile clamping succeeds and bitstream conforms.
  ASSERT_NO_FATAL_FAILURE(EncodeOneFrameAndDecode(&enc, cfg.g_w, cfg.g_h));
}

TEST(LevelTest, TileLevelConformanceMultiOperatingPoints) {
  aom_codec_iface_t *codec = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(codec, &cfg, AOM_USAGE_REALTIME));
  cfg.g_w = 192;
  cfg.g_h = 288;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = 30;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, codec, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AV1E_SET_SUPERBLOCK_SIZE,
                                            AOM_SUPERBLOCK_SIZE_64X64));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AV1E_SET_TILE_COLUMNS, 2));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AV1E_SET_TILE_ROWS, 2));

  // Configure 2 temporal layers so there are 2 operating points.
  aom_svc_params_t svc_params = {};
  svc_params.number_spatial_layers = 1;
  svc_params.number_temporal_layers = 2;
  svc_params.scaling_factor_num[0] = 1;
  svc_params.scaling_factor_den[0] = 1;
  svc_params.framerate_factor[0] = 2;
  svc_params.framerate_factor[1] = 1;
  for (int i = 0; i < AOM_MAX_LAYERS; ++i) {
    svc_params.max_quantizers[i] = 52;
    svc_params.min_quantizers[i] = 10;
    svc_params.layer_target_bitrate[i] = 100;
  }
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_SVC_PARAMS, &svc_params));

  // Operating point 0 targets Level 3.0 (4).
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_TARGET_SEQ_LEVEL_IDX, 4));
  // Operating point 1 targets Level 2.0 (0): 1 * 100 + 0 = 100.
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_TARGET_SEQ_LEVEL_IDX, 100));

  aom_fixed_buf_t *glob_headers = aom_codec_get_global_headers(&enc);
  ASSERT_NE(glob_headers, nullptr);
  ASSERT_NE(glob_headers->buf, nullptr);
  Av1Config av1_config;
  EXPECT_EQ(
      0, get_av1config_from_obu(static_cast<const uint8_t *>(glob_headers->buf),
                                glob_headers->sz, false, &av1_config));
  // Operating point 0 signals Level 3.0 (4). Since OP 1 targets Level 2.0, the
  // tile configuration is clamped to the minimum target level across operating
  // points (Level 2.0 <= 8 tiles).
  EXPECT_EQ(av1_config.seq_level_idx_0, 4);
  free(glob_headers->buf);
  free(glob_headers);

  // Encode a frame to ensure tile clamping succeeds and bitstream conforms.
  ASSERT_NO_FATAL_FAILURE(EncodeOneFrameAndDecode(&enc, cfg.g_w, cfg.g_h));
}

TEST_P(LevelTest, TestTargetLevel19) {
  std::unique_ptr<libaom_test::VideoSource> video;
  video.reset(new libaom_test::Y4mVideoSource("park_joy_90p_8_420.y4m", 0, 10));
  ASSERT_NE(video, nullptr);
  // Level index 19 corresponding to level 6.3.
  target_level_ = 19;
  ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
}

TEST_P(LevelTest, TestLevelMonitoringLowBitrate) {
  // To save run time, we only test speed 4.
  if (cpu_used_ == 4) {
    libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 40);
    target_level_ = kLevelKeepStats;
    cfg_.rc_target_bitrate = 1000;
    cfg_.g_limit = 40;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_EQ(level_[0], 0);
  }
}

TEST_P(LevelTest, TestLevelMonitoringHighBitrate) {
  // To save run time, we only test speed 4.
  if (cpu_used_ == 4) {
    libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 40);
    target_level_ = kLevelKeepStats;
    cfg_.rc_target_bitrate = 4000;
    cfg_.g_limit = 40;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_EQ(level_[0], 4);
  }
}

TEST_P(LevelTest, TestTargetLevel0) {
  // To save run time, we only test speed 4.
  if (cpu_used_ == 4) {
    libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 50);
    const int target_level = 0;
    target_level_ = target_level;
    cfg_.rc_target_bitrate = 4000;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_EQ(level_[0], target_level);
  }
}

TEST_P(LevelTest, TestTargetLevelRecode) {
  if (cpu_used_ == 4 && encoding_mode_ == ::libaom_test::kTwoPassGood) {
    libaom_test::I420VideoSource video("rand_noise_w1280h720.yuv", 1280, 720,
                                       25, 1, 0, 10);
    const int target_level = 0005;
    target_level_ = target_level;
    cfg_.rc_target_bitrate = 5000;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }
}

AV1_INSTANTIATE_TEST_SUITE(LevelTest,
                           ::testing::Values(::libaom_test::kTwoPassGood,
                                             ::libaom_test::kOnePassGood),
                           ::testing::ValuesIn(kCpuUsedVectors));
}  // namespace
