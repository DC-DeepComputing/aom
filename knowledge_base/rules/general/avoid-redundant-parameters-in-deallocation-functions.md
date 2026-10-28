---
name: libaom/avoid-redundant-parameters-in-deallocation-functions
description: When implementing deallocation or cleanup functions for structs with dynamically allocated members, rely on stored struct dimensions rather than passing redundant count parameters
rubric: design
severity: should_fix
---

**Rule.** Do not pass redundant size, count, or dimension parameters to
deallocation or cleanup functions when the target struct already stores its
allocated dimensions internally. Have the deallocation function read the
struct's own members to determine loop bounds and cleanup extents.

**Why.** Passing external size or count parameters to deallocation functions
creates bloated signatures and risks caller-callee mismatches (such as callers
passing newly requested dimensions instead of previously allocated counts).
Relying on stored struct fields ensures cleanup matches the actual allocated
state and prevents memory leaks or out-of-bounds accesses.

## Examples

Bad:

```cpp
void av1_loop_restoration_dealloc(AV1LrSync *lr_sync, int num_workers) {
  if (lr_sync->lrworkerdata) {
    for (int worker_idx = 0; worker_idx < num_workers - 1; worker_idx++) {
      aom_free(lr_sync->lrworkerdata[worker_idx].rstworkerdata);
    }
    aom_free(lr_sync->lrworkerdata);
  }
}
```

Good:

```cpp
void av1_loop_restoration_dealloc(AV1LrSync *lr_sync) {
  if (lr_sync->lrworkerdata) {
    for (int worker_idx = 0; worker_idx < lr_sync->num_workers - 1; worker_idx++) {
      aom_free(lr_sync->lrworkerdata[worker_idx].rstworkerdata);
    }
    aom_free(lr_sync->lrworkerdata);
  }
}
```
