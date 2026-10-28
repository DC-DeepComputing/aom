---
name: libaom/av1/check-encoder-config-for-monochrome-not-input-image
description: When validating input images or checking chroma plane usage in the encoder, inspect encoder configuration (cfg.monochrome) rather than img->monochrome
rubric: correctness
severity: should_fix
---

**Rule.** When validating input images or determining whether chroma planes are
active during encoding, check the encoder configuration (`ctx->cfg.monochrome`
or `oxcf->tool_cfg.enable_monochrome`) rather than the input image container
(`img->monochrome`). Do not rely on `img->monochrome` to enforce or bypass
chroma plane constraints.

**Why.** The encoder configuration dictates whether chroma planes will be
encoded or ignored. Applications can pass color images (with `img->monochrome ==
0`) to a monochrome encoder, which discards chroma data, or may omit setting
`img->monochrome`. Relying on `img->monochrome` incorrectly enforces chroma
plane invariants during monochrome encoding or bypasses them during color
encoding.

## Examples

Bad:

```cpp
static aom_codec_err_t validate_img(aom_codec_alg_priv_t *ctx,
                                    const aom_image_t *img) {
  if (!img->monochrome && img->fmt != AOM_IMG_FMT_NV12 &&
      img->stride[AOM_PLANE_U] != img->stride[AOM_PLANE_V]) {
    ERROR("Image U/V strides must match");
  }
  const int num_planes = img->monochrome ? 1 : 3;
  // ...
}
```

Good:

```cpp
static aom_codec_err_t validate_img(aom_codec_alg_priv_t *ctx,
                                    const aom_image_t *img) {
  if (!ctx->cfg.monochrome && img->fmt != AOM_IMG_FMT_NV12 &&
      img->stride[AOM_PLANE_U] != img->stride[AOM_PLANE_V]) {
    ERROR("Image U/V strides must match");
  }
  const int num_planes = ctx->cfg.monochrome ? 1 : 3;
  // ...
}
```
