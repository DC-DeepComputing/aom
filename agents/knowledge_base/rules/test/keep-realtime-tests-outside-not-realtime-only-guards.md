---
name: libaom/test/keep-realtime-tests-outside-not-realtime-only-guards
description: 'When configuring encoder tests, prefer AOM_USAGE_REALTIME for mode-independent tests and guard non-realtime tests with #if !CONFIG_REALTIME_ONLY while keeping realtime tests unguarded'
rubric: correctness
severity: blocking
---

**Rule.** When writing encoder tests or test scripts, prefer using
AOM_USAGE_REALTIME if the test behavior or bug reproduction is mode-independent
so the test runs across all build configurations. When tests or examples require
non-realtime modes (AOM_USAGE_GOOD_QUALITY or AOM_USAGE_ALL_INTRA), guard them
with #if !CONFIG_REALTIME_ONLY in C/C++ or exclude them via realtime_only_build
in test scripts (such as test/examples.sh). Conversely, keep tests and examples
exercising realtime mode outside !CONFIG_REALTIME_ONLY guards so they execute in
CONFIG_REALTIME_ONLY builds.

**Why.** In CONFIG_REALTIME_ONLY builds, non-realtime modes
(AOM_USAGE_GOOD_QUALITY and AOM_USAGE_ALL_INTRA) are disabled and
aom_codec_enc_config_default() fails if invoked with them. Preferring realtime
mode or guarding non-realtime tests prevents build and CI failures under
CONFIG_REALTIME_ONLY, while keeping realtime tests unguarded ensures realtime
encoder features are properly tested.

## Examples

Bad:

```cpp
// Mode-independent test fails under CONFIG_REALTIME_ONLY
TEST(EncodeAPI, IssueOrBuganizer) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);
}

// Realtime test mistakenly guarded
#if !CONFIG_REALTIME_ONLY
TEST(EncodeAPI, RealtimeFeatureTest) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
}
#endif
```

Good:

```cpp
// Use realtime mode for mode-independent tests
TEST(EncodeAPI, IssueOrBuganizer) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
            AOM_CODEC_OK);
}

// Guard tests that specifically require non-realtime modes
#if !CONFIG_REALTIME_ONLY
TEST(EncodeAPI, GoodQualitySpecificTest) {
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_GOOD_QUALITY),
            AOM_CODEC_OK);
}
#endif  // !CONFIG_REALTIME_ONLY
```
