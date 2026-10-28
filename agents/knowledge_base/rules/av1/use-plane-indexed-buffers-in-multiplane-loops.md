---
name: libaom/av1/use-plane-indexed-buffers-in-multiplane-loops
description: When iterating over frame planes in a YV12_BUFFER_CONFIG, access plane-indexed buffers and chroma-aware strides instead of hardcoding luma fields
rubric: correctness
severity: should_fix
---

**Rule.** When iterating over image planes in a `YV12_BUFFER_CONFIG` frame
buffer, access plane data using indexed plane buffers (such as
`src->buffers[plane]`) and chroma-aware strides (such as `src->strides[is_uv]`
where `is_uv = plane > 0`). Do not hardcode luma buffer fields (`y_buffer`,
`y_stride`) inside loops spanning multiple planes.

**Why.** Hardcoding luma accessors inside a plane loop causes chroma planes (U
and V) to incorrectly read from or overwrite the luma buffer rather than
processing their own plane data, resulting in visual corruption or silent data
corruption.

## Examples

Bad:

```cpp
for (int i = 0; i < AOMMIN(num_planes, MAX_MB_PLANE); ++i) {
  const int is_uv = i > 0;
  // Bug: hardcoded luma buffer and stride for all planes
  scale_plane_2_to_1_bilinear(src->y_buffer, src->y_stride, dst->y_buffer,
                              dst->y_stride, dst_w, dst_h, c0c1);
}
```

Good:

```cpp
for (int i = 0; i < AOMMIN(num_planes, MAX_MB_PLANE); ++i) {
  const int is_uv = i > 0;
  // Correct: plane-indexed buffer and stride access
  scale_plane_2_to_1_bilinear(src->buffers[i], src->strides[is_uv],
                              dst->buffers[i], dst->strides[is_uv], dst_w,
                              dst_h, c0c1);
}
```
