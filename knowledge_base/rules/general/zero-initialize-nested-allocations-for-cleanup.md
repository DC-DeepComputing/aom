---
name: libaom/zero-initialize-nested-allocations-for-cleanup
description: When allocating dynamic pointer arrays or structures with nested allocations, zero-initialize outer memory with aom_calloc and reuse a NULL-tolerant cleanup routine
rubric: safety
severity: should_fix
---

**Rule.** When allocating multidimensional pointer arrays (e.g., `type **`) or
structures containing nested dynamically allocated pointers, allocate the outer
memory using `aom_calloc()` so unallocated entries remain `NULL`. Reuse a
unified, NULL-tolerant cleanup routine or error path (relying on
`aom_free(NULL)` being a safe no-op) to deallocate partially constructed
structures rather than writing ad-hoc rollback loops or leaving uninitialized
pointers that crash during cleanup.

**Why.** Using `aom_malloc()` leaves unallocated pointer members and array
entries as uninitialized garbage memory. If an inner allocation fails midway
through initialization, unified cleanup routines that iterate through all slots
will attempt to free garbage pointers, resulting in undefined behavior or
crashes. Zero-initializing with `aom_calloc()` guarantees that all unallocated
pointer slots remain `NULL`, allowing a single shared cleanup function or
rollback label to safely release both fully and partially constructed structures
without complex, bug-prone manual rollback loops.

## Examples

Bad:

```cpp
// Bad: malloc leaves unallocated pointers uninitialized; ad-hoc rollback loop
pred_pos = (int **)aom_malloc(sizeof(*pred_pos) * num_pos);
if (!pred_pos) return false;
for (int i = 0; i < num_pos; i++) {
  pred_pos[i] = (int *)aom_malloc(sizeof(**pred_pos) * 3);
  if (!pred_pos[i]) {
    for (int j = i - 1; j >= 0; --j) aom_free(pred_pos[j]);
    aom_free(pred_pos);
    return false;
  }
}

// Bad: struct array allocated with malloc leaves nested pointers uninitialized
motions = (RANSAC_MOTION *)aom_malloc(sizeof(*motions) * num_motions);
for (int i = 0; i < num_motions; ++i) {
  motions[i].inliers = (int *)aom_malloc(sizeof(*motions->inliers) * npoints);
  if (!motions[i].inliers) goto cleanup; // cleanup frees motions[i].inliers for all num_motions (crashes on garbage)
}
```

Good:

```cpp
// Good: calloc zeros outer array, enabling unified cleanup helper
pred_pos = (int **)aom_calloc(num_pos, sizeof(*pred_pos));
if (!pred_pos) return false;
for (int i = 0; i < num_pos; i++) {
  pred_pos[i] = (int *)aom_malloc(sizeof(**pred_pos) * 3);
  if (!pred_pos[i]) {
    dealloc_arrays(pred_pos, num_pos);
    return false;
  }
}

// Good: calloc ensures unallocated nested pointers are NULL and safely freed
motions = (RANSAC_MOTION *)aom_calloc(num_motions, sizeof(*motions));
for (int i = 0; i < num_motions; ++i) {
  motions[i].inliers = (int *)aom_malloc(sizeof(*motions->inliers) * npoints);
  if (!motions[i].inliers) goto cleanup; // cleanup safely calls aom_free() on all slots (NULL is a safe no-op)
}
```
