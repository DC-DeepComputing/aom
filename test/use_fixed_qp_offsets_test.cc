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
#include "config/aom_config.h"

#include <cstdio>

#include "av1/encoder/av1_quantize.h"
#include "gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {

const ::libaom_test::TestMode kTestMode[] =
#if CONFIG_REALTIME_ONLY
    { ::libaom_test::kRealTime };
#else
    { ::libaom_test::kRealTime, ::libaom_test::kOnePassGood };
#endif

const int kUseFixedQPOffsetsMode[] = { 1, 2 };
const int kDirectCqQIndex[] = { 200, 201, 204, 249, 255, 280 };
const int kFrameCount = 17;
const int kCpuUsed = 6;

class UseFixedQPOffsetsTest
    : public ::libaom_test::CodecTestWith3Params<libaom_test::TestMode, int,
                                                 int>,
      public ::libaom_test::EncoderTest {
 protected:
  UseFixedQPOffsetsTest() : EncoderTest(GET_PARAM(0)) {}
  ~UseFixedQPOffsetsTest() override = default;

  void SetUp() override {
    InitializeConfig(GET_PARAM(1));
    cfg_.kf_max_dist = 9999;
    cfg_.rc_end_usage = AOM_Q;
    cfg_.use_fixed_qp_offsets = GET_PARAM(2);
    frame_qp_ = GET_PARAM(3);
    expected_leaf_qindex_ =
        av1_quantizer_to_qindex(frame_qp_ > 63 ? 63 : frame_qp_);
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, kCpuUsed);
      encoder->Control(AOME_SET_CQ_LEVEL, frame_qp_);
    }
  }

  void PostEncodeFrameHook(::libaom_test::Encoder *encoder) override {
    int q = 0;
    encoder->Control(AOME_GET_LAST_QUANTIZER, &q);

    // If a call to encoder->EncodeFrame() results in a last QP of 0,
    // interpret as the frame being read into the lookahead buffer.
    if (q == 0) return;

    if (cfg_.use_fixed_qp_offsets == 2) {
      // Setting use_fixed_qp_offsets = 2 means every frame should use the same
      // qindex.
      ASSERT_EQ(q, expected_leaf_qindex_);
    } else {
      ASSERT_LE(q, expected_leaf_qindex_);
    }
  }

  void DoTest() {
    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 33);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  int frame_qp_;
  int expected_leaf_qindex_;
};

class UseFixedCQIndexOffsetsTest
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int>,
      public ::libaom_test::EncoderTest {
 protected:
  UseFixedCQIndexOffsetsTest() : EncoderTest(GET_PARAM(0)) {}
  ~UseFixedCQIndexOffsetsTest() override = default;

  void SetUp() override {
    InitializeConfig(GET_PARAM(1));
    cfg_.kf_max_dist = 9999;
    cfg_.rc_end_usage = AOM_Q;
    cfg_.use_fixed_qp_offsets = 2;
    cq_qindex_ = GET_PARAM(2);
    expected_leaf_qindex_ = AOMMIN(cq_qindex_, MAXQ);
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, kCpuUsed);
      encoder->Control(AOME_SET_CQ_QINDEX, cq_qindex_);
    }
  }

  void PostEncodeFrameHook(::libaom_test::Encoder *encoder) override {
    int q = 0;
    encoder->Control(AOME_GET_LAST_QUANTIZER, &q);

    // If a call to encoder->EncodeFrame() results in a last QP of 0,
    // interpret as the frame being read into the lookahead buffer.
    if (q == 0) return;

    ASSERT_EQ(q, expected_leaf_qindex_);
  }

  void DoTest() {
    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 33);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  int cq_qindex_;
  int expected_leaf_qindex_;
};

