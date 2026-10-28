---
name: libaom/av1/group-yv12-buffer-and-crop-dimensions
description: When assigning or copying dimension fields of YV12_BUFFER_CONFIG, group all aligned buffer dimensions together before assigning crop dimensions rather than interleaving them plane-by-plane
rubric: readability
severity: nit
---

**Rule.** When copying, setting, or updating dimension fields on a
YV12_BUFFER_CONFIG structure, group all full and aligned buffer dimensions
(y_width, y_height, uv_width, uv_height) contiguously together before assigning
display or crop dimensions (y_crop_width, y_crop_height, uv_crop_width,
uv_crop_height). Do not interleave aligned and crop assignments plane-by-plane.

**Why.** Grouping aligned buffer dimensions separately from crop dimensions
maintains clear logical separation between physical memory buffer layout
geometry and display/crop geometry. This improves readability and makes
dimension updates easier to verify and audit against out-of-order assignments.

## Examples

Bad:

```cpp
// Bad: Interleaving aligned buffer and crop dimensions plane by plane
buf->img.y_width = src->y_width;
buf->img.y_height = src->y_height;
buf->img.y_crop_width = src->y_crop_width;
buf->img.y_crop_height = src->y_crop_height;
buf->img.uv_width = src->uv_width;
buf->img.uv_height = src->uv_height;
buf->img.uv_crop_width = src->uv_crop_width;
buf->img.uv_crop_height = src->uv_crop_height;
```

Good:

```cpp
// Good: Group all aligned dimensions together, followed by all crop dimensions
buf->img.y_width = src->y_width;
buf->img.y_height = src->y_height;
buf->img.uv_width = src->uv_width;
buf->img.uv_height = src->uv_height;
buf->img.y_crop_width = src->y_crop_width;
buf->img.y_crop_height = src->y_crop_height;
buf->img.uv_crop_width = src->uv_crop_width;
buf->img.uv_crop_height = src->uv_crop_height;
```
