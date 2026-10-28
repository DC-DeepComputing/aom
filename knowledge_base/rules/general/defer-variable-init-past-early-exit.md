---
name: libaom/defer-variable-init-past-early-exit
description: When writing function entry checks or early bailouts, place return guards before struct field reads, variable initializations, dynamic allocations, or setup calls
rubric: safety
severity: should_fix
---

**Rule.** Do not read struct fields, unpack parameters, declare or initialize
local variables, execute setup calls (e.g. av1_invalid_rd_stats), or allocate
dynamic heap memory (e.g. aom_malloc, CHECK_MEM_ERROR) before early-exit,
bailout, error, or pruning checks when they are not needed on the early-return
path. Place all entry precondition guards and bailout checks upfront, and defer
field reads, local initializations, and heap allocations until after preceding
guard conditions have passed.

**Why.** Reading struct fields, calculating intermediate values, or allocating
dynamic memory before an early return wastes CPU cycles and memory bandwidth
whenever an error or bailout branch is taken. Placing guard conditions upfront
ensures invalid states exit immediately, keeps entry preconditions clean,
tightly scopes variable lifetimes, and eliminates redundant cleanup or
deallocation on early-exit paths.

## Examples

Bad:

```cpp
// Bad: Struct field reads and parameter unpacking before error/validity guard
const int capacity = buf->capacity;
int c = enc->cnt;
if (enc->error || !buf->is_valid) return -1;

// Bad: Dynamic allocation before early bailout check
int16_t *hbuf;
CHECK_MEM_ERROR(cm, hbuf, (int16_t *)aom_malloc(width * sizeof(*hbuf)));
if (xd->bd != 8) {
  aom_free(hbuf);
  return best_sad;
}

// Bad: Local structure setup before pruning check
RD_STATS this_rdc;
av1_invalid_rd_stats(&this_rdc);
if (args->prune_mode_based_on_sad && this_sad > sad_threshold) return;
```

Good:

```cpp
// Good: Error and validity guards placed before field reads
if (enc->error || !buf->is_valid) return -1;
const int capacity = buf->capacity;
int c = enc->cnt;

// Good: Bailout check placed before dynamic allocation
if (xd->bd != 8) return best_sad;
int16_t *hbuf;
CHECK_MEM_ERROR(cm, hbuf, (int16_t *)aom_malloc(width * sizeof(*hbuf)));

// Good: Pruning check evaluated before local setup call
if (args->prune_mode_based_on_sad && this_sad > sad_threshold) return;
RD_STATS this_rdc;
av1_invalid_rd_stats(&this_rdc);
```
