---
name: libaom/av1/encapsulate-format-constraints-in-setup-helpers
description: When configuring format- or profile-dependent feature parameters, encapsulate sequence-level constraints inside setup helpers rather than sanitizing initialized structs at call sites
rubric: design
severity: should_fix
---

**Rule.** Encapsulate format-, profile-, or sequence-dependent constraints (such
as omitting chroma parameters for monochrome streams or subsampling-specific
settings) directly inside parameter setup helpers. Do not rely on callers to
post-process, reset, or sanitize configuration structs after invoking setup
routines.

**Why.** Centralizing format constraints within parameter setup helpers ensures
structs are always completely and validly populated on return, eliminates
duplicated or easily forgotten caller-side fixups, and prevents inconsistent
state when setup helpers are reused across different encoder paths.

## Examples

Bad:

```cpp
// Caller must remember to clean up format-incompatible fields:
set_grain_syn_params(cm);
if (seq_params->monochrome) {
  av1_reset_film_grain_chroma_params(&cm->film_grain_params);
}
```

Good:

```cpp
// Setup helper internally enforces sequence format constraints:
static void set_grain_syn_params(AV1_COMMON *cm) {
  // ...
  if (!cm->seq_params->monochrome) {
    film_grain_params->num_cb_points = 1;
    film_grain_params->scaling_points_cb[0][0] = 128;
    film_grain_params->scaling_points_cb[0][1] = 100;
    film_grain_params->num_cr_points = 1;
    film_grain_params->scaling_points_cr[0][0] = 128;
    film_grain_params->scaling_points_cr[0][1] = 100;
  }
}
```
