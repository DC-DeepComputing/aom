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

#include <cstring>
#include <vector>

#include "gtest/gtest.h"

#include "config/aom_config.h"

#include "aom/aomdx.h"
#include "aom/aom_decoder.h"
#if CONFIG_AV1_ENCODER
#include "aom/aom_encoder.h"
#include "aom/aomcx.h"
#endif

namespace {

TEST(DecodeAPI, InvalidParams) {
  uint8_t buf[1] = { 0 };
  aom_codec_ctx_t dec;

  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_dec_init(nullptr, nullptr, nullptr, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_dec_init(&dec, nullptr, nullptr, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_decode(nullptr, nullptr, 0, nullptr));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_decode(nullptr, buf, 0, nullptr));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_decode(nullptr, buf, sizeof(buf), nullptr));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_decode(nullptr, nullptr, sizeof(buf), nullptr));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_destroy(nullptr));
  EXPECT_NE(aom_codec_error(nullptr), nullptr);
  EXPECT_EQ(aom_codec_error_detail(nullptr), nullptr);

  aom_codec_iface_t *iface = aom_codec_av1_dx();
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_dec_init(nullptr, iface, nullptr, 0));

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_dec_init(&dec, iface, nullptr, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
            aom_codec_decode(&dec, nullptr, sizeof(buf), nullptr));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_decode(&dec, buf, 0, nullptr));

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&dec));
}

TEST(DecodeAPI, InvalidControlId) {
  aom_codec_iface_t *iface = aom_codec_av1_dx();
  aom_codec_ctx_t dec;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_dec_init(&dec, iface, nullptr, 0));
  EXPECT_EQ(AOM_CODEC_ERROR, aom_codec_control(&dec, -1, 0));
  EXPECT_EQ(AOM_CODEC_INVALID_PARAM, aom_codec_control(&dec, 0, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&dec));
}

TEST(DecodeAPI, Buganizer503691210) {
  aom_codec_iface_t *iface = aom_codec_av1_dx();
  aom_codec_ctx_t dec;
  aom_codec_dec_cfg_t cfg = { 1, 424, 144, 1 };
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_dec_init(&dec, iface, &cfg, 0));

  av1_ref_frame_t ref;
  ref.idx = 0;
  // These control calls should fail with AOM_CODEC_ERROR because the decoder
  // is not fully initialized yet (no frames decoded).
  EXPECT_EQ(AOM_CODEC_ERROR, aom_codec_control(&dec, AV1_GET_REFERENCE, &ref));
  EXPECT_EQ(AOM_CODEC_ERROR, aom_codec_control(&dec, AV1_SET_REFERENCE, &ref));
  EXPECT_EQ(AOM_CODEC_ERROR, aom_codec_control(&dec, AV1_COPY_REFERENCE, &ref));

  aom_image_t img;
  EXPECT_EQ(AOM_CODEC_ERROR,
            aom_codec_control(&dec, AV1_COPY_NEW_FRAME_IMAGE, &img));

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&dec));
}

#if CONFIG_AV1_ENCODER && !CONFIG_REALTIME_ONLY
// The list of output frames stays valid until the next aom_codec_decode()
// call, so the caller may iterate over it more than once and gets the same
// film grain image each time.
TEST(DecodeAPI, GetFrameRepeatedIterationWithFilmGrain) {
  constexpr int kWidth = 64;
  constexpr int kHeight = 64;

  aom_codec_iface_t *enc_iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t enc_cfg;
  ASSERT_EQ(
      aom_codec_enc_config_default(enc_iface, &enc_cfg, AOM_USAGE_REALTIME),
      AOM_CODEC_OK);
  enc_cfg.g_w = kWidth;
  enc_cfg.g_h = kHeight;
  enc_cfg.g_lag_in_frames = 0;
  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, enc_iface, &enc_cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 10), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_FILM_GRAIN_TEST_VECTOR, 1),
            AOM_CODEC_OK);

  aom_image_t raw;
  ASSERT_NE(aom_img_alloc(&raw, AOM_IMG_FMT_I420, kWidth, kHeight, 1), nullptr);
  memset(raw.img_data, 128, raw.sz);
  ASSERT_EQ(aom_codec_encode(&enc, &raw, 0, 1, 0), AOM_CODEC_OK);
  aom_img_free(&raw);

  std::vector<uint8_t> frame;
  aom_codec_iter_t enc_iter = nullptr;
  const aom_codec_cx_pkt_t *pkt;
  while ((pkt = aom_codec_get_cx_data(&enc, &enc_iter)) != nullptr) {
    if (pkt->kind != AOM_CODEC_CX_FRAME_PKT) continue;
    const uint8_t *data = static_cast<const uint8_t *>(pkt->data.frame.buf);
    frame.insert(frame.end(), data, data + pkt->data.frame.sz);
  }
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
  ASSERT_FALSE(frame.empty());

  aom_codec_ctx_t dec;
  ASSERT_EQ(aom_codec_dec_init(&dec, aom_codec_av1_dx(), nullptr, 0),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_decode(&dec, frame.data(), frame.size(), nullptr),
            AOM_CODEC_OK);
  const unsigned char *first_y = nullptr;
  for (int i = 0; i < 8; ++i) {
    aom_codec_iter_t iter = nullptr;
    aom_image_t *img = aom_codec_get_frame(&dec, &iter);
    EXPECT_NE(img, nullptr) << "iteration " << i;
    if (img == nullptr) continue;
    EXPECT_EQ(img->d_w, static_cast<unsigned int>(kWidth));
    EXPECT_EQ(img->d_h, static_cast<unsigned int>(kHeight));
    if (i == 0) first_y = img->planes[AOM_PLANE_Y];
    EXPECT_EQ(img->planes[AOM_PLANE_Y], first_y) << "iteration " << i;
    EXPECT_EQ(aom_codec_get_frame(&dec, &iter), nullptr);
  }
  EXPECT_EQ(aom_codec_decode(&dec, nullptr, 0, nullptr), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_destroy(&dec), AOM_CODEC_OK);
}
#endif  // CONFIG_AV1_ENCODER && !CONFIG_REALTIME_ONLY

}  // namespace
