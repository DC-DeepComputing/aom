---
name: libaom/use-descriptive-threshold-and-limit-names
description: When declaring or updating thresholds, limits, and heuristic flags, use descriptive names specifying metrics, scale qualifiers (norm_), explicit bounds/inclusive ranges (_AND_ABOVE), and controlled actions
rubric: readability
severity: should_fix
---

**Rule.** When declaring or modifying variables, struct members, constants,
macros, enum values, or speed feature flags for thresholds, range bounds, or
heuristics, use descriptive names that specify the evaluated metric, scale
qualifiers, numerical bounds or inclusive ranges, controlled stages or actions,
and all governed frame types:
1. Scale and metric qualifiers: Include matching metric names and scale prefixes (such as norm_, per_pixel_, avg_, or log-domain prefixes, e.g., norm_uv_sad_thresh) to distinguish normalized quantities from raw measurements (SAD, SSE, variance).
2. Numerical bounds and inclusive ranges: Use explicit limit suffixes such as _low/_high or _upper_limit/_lower_limit (e.g., block_sad_low, shift_upper_limit) instead of generic numbered names (thresh1, thresh2) or ambiguous directional suffixes (_top/_bottom). For inclusive threshold ranges, use explicit phrasing like _AND_ABOVE/_OR_ABOVE or _AND_BELOW/_OR_BELOW rather than ambiguous suffixes like _ABOVE or _BELOW to clarify whether boundary values are included.
3. Controlled actions and frame types: Specify the exact action controlled (e.g., thresh_active_maps_skip_lf_cdef) and update identifier suffixes to reflect all governed contexts when expanding feature scope across frame types (e.g., rc_compute_spatial_var_sc_kf).

**Why.** In heuristic and mode decision logic where raw metrics (such as
unscaled SAD, SSE, or variance) and normalized metrics (scaled by block area,
pixel count, or bit depth) coexist, unqualified threshold names mislead readers
about cutoff magnitudes and risk scale-mismatch comparison bugs. Similarly,
generic numbered names (thresh1), ambiguous directional suffixes (_top/_bottom),
or ambiguous threshold range suffixes (_ABOVE/_BELOW vs. _AND_ABOVE/_AND_BELOW)
obscure what metric is evaluated, whether boundary values are included (> vs.
>=), or which encoding stage and frame types are controlled.

## Examples

Bad:

```cpp
// Ambiguous threshold names lack scale qualifiers (norm_) despite comparing against normalized metrics
int uv_sad_thr = 100;
if (source_variance < source_var_thr && norm_uv_sad > uv_sad_thr) {
  // ...
}

// Ambiguous numbered thresholds, directional suffixes, and inclusive boundary naming
uint64_t thresh1 = 25000;
uint64_t thresh2 = 50000;
int shift_top = 1;
#define MAX_TILE_AREA_7_ABOVE (4096 * 4608)  // Ambiguous whether level 7 is included (> 7 vs. >= 7)

// Threshold omitting controlled action or stale frame-type scope
int thresh_active_maps;
int rc_compute_spatial_var_sc;  // heuristic was expanded to also cover keyframes
```

Good:

```cpp
// Threshold names explicitly reflect the normalized metric scale
int norm_uv_sad_thresh = 100;
if (source_variance < source_var_thr && norm_uv_sad > norm_uv_sad_thresh) {
  // ...
}

// Descriptive metric names, explicit limit suffixes, and clear inclusive boundary names
uint64_t block_sad_low = 25000;
uint64_t block_sad_high = 50000;
int shift_upper_limit = 1;
#define MAX_TILE_AREA_7_AND_ABOVE (4096 * 4608)  // Explicitly indicates level 7 and higher (>= 7)

// Threshold specifying controlled action and updated frame-type scope
int thresh_active_maps_skip_lf_cdef;
int rc_compute_spatial_var_sc_kf;
```
