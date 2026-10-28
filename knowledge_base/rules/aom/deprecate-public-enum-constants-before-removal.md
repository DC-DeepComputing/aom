---
name: libaom/aom/deprecate-public-enum-constants-before-removal
description: When dropping internal support for public API enum constants, retain them in public headers with a \deprecated notice rather than deleting them
rubric: design
severity: nit
---

**Rule.** When dropping internal support for enum constants or macros in public
API headers (aom/*.h), do not immediately delete the identifiers. Retain their
definitions in public headers and document them with a Doxygen \deprecated
comment stating that the value is unsupported and will be removed in a future
release.

**Why.** Deleting public API enum constants immediately breaks compilation for
external applications and library consumers. Retaining them with a \deprecated
notice preserves backwards source compatibility while signaling the deprecation
grace period.

## Examples

Bad:

```cpp
// Direct removal of unsupported enum constants from public headers:
typedef enum aom_img_fmt {
  AOM_IMG_FMT_NONE,
  AOM_IMG_FMT_YV12 = AOM_IMG_FMT_PLANAR | AOM_IMG_FMT_UV_FLIP | 1,
  AOM_IMG_FMT_I420 = AOM_IMG_FMT_PLANAR | 2,
  // AOM_IMG_FMT_AOMYV12 and AOM_IMG_FMT_AOMI420 deleted
  AOM_IMG_FMT_I422 = AOM_IMG_FMT_PLANAR | 5,
} aom_img_fmt_t;
```

Good:

```cpp
typedef enum aom_img_fmt {
  AOM_IMG_FMT_NONE,
  AOM_IMG_FMT_YV12 = AOM_IMG_FMT_PLANAR | AOM_IMG_FMT_UV_FLIP | 1,
  AOM_IMG_FMT_I420 = AOM_IMG_FMT_PLANAR | 2,
  /** Planar 4:2:0 format with aom color space
   *
   * \deprecated This value is unsupported and will be removed in a future
   * release.
   */
  AOM_IMG_FMT_AOMYV12 = AOM_IMG_FMT_PLANAR | AOM_IMG_FMT_UV_FLIP | 3,
  /** Planar 4:2:0 format with aom color space
   *
   * \deprecated This value is unsupported and will be removed in a future
   * release.
   */
  AOM_IMG_FMT_AOMI420 = AOM_IMG_FMT_PLANAR | 4,
  AOM_IMG_FMT_I422 = AOM_IMG_FMT_PLANAR | 5,
} aom_img_fmt_t;
```