#if !CONFIG_REALTIME_ONLY
class ExtendedCqLevelEndToEndTest
    : public ::libaom_test::CodecTestWithParam<libaom_test::TestMode>,
      public ::libaom_test::EncoderTest {
 protected:
  ExtendedCqLevelEndToEndTest()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)) {}
  ~ExtendedCqLevelEndToEndTest() override = default;

  void SetUp() override { InitializeConfig(encoding_mode_); }

  void BeginPassHook(unsigned int) override {
    encoded_bytes_ = 0;
    decoded_frames_ = 0;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, kCpuUsed);
      encoder->Control(AOME_SET_CQ_LEVEL, cq_level_);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 0);
      encoder->Control(AV1E_SET_DELTAQ_MODE, 0);
      encoder->Control(AV1E_SET_ENABLE_TPL_MODEL, 0);
      encoder->Control(AV1E_SET_ENABLE_KEYFRAME_FILTERING, 1);
      encoder->Control(AV1E_SET_MIN_GF_INTERVAL, 16);
      encoder->Control(AV1E_SET_MAX_GF_INTERVAL, 16);
      encoder->Control(AV1E_SET_GF_MIN_PYRAMID_HEIGHT, 4);
      encoder->Control(AV1E_SET_GF_MAX_PYRAMID_HEIGHT, 4);
    }
  }

  void FramePktHook(const aom_codec_cx_pkt_t *pkt) override {
    encoded_bytes_ += pkt->data.frame.sz;
  }

  void DecompressedFrameHook(const aom_image_t &, aom_codec_pts_t) override {
    ++decoded_frames_;
  }

  void EncodeAtCqLevel(int cq_level, size_t *encoded_bytes) {
    cq_level_ = cq_level;
    InitializeConfig(encoding_mode_);
    cfg_.rc_end_usage = AOM_Q;
    cfg_.use_fixed_qp_offsets = 1;
    cfg_.g_lag_in_frames = 19;
    cfg_.kf_min_dist = 65;
    cfg_.kf_max_dist = 65;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, kFrameCount);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    EXPECT_EQ(decoded_frames_, kFrameCount);
    EXPECT_GT(encoded_bytes_, 0u);
    *encoded_bytes = encoded_bytes_;
  }

 private:
  const ::libaom_test::TestMode encoding_mode_;
  int cq_level_;
  size_t encoded_bytes_;
  int decoded_frames_;
};
#endif  // !CONFIG_REALTIME_ONLY

TEST_P(UseFixedQPOffsetsTest, TestQPOffsets) { DoTest(); }

TEST_P(UseFixedCQIndexOffsetsTest, TestDirectQIndexOffsets) { DoTest(); }

#if !CONFIG_REALTIME_ONLY
TEST_P(ExtendedCqLevelEndToEndTest,
       HighCqLevelsProduceDecodableMonotonicBitstreams) {
  size_t previous_encoded_bytes = 0;
  ASSERT_NO_FATAL_FAILURE(EncodeAtCqLevel(63, &previous_encoded_bytes));
  for (int cq_level = 64; cq_level <= 70; ++cq_level) {
    size_t encoded_bytes = 0;
    ASSERT_NO_FATAL_FAILURE(EncodeAtCqLevel(cq_level, &encoded_bytes));
    EXPECT_LT(encoded_bytes, previous_encoded_bytes)
        << "cq_level = " << cq_level;
    previous_encoded_bytes = encoded_bytes;
  }
}
#endif  // !CONFIG_REALTIME_ONLY

TEST(ExtendedCqLevelTest, AcceptsHighCqLevelOnlyInConstantQualityMode) {
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(aom_codec_av1_cx(), &cfg, 0));

  cfg.rc_end_usage = AOM_Q;
  aom_codec_ctx_t enc_q;
  ASSERT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc_q, aom_codec_av1_cx(), &cfg,
                                             0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc_q, AOME_SET_CQ_LEVEL, 70));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_control(&enc_q, AOME_SET_CQ_LEVEL, 71));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc_q, AOME_SET_CQ_QINDEX, 280));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_control(&enc_q, AOME_SET_CQ_QINDEX, 281));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc_q));

  cfg.rc_end_usage = AOM_CQ;
  aom_codec_ctx_t enc_cq;
  ASSERT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc_cq, aom_codec_av1_cx(), &cfg,
                                             0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_control(&enc_cq, AOME_SET_CQ_LEVEL, 70));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc_cq, AOME_SET_CQ_QINDEX, 255));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_control(&enc_cq, AOME_SET_CQ_QINDEX, 256));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc_cq));
}

// Exercise both public ways to select CQ settings without requiring test data.
class CqSettingSelectionTest : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    const unsigned int usage =
#if CONFIG_REALTIME_ONLY
        AOM_USAGE_REALTIME;
