---
name: libaom/av1/avoid-freeing-aliased-thread-context-pointers
description: When worker thread structures shallow-copy or alias primary thread contexts, deallocate worker buffers through dedicated ownership pointers rather than active member pointers
rubric: safety
severity: nit
---

**Rule.** When worker thread structures shallow-copy or alias primary thread
contexts (such as 'MACROBLOCK mb'), do not deallocate buffers using active
member pointers. Track dynamically allocated worker-owned buffers in dedicated
ownership pointers (such as 'td->mv_costs_alloc' or 'td->dv_costs_alloc') and
deallocate through those dedicated pointers.

**Why.** Worker thread structures frequently alias buffers allocated and owned
by the primary thread. Freeing active member pointers directly during worker
cleanup causes double-free errors when the worker reused the primary thread's
buffer instead of allocating its own.

## Examples

Bad:

```cpp
// Worker thread cleanup freeing active member pointer directly:
if (cpi->sf.inter_sf.mv_cost_upd_level != INTERNAL_COST_UPD_OFF) {
  aom_free(thread_data->td->mb.mv_costs);
}
```

Good:

```cpp
// Worker thread cleanup freeing dedicated ownership pointer:
if (cpi->sf.inter_sf.mv_cost_upd_level != INTERNAL_COST_UPD_OFF) {
  aom_free(thread_data->td->mv_costs_alloc);
  thread_data->td->mv_costs_alloc = NULL;
}
```
