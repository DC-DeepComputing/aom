---
name: libaom/omit-redundant-null-check-with-tracked-allocation-size
description: When guarding dynamic buffer reallocation with tracked allocation size or capacity fields initialized to zero, omit redundant ptr == NULL checks
rubric: readability
severity: nit
---

**Rule.** When guarding dynamic buffer allocation or resizing where a companion
allocation size or capacity member is initialized to 0 and reset to 0 upon
freeing, do not include a redundant 'ptr == NULL' check in the conditional
guard. Rely on the allocation size comparison (such as 'alloc_size !=
required_size' or 'alloc_size < required_size') directly.

**Why.** If the companion allocation size or capacity field is initialized to 0
and cleared on deallocation, checking 'alloc_size != required_size' or
'alloc_size < required_size' already evaluates to true for unallocated or freed
buffers when the required size is non-zero. Adding an explicit 'ptr == NULL ||
...' check introduces redundant conditional logic and visual clutter.

## Examples

Bad:

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

Good:

```cpp
if (cpi->src_sad_blk_alloc_size != sb_cols * sb_rows) {
  aom_free(cpi->src_sad_blk_64x64);
  CHECK_MEM_ERROR(cm, cpi->src_sad_blk_64x64,
                  (uint64_t *)aom_calloc(sb_cols * sb_rows,
                                         sizeof(*cpi->src_sad_blk_64x64)));
  cpi->src_sad_blk_alloc_size = sb_cols * sb_rows;
}
```
