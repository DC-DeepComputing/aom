---
name: libaom/test/initialize-allocated-test-image-buffers
description: When allocating test image buffers with aom_img_alloc(), explicitly initialize plane memory with FillImage(), FillImageRandom(), or VideoSource before encoding or passing to codec APIs
rubric: safety
severity: should_fix
---

**Rule.** In unit and regression tests, explicitly initialize allocated
`aom_image_t` image buffers and planes (e.g., using `FillImage()`,
`FillImageRandom()`, `FillImageGradient()`, or test sources like
`RandomVideoSource`) after calling `aom_img_alloc()` or
`aom_img_alloc_with_border()` before passing them to `aom_codec_encode()` or
other codec APIs. Do not pass uninitialized image buffers directly to codec
functions.

**Why.** `aom_img_alloc()` allocates plane memory without zeroing. Passing
uninitialized image memory to `aom_codec_encode()` or other codec functions
causes encoder search, transform, and filtering routines to read uninitialized
memory, triggering Valgrind and MemorySanitizer (MSan) check failures during
automated CI runs. Explicitly initializing test frames ensures deterministic
test execution and prevents false-positive sanitizer failures.

## Examples

Bad:

```cpp
aom_image_t *raw = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 640, 360, 1);
ASSERT_NE(raw, nullptr);

for (const auto &f : frames) {
  ASSERT_EQ(aom_codec_encode(&enc, raw, f.pts, f.duration, f.flags),
            AOM_CODEC_OK);
}
```

Good:

```cpp
aom_image_t *raw = aom_img_alloc(nullptr, AOM_IMG_FMT_I420, 640, 360, 1);
ASSERT_NE(raw, nullptr);
FillImageRandom(raw);  // or FillImage(raw, 128)

for (const auto &f : frames) {
  ASSERT_EQ(aom_codec_encode(&enc, raw, f.pts, f.duration, f.flags),
            AOM_CODEC_OK);
}
```
