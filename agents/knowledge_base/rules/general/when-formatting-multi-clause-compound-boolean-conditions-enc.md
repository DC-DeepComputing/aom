---
name: libaom/when-formatting-multi-clause-compound-boolean-conditions-enc
description: When formatting multi-clause compound boolean conditions, enclose relational comparison sub-expressions in parentheses to maintain consistency with neighboring parenthesized clauses
rubric: readability
severity: nit
---

**Rule.** In multi-clause compound boolean conditions where surrounding clauses
are enclosed in parentheses, wrap relational comparison sub-expressions in
parentheses to maintain consistent styling across all clauses.

**Why.** Consistent parenthesization across all sub-clauses in compound boolean
conditions avoids visual inconsistency and matches neighboring clauses.

## Examples

Bad:

```cpp
if ((cpi->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ) &&
    cpi->sf.rt_sf.overshoot_detection_cbr == FAST_DETECTION_MAXQ &&
    (cpi->cyclic_refresh->counter_encode_maxq_scene_change == 0) &&
    !frame_is_intra_only(cm)) {
  return 1;
}
```

Good:

```cpp
if ((cpi->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ) &&
    (cpi->sf.rt_sf.overshoot_detection_cbr == FAST_DETECTION_MAXQ) &&
    (cpi->cyclic_refresh->counter_encode_maxq_scene_change == 0) &&
    !frame_is_intra_only(cm)) {
  return 1;
}
```
