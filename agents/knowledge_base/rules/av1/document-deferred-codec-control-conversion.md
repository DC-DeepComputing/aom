---
name: libaom/av1/document-deferred-codec-control-conversion
description: When deferring unit conversion or state derivation in codec control setters, add a comment explaining where and why conversion is deferred
rubric: readability
severity: nit
---

**Rule.** When a codec control setter stores raw configuration values (such as
millisecond durations or threshold limits) and defers unit conversion or
internal state updates because the calculation depends on dynamic runtime
parameters (such as framerate or resolution), add an inline comment explaining
where and why the conversion is deferred.

**Why.** Codec control setters often update internal runtime state directly;
storing raw units without immediate conversion can confuse reviewers and
maintainers about data flow unless the dynamic dependency is explicitly
documented.

## Examples

Bad:

```cpp
static aom_codec_err_t ctrl_set_max_consec_frame_drop_ms_cbr(
    aom_codec_alg_priv_t *ctx, va_list args) {
  struct aom_codec_alg_priv *cpi = ctx;
  const int max_consec_drop_ms =
      CAST(AV1E_SET_MAX_CONSEC_FRAME_DROP_MS_CBR, args);
  if (max_consec_drop_ms < 0) return AOM_CODEC_INVALID_PARAM;
  ctx->oxcf.rc_cfg.max_consec_drop_ms = max_consec_drop_ms;
  cpi->rc.drop_count_consec = 0;
  return AOM_CODEC_OK;
}
```

Good:

```cpp
static aom_codec_err_t ctrl_set_max_consec_frame_drop_ms_cbr(
    aom_codec_alg_priv_t *ctx, va_list args) {
  struct aom_codec_alg_priv *cpi = ctx;
  const int max_consec_drop_ms =
      CAST(AV1E_SET_MAX_CONSEC_FRAME_DROP_MS_CBR, args);
  if (max_consec_drop_ms < 0) return AOM_CODEC_INVALID_PARAM;
  // max_consec_drop_ms will be converted to frame units inside encoder
  // based on framerate (which can change dynamically).
  ctx->oxcf.rc_cfg.max_consec_drop_ms = max_consec_drop_ms;
  cpi->rc.drop_count_consec = 0;
  return AOM_CODEC_OK;
}
```
