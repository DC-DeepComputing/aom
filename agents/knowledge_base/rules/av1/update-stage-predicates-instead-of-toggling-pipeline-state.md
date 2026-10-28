---
name: libaom/av1/update-stage-predicates-instead-of-toggling-pipeline-state
description: When altering downstream encoder stage behavior, update stage predicate helpers to evaluate configuration modes directly instead of temporarily toggling shared pipeline state flags
rubric: design
severity: should_fix
---

**Rule.** Do not temporarily mutate or toggle persistent pipeline context flags
(such as `ppi->lap_enabled` or `cpi->compressor_stage`) before and after
operations to alter downstream stage behavior. Instead, update stage predicate
helper functions (such as `is_stat_consumption_stage()` or
`has_no_stats_stage()`) to evaluate the relevant configuration modes and pass
settings directly.

**Why.** Temporarily toggling shared or pipeline-level state flags is fragile,
risks leaking mutated state across early exits or error paths, and introduces
concurrency hazards in multi-threaded workflows. Centralizing stage checks
within predicate helper functions keeps shared context state consistent and
makes stage-conditional logic explicit.

## Examples

Bad:

```cpp
// Temporarily toggling shared pipeline state flag to suppress stage logic
if (cpi_lap != NULL && is_one_pass_rt_lag_params(cpi))
  cpi_lap->ppi->lap_enabled = 0;
av1_encode_strategy(cpi, ...);
if (cpi_lap != NULL && is_one_pass_rt_lag_params(cpi))
  cpi_lap->ppi->lap_enabled = 1;
```

Good:

```cpp
// Directly evaluate mode and pass configuration in stage predicate helpers
static inline int is_stat_consumption_stage(const AV1_COMP *const cpi) {
  return (is_stat_consumption_stage_twopass(cpi) ||
          (cpi->oxcf.pass == AOM_RC_ONE_PASS &&
           (cpi->compressor_stage == ENCODE_STAGE) && cpi->ppi->lap_enabled &&
           cpi->oxcf.mode != REALTIME));
}
```
