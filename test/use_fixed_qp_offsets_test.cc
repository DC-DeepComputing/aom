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
#include <algorithm>

#include "config/aom_config.h"

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

#if CONFIG_REALTIME_ONLY
const unsigned int kUsage = AOM_USAGE_REALTIME;
#else
const unsigned int kUsage = AOM_USAGE_GOOD_QUALITY;
#endif

const int kUseFixedQPOffsetsMode[] = { 1, 2 };
const int kDirectCqQIndex[] = { 200, 201, 204, 249, 255, 280 };
const int kCpuUsed = 6;

enum CqSettingSequence {
  kControlQIndexThenLevel,
  kControlLevelThenQIndex,
  kOptionQIndexThenLevel,
  kOptionLevelThenQIndex,
  kControlQIndexThenOptionLevel,
  kOptionQIndexThenControlLevel,
  kControlLevelThenOptionQIndex,
  kOptionLevelThenControlQIndex,
};

const CqSettingSequence kCqSettingSequences[] = {
  kControlQIndexThenLevel,       kControlLevelThenQIndex,
  kOptionQIndexThenLevel,        kOptionLevelThenQIndex,
  kControlQIndexThenOptionLevel, kOptionQIndexThenControlLevel,
  kControlLevelThenOptionQIndex, kOptionLevelThenControlQIndex,
};

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

class CqSettingOrderTest
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode,
                                                 CqSettingSequence>,
      public ::libaom_test::EncoderTest {
 protected:
  CqSettingOrderTest() : EncoderTest(GET_PARAM(0)) {}
  ~CqSettingOrderTest() override = default;

  void SetUp() override {
    InitializeConfig(GET_PARAM(1));
    cfg_.rc_end_usage = AOM_Q;
    cfg_.use_fixed_qp_offsets = 2;
    cfg_.g_lag_in_frames = 0;
    sequence_ = GET_PARAM(2);
    const bool cq_level_last = sequence_ == kControlQIndexThenLevel ||
                               sequence_ == kOptionQIndexThenLevel ||
                               sequence_ == kControlQIndexThenOptionLevel ||
                               sequence_ == kOptionQIndexThenControlLevel;
    expected_qindex_ = cq_level_last ? av1_quantizer_to_qindex(35) : 200;
    checked_frames_ = 0;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() != 0) return;

    encoder->Control(AOME_SET_CPUUSED, kCpuUsed);
    switch (sequence_) {
      case kControlQIndexThenLevel:
        encoder->Control(AOME_SET_CQ_QINDEX, 200);
        encoder->Control(AOME_SET_CQ_LEVEL, 35);
        break;
      case kControlLevelThenQIndex:
        encoder->Control(AOME_SET_CQ_LEVEL, 35);
        encoder->Control(AOME_SET_CQ_QINDEX, 200);
        break;
      case kOptionQIndexThenLevel:
        encoder->SetOption("cq-qindex", "200");
        encoder->SetOption("cq-level", "35");
        break;
      case kOptionLevelThenQIndex:
        encoder->SetOption("cq-level", "35");
        encoder->SetOption("cq-qindex", "200");
        break;
      case kControlQIndexThenOptionLevel:
        encoder->Control(AOME_SET_CQ_QINDEX, 200);
        encoder->SetOption("cq-level", "35");
        break;
      case kOptionQIndexThenControlLevel:
        encoder->SetOption("cq-qindex", "200");
        encoder->Control(AOME_SET_CQ_LEVEL, 35);
        break;
      case kControlLevelThenOptionQIndex:
        encoder->Control(AOME_SET_CQ_LEVEL, 35);
        encoder->SetOption("cq-qindex", "200");
        break;
      case kOptionLevelThenControlQIndex:
        encoder->SetOption("cq-level", "35");
        encoder->Control(AOME_SET_CQ_QINDEX, 200);
        break;
    }
  }

  void PostEncodeFrameHook(::libaom_test::Encoder *encoder) override {
    int qindex = 0;
    encoder->Control(AOME_GET_LAST_QUANTIZER, &qindex);
    if (qindex == 0) return;

    EXPECT_EQ(qindex, expected_qindex_);
    ++checked_frames_;
  }

  void DoTest() {
    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 3);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    EXPECT_GT(checked_frames_, 0);
  }

 private:
  CqSettingSequence sequence_;
  int expected_qindex_;
  int checked_frames_;
};

#if !CONFIG_REALTIME_ONLY
const int kFrameCount = 17;

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
#if CONFIG_AV1_DECODER
    EXPECT_EQ(decoded_frames_, kFrameCount);
#endif  // CONFIG_AV1_DECODER
    EXPECT_GT(encoded_bytes_, 0u);
    *encoded_bytes = encoded_bytes_;
  }

 private:
  const ::libaom_test::TestMode encoding_mode_;
  int cq_level_;
  size_t encoded_bytes_;
  int decoded_frames_;
};

