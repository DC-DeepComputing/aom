---
name: libaom/av1/extend-frame-borders-after-resizing
description: When implementing or modifying frame resizing routines that extend buffers (such as *_resize_and_extend_frame*), invoke aom_extend_frame_borders() on the destination buffer after scaling all planes
rubric: correctness
severity: should_fix
---

**Rule.** In frame scaling and resizing routines contractually designated to
extend frame borders (such as `av1_resize_and_extend_frame_*` across C and SIMD
backends), ensure `aom_extend_frame_borders()` is invoked on the destination
`YV12_BUFFER_CONFIG` buffer after all image planes have been resized.

**Why.** Reference frames and scaled intermediate buffers require extended
padding borders for motion compensation, boundary pixel clamping, and
interpolation filters. Omitting the border extension call leaves boundary
padding uninitialized or stale, resulting in out-of-bounds reads or visual
artifacts during subsequent motion prediction.

## Examples

Bad:

```cpp
void av1_resize_and_extend_frame_c(const YV12_BUFFER_CONFIG *src,
                                   YV12_BUFFER_CONFIG *dst,
                                   const InterpFilter filter,
                                   const int phase_scaler,
                                   const int num_planes) {
  for (int i = 0; i < AOMMIN(num_planes, MAX_MB_PLANE); ++i) {
    av1_resize_plane(src->buffers[i], src->strides[i], ...);
  }
  // Missing border extension on dst buffer
}
```

Good:

```cpp
void av1_resize_and_extend_frame_c(const YV12_BUFFER_CONFIG *src,
                                   YV12_BUFFER_CONFIG *dst,
                                   const InterpFilter filter,
                                   const int phase_scaler,
                                   const int num_planes) {
  for (int i = 0; i < AOMMIN(num_planes, MAX_MB_PLANE); ++i) {
    av1_resize_plane(src->buffers[i], src->strides[i], ...);
  }
  aom_extend_frame_borders(dst, num_planes);
}
```
