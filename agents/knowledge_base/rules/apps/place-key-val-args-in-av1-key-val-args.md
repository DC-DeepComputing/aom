---
name: libaom/apps/place-key-val-args-in-av1-key-val-args
description: When adding or updating CLI options in aomenc, place key-value options in av1_key_val_args and migrate related legacy av1_ctrl_args options
rubric: readability
severity: blocking
---

**Rule.** In apps/aomenc.c, place CLI options configured via key-value strings
(aom_codec_set_option) in av1_key_val_args[] rather than av1_ctrl_args[],
keeping av1_ctrl_args[] strictly synchronized 1:1 with av1_arg_ctrl_map[]. When
adding new encoder CLI options or companion controls, prefer registering them as
key-value options in av1_key_val_args[] and migrate related legacy options from
av1_ctrl_args[] / av1_arg_ctrl_map[] into av1_key_val_args[].

**Why.** av1_ctrl_args[] and av1_arg_ctrl_map[] are parallel arrays that must
maintain exact 1:1 positional alignment to dispatch codec control IDs; adding
key-value options to av1_ctrl_args[] breaks array synchronization. Additionally,
the modern key-value option API (aom_codec_set_option) is preferred over
extending legacy control ID mappings, and co-locating related options in
av1_key_val_args[] keeps option definitions unified.

## Examples

Bad:

```cpp
// Incorrect: key-value option placed in av1_ctrl_args without control ID mapping
static const arg_def_t *const av1_ctrl_args[] = {
  &g_av1_codec_arg_defs.cpu_used,
  &g_av1_codec_arg_defs.dist_metric,
  NULL,
};

// Also bad: keeping related options split across legacy ctrl args and key-val args
static const int av1_arg_ctrl_map[] = {
  ...
  AOME_SET_SHARPNESS,
};
static const arg_def_t *const av1_ctrl_args[] = {
  ...
  &g_av1_codec_arg_defs.sharpness,
};
static const arg_def_t *const av1_key_val_args[] = {
  ...
  &g_av1_codec_arg_defs.enable_adaptive_sharpness,
  NULL,
};
```

Good:

```cpp
// Correct: place key-value option in av1_key_val_args
static const arg_def_t *const av1_key_val_args[] = {
  &g_av1_codec_arg_defs.dist_metric,
  NULL,
};

static const arg_def_t *const av1_ctrl_args[] = {
  &g_av1_codec_arg_defs.cpu_used,
  NULL,
};

// Correct: migrate legacy option to av1_key_val_args alongside related new options
static const arg_def_t *const av1_key_val_args[] = {
  ...
  &g_av1_codec_arg_defs.sharpness,
  &g_av1_codec_arg_defs.enable_adaptive_sharpness,
  NULL,
};
```
