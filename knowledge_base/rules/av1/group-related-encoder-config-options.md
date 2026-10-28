---
name: libaom/av1/group-related-encoder-config-options
description: When adding CLI arguments, config options, or codec controls related to existing settings, group their struct fields, defaults, validations, and mappings adjacent to those settings
rubric: readability
severity: nit
---

**Rule.** When adding a CLI argument definition (`arg_def_t`, `ARG_DEF`,
`ARG_DEF_ENUM`), configuration struct field (`av1_extracfg`,
`av1_codec_arg_definitions_t`, `aom_codec_enc_cfg_t`), or codec control
(`AV1E_SET_*`) that modifies, enables, or is closely related to an existing
setting (such as an adaptive variant, sub-mode, or threshold like
`enable_adaptive_sharpness` for `sharpness`), declare and group it directly
adjacent to the related existing setting. Maintain this adjacent grouping
consistently across struct definitions, default initializers (e.g.,
`default_extra_cfg`, `g_av1_codec_arg_defs`), `RANGE_CHECK` validation macros,
setter functions (`ctrl_set_*`), CLI option parsers, and control dispatch maps
(`encoder_ctrl_maps`) rather than appending new entries at the end.

**Why.** Placing related configuration options, CLI argument definitions,
defaults, validations, and control dispatch mappings adjacent to their parent
feature keeps configuration interfaces logically organized, improves code
discoverability, and ensures related controls can be easily located, compared,
and maintained across header declarations, initializers, and dispatch tables.

## Examples

Bad:

```cpp
// In av1/arg_defs.c and config structs: appending related options at the end
.sharpness = ARG_DEF(NULL, "sharpness", 1, "Bias towards block sharpness..."),
.static_thresh = ARG_DEF(NULL, "static-thresh", 1, "A threshold on..."),
.enable_adaptive_sharpness =
    ARG_DEF(NULL, "enable-adaptive-sharpness", 1, "Enable adaptive sharpness...");

// In validate_config() and encoder_ctrl_maps[]:
RANGE_CHECK_HI(extra_cfg, sharpness, 7);
RANGE_CHECK(extra_cfg, screen_detection_mode, 1, 2);
RANGE_CHECK_HI(extra_cfg, enable_adaptive_sharpness, 1);

{ AV1E_SET_SHARPNESS, ctrl_set_sharpness },
{ AV1E_SET_SCREEN_CONTENT_MODE, ctrl_set_screen_content_mode },
{ AV1E_SET_ENABLE_ADAPTIVE_SHARPNESS, ctrl_set_enable_adaptive_sharpness },
```

Good:

```cpp
// Group related CLI arguments and config fields adjacent to base setting
.sharpness = ARG_DEF(NULL, "sharpness", 1, "Bias towards block sharpness..."),
.enable_adaptive_sharpness =
    ARG_DEF(NULL, "enable-adaptive-sharpness", 1, "Enable adaptive sharpness..."),
.static_thresh = ARG_DEF(NULL, "static-thresh", 1, "A threshold on...");

// Group validations and control maps adjacent to base setting
RANGE_CHECK_HI(extra_cfg, sharpness, 7);
RANGE_CHECK_HI(extra_cfg, enable_adaptive_sharpness, 1);
RANGE_CHECK(extra_cfg, screen_detection_mode, 1, 2);

{ AV1E_SET_SHARPNESS, ctrl_set_sharpness },
{ AV1E_SET_ENABLE_ADAPTIVE_SHARPNESS, ctrl_set_enable_adaptive_sharpness },
{ AV1E_SET_SCREEN_CONTENT_MODE, ctrl_set_screen_content_mode },
```
