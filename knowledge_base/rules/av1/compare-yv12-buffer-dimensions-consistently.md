---
name: libaom/av1/compare-yv12-buffer-dimensions-consistently
description: When comparing frame dimensions in YV12_BUFFER_CONFIG, compare matching dimension metrics consistently (crop against crop, or aligned against aligned) rather than mixing them
rubric: correctness
severity: should_fix
---

**Rule.** When checking whether frame dimensions have changed, grown, or matched
in YV12_BUFFER_CONFIG structures, compare matching dimension metrics
consistently. Compare unpadded crop dimensions against crop dimensions
(y_crop_width, y_crop_height, uv_crop_width, uv_crop_height) and aligned buffer
dimensions against aligned dimensions (y_width, y_height, uv_width, uv_height),
rather than mixing crop and aligned fields.

**Why.** In YV12_BUFFER_CONFIG, y_width and y_height represent aligned buffer
allocation bounds (typically aligned to multiples of 8 or 16), whereas
y_crop_width and y_crop_height store unpadded display or coded dimensions.
Comparing unpadded crop dimensions against aligned dimensions produces
inconsistent size comparisons, which can trigger spurious reallocations or fail
to detect dimension changes.

## Examples

Bad:

```cpp
const int width = src->y_crop_width;
const int height = src->y_crop_height;
const int larger_dimensions =
    width > buf->img.y_width || height > buf->img.y_height;
```

Good:

```cpp
const int width = src->y_crop_width;
const int height = src->y_crop_height;
const int larger_dimensions =
    width > buf->img.y_crop_width || height > buf->img.y_crop_height;
```
