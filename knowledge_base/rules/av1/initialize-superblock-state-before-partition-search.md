---
name: libaom/av1/initialize-superblock-state-before-partition-search
description: When implementing superblock encoding entry points or partition search paths, invoke superblock initialization routines before conditional fast paths or early returns
rubric: correctness
severity: should_fix
---

**Rule.** In superblock encoding entry points (such as `encode_nonrd_sb` or
`encode_rd_sb`), invoke superblock initialization routines (such as
`init_encode_nonrd_sb` or `init_encode_rd_sb`) before conditional fast paths, ML
partition searches, or early-returning search modes. Ensure that all
superblock-level context, rate-distortion statistics, and delta-q parameters are
fully initialized across all partition paths.

**Why.** Placing superblock initialization routines below early-return branches
or feature fast paths causes those bypass paths to execute with uninitialized or
stale superblock context and stats, leading to encoder state corruption or
divergent behavior.

## Examples

Bad:

```cpp
#if CONFIG_RT_ML_PARTITIONING
  if (sf->part_sf.partition_search_type == ML_BASED_PARTITION) {
    RD_STATS dummy_rdc;
    get_estimated_pred(cpi, tile_info, x, mi_row, mi_col);
    av1_nonrd_pick_partition(cpi, td, tile_data, tp, mi_row, mi_col,
                             BLOCK_64X64, &dummy_rdc, 1, INT64_MAX, pc_root);
    return;
  }
#endif

#if !CONFIG_REALTIME_ONLY
  init_encode_nonrd_sb(cpi, td, tile_data, mi_row, mi_col);
#endif
```

Good:

```cpp
#if !CONFIG_REALTIME_ONLY
  init_encode_nonrd_sb(cpi, td, tile_data, mi_row, mi_col);
#endif

#if CONFIG_RT_ML_PARTITIONING
  if (sf->part_sf.partition_search_type == ML_BASED_PARTITION) {
    RD_STATS dummy_rdc;
    get_estimated_pred(cpi, tile_info, x, mi_row, mi_col);
    av1_nonrd_pick_partition(cpi, td, tile_data, tp, mi_row, mi_col,
                             BLOCK_64X64, &dummy_rdc, 1, INT64_MAX, pc_root);
    return;
  }
#endif
```
