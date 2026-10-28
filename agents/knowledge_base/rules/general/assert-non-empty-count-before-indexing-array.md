---
name: libaom/assert-non-empty-count-before-indexing-array
description: When indexing elements of dynamic buffers, worker pools, or collection arrays (e.g., array[0]), assert that the count or capacity is positive (assert(count > 0)).
rubric: correctness
severity: should_fix
---

**Rule.** When directly indexing elements of dynamically allocated arrays,
multi-threading worker contexts, or buffer structures (such as accessing
array[0] or array[count - 1]), assert that the element count or capacity is
positive (e.g. assert(count > 0) or assert(num_workers > 0)). Ensure
preconditions are asserted before accessing array elements so uninitialized or
zero-capacity states are detected.

**Why.** Directly dereferencing array elements like array[0] assumes the backing
memory is allocated and non-empty. Asserting positive counts verifies caller
preconditions and catches uninitialized or zero-capacity states early in debug
builds.

## Examples

Bad:

```cpp
void av1_init_lr_mt_buffers(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  AV1LrSync *lr_sync = &cpi->mt_info.lr_row_sync;
  if (lr_sync->sync_range &&
      cpi->ppi->gf_group.frame_parallel_level[cpi->gf_frame_index] == 0) {
    // Indexing element 0 without asserting non-empty worker array.
    lr_sync->lrworkerdata[0].rst_tmpbuf = cm->rst_tmpbuf;
    lr_sync->lrworkerdata[0].rlbs = cm->rlbs;
  }
}
```

Good:

```cpp
void av1_init_lr_mt_buffers(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  AV1LrSync *lr_sync = &cpi->mt_info.lr_row_sync;

  // Assert non-empty worker count before accessing array elements.
  assert(lr_sync->num_workers > 0);
  if (lr_sync->sync_range &&
      cpi->ppi->gf_group.frame_parallel_level[cpi->gf_frame_index] == 0) {
    lr_sync->lrworkerdata[0].rst_tmpbuf = cm->rst_tmpbuf;
    lr_sync->lrworkerdata[0].rlbs = cm->rlbs;
  }
}
```
