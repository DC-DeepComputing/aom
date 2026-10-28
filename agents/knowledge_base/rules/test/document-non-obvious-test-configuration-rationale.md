---
name: libaom/test/document-non-obvious-test-configuration-rationale
description: When setting non-obvious test configurations or adjusting test thresholds to accommodate commits or build modes, accurately document technical constraints and commit SHAs
rubric: readability
severity: should_fix
---

**Rule.** When writing regression or unit tests that rely on specific
non-obvious configuration modes or parameters to reproduce an issue, add a
comment explaining why that specific setup is required. Additionally, when
adjusting test thresholds, tolerances, or assertions under conditional build
flags (such as #if CONFIG_REALTIME_ONLY) or to accommodate behavior changes from
past commits, accurately document the underlying configuration constraints (such
as forced zero-lag mode) and include the relevant commit hash (SHA) or issue
reference.

**Why.** In complex subsystems like encoder configuration and rate control,
certain modes or build flags automatically override, clamp, or reset specific
parameters (such as realtime mode forcing lag_in_frames to 0). Accurately
documenting technical constraints along with commit hashes or issue references
provides immediate git traceability and prevents future maintainers from
misunderstanding test preconditions or regressions.

## Examples

Bad:

```cpp
// Variant 1: Missing explanation of why good_quality mode is required
// This test is based on issue:449376308 to reproduce a segfault.
TEST(EncodeAPI, Issue449376308) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);
  cfg.g_lag_in_frames = 1;
}

// Variant 2: Inaccurate constraint description or missing commit SHA reference
#if CONFIG_REALTIME_ONLY
// Threshold lowered under CONFIG_REALTIME_ONLY. If we set
// cfg_.g_lag_in_frames=0, the variation is smaller.
ASSERT_GE(effective_datarate_, cfg_.rc_target_bitrate * 0.65);
#endif
```

Good:

```cpp
// Variant 1: Explicitly documents why good_quality is necessary
// This test is based on issue:449376308. Good-quality mode is needed
// because realtime mode forces lag_in_frames to 0 in set_encoder_config().
TEST(EncodeAPI, Issue449376308) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);
  cfg.g_lag_in_frames = 1;
}

// Variant 2: Accurately explains root cause and cites the commit hash
#if CONFIG_REALTIME_ONLY
// With CONFIG_REALTIME_ONLY=1, lag_in_frames is set to 0, and along with
// other factors in that configuration the threshold needs to be lowered
// after commit 2fed9c389d.
ASSERT_GE(effective_datarate_, cfg_.rc_target_bitrate * 0.65);
#endif
```
