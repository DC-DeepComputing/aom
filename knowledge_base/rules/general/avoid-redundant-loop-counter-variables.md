---
name: libaom/avoid-redundant-loop-counter-variables
description: When iterating loops or tracking progress, use the existing loop counter or index directly instead of maintaining duplicate mirror variables
rubric: readability
severity: nit
---

**Rule.** Do not declare or maintain redundant local variables that duplicate
the value and state of an existing loop counter, index, or progress iterator.
Use the primary loop variable directly across loop bodies, nested error
handlers, and cleanup calls.

**Why.** Maintaining separate mirror variables that copy loop counters adds
visual clutter, increases mutable local state (especially with volatile
qualifiers across setjmp/longjmp), and risks desynchronization bugs if one
variable is updated while the other is not.

## Examples

Bad:

```cpp
volatile int i = 0;
volatile int num_workers_prepared = 0;
while (i < num_workers) {
  if (setjmp(error->jmp)) {
    restore_workers(ppi, parallel_frames, num_workers_prepared);
  }
  prepare_worker(ppi, i);
  i += mt_info->num_workers;
  num_workers_prepared = i;
}
```

Good:

```cpp
volatile int i = 0;
while (i < num_workers) {
  if (setjmp(error->jmp)) {
    restore_workers(ppi, parallel_frames, i);
  }
  prepare_worker(ppi, i);
  i += mt_info->num_workers;
}
```
