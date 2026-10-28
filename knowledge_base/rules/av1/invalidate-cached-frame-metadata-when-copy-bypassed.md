---
name: libaom/av1/invalidate-cached-frame-metadata-when-copy-bypassed
description: When conditionally caching or updating frame buffers across GOPs or frames, explicitly invalidate associated metadata sentinels whenever the buffer copy is bypassed
rubric: correctness
severity: should_fix
---

**Rule.** When conditionally caching, copying, or updating reference frame
buffers across GOPs or encoding passes, explicitly invalidate associated
metadata sentinels (such as resetting display order hints to -1) whenever the
copy or update condition is not met (e.g., during frame resizing). Do not leave
stale metadata intact when skipping the underlying buffer update.

**Why.** If a cached frame buffer is not updated because preconditions like
matching dimensions fail, leaving the previous metadata sentinel intact causes
downstream encoding logic in subsequent GOPs to treat stale buffer contents as
valid, resulting in corrupted reference state and rate-distortion mismatches.

## Examples

Bad:

```cpp
// If source resolution changed, prev_gop_arf_disp_order retains stale metadata
if (cpi->source->y_width == tpl_data->prev_gop_arf_src.y_width &&
    cpi->source->y_height == tpl_data->prev_gop_arf_src.y_height) {
  aom_yv12_copy_frame(cpi->source, &tpl_data->prev_gop_arf_src, num_planes);
  tpl_data->prev_gop_arf_disp_order = current_frame->display_order_hint;
}
```

Good:

```cpp
// Invalidate sentinel before conditional update so stale buffer is not used
tpl_data->prev_gop_arf_disp_order = -1;
if (cpi->source->y_width == tpl_data->prev_gop_arf_src.y_width &&
    cpi->source->y_height == tpl_data->prev_gop_arf_src.y_height) {
  aom_yv12_copy_frame(cpi->source, &tpl_data->prev_gop_arf_src, num_planes);
  tpl_data->prev_gop_arf_disp_order = current_frame->display_order_hint;
}
```
