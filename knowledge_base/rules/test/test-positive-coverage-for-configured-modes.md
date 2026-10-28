---
name: libaom/test/test-positive-coverage-for-configured-modes
description: When testing encoder features or frame flags requiring specific configuration settings, add positive tests for non-default usage modes configured to satisfy prerequisites alongside negative tests
rubric: testing
severity: should_fix
---

**Rule.** When adding unit or API tests for encoder features, controls, or frame
flags that require specific configuration constraints (such as zero lag in
frames), do not only test default modes that satisfy the constraint alongside
negative tests for unsupported default modes. Add positive test cases for other
usage modes (e.g., AOM_USAGE_GOOD_QUALITY) with their configuration modified to
satisfy the prerequisite.

**Why.** Testing only default configurations (such as realtime mode where lag is
0 by default) along with negative tests for modes whose defaults violate
prerequisites misses valid user configurations. Explicit positive tests ensure
features work across all usage modes when properly configured.

## Examples

Bad:

```cpp
// Only testing negative failure for good quality mode when g_lag_in_frames != 0:
TEST(EncodeAPI, FeatureNotSupportedWithLagInFrames) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);
  ASSERT_NE(cfg.g_lag_in_frames, 0);

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_encode(&enc, image, 0, 1, feature_flags),
            AOM_CODEC_INCAPABLE);
}
```

Good:

```cpp
// Positive test verifying good quality mode succeeds when g_lag_in_frames is set to 0:
TEST(EncodeAPI, FeatureGoodQualityZeroLagInFrames) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);
  ASSERT_NE(cfg.g_lag_in_frames, 0);
  cfg.g_lag_in_frames = 0;

  aom_codec_ctx_t enc;
  ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_encode(&enc, image, 0, 1, feature_flags), AOM_CODEC_OK);
}
```
