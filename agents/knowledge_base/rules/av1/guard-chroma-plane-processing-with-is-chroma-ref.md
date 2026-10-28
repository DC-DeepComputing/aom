---
name: libaom/av1/guard-chroma-plane-processing-with-is-chroma-ref
description: When iterating over multiple image planes, guard chroma plane processing with an is_chroma_ref check to avoid accessing unreferenced chroma data in subsampled formats
rubric: correctness
severity: should_fix
---

**Rule.** When iterating over color planes (`for (int plane = 0; plane <
num_planes; ++plane)`), guard chroma planes (where `plane > 0`) by checking `if
(plane && !xd->is_chroma_ref) break;` (or `return`). Do not unconditionally
process chroma plane data without verifying that the current block transmits
chroma reference information.

**Why.** In subsampled chroma formats (such as YUV 4:2:0 or 4:2:2), multiple
sub-8x8 luma blocks share a single 4x4 chroma block, meaning non-terminal
sub-blocks have `is_chroma_ref == false`. Iterating through chroma planes
without checking `is_chroma_ref` accesses invalid chroma reference data and
performs unnecessary or corrupt transform and rate-distortion operations.

## Examples

Bad:

```cpp
for (int plane = 0; plane < num_planes; ++plane) {
  const struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  // Unconditionally processes chroma planes even when block has no chroma ref
  av1_predict_intra_block(xd, p, pd, ...);
}
```

Good:

```cpp
for (int plane = 0; plane < num_planes; ++plane) {
  if (plane && !xd->is_chroma_ref) break;
  const struct macroblock_plane *const p = &x->plane[plane];
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  av1_predict_intra_block(xd, p, pd, ...);
}
```
