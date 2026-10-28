---
name: libaom/merge-buffer-reallocation-blocks
description: When reallocating dynamic buffers upon size changes or initialization, rely on safe aom_free(NULL) and merge deallocation and allocation into a single conditional block
rubric: readability
severity: nit
---

**Rule.** When reallocating dynamic buffers on demand or when dimensions change,
merge deallocation and allocation checks into a single conditional block (such
as if (ptr == NULL || alloc_size != req_size)) calling aom_free(ptr) followed by
reallocation. Do not guard aom_free() with an explicit ptr != NULL check or
split the workflow into separate deallocation and allocation if-blocks.

**Why.** aom_free() safely handles NULL pointers as a no-op, matching standard
free(). Splitting reallocation into separate if (ptr != NULL && ...) and if (ptr
== NULL) branches introduces redundant NULL guards, extra branch nesting, and
unnecessary intermediate NULL assignments.

## Examples

Bad:

```cpp
if (cpi->src_sad_blk_64x64 != NULL &&
    cpi->src_sad_blk_alloc_size != sb_cols * sb_rows) {
  aom_free(cpi->src_sad_blk_64x64);
  cpi->src_sad_blk_64x64 = NULL;
}
if (cpi->src_sad_blk_64x64 == NULL) {
  CHECK_MEM_ERROR(cm, cpi->src_sad_blk_64x64,
                  (uint64_t *)aom_calloc(sb_cols * sb_rows,
                                         sizeof(*cpi->src_sad_blk_64x64)));
  cpi->src_sad_blk_alloc_size = sb_cols * sb_rows;
}
```

Good:

```cpp
if (cpi->src_sad_blk_64x64 == NULL ||
    cpi->src_sad_blk_alloc_size != sb_cols * sb_rows) {
  aom_free(cpi->src_sad_blk_64x64);
  CHECK_MEM_ERROR(cm, cpi->src_sad_blk_64x64,
                  (uint64_t *)aom_calloc(sb_cols * sb_rows,
                                         sizeof(*cpi->src_sad_blk_64x64)));
  cpi->src_sad_blk_alloc_size = sb_cols * sb_rows;
}
```
