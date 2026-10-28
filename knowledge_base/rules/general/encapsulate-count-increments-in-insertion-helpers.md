---
name: libaom/encapsulate-count-increments-in-insertion-helpers
description: When writing helper functions that validate and insert elements into a buffer or stack, update the element count inside the helper rather than at caller sites
rubric: design
severity: should_fix
---

**Rule.** When writing or modifying helper functions that validate, filter, and
append elements into an array, stack, or candidate buffer (such as
`ref_mv_stack`), encapsulate incrementing the item count (`++*count`) within the
helper itself. Avoid splitting insertion logic across caller and callee by
requiring call sites to manually increment the element count upon a successful
insertion.

**Why.** Encapsulating element insertion and count updates together ensures the
data structure's invariants are maintained in one place, prevents caller sites
from forgetting to update the count, and avoids duplicated post-call increment
logic across callers.

## Examples

Bad:

```cpp
// Inside helper:
ref_mv_stack[*refmv_count] = cand_mv;
ref_mv_weight[*refmv_count] = REF_CAT_LEVEL;
return true;

// At caller site:
if (check_rmb_cand(cand_mv, ref_mv_stack, ref_mv_weight, *refmv_count, ...)) {
  ++*refmv_count;
  break;
}
```

Good:

```cpp
// Inside helper:
ref_mv_stack[*refmv_count] = cand_mv;
ref_mv_weight[*refmv_count] = REF_CAT_LEVEL;
++*refmv_count;
return true;

// At caller site:
if (check_rmb_cand(cand_mv, ref_mv_stack, ref_mv_weight, refmv_count, ...)) {
  break;
}
```
