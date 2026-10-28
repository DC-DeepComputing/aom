---
name: libaom/av1/guard-mt-sync-allocation-with-feature-check
description: When allocating or reallocating multi-threading synchronization structures for encoder tools, guard allocations with feature-usage checks like is_restoration_used to avoid allocating when disabled
rubric: safety
severity: should_fix
---

**Rule.** When allocating or reallocating multi-threading synchronization
structures for encoder features or coding tools (such as loop restoration, loop
filter, CDEF, or TPL), guard the allocation block with a feature-usage check
(such as is_restoration_used(cm)). Do not unconditionally initialize or allocate
multi-threading sync structures when the underlying coding tool is disabled.

**Why.** Unconditionally initializing and allocating multi-threading
synchronization structures incurs unnecessary heap allocations, mutex
initialization overhead, and deallocations on frames or configurations where the
feature is inactive.

## Examples

Bad:

```cpp
// Unconditionally allocating LR MT sync objects without checking if restoration is active:
AV1LrSync *lr_sync = &mt_info->lr_row_sync;
int num_rows_lr = av1_lr_count_units(rst_unit_size, cm->height);
int num_lr_workers = av1_get_num_mod_workers_for_alloc(p_mt_info, MOD_LR);
if (!lr_sync->sync_range || num_rows_lr > lr_sync->rows ||
    num_lr_workers > lr_sync->num_workers ||
    MAX_MB_PLANE > lr_sync->num_planes) {
  av1_loop_restoration_dealloc(lr_sync, lr_sync->num_workers);
  av1_loop_restoration_alloc(lr_sync, cm, num_lr_workers, num_rows_lr,
                             MAX_MB_PLANE, cm->width);
}
```

Good:

```cpp
// Guard MT sync allocation with tool usage check
if (is_restoration_used(cm)) {
  AV1LrSync *lr_sync = &mt_info->lr_row_sync;
  int num_rows_lr = av1_lr_count_units(rst_unit_size, cm->height);
  int num_lr_workers = av1_get_num_mod_workers_for_alloc(p_mt_info, MOD_LR);
  if (!lr_sync->sync_range || num_rows_lr > lr_sync->rows ||
      num_lr_workers > lr_sync->num_workers ||
      MAX_MB_PLANE > lr_sync->num_planes) {
    av1_loop_restoration_dealloc(lr_sync, lr_sync->num_workers);
    av1_loop_restoration_alloc(lr_sync, cm, num_lr_workers, num_rows_lr,
                               MAX_MB_PLANE, cm->width);
  }
}
```
