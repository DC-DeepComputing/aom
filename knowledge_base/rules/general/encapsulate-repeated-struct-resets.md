---
name: libaom/encapsulate-repeated-struct-resets
description: When resetting struct members, worker synchronization flags, or cached state, encapsulate field assignments inside dedicated reset, clear, or prepare helpers rather than open-coding them at callers
rubric: design
severity: nit
---

**Rule.** Encapsulate struct field resets, cache invalidations, and worker
synchronization state clearings inside dedicated inline helper functions (such
as clear_*, reset_*, init_*, or prepare_*_workers / reset_*_job_info) rather
than open-coding member assignments across caller sites. When a dedicated
preparation or reset helper exists, consolidate all related field
initializations and synchronization flags (such as mt_exit, pack_bs_mt_exit,
cdef_mt_exit, next_job_idx, or cache flags like use_dc_pred_cache /
dc_pred_is_cached) inside the helper instead of resetting individual fields
directly at call sites.

**Why.** Encapsulating all related struct member resets into a single dedicated
helper eliminates code duplication, prevents callers from leaking internal
synchronization structure details or forgetting to clear associated state flags
across iterative passes, and ensures consistent state transitions whenever new
fields are added or when helpers are invoked from multiple call sites.

## Examples

Bad:

```cpp
// Variant 1: Duplicated field resets open-coded at callers
xd->cfl.use_dc_pred_cache = 0;
xd->cfl.dc_pred_is_cached[0] = 0;
xd->cfl.dc_pred_is_cached[1] = 0;

// Variant 2: Caller directly resetting worker sync flags before launch or reset helper
void av1_write_tile_obu_mt(AV1_COMP *const cpi, ...) {
  // Bad: Caller open-codes synchronization state reset instead of encapsulating it
  cpi->mt_info.pack_bs_sync.pack_bs_mt_exit = false;
  prepare_pack_bs_workers(cpi, pack_bs_params, pack_bs_worker_hook, num_workers);
  launch_workers(&cpi->mt_info, num_workers);
}
```

Good:

```cpp
// Variant 1: Shared inline helper for clearing state
static INLINE void clear_cfl_dc_pred_flags(CFL_CTX *cfl) {
  cfl->use_dc_pred_cache = 0;
  cfl->dc_pred_is_cached[CFL_PRED_U] = 0;
  cfl->dc_pred_is_cached[CFL_PRED_V] = 0;
}
clear_cfl_dc_pred_flags(&xd->cfl);

// Variant 2: Dedicated preparation helper encapsulates all synchronization resets
static void prepare_pack_bs_workers(AV1_COMP *const cpi, ...) {
  PackBitstreamSync *const pack_bs_sync = &cpi->mt_info.pack_bs_sync;
  pack_bs_sync->next_job_idx = 0;
  pack_bs_sync->pack_bs_mt_exit = false;
  ...
}

void av1_write_tile_obu_mt(AV1_COMP *const cpi, ...) {
  prepare_pack_bs_workers(cpi, pack_bs_params, pack_bs_worker_hook, num_workers);
  launch_workers(&cpi->mt_info, num_workers);
}
```
