---
name: libaom/av1/centralize-speed-tuning-in-speed-features
description: When introducing speed-versus-quality heuristics or mode-dependent tradeoffs, define flags in speed_features.{c,h} instead of hardcoding quality/speed thresholds in algorithm files
rubric: design
severity: should_fix
---

**Rule.** Centralize speed-tuning heuristics, decoder-speed optimizations, and
mode- or quality-dependent tradeoffs as speed feature flags in
av1/encoder/speed_features.{c,h}. Do not hardcode mode, speed preset, or quality
thresholds (such as cq_level, base_qindex, or oxcf.mode) directly in algorithm
implementation files; read the configured flag from cpi->sf instead.

**Why.** Centralizing all speed-versus-quality tradeoffs and preset
configurations in speed_features.c and speed_features.h ensures that speed
heuristics across encoder modes can be inspected, tuned, and benchmarked in one
place rather than scattered across individual algorithm files.

## Examples

Bad:

```cpp
// In av1/encoder/pickcdef.c:
// BAD: Hardcoding mode and quality threshold checks directly in algorithm code.
const bool should_zero_cdef_strengths =
    should_reduce_cdef_strengths && cpi->oxcf.rc_cfg.cq_level <= 140;
```

Good:

```cpp
// In av1/encoder/speed_features.c:
// GOOD: Centralize the heuristic under the appropriate speed feature category.
if (cpi->oxcf.mode == ALLINTRA) {
  if (cm->quant_params.base_qindex <= 140) {
    sf->lpf_sf.zero_low_cdef_strengths = 1;
  }
}

// In av1/encoder/pickcdef.c:
const bool should_zero_cdef_strengths =
    should_reduce_cdef_strengths && cpi->sf.lpf_sf.zero_low_cdef_strengths;
```
