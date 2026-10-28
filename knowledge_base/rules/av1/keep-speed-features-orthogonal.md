---
name: libaom/av1/keep-speed-features-orthogonal
description: When gating encoder decision paths, mode pruning, or search heuristics, keep speed feature flags orthogonal and avoid checking unrelated speed features
rubric: correctness
severity: should_fix
---

**Rule.** Keep encoder speed features orthogonal and independently tunable
across speed presets. Do not gate a decision path, search heuristic, or mode
evaluation behind an unrelated speed feature flag when a dedicated speed feature
already governs that behavior.

**Why.** Coupling unrelated speed feature flags creates unintended side effects
across speed presets, making tuning fragile and causing changes in one
optimization (such as palette search pruning) to inadvertently alter unrelated
algorithms (such as hybrid intra RD mode selection).

## Examples

Bad:

```cpp
// Unrelated speed feature prune_screen_palette_search couples with hybrid intra search
if (hybrid_intra_pickmode && bsize < BLOCK_16X16 &&
    cpi->sf.rt_sf.prune_screen_palette_search) {
  if (x->source_variance >= var_thresh[hybrid_intra_pickmode - 1])
    use_rdopt = 1;
}
```

Good:

```cpp
// Dedicated speed feature hybrid_intra_pickmode governs the decision path independently
if (hybrid_intra_pickmode && bsize < BLOCK_16X16) {
  if (x->source_variance >= var_thresh[hybrid_intra_pickmode - 1])
    use_rdopt = 1;
}
```
