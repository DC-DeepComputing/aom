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

#include <vector>

#include "aom/aom_encoder.h"
#include "aom/aomcx.h"
#include "config/aom_config.h"
#include "gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/video_source.h"

namespace {

#if !CONFIG_REALTIME_ONLY
// Encodes with the fixed-wiener-filter option. The EncoderTest driver checks
// that every decoded frame matches the encoder's reconstruction, i.e. that the
// forced filter is signalled exactly as it is applied.
// Params: test mode, cpu-used, taps.
class FixedWienerFilterTest
    : public ::libaom_test::CodecTestWith3Params<libaom_test::TestMode, int,
                                                 const char *>,
      public ::libaom_test::EncoderTest {
 protected:
  FixedWienerFilterTest()
      : EncoderTest(GET_PARAM(0)), mode_(GET_PARAM(1)), speed_(GET_PARAM(2)),
        taps_(GET_PARAM(3)) {}

  void SetUp() override { InitializeConfig(mode_); }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, speed_);
      encoder->SetOption("fixed-wiener-filter", taps_);
    }
  }

  void FramePktHook(const aom_codec_cx_pkt_t *) override { ++frames_; }

  void Run(unsigned int width, unsigned int height, unsigned int frames) {
    ::libaom_test::RandomVideoSource video;
    video.SetSize(width, height);
    video.set_limit(frames);
    frames_ = 0;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    EXPECT_GT(frames_, 0u);
  }

  const libaom_test::TestMode mode_;
  const int speed_;
  const char *const taps_;
  unsigned int frames_ = 0;
};

// Smaller than one loop restoration unit.
TEST_P(FixedWienerFilterTest, SmallFrame) { Run(100, 72, 3); }

// Several loop restoration units per plane.
TEST_P(FixedWienerFilterTest, MultipleUnits) { Run(600, 400, 2); }

AV1_INSTANTIATE_TEST_SUITE(
    FixedWienerFilterTest,
    ::testing::Values(::libaom_test::kOnePassGood, ::libaom_test::kAllIntra),
    ::testing::Values(2, 6),
    ::testing::Values("0,8,32,8,32",       // low-pass (smoothing)
                      "0,0,0,0,0",         // identity
                      "-5,-23,46,-23,46",  // taps at the range limits
                      "10,8,-17,8,-17"));

// Noise made of one repeated 16x16 tile, which intrabc can copy.
class TiledNoiseVideoSource : public ::libaom_test::RandomVideoSource {
 protected:
  void FillFrame() override {
    if (!img_) return;
    uint8_t tile[16 * 16];
    for (uint8_t &p : tile) p = rnd_.Rand8();
    for (int plane = 0; plane < 3; ++plane) {
      const int w = aom_img_plane_width(img_, plane);
      const int h = aom_img_plane_height(img_, plane);
      for (int y = 0; y < h; ++y) {
        uint8_t *row = img_->planes[plane] + y * img_->stride[plane];
        for (int x = 0; x < w; ++x) row[x] = tile[(y % 16) * 16 + x % 16];
      }
    }
  }
};

// Checks the filter really is applied to every frame, including at speeds where
// the encoder would otherwise turn loop restoration off: on noise, a strong
// low-pass filter costs a lot of PSNR compared with an identity filter.
// Params: test mode, cpu-used.
class FixedWienerFilterEffectTest
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int>,
      public ::libaom_test::EncoderTest {
 protected:
  FixedWienerFilterEffectTest()
      : EncoderTest(GET_PARAM(0)), mode_(GET_PARAM(1)), speed_(GET_PARAM(2)) {}

  void SetUp() override {
    InitializeConfig(mode_);
    init_flags_ = AOM_CODEC_USE_PSNR;
    // Code every frame well, so that only the filter makes them differ from
    // the source.
    cfg_.rc_end_usage = AOM_Q;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, speed_);
      encoder->Control(AOME_SET_CQ_LEVEL, 10);
      encoder->SetOption("fixed-wiener-filter", taps_);
      if (screen_content_) {
        encoder->Control(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_SCREEN);
      }
    }
  }

  void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) override {
    psnrs_.push_back(pkt->data.psnr.psnr[0]);
  }

  // Returns the PSNR of each frame.
  std::vector<double> FramePsnrs(const char *taps) {
    taps_ = taps;
    psnrs_.clear();
    ::libaom_test::RandomVideoSource noise;
    TiledNoiseVideoSource tiled_noise;
    ::libaom_test::RandomVideoSource &video =
        screen_content_ ? tiled_noise : noise;
    // Intrabc isn't used in frames that are too small.
    if (screen_content_) {
      video.SetSize(352, 288);
    } else {
      video.SetSize(128, 96);
    }
    video.set_limit(2);
    EXPECT_NO_FATAL_FAILURE(RunLoop(&video));
    return psnrs_;
  }

  void ExpectFilterIsApplied() {
    const std::vector<double> identity = FramePsnrs("0,0,0,0,0");
    const std::vector<double> low_pass = FramePsnrs("0,8,32,8,32");
    ASSERT_EQ(identity.size(), 2u);
    ASSERT_EQ(low_pass.size(), identity.size());
    for (size_t i = 0; i < identity.size(); ++i) {
      EXPECT_LT(low_pass[i], identity[i] - 1.0) << "frame " << i;
    }
  }

  const libaom_test::TestMode mode_;
  const int speed_;
  const char *taps_ = nullptr;
  bool screen_content_ = false;
  std::vector<double> psnrs_;
};

TEST_P(FixedWienerFilterEffectTest, FilterIsApplied) {
  ExpectFilterIsApplied();
}

// Screen content enables intrabc, which rules out loop restoration in the
// frames that use it, so the fixed filter must take precedence over it.
TEST_P(FixedWienerFilterEffectTest, FilterIsAppliedToScreenContent) {
  screen_content_ = true;
  ExpectFilterIsApplied();
}

AV1_INSTANTIATE_TEST_SUITE(FixedWienerFilterEffectTest,
                           ::testing::Values(::libaom_test::kOnePassGood,
                                             ::libaom_test::kAllIntra),
                           ::testing::Values(2, 6));

TEST(FixedWienerFilterConfigTest, RequiresLoopRestoration) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  aom_codec_ctx_t enc;

  // Loop restoration is always off in realtime mode.
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_set_option(&enc, "fixed-wiener-filter", "0,8,32,8,32"),
            AOM_CODEC_INVALID_PARAM);
  EXPECT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);

  // Or when disabled.
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_ENABLE_RESTORATION, 0),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_set_option(&enc, "fixed-wiener-filter", "0,8,32,8,32"),
            AOM_CODEC_INVALID_PARAM);
  EXPECT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}
#endif  // !CONFIG_REALTIME_ONLY

}  // namespace
