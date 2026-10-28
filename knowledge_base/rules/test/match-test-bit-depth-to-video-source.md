---
name: libaom/test/match-test-bit-depth-to-video-source
description: When writing encoder unit tests, do not configure high bit depth (g_bit_depth > 8) unless the video source provides matching high-bit-depth frames or upshifted inputs
rubric: correctness
severity: should_fix
---

**Rule.** When writing or parameterizing encoder unit tests, do not configure a
higher codec bit depth (cfg_.g_bit_depth > 8 or cfg_.g_input_bit_depth > 8)
unless the test's video source supplies matching high-bit-depth frames or
explicitly upshifts 8-bit input frames. If the test uses an 8-bit video source
(such as standard YUVVideoSource with AOM_IMG_FMT_I420), restrict the test
configuration to bit depth 8 (AOM_BITS_8).

**Why.** The libaom encoder validates that input frame formats and sample
representations match the configured codec bit depth. Configuring g_bit_depth >
8 or setting AOM_CODEC_USE_HIGHBITDEPTH while passing 8-bit video frames without
upshifting violates input validation, leading to test failures, invalid memory
reads, or sanitizer warnings.

## Examples

Bad:

```cpp
void SetUp() override {
  InitializeConfig(::libaom_test::kAllIntra);
  cfg_.g_input_bit_depth = bd_;
  cfg_.g_bit_depth = static_cast<aom_bit_depth_t>(bd_);
  if (cfg_.g_bit_depth > 8) init_flags_ |= AOM_CODEC_USE_HIGHBITDEPTH;
}
// DoTest() supplies an 8-bit YUVVideoSource (AOM_IMG_FMT_I420) without upshifting.
```

Good:

```cpp
void SetUp() override {
  InitializeConfig(::libaom_test::kAllIntra);
  // Default to 8-bit depth when testing with an 8-bit video source.
}

// Or, when parameterizing high bit-depth, supply matching high-bit-depth sources:
void SetUp() override {
  InitializeConfig(::libaom_test::kAllIntra);
  cfg_.g_input_bit_depth = test_video_param_.input_bit_depth;
  cfg_.g_bit_depth = test_video_param_.bit_depth;
  if (cfg_.g_bit_depth > 8) init_flags_ |= AOM_CODEC_USE_HIGHBITDEPTH;
}
```
