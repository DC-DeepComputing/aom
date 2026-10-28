---
name: libaom/apps/group-unconditional-cli-args-before-conditional-blocks
description: 'When defining CLI argument arrays, group unconditional arguments together with related options before conditionally-compiled #if CONFIG_* feature blocks'
rubric: readability
severity: nit
---

**Rule.** In CLI argument definition arrays (such as av1_key_val_args or
av1_ctrl_args), group unconditional arguments together with related options
before conditionally compiled #if CONFIG_* blocks.

**Why.** Keeping unconditional arguments logically grouped with related options
before #if CONFIG_* blocks prevents standard options from being buried after
conditionally compiled experimental flags and keeps argument tables cleanly
organized.

## Examples

Bad:

```cpp
const arg_def_t *av1_key_val_args[] = {
  &g_av1_codec_arg_defs.disable_ml_transform_speed_features,
#if CONFIG_SDP
  &g_av1_codec_arg_defs.enable_sdp,
#endif
#if CONFIG_CCSO
  &g_av1_codec_arg_defs.enable_ccso,
#endif
  &g_av1_codec_arg_defs.disable_ml_partition_speed_features,
  NULL,
};
```

Good:

```cpp
const arg_def_t *av1_key_val_args[] = {
  &g_av1_codec_arg_defs.disable_ml_transform_speed_features,
  &g_av1_codec_arg_defs.disable_ml_partition_speed_features,
#if CONFIG_SDP
  &g_av1_codec_arg_defs.enable_sdp,
#endif
#if CONFIG_CCSO
  &g_av1_codec_arg_defs.enable_ccso,
#endif
  NULL,
};
```
