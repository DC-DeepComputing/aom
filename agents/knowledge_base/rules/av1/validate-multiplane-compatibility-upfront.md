---
name: libaom/av1/validate-multiplane-compatibility-upfront
description: When dispatching optimized multi-plane frame processing or resizing routines, validate compatibility across all planes (Y and UV) upfront before executing to prevent wasted compute before fallback
rubric: safety
severity: should_fix
---

**Rule.** When executing optimized multi-plane image or frame processing paths
(such as SIMD frame resizing or filtering), validate compatibility and support
across all planes (both luma and chroma) upfront before beginning processing. Do
not process the luma plane speculatively and fall back mid-way if a subsequent
chroma plane is unsupported.

**Why.** The luma (Y) plane contains the vast majority of image data and
requires substantial processing time. Speculatively running an optimized fast
path on the Y plane before checking chroma plane compatibility wastes
significant compute when unsupported chroma dimensions force a fallback to
reference or non-normative scalers.

## Examples

Bad:

```cpp
int scaled = 0;
for (int plane = 0; plane < num_planes; ++plane) {
  if (can_scale_plane(src, dst, plane)) {
    scale_plane_optimized(src, dst, plane);
    scaled = 1;
  }
}
if (!scaled) {
  fallback_scale_all_planes(src, dst);
}
```

Good:

```cpp
bool has_optimized = has_optimized_scaler(src->y_w, src->y_h, dst->y_w, dst->y_h);
if (num_planes > 1) {
  has_optimized = has_optimized &&
                  has_optimized_scaler(src->uv_w, src->uv_h, dst->uv_w, dst->uv_h);
}
if (has_optimized) {
  av1_resize_and_extend_frame(src, dst, filter, phase, num_planes);
} else {
  fallback_scale_all_planes(src, dst);
}
```
