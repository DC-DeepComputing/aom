---
name: libaom/av1/do-not-gate-mt-sync-realloc-on-per-frame-flags
description: When allocating or reallocating encoder multi-threading synchronization structures, do not gate buffer checks on per-frame feature flags like is_restoration_used
rubric: correctness
severity: should_fix
---

**Rule.** Do not gate multi-threading synchronization struct allocation or
reallocation checks behind transient per-frame feature flags (such as
is_restoration_used(cm)). Always ensure MT sync buffers are evaluated and
reallocated whenever frame dimensions, row counts, plane counts, or worker
counts change.

**Why.** Per-frame coding tools (such as loop restoration) can be dynamically
toggled frame-by-frame (e.g. disabled on lossless frames). If synchronization
struct reallocation is conditioned on whether the tool is active for the current
frame, subsequent frames that re-enable the tool after dynamic resolution
changes will retain undersized or stale worker synchronization buffers, causing
synchronization failures or memory corruption.

## Examples

Bad:

```cpp
#if !CONFIG_REALTIME_ONLY
if (is_restoration_used(cm)) {
  AV1LrSync *lr_sync = &mt_info->lr_row_sync;
  if (!lr_sync->sync_range || num_rows_lr > lr_sync->rows ||
      num_lr_workers > lr_sync->num_workers ||
      num_planes > lr_sync->num_planes) {
    av1_loop_restoration_dealloc(lr_sync, lr_sync->num_workers);
    av1_loop_restoration_alloc(lr_sync, cm, num_lr_workers, num_rows_lr,
                               num_planes, cm->width);
  }
}
#endif
```

Good:

```cpp
#if !CONFIG_REALTIME_ONLY
AV1LrSync *lr_sync = &mt_info->lr_row_sync;
if (!lr_sync->sync_range || num_rows_lr > lr_sync->rows ||
    num_lr_workers > lr_sync->num_workers ||
    num_planes > lr_sync->num_planes) {
  av1_loop_restoration_dealloc(lr_sync, lr_sync->num_workers);
  av1_loop_restoration_alloc(lr_sync, cm, num_lr_workers, num_rows_lr,
                             num_planes, cm->width);
}
#endif
```
