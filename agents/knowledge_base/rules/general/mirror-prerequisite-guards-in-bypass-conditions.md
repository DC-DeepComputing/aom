---
name: libaom/mirror-prerequisite-guards-in-bypass-conditions
description: When bypassing logic or returning early assuming a prior function updated state, ensure the bypass guard mirrors all execution preconditions of that prerequisite function
rubric: correctness
severity: should_fix
---

**Rule.** When adding or modifying early returns or bypass conditions that skip
state updates under the assumption that a prior function, pass, or branch
already updated or reset that state, ensure the bypass condition checks all
preconditions required for the prerequisite function to have actually executed.
Do not omit prerequisite guards or assume unconditional execution when the prior
update is gated by additional feature flags, mode settings, or metrics.

**Why.** If a bypass or early return condition omits any of the execution
preconditions of the assumed prerequisite function, the code will skip necessary
updates or resets even when the prerequisite function never ran. This leaves
data structures in stale, uninitialized, or unadjusted states, causing silent
logic errors and encoder rate control or quality regressions.

## Examples

Bad:

```cpp
// Bypassing counter reset assuming av1_encodedframe_overshoot_cbr() ran,
// but omitting cpi->rc.high_source_sad which gates that prerequisite function.
if ((cpi->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ) &&
    (cpi->sf.rt_sf.overshoot_detection_cbr == FAST_DETECTION_MAXQ) &&
    (cpi->cyclic_refresh->counter_encode_maxq_scene_change == 0) &&
    !frame_is_intra_only(cm) && !cpi->ppi->use_svc) {
  return;
}
```

Good:

```cpp
// Bypass guard explicitly mirrors all preconditions of the prerequisite function.
if ((cpi->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ) &&
    (cpi->sf.rt_sf.overshoot_detection_cbr == FAST_DETECTION_MAXQ) &&
    cpi->rc.high_source_sad &&
    (cpi->cyclic_refresh->counter_encode_maxq_scene_change == 0) &&
    !frame_is_intra_only(cm) && !cpi->ppi->use_svc) {
  return;
}
```
