---
name: libaom/av1/default-encoder-quantizer-bounds-at-extremes
description: When defining or updating default encoder configuration presets (aom_codec_enc_cfg_t), keep rc_min_quantizer and rc_max_quantizer at extreme bounds (0 and 63) rather than clamped defaults.
rubric: readability
severity: should_fix
---

**Rule.** In default encoder configuration presets (aom_codec_enc_cfg_t),
initialize rc_min_quantizer to 0 and rc_max_quantizer to 63. Do not set clamped
or application-specific quantizer limits in baseline encoder usage profiles.

**Why.** Default encoder usage presets provide baseline unconstrained
configurations for varied client applications. Imposing clamped default
quantizer limits restricts rate control policies and prevents clients from
utilizing the full quantizer spectrum.

## Examples

Bad:

```cpp
static const aom_codec_enc_cfg_t default_cfg = {
    // ...
    2,   // rc_min_quantizer
    56,  // rc_max_quantizer
    // ...
};
```

Good:

```cpp
static const aom_codec_enc_cfg_t default_cfg = {
    // ...
    0,   // rc_min_quantizer
    63,  // rc_max_quantizer
    // ...
};
```
