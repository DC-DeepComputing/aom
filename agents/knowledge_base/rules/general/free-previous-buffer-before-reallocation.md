---
name: libaom/free-previous-buffer-before-reallocation
description: When reallocating or resizing dynamically allocated buffers or worker arrays on a struct, explicitly free previously allocated memory before allocating new buffers to prevent leaks
rubric: safety
severity: should_fix
---

**Rule.** Before reallocating or overwriting dynamically allocated pointer
fields on a struct (such as when resizing buffers or increasing worker thread
counts), explicitly deallocate previously allocated memory using aom_free() or
the appropriate cleanup routine. Verify whether existing cleanup helpers free
the container array itself or only its elements before allocating anew.

**Why.** Overwriting non-null dynamic pointer fields without deallocating their
prior allocations causes persistent memory leaks across repeated
reconfigurations or capacity increases.

## Examples

Bad:

```cpp
if (p_mt_info->num_workers >= num_workers && p_mt_info->num_workers > 0) {
  return;
}
AOM_CHECK_MEM_ERROR(&ppi->error, p_mt_info->workers,
                    aom_malloc(num_workers * sizeof(*p_mt_info->workers)));
```

Good:

```cpp
if (p_mt_info->num_workers >= num_workers && p_mt_info->num_workers > 0) {
  return;
}
aom_free(p_mt_info->workers);
AOM_CHECK_MEM_ERROR(&ppi->error, p_mt_info->workers,
                    aom_malloc(num_workers * sizeof(*p_mt_info->workers)));
```
