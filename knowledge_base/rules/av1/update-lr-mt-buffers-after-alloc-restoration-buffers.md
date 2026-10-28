---
name: libaom/av1/update-lr-mt-buffers-after-alloc-restoration-buffers
description: When allocating or reallocating loop restoration buffers via av1_alloc_restoration_buffers, update multi-threaded worker buffer pointers using av1_init_lr_mt_buffers when multi-threading is enabled
rubric: correctness
severity: blocking
---

**Rule.** When allocating or reallocating frame restoration buffers
(cm->rst_tmpbuf, cm->rlbs) with av1_alloc_restoration_buffers(), update the
multi-threaded loop restoration worker buffer pointers by calling
av1_init_lr_mt_buffers(cpi) when p_mt_info.num_workers > 1. Do not leave worker
thread structures pointing to uninitialized or stale restoration buffers.

**Why.** In multi-threaded loop restoration, worker threads access rst_tmpbuf
and rlbs through AV1LrSync worker data structs (lr_sync->lrworkerdata), where
worker instances alias the frame-level restoration buffers. Omitting
av1_init_lr_mt_buffers() after buffer allocation causes worker threads to
dereference stale or NULL pointers and crash.

## Examples

Bad:

```cpp
const bool is_sgr_enabled = !cpi->sf.lpf_sf.disable_sgr_filter;
av1_alloc_restoration_buffers(cm, is_sgr_enabled);
```

Good:

```cpp
const bool is_sgr_enabled = !cpi->sf.lpf_sf.disable_sgr_filter;
av1_alloc_restoration_buffers(cm, is_sgr_enabled);
// Store the allocated restoration buffers in MT object.
if (cpi->ppi->p_mt_info.num_workers > 1) {
  av1_init_lr_mt_buffers(cpi);
}
```
