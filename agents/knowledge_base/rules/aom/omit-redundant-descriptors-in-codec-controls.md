---
name: libaom/aom/omit-redundant-descriptors-in-codec-controls
description: When naming public codec controls, configuration enums, or dispatch handlers, name them concisely after the underlying feature or detected characteristic, omitting redundant descriptors like _TOOLS
rubric: readability
severity: nit
---

**Rule.** When defining or adding public codec control identifiers (such as
`AV1E_SET_*`, `AOME_SET_*`, `AV1D_GET_*`, or `AOMD_GET_*`), configuration enums,
`AOM_CTRL_USE_TYPE` macros, and interface dispatch handlers (such as
`ctrl_set_*` / `ctrl_get_*`), name them concisely after the underlying feature
or content characteristic being configured or detected (e.g.,
`AV1E_SET_SCREEN_CONTENT_DETECTION_MODE` and `aom_screen_detection_mode`). Avoid
redundant descriptor words (such as `_TOOLS`, `_FEATURE`, `_PARAM`, or
`_OPTION`) when preceding terms already identify the capability, and avoid
naming detection controls after downstream coding tools (e.g., avoid
`_TOOLS_DETECTION_`) because detection classifies source content characteristics
rather than tools directly.

**Why.** Public codec controls, configuration enums, and dispatch handlers form
libaom's long-term external API and internal dispatch architecture. Omitting
redundant descriptor words keeps control names concise, clear, and consistent
across the codebase. Furthermore, detection algorithms classify input content
characteristics rather than coding tools themselves; coding tools (such as
IntraBC or palette mode) are selectively enabled downstream based on that
classification, making `_TOOLS` in detection identifiers inaccurate and
redundant.

## Examples

Bad:

```cpp
// Bad: Redundant descriptor '_TOOLS' in control identifier and dispatch handler
AV1E_SET_SCREEN_CONTENT_TOOLS_DETECTION_MODE = 171,
AOM_CTRL_USE_TYPE(AV1E_SET_SCREEN_CONTENT_TOOLS_DETECTION_MODE, unsigned int)
#define AOM_CTRL_AV1E_SET_SCREEN_CONTENT_TOOLS_DETECTION_MODE

static aom_codec_err_t ctrl_set_screen_content_tools_detection_mode(
    aom_codec_alg_priv_t *ctx, va_list args) {
  struct av1_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.screen_content_tools_detection_mode =
      CAST(AV1E_SET_SCREEN_CONTENT_TOOLS_DETECTION_MODE, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

// Bad: Referencing downstream coding tools in detection enum definition
typedef enum {
  AOM_SCREEN_TOOLS_DETECTION_OFF = 0,
  AOM_SCREEN_TOOLS_DETECTION_STANDARD = 1,
} aom_screen_tools_detection_mode;
```

Good:

```cpp
// Good: Concise naming focused on core feature and detected content characteristic
AV1E_SET_SCREEN_CONTENT_DETECTION_MODE = 171,
AOM_CTRL_USE_TYPE(AV1E_SET_SCREEN_CONTENT_DETECTION_MODE, unsigned int)
#define AOM_CTRL_AV1E_SET_SCREEN_CONTENT_DETECTION_MODE

static aom_codec_err_t ctrl_set_screen_content_detection_mode(
    aom_codec_alg_priv_t *ctx, va_list args) {
  struct av1_extracfg extra_cfg = ctx->extra_cfg;
  extra_cfg.screen_content_detection_mode =
      CAST(AV1E_SET_SCREEN_CONTENT_DETECTION_MODE, args);
  return update_extra_cfg(ctx, &extra_cfg);
}

// Good: Detection enum named after content characteristic rather than tools
typedef enum {
  AOM_SCREEN_DETECTION_OFF = 0,
  AOM_SCREEN_DETECTION_STANDARD = 1,
} aom_screen_detection_mode;
```
