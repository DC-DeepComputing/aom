---
name: libaom/av1/canonical-encoder-config-defaults
description: When initializing encoder configuration parameters, define defaults in a single canonical scope rather than duplicating assignments across usage presets and tuning handlers
rubric: design
severity: should_fix
---

**Rule.** Define encoder configuration parameter defaults in a single canonical
location matching the feature's scope (such as usage-mode initialization in
encoder_init()), rather than duplicating default assignments across both
initialization presets and secondary option or tuning handlers (such as
handle_tuning()). Keep secondary option handlers focused only on enabling or
overriding feature-specific flags.

**Why.** Duplicating default assignments across multiple configuration functions
creates code duplication, increases maintenance drift, and risks unintentionally
overwriting user-specified configuration values when secondary option handlers
execute.

## Examples

Bad:

```cpp
// In handle_tuning() for AOM_TUNE_SSIMULACRA2:
extra_cfg->enable_qm = 1;
extra_cfg->qm_min = DEFAULT_QM_FIRST_ALLINTRA;
extra_cfg->qm_max = DEFAULT_QM_LAST_ALLINTRA;

// Duplicate setting in encoder_init():
if (priv->cfg.g_usage == AOM_USAGE_ALL_INTRA) {
  priv->extra_cfg.qm_min = DEFAULT_QM_FIRST_ALLINTRA;
  priv->extra_cfg.qm_max = DEFAULT_QM_LAST_ALLINTRA;
}
```

Good:

```cpp
// In handle_tuning() for AOM_TUNE_SSIMULACRA2:
extra_cfg->enable_qm = 1;

// Canonical default assignment in encoder_init():
if (priv->cfg.g_usage == AOM_USAGE_ALL_INTRA) {
  priv->extra_cfg.qm_min = DEFAULT_QM_FIRST_ALLINTRA;
  priv->extra_cfg.qm_max = DEFAULT_QM_LAST_ALLINTRA;
}
```