class ExtendedCqLosslessTest
    : public ::libaom_test::CodecTestWithParam<libaom_test::TestMode>,
      public ::libaom_test::EncoderTest {
 protected:
  ExtendedCqLosslessTest()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)) {}
  ~ExtendedCqLosslessTest() override = default;

  void SetUp() override {
    InitializeConfig(encoding_mode_);
    cfg_.rc_end_usage = AOM_Q;
    cfg_.rc_min_quantizer = 0;
    cfg_.rc_max_quantizer = 0;
    cfg_.use_fixed_qp_offsets = 1;
    cfg_.g_lag_in_frames = 0;
    init_flags_ = AOM_CODEC_USE_PSNR;
  }

  void BeginPassHook(unsigned int) override {
    min_psnr_ = 100.0;
    psnr_packets_ = 0;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, kCpuUsed);
      encoder->Control(AOME_SET_CQ_LEVEL, 70);
    }
  }

  void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) override {
    min_psnr_ = std::min(min_psnr_, pkt->data.psnr.psnr[0]);
    ++psnr_packets_;
  }

#if CONFIG_AV1_DECODER
  bool HandleDecodeResult(const aom_codec_err_t res_dec,
                          libaom_test::Decoder *decoder) override {
    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    if (res_dec == AOM_CODEC_OK) {
      int base_qindex = -1;
      AOM_CODEC_CONTROL_TYPECHECKED(decoder->GetDecoder(),
                                    AOMD_GET_LAST_QUANTIZER, &base_qindex);
      EXPECT_EQ(base_qindex, 0);
    }
    return res_dec == AOM_CODEC_OK;
  }
#endif  // CONFIG_AV1_DECODER

  void DoTest() {
    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 3);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    EXPECT_GT(psnr_packets_, 0);
    EXPECT_GE(min_psnr_, 100.0);
  }

 private:
  const ::libaom_test::TestMode encoding_mode_;
  double min_psnr_;
  int psnr_packets_;
};
#endif  // !CONFIG_REALTIME_ONLY

TEST_P(UseFixedQPOffsetsTest, TestQPOffsets) { DoTest(); }

TEST_P(UseFixedCQIndexOffsetsTest, TestDirectQIndexOffsets) { DoTest(); }

TEST_P(CqSettingOrderTest, LastSettingWins) { DoTest(); }

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

TEST_P(ExtendedCqLosslessTest, ExtendedCqLevelPreservesLossless) { DoTest(); }
#endif  // !CONFIG_REALTIME_ONLY

TEST(ExtendedCqLevelTest, AcceptsHighCqLevelOnlyInConstantQualityMode) {
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(aom_codec_av1_cx(), &cfg, kUsage));

  cfg.rc_end_usage = AOM_Q;
  aom_codec_ctx_t enc_q;
  ASSERT_EQ(AOM_CODEC_OK,
            aom_codec_enc_init(&enc_q, aom_codec_av1_cx(), &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc_q, AOME_SET_CQ_LEVEL, 70));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_control(&enc_q, AOME_SET_CQ_LEVEL, 71));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc_q, AOME_SET_CQ_QINDEX, 280));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_control(&enc_q, AOME_SET_CQ_QINDEX, 281));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc_q));

  cfg.rc_end_usage = AOM_CQ;
  aom_codec_ctx_t enc_cq;
  ASSERT_EQ(AOM_CODEC_OK,
            aom_codec_enc_init(&enc_cq, aom_codec_av1_cx(), &cfg, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_control(&enc_cq, AOME_SET_CQ_LEVEL, 70));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc_cq, AOME_SET_CQ_QINDEX, 255));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_control(&enc_cq, AOME_SET_CQ_QINDEX, 256));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc_cq));
}

AV1_INSTANTIATE_TEST_SUITE(UseFixedQPOffsetsTest,
                           ::testing::ValuesIn(kTestMode),
                           ::testing::ValuesIn(kUseFixedQPOffsetsMode),
                           ::testing::Values(35, 70));

AV1_INSTANTIATE_TEST_SUITE(UseFixedCQIndexOffsetsTest,
                           ::testing::ValuesIn(kTestMode),
                           ::testing::ValuesIn(kDirectCqQIndex));

AV1_INSTANTIATE_TEST_SUITE(CqSettingOrderTest, ::testing::ValuesIn(kTestMode),
                           ::testing::ValuesIn(kCqSettingSequences));

#if !CONFIG_REALTIME_ONLY
AV1_INSTANTIATE_TEST_SUITE(ExtendedCqLevelEndToEndTest,
                           ::testing::Values(::libaom_test::kOnePassGood));

AV1_INSTANTIATE_TEST_SUITE(ExtendedCqLosslessTest,
                           ::testing::Values(::libaom_test::kOnePassGood));
#endif  // !CONFIG_REALTIME_ONLY
}  // namespace