#else
        AOM_USAGE_GOOD_QUALITY;
#endif
    ASSERT_EQ(AOM_CODEC_OK,
              aom_codec_enc_config_default(aom_codec_av1_cx(), &cfg_, usage));
    cfg_.g_w = 64;
    cfg_.g_h = 64;
    cfg_.g_threads = 1;
    cfg_.g_timebase.num = 1;
    cfg_.g_timebase.den = 30;
    cfg_.g_lag_in_frames = 0;
    cfg_.rc_end_usage = AOM_Q;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 63;
    cfg_.use_fixed_qp_offsets = 2;
    ASSERT_EQ(AOM_CODEC_OK,
              aom_codec_enc_init(&enc_, aom_codec_av1_cx(), &cfg_, 0));
    initialized_ = true;
    ASSERT_EQ(AOM_CODEC_OK, aom_codec_control(&enc_, AOME_SET_CPUUSED, 8));
    ASSERT_NE(nullptr, aom_img_alloc(&img_, AOM_IMG_FMT_I420, 64, 64, 1));
    for (int plane = 0; plane < 3; ++plane) {
      const int size = plane == 0 ? 64 : 32;
      for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
          img_.planes[plane][y * img_.stride[plane] + x] =
              static_cast<uint8_t>((17 * x + 13 * y + 20 * plane) & 255);
        }
      }
    }
  }

  void TearDown() override {
    aom_img_free(&img_);
    if (initialized_) EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc_));
  }

  aom_codec_err_t SetQuality(bool use_qindex, unsigned int value,
                             bool use_option) {
    if (use_option) {
      char value_string[16];
      std::snprintf(value_string, sizeof(value_string), "%u", value);
      return aom_codec_set_option(&enc_, use_qindex ? "cq-qindex" : "cq-level",
                                  value_string);
    }
    return use_qindex ? aom_codec_control(&enc_, AOME_SET_CQ_QINDEX, value)
                      : aom_codec_control(&enc_, AOME_SET_CQ_LEVEL, value);
  }

  aom_codec_err_t SetCqLevel(unsigned int value) {
    return SetQuality(false, value, GetParam());
  }

  aom_codec_err_t SetCqQindex(unsigned int value) {
    return SetQuality(true, value, GetParam());
  }

  aom_codec_err_t SetMode(aom_rc_mode mode) {
    aom_codec_enc_cfg_t next_cfg = cfg_;
    next_cfg.rc_end_usage = mode;
    next_cfg.use_fixed_qp_offsets = mode == AOM_Q ? 2 : 0;
    const aom_codec_err_t result = aom_codec_enc_config_set(&enc_, &next_cfg);
    if (result == AOM_CODEC_OK) cfg_ = next_cfg;
    return result;
  }

  void EncodeAndExpectQindex(int expected_qindex) {
    ASSERT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc_, &img_, pts_++, 1, 0));
    aom_codec_iter_t iter = nullptr;
    while (aom_codec_get_cx_data(&enc_, &iter) != nullptr) {
    }
    int qindex = -1;
    ASSERT_EQ(AOM_CODEC_OK,
              aom_codec_control(&enc_, AOME_GET_LAST_QUANTIZER, &qindex));
    EXPECT_EQ(expected_qindex, qindex);
  }

  aom_codec_enc_cfg_t cfg_ = {};
  aom_codec_ctx_t enc_ = {};
  aom_image_t img_ = {};
  bool initialized_ = false;
  aom_codec_pts_t pts_ = 0;
};

TEST_P(CqSettingSelectionTest, LastSuccessfulSetterSelectsRepresentation) {
  ASSERT_EQ(AOM_CODEC_OK, SetCqQindex(201));
  ASSERT_NO_FATAL_FAILURE(EncodeAndExpectQindex(201));
  ASSERT_EQ(AOM_CODEC_OK, SetCqLevel(10));
  ASSERT_NO_FATAL_FAILURE(EncodeAndExpectQindex(40));
  ASSERT_EQ(AOM_CODEC_OK, SetCqQindex(204));
  ASSERT_NO_FATAL_FAILURE(EncodeAndExpectQindex(204));
}

