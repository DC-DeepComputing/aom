---
name: libaom/av1/omit-deprecated-image-formats-from-validation
description: When validating input image formats or writing format dispatch switches, omit deprecated and unsupported formats like AOM_IMG_FMT_AOMYV12 to ensure they are cleanly rejected
rubric: design
severity: should_fix
---

**Rule.** Do not add switch cases or handling logic for deprecated and
unsupported image formats (such as AOM_IMG_FMT_AOMYV12 and AOM_IMG_FMT_AOMI420)
in encoder image validation or conversion routines. Omit deprecated format enums
from valid format switch statements so that unsupported formats are cleanly
rejected with an error instead of being implicitly accepted.

**Why.** Deprecated format enum constants are retained in public headers solely
for backward compatibility and are not supported by the encoder. Adding switch
cases for them gives the false impression of support, bypasses rejection checks,
and complicates their eventual removal from the public API.

## Examples

Bad:

```cpp
static aom_codec_err_t validate_img(aom_codec_alg_priv_t *ctx,
                                    const aom_image_t *img) {
  switch (img->fmt) {
    case AOM_IMG_FMT_YV12:
    case AOM_IMG_FMT_NV12:
    case AOM_IMG_FMT_I420:
    case AOM_IMG_FMT_AOMYV12:
    case AOM_IMG_FMT_AOMI420:
    case AOM_IMG_FMT_YV1216:
    case AOM_IMG_FMT_I42016: break;
    default: return AOM_CODEC_INVALID_PARAM;
  }
  return AOM_CODEC_OK;
}
```

Good:

```cpp
static aom_codec_err_t validate_img(aom_codec_alg_priv_t *ctx,
                                    const aom_image_t *img) {
  switch (img->fmt) {
    case AOM_IMG_FMT_YV12:
    case AOM_IMG_FMT_NV12:
    case AOM_IMG_FMT_I420:
    case AOM_IMG_FMT_YV1216:
    case AOM_IMG_FMT_I42016: break;
    default: return AOM_CODEC_INVALID_PARAM;
  }
  return AOM_CODEC_OK;
}
```
