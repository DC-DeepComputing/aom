---
name: libaom/av1/gate-speed-feature-threshold-on-specific-level
description: When introducing or tuning heuristic thresholds for encoder speed features, explicitly gate conditions on the specific speed feature level being targeted
rubric: correctness
severity: should_fix
---

**Rule.** When introducing or modifying heuristic threshold adjustments for an
encoder speed feature, explicitly gate the condition on the specific speed
feature level being targeted (e.g., cpi->sf.rt_sf.<feature> == <level>). Do not
apply threshold shifts globally without checking the feature level unless the
heuristic is explicitly intended and benchmarked across all presets.

**Why.** Speed features configure heuristic shortcuts and thresholds at
graduated aggressiveness levels across presets. Modifying a shared threshold or
condition without gating on the targeted feature level can silently alter
rate-distortion performance, visual quality, or encoding speed in other untested
speed presets.

## Examples

Bad:

```cpp
// Modifies UV partition threshold for low SAD blocks across all speed feature levels
if (x->content_state_sb.source_sad_nonrd == kVeryLowSad)
  thresh_exit_part_uv = thresh_exit_part_uv >> 3;
```

Good:

```cpp
// Explicitly gates threshold adjustment to part_early_exit_zeromv == 1
if (x->content_state_sb.source_sad_nonrd >= kVeryLowSad &&
    cpi->sf.rt_sf.part_early_exit_zeromv == 1)
  thresh_exit_part_uv = thresh_exit_part_uv >> 3;
```
