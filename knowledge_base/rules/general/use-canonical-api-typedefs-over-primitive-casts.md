---
name: libaom/use-canonical-api-typedefs-over-primitive-casts
description: When declaring variables or function parameters passed to libaom APIs, use canonical API typedefs (such as aom_codec_pts_t) rather than primitive types with casts
rubric: readability
severity: should_fix
---

**Rule.** Declare variables and function parameters passed to libaom API
functions using the canonical API typedefs (such as aom_codec_pts_t,
aom_codec_frame_flags_t, or aom_codec_flags_t) rather than primitive integer
types (such as int or long). Avoid declaring primitive types that require
explicit casts at API call sites.

**Why.** Using canonical API typedefs directly in function signatures and
variable declarations ensures correct bit-width and signedness across platforms
(for example, aom_codec_pts_t is 64-bit while int is 32-bit). It clarifies
parameter semantics and avoids unnecessary type casting at API call sites.

## Examples

Bad:

```cpp
static void EncodeOne(aom_codec_ctx_t *codec, aom_image_t *img, int pts) {
  ASSERT_EQ(aom_codec_encode(codec, img, (aom_codec_pts_t)pts, 1, 0),
            AOM_CODEC_OK);
}
```

Good:

```cpp
static void EncodeOne(aom_codec_ctx_t *codec, aom_image_t *img,
                      aom_codec_pts_t pts) {
  ASSERT_EQ(aom_codec_encode(codec, img, pts, 1, 0), AOM_CODEC_OK);
}
```
