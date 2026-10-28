---
name: libaom/separate-allocation-checks-for-independent-buffers
description: When allocating or reallocating independently used buffers, check each buffer's NULL sentinel separately instead of coupling lifecycle checks to unrelated buffers
rubric: correctness
severity: should_fix
---

**Rule.** Do not couple the allocation or reallocation check of one buffer to
the NULL status of a different, independently used buffer. Manage each buffer's
allocation condition, size checks, and lifecycle in separate conditional blocks.

**Why.** Buffers that can be conditionally enabled or used independently (such
as motion vector arrays and segmentation maps) have distinct lifecycles. Gating
one buffer's allocation on another buffer's NULL sentinel can leave required
buffers unallocated or trigger redundant reallocations when features are
dynamically toggled.

## Examples

Bad:

```cpp
if (buf->seg_map == NULL || buf_rows != mi_params->mi_rows ||
    buf_cols != mi_params->mi_cols) {
  buf->mi_rows = mi_params->mi_rows;
  buf->mi_cols = mi_params->mi_cols;
  if (cm->seq_params->order_hint_info.enable_ref_frame_mvs) {
    aom_free(buf->mvs);
    CHECK_MEM_ERROR(cm, buf->mvs, ...);
  }
  aom_free(buf->seg_map);
  CHECK_MEM_ERROR(cm, buf->seg_map, ...);
}
```

Good:

```cpp
if (buf->seg_map == NULL || buf_rows != mi_params->mi_rows ||
    buf_cols != mi_params->mi_cols) {
  aom_free(buf->seg_map);
  CHECK_MEM_ERROR(cm, buf->seg_map, ...);
}
if (buf->mvs == NULL || buf_rows != mi_params->mi_rows ||
    buf_cols != mi_params->mi_cols) {
  if (cm->seq_params->order_hint_info.enable_ref_frame_mvs) {
    aom_free(buf->mvs);
    CHECK_MEM_ERROR(cm, buf->mvs, ...);
  }
}
buf->mi_rows = mi_params->mi_rows;
buf->mi_cols = mi_params->mi_cols;
```
