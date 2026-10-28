---
name: libaom/av1/use-standard-kf-prefix-for-key-frames
description: When naming key-frame-related functions, variables, macros, or CDF distributions, use the standard kf prefix instead of ambiguous abbreviations like if
rubric: readability
severity: nit
---

**Rule.** Use the standard `kf` (or `KF_`) prefix for key-frame-related
functions, variables, struct fields, mode CDF distributions, and constants.
Avoid ambiguous or non-standard prefixes like `if` (which visually conflicts
with the C `if` keyword and ambiguously suggests 'inter-frame' or
'intra-frame').

**Why.** In libaom, `kf` is the established, ubiquitous abbreviation for key
frame contexts, probabilities, and configurations (e.g., `kf_y_mode`,
`default_kf_y_mode_cdf`, `KF_MODE_CONTEXTS`, `kf_cfg`). Using `if` causes visual
confusion with the C language `if` conditional keyword and creates ambiguity
between intra and inter frames.

## Examples

Bad:

```cpp
// Ambiguous 'if' prefix conflicts visually with C 'if' keyword.
void av1_get_if_y_mode_cdf_ml(const MACROBLOCKD *xd, aom_cdf_prob *cdf);
int if_dist = enc_cfg->if_max_dist;
```

Good:

```cpp
// Standard 'kf' prefix clearly denotes key-frame context.
void av1_get_kf_y_mode_cdf_ml(const MACROBLOCKD *xd, aom_cdf_prob *cdf);
int kf_dist = enc_cfg->kf_max_dist;
```
