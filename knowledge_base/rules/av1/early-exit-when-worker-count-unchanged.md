---
name: libaom/av1/early-exit-when-worker-count-unchanged
description: When dynamically creating, initializing, or reallocating worker thread pools, check and early-exit if the requested worker count equals the active count
rubric: safety
severity: nit
---

**Rule.** When allocating or resizing worker thread pools dynamically (such as
in av1_create_workers or worker initialization routines), assert valid positive
thread counts and early-exit if the requested worker count is already equal to
the active worker count (p_mt_info->num_workers). Avoid tearing down and
recreating thread structures when the thread pool configuration is unchanged.

**Why.** Tearing down existing worker threads, deallocating context buffers, and
spawning new OS threads is expensive. Checking for an unchanged worker count
avoids redundant worker lifecycle churn across frame reconfigurations or encode
passes.

## Examples

Bad:

```cpp
void av1_create_workers(AV1_PRIMARY *ppi, int num_workers) {
  PrimaryMultiThreadInfo *const p_mt_info = &ppi->p_mt_info;
  if (p_mt_info->num_workers > 0) {
    terminate_worker_data(ppi);
    aom_free(p_mt_info->tile_thr_data);
    aom_free(p_mt_info->workers);
    p_mt_info->num_workers = 0;
  }
  // Allocate and create new workers...
}
```

Good:

```cpp
void av1_create_workers(AV1_PRIMARY *ppi, int num_workers) {
  PrimaryMultiThreadInfo *const p_mt_info = &ppi->p_mt_info;
  assert(num_workers > 0);
  if (num_workers == p_mt_info->num_workers) return;

  if (p_mt_info->num_workers > 0) {
    terminate_worker_data(ppi);
    aom_free(p_mt_info->tile_thr_data);
    aom_free(p_mt_info->workers);
    p_mt_info->num_workers = 0;
  }
  // Allocate and create new workers...
}
```
