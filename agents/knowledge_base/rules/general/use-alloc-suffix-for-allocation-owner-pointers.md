---
name: libaom/use-alloc-suffix-for-allocation-owner-pointers
description: When naming struct members that store dynamically allocated memory passed to aom_free(), use the '_alloc' suffix rather than '_base_ptr' or bare pointer names
rubric: readability
severity: nit
---

**Rule.** Name struct members that hold and own dynamically allocated memory
with the '_alloc' suffix (e.g., 'mv_costs_alloc', 'buffer_alloc', 'mi_alloc')
rather than '_base_ptr' or generic pointer names. Reserve '_alloc' for pointers
that store the returned address from dynamic allocation (such as aom_malloc or
aom_memalign) and are passed to aom_free(), distinguishing allocation ownership
from active, offset, or aliased working pointers.

**Why.** In C codebases without smart pointers, explicit naming conventions
clarify memory ownership and lifecycle management. Suffixing owning pointers
with '_alloc' distinguishes memory ownership from working or aliased pointers
that navigate or index into the buffer, preventing memory leaks, double-frees,
or freeing non-base pointers.

## Examples

Bad:

```cpp
typedef struct ThreadData {
  MACROBLOCK mb;
  MvCosts *mv_costs_base_ptr;
  IntraBCMVCosts *dv_costs_base_ptr;
  uint8_t *buffer_base;
} ThreadData;

void free_thread_data(ThreadData *td) {
  aom_free(td->mv_costs_base_ptr);
  aom_free(td->dv_costs_base_ptr);
  aom_free(td->buffer_base);
}
```

Good:

```cpp
typedef struct ThreadData {
  MACROBLOCK mb;
  MvCosts *mv_costs_alloc;
  IntraBCMVCosts *dv_costs_alloc;
  uint8_t *buffer_alloc;
} ThreadData;

void free_thread_data(ThreadData *td) {
  aom_free(td->mv_costs_alloc);
  aom_free(td->dv_costs_alloc);
  aom_free(td->buffer_alloc);
}
```
