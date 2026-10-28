---
name: libaom/av1/parameterize-frame-buffer-helpers-by-plane
description: When implementing frame-buffer processing or restoration helpers on YV12_BUFFER_CONFIG, accept a plane index parameter and handle all color planes
rubric: correctness
severity: should_fix
---

**Rule.** When implementing frame-buffer restoration, filtering, or processing
helper functions on frame buffers (such as `YV12_BUFFER_CONFIG`), accept an
explicit `plane` index parameter (e.g., `AOM_PLANE_Y`, `AOM_PLANE_U`,
`AOM_PLANE_V`) and support all color planes rather than hardcoding operations
solely to the luma plane. Ensure dimensions, strides, and buffer pointers
(`y_buffer`/`y_crop_width`/`y_stride` vs.
`u_buffer`/`v_buffer`/`uv_crop_width`/`uv_stride`) are properly dispatched
according to the selected plane.

**Why.** Hardcoding buffer processing functions to the luma plane prevents
chroma plane processing and misleads callers into assuming the helper can
restore or filter full frame buffers. Explicitly accepting a plane parameter
enables caller-directed plane selection and ensures proper handling of chroma
subsampling dimensions and strides.

## Examples

Bad:

```cpp
void av1_restore_cnn_buffer(AV1_COMMON *cm, const CNN_CONFIG *cnn_config) {
  YV12_BUFFER_CONFIG *buf = &cm->cur_frame->buf;
  // Hardcoded to only process the luma (Y) plane.
  av1_restore_cnn(buf->y_buffer, buf->y_crop_width, buf->y_crop_height,
                  buf->y_stride, cnn_config);
}
```

Good:

```cpp
void av1_restore_cnn_plane(AV1_COMMON *cm, const CNN_CONFIG *cnn_config,
                           int plane) {
  YV12_BUFFER_CONFIG *buf = &cm->cur_frame->buf;
  switch (plane) {
    case AOM_PLANE_Y:
      av1_restore_cnn(buf->y_buffer, buf->y_crop_width, buf->y_crop_height,
                      buf->y_stride, cnn_config);
      break;
    case AOM_PLANE_U:
      av1_restore_cnn(buf->u_buffer, buf->uv_crop_width, buf->uv_crop_height,
                      buf->uv_stride, cnn_config);
      break;
    case AOM_PLANE_V:
      av1_restore_cnn(buf->v_buffer, buf->uv_crop_width, buf->uv_crop_height,
                      buf->uv_stride, cnn_config);
      break;
    default: assert(0 && "Invalid plane index");
  }
}
```
