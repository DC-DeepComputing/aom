---
name: libaom/av1/avoid-redundant-mode-checks-for-unsupported-features
description: When validating unsupported encoder configurations or feature combinations, check the incompatible parameters directly across all modes rather than narrowing guards to specific quality modes
rubric: correctness
severity: nit
---

**Rule.** When validating encoder parameter compatibility or guarding against
unsupported feature combinations (such as Scalable Video Coding with lookahead
frame lagging), validate against the incompatible configuration directly. Do not
unnecessarily qualify validation checks with specific encoder quality modes
(e.g., oxcf.mode == GOOD) or usage modes when the underlying feature limitation
is fundamentally unsupported across all modes.

**Why.** Qualifying parameter compatibility checks with specific encoder modes
(often overfitted from bug report reproduction steps) allows the same invalid or
crashing configuration to bypass validation when running under other quality or
usage modes (such as REALTIME or ALL_INTRA).

## Examples

Bad:

```cpp
// Invalidate SVC with frame lagging only for good quality mode:
if (cpi->oxcf.gf_cfg.lag_in_frames > 0 && cpi->oxcf.mode == GOOD) {
  return AOM_CODEC_INVALID_PARAM;
}
```

Good:

```cpp
// Invalidate SVC with frame lagging universally across all encoder modes:
if (cpi->oxcf.gf_cfg.lag_in_frames > 0) {
  return AOM_CODEC_INVALID_PARAM;
}
```
