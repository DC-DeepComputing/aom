---
name: libaom/extract-shared-helpers-for-duplicate-option-handling
description: When configuring encoder options across codec controls and string parsers, centralize validation in validate_config() and extract shared helpers for dependent parameters
rubric: correctness
severity: should_fix
---

**Rule.** When implementing encoder configuration option setters and constraint
checks, centralize parameter validation and compatibility checks in
validate_config() (invoked via update_extra_cfg()) rather than placing checks
solely in individual ctrl_set_* control helpers, ensuring all configuration
entry points (aom_codec_control, aom_codec_set_option, and initialization) are
uniformly validated. Additionally, extract shared helper functions (e.g.,
handle_*) for dependent parameter updates and non-trivial option parsing logic
shared between ctrl_set_* handlers and string option setters
(encoder_set_option) instead of duplicating logic.

**Why.** Encoder configuration parameters can be configured either
programmatically via aom_codec_control() or via string options in
aom_codec_set_option(). Placing constraint checks inside ctrl_set_* functions
allows configuration paths passing through aom_codec_set_option() or direct
config setters to bypass validation. Placing checks in validate_config()
guarantees consistent validation and descriptive ERROR() messaging across all
entry points, while extracting shared helpers for dependent parameter
adjustments prevents logic duplication and behavioral divergence.

## Examples

Bad:

```cpp
// Anti-pattern 1: Placing validation checks in ctrl_set_* instead of validate_config()
static aom_codec_err_t ctrl_set_deltaq_mode(aom_codec_alg_priv_t *ctx,
                                            va_list args) {
  struct av1_extracfg extra_cfg = ctx->extra_cfg;
  const DELTAQ_MODE deltaq_arg = CAST(AV1E_SET_DELTAQ_MODE, args);
  if (deltaq_arg == DELTA_Q_VARIANCE_BOOST && ctx->cfg.g_usage != ALLINTRA) {
    return AOM_CODEC_INCAPABLE;
  }
  extra_cfg.deltaq_mode = deltaq_arg;
  return update_extra_cfg(ctx, &extra_cfg);
}

// In encoder_set_option():
} else if (arg_match_helper(&arg, &g_av1_codec_arg_defs.deltaq_mode, argv, err_string)) {
  extra_cfg.deltaq_mode = arg_parse_uint_helper(&arg, err_string);
  // Missing validation check when configured via aom_codec_set_option!
}

// Anti-pattern 2: Duplicating dependent parameter logic in ctrl_set_* and encoder_set_option
// In ctrl_set_tuning:
if (extra_cfg.tuning == AOM_TUNE_SSIMULACRA2) extra_cfg.enable_qm = 1;
// In encoder_set_option:
if (extra_cfg.tuning == AOM_TUNE_SSIMULACRA2) extra_cfg.enable_qm = 1;
```

Good:

```cpp
// Pattern 1: Centralized validation in validate_config() invoked via update_extra_cfg()
static aom_codec_err_t validate_config(aom_codec_alg_priv_t *ctx,
                                       const aom_codec_enc_cfg_t *cfg,
                                       const struct av1_extracfg *extra_cfg) {
  if (cfg->g_usage != ALLINTRA &&
      extra_cfg->deltaq_mode == DELTA_Q_VARIANCE_BOOST) {
    ERROR("Variance Boost (deltaq_mode = 6) can only be set in all intra mode");
  }
  return AOM_CODEC_OK;
}

// Pattern 2: Shared helper for dependent parameter updates across entry points
static aom_codec_err_t handle_tuning(aom_codec_alg_priv_t *ctx,
                                     struct av1_extracfg *extra_cfg) {
  if (extra_cfg->tuning == AOM_TUNE_SSIMULACRA2) extra_cfg->enable_qm = 1;
  return AOM_CODEC_OK;
}
// Used in both ctrl_set_tuning and encoder_set_option before update_extra_cfg()
```
