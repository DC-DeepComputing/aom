---
name: libaom/avoid-redundant-null-check-before-free
description: When deallocating memory with aom_free(), free(), or cleanup helpers, invoke them unconditionally without guarding on NULL checks or feature flags
rubric: safety
severity: should_fix
---

**Rule.** Do not guard calls to aom_free(), standard free(), or subsystem
cleanup functions (such as av1_free_svc_cyclic_refresh() or other *_free_*()
helpers) with explicit if (ptr != NULL) / if (ptr) checks or
feature/configuration flags (such as oxcf, aq_mode, or tuning). Call aom_free()
and free() unconditionally, as they safely handle NULL pointers as a no-op.
Additionally, invoke resource cleanup helpers unconditionally (or check buffer
pointer state directly rather than configuration flags) to prevent memory leaks
during dynamic reconfiguration when feature modes are disabled after allocation.

**Why.** Standard free() and libaom's aom_free() safely treat NULL pointers as a
no-op, making explicit pointer checks redundant boilerplate that clutters
cleanup routines. Furthermore, in dynamic reconfiguration workflows (such as
layer changes, resolution updates, or codec control parameter changes), a
feature may have been active when memory was allocated and subsequently disabled
before cleanup runs. Guarding cleanup functions on current feature or
configuration flags causes memory leaks, whereas unconditional cleanup safely
inspects pointer state or executes a safe no-op.

## Examples

Bad:

```cpp
// Redundant NULL check before aom_free() or free()
if (cm->cdef_info.linebuf[plane] != NULL) {
  aom_free(cm->cdef_info.linebuf[plane]);
}
cm->cdef_info.linebuf[plane] = NULL;

if (hbuf) free(hbuf);

// Guarding deallocations on configuration/feature flags risks leaks on reconfiguration
if (cpi->oxcf.tune_cfg.tuning == AOM_TUNE_VMAF_SALIENCY_MAP) {
  aom_free(cpi->saliency_map);
}
if (cpi->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ) {
  av1_free_svc_cyclic_refresh(cpi);
}
```

Good:

```cpp
// aom_free() and free() are NULL-safe and should be called unconditionally
aom_free(cm->cdef_info.linebuf[plane]);
cm->cdef_info.linebuf[plane] = NULL;

free(hbuf);

// Deallocate or invoke cleanup helpers unconditionally
aom_free(cpi->saliency_map);
av1_free_svc_cyclic_refresh(cpi);
```
