---
name: libaom/av1/guard-expensive-block-metric-computations
description: When computing expensive block metrics or statistics in the encoder, guard the calculations with the conditions or speed features that consume them
rubric: safety
severity: should_fix
---

**Rule.** Guard expensive preprocessing, block analysis, and variance
computations behind the conditional checks or speed features that actually
consume their results. Do not compute metrics unconditionally when downstream
pruning or search logic only uses them for specific block sizes, modes, or
speed-feature configurations.

**Why.** Unconditionally calculating block statistics, variance metrics, or
prediction characteristics incurs unnecessary CPU overhead on blocks or encoder
configurations where no active speed feature or downstream decision branches
consume those values.

## Examples

Bad:

```cpp
if (cpi->oxcf.mode == ALLINTRA) {
  double var_min, var_max;
  // Bad: Computes sub-block variance unconditionally before checking if needed.
  log_sub_block_var(cpi, x, bsize, &var_min, &var_max);
  if (bsize >= BLOCK_16X16) {
    // ...
  } else if (cpi->sf.part_sf.prune_rect_part_using_4x4_var_deviation &&
             !x->must_find_valid_partition) {
    // ...
  }
}
```

Good:

```cpp
if (cpi->oxcf.mode == ALLINTRA) {
  const bool bsize_at_least_16x16 = (bsize >= BLOCK_16X16);
  const bool prune_rect_part_using_4x4_var_deviation =
      cpi->sf.part_sf.prune_rect_part_using_4x4_var_deviation &&
      !x->must_find_valid_partition;

  // Good: Only computes variance when at least one active condition requires it.
  if (bsize_at_least_16x16 || prune_rect_part_using_4x4_var_deviation) {
    double var_min, var_max;
    log_sub_block_var(cpi, x, bsize, &var_min, &var_max);
    // ...
  }
}
```