TEST_P(CqSettingSelectionTest, ControlAndKeyValueSettersShareSelection) {
  ASSERT_EQ(AOM_CODEC_OK, SetCqQindex(201));
  ASSERT_EQ(AOM_CODEC_OK, SetQuality(false, 10, !GetParam()));
  ASSERT_NO_FATAL_FAILURE(EncodeAndExpectQindex(40));
  ASSERT_EQ(AOM_CODEC_OK, SetCqQindex(204));
  ASSERT_NO_FATAL_FAILURE(EncodeAndExpectQindex(204));
}

TEST_P(CqSettingSelectionTest, InvalidSetterPreservesPreviousSelection) {
  ASSERT_EQ(AOM_CODEC_OK, SetCqQindex(201));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, SetCqLevel(71));
  ASSERT_NO_FATAL_FAILURE(EncodeAndExpectQindex(201));
  if (GetParam()) {
    EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
              aom_codec_set_option(&enc_, "cq-level", "invalid"));
    ASSERT_NO_FATAL_FAILURE(EncodeAndExpectQindex(201));
  }

  ASSERT_EQ(AOM_CODEC_OK, SetCqLevel(10));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, SetCqQindex(281));
  ASSERT_NO_FATAL_FAILURE(EncodeAndExpectQindex(40));
  if (GetParam()) {
    EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
              aom_codec_set_option(&enc_, "cq-qindex", "invalid"));
    ASSERT_NO_FATAL_FAILURE(EncodeAndExpectQindex(40));
  }
}

TEST_P(CqSettingSelectionTest, ModeChangeIgnoresInactiveExtendedSetting) {
  ASSERT_EQ(AOM_CODEC_OK, SetCqLevel(70));
  ASSERT_EQ(AOM_CODEC_OK, SetCqQindex(200));
  EXPECT_EQ(AOM_CODEC_OK, SetMode(AOM_CQ));

  ASSERT_EQ(AOM_CODEC_OK, SetMode(AOM_Q));
  ASSERT_EQ(AOM_CODEC_OK, SetCqQindex(280));
  ASSERT_EQ(AOM_CODEC_OK, SetCqLevel(10));
  EXPECT_EQ(AOM_CODEC_OK, SetMode(AOM_CQ));
}

TEST_P(CqSettingSelectionTest, ExtendedActiveSettingRequiresConstantQuality) {
  ASSERT_EQ(AOM_CODEC_OK, SetCqLevel(70));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, SetMode(AOM_CQ));
  // A rejected mode change must leave the encoder in AOM_Q.
  ASSERT_EQ(AOM_CODEC_OK, SetCqQindex(280));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, SetMode(AOM_CQ));

  ASSERT_EQ(AOM_CODEC_OK, SetCqQindex(201));
  ASSERT_EQ(AOM_CODEC_OK, SetMode(AOM_CQ));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, SetCqLevel(64));
  ASSERT_EQ(AOM_CODEC_OK, SetMode(AOM_Q));
  ASSERT_NO_FATAL_FAILURE(EncodeAndExpectQindex(201));

  ASSERT_EQ(AOM_CODEC_OK, SetCqLevel(10));
  ASSERT_EQ(AOM_CODEC_OK, SetMode(AOM_CQ));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, SetCqQindex(256));
  ASSERT_EQ(AOM_CODEC_OK, SetMode(AOM_Q));
  ASSERT_NO_FATAL_FAILURE(EncodeAndExpectQindex(40));
}

INSTANTIATE_TEST_SUITE_P(All, CqSettingSelectionTest, ::testing::Bool());

AV1_INSTANTIATE_TEST_SUITE(UseFixedQPOffsetsTest,
                           ::testing::ValuesIn(kTestMode),
                           ::testing::ValuesIn(kUseFixedQPOffsetsMode),
                           ::testing::Values(35, 70));

AV1_INSTANTIATE_TEST_SUITE(UseFixedCQIndexOffsetsTest,
                           ::testing::ValuesIn(kTestMode),
                           ::testing::ValuesIn(kDirectCqQIndex));

#if !CONFIG_REALTIME_ONLY
AV1_INSTANTIATE_TEST_SUITE(ExtendedCqLevelEndToEndTest,
                           ::testing::Values(::libaom_test::kOnePassGood));
#endif  // !CONFIG_REALTIME_ONLY
}  // namespace
