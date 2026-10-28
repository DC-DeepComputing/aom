---
name: libaom/place-conditional-assertions-inside-guarding-branch
description: When asserting preconditions for conditionally accessed buffers or resources, place assert() inside the guarding branch rather than unconditionally before the if statement
rubric: correctness
severity: blocking
---

**Rule.** Place debug assertions (`assert()` or `aom_assert()`) that verify
buffer or resource validity inside the conditional block (`if`) that performs
the access, rather than unconditionally before the condition. Do not assert
preconditions unconditionally before an `if` guard when the resource is only
allocated, configured, or accessed within that specific branch.

**Why.** When a resource, worker pool, or buffer is only initialized or required
for specific configurations or active coding tools, asserting its validity
unconditionally before the guarding `if` condition triggers false assertion
failures in debug builds whenever the feature is inactive or the branch is
bypassed.

## Examples

Bad:

```cpp
assert(lr_sync->num_workers > 0);
if (lr_sync->sync_range &&
    cpi->ppi->gf_group.frame_parallel_level[cpi->gf_frame_index] == 0) {
  lr_sync->lrworkerdata[0].rst_tmpbuf = cm->rst_tmpbuf;
  lr_sync->lrworkerdata[0].rlbs = cm->rlbs;
}
```

Good:

```cpp
if (lr_sync->sync_range &&
    cpi->ppi->gf_group.frame_parallel_level[cpi->gf_frame_index] == 0) {
  assert(lr_sync->num_workers > 0);
  lr_sync->lrworkerdata[0].rst_tmpbuf = cm->rst_tmpbuf;
  lr_sync->lrworkerdata[0].rlbs = cm->rlbs;
}
```
