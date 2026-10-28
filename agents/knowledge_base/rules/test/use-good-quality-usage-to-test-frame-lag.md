---
name: libaom/test/use-good-quality-usage-to-test-frame-lag
description: When writing encoder unit tests for features or error handling requiring non-zero frame lag (g_lag_in_frames > 0), use AOM_USAGE_GOOD_QUALITY instead of AOM_USAGE_ALL_INTRA or AOM_USAGE_REALTIME
rubric: testing
severity: should_fix
---

**Rule.** When writing encoder unit tests for features, capability checks, or
error handling that depend on non-zero frame lag (g_lag_in_frames > 0),
initialize the encoder configuration with AOM_USAGE_GOOD_QUALITY instead of
AOM_USAGE_ALL_INTRA or AOM_USAGE_REALTIME. Verify that cfg.g_lag_in_frames is
non-zero before initiating test encoding.

**Why.** AOM_USAGE_ALL_INTRA enforces g_lag_in_frames == 0 during configuration
validation (RANGE_CHECK_HI(cfg, g_lag_in_frames, 0)), causing initialization
failure if non-zero lag is configured, while AOM_USAGE_REALTIME defaults to 0
lag. AOM_USAGE_GOOD_QUALITY permits non-zero frame lag and defaults to a
positive lag value, making it the proper usage mode for exercising lookahead and
frame-lag behaviors.

## Examples

Bad:

```cpp
TEST(EncodeAPI, PerFramePsnrOnlySupportedWithRealtime) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  // BAD: ALL_INTRA enforces g_lag_in_frames == 0 in config validation,
  // preventing testing of non-zero frame lag behavior.
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA),
            AOM_CODEC_OK);
  ...
}
```

Good:

```cpp
TEST(EncodeAPI, PerFramePsnrNotSupportedWithLagInFrames) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  // GOOD: GOOD_QUALITY permits and defaults to non-zero frame lag.
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);
  ASSERT_NE(cfg.g_lag_in_frames, 0u);
  ...
  aom_enc_frame_flags_t psnr_flags = AOM_EFLAG_CALCULATE_PSNR;
  ASSERT_EQ(aom_codec_encode(&enc, image, 0, 1, psnr_flags),
            AOM_CODEC_INCAPABLE);
}
```
