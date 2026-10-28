---
name: libaom/group-struct-initialization-contiguously
description: When initializing or configuring a struct or object, keep related memory allocation and field assignments contiguous rather than interleaving unrelated struct modifications
rubric: readability
severity: nit
---

**Rule.** Keep memory allocation, setup calls, and field assignments for a given
structure contiguous within the function scope. Do not interleave unrelated
struct assignments or secondary state modifications into the middle of another
structure's initialization sequence.

**Why.** Interleaving unrelated assignments disrupts code cohesion and locality
of reference, making it harder to verify that a structure is fully and correctly
initialized and increasing the risk of omitted or out-of-order field
assignments.

## Examples

Bad:

```cpp
// Bad: Interleaving unrelated struct modifications in the middle of intra_row_mt_sync setup
row_mt_sync_mem_alloc(intra_row_mt_sync, cm, mi_rows);

mt_info->enc_row_mt.mb_wiener_mt_exit = false;
intra_row_mt_sync->intrabc_extra_top_right_sb_delay = 0;
intra_row_mt_sync->num_threads_working = num_workers;
```

Good:

```cpp
// Good: Keep intra_row_mt_sync allocation and field setup contiguous
row_mt_sync_mem_alloc(intra_row_mt_sync, cm, mi_rows);

intra_row_mt_sync->intrabc_extra_top_right_sb_delay = 0;
intra_row_mt_sync->num_threads_working = num_workers;
mt_info->enc_row_mt.mb_wiener_mt_exit = false;
```
