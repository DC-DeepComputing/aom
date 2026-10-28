---
name: libaom/round-up-subsampled-plane-dimensions
description: When computing plane dimensions or contiguous buffer offsets with chroma subsampling, round up dimensions with shift offsets and accumulate plane offsets sequentially
rubric: correctness
severity: should_fix
---

**Rule.** When computing plane dimensions for an aom_image_t, use
aom_img_plane_width(img, plane) and aom_img_plane_height(img, plane). When
calculating subsampled chroma plane dimensions or strides from raw dimensions,
round up using (dim + subsampling) >> subsampling. When laying out contiguous
multi-plane buffers with chroma subsampling, accumulate plane sizes sequentially
(such as plane_offset += plane_height * plane_stride) instead of multiplying
plane index by current plane dimensions.

**Why.** Chroma subsampling (4:2:0, 4:2:2) scales plane dimensions
non-uniformly. Truncating odd dimensions with bare right-shifts (dim >>
subsampling) produces undersized allocations. Multiplying plane index by current
plane dimensions assumes uniform plane sizes, causing subsampled chroma plane
offsets to overlap preceding luma buffer memory and trigger out-of-bounds
accesses.

## Examples

Bad:

```cpp
// Truncating raw subsampled dimensions without rounding:
int plane_height = plane ? height >> subsampling_y : height;
int plane_stride = plane ? stride >> subsampling_x : stride;

// Multiplying plane index by current plane dimensions overlaps buffers:
frame_to_filter->buffers[plane] =
    frame_to_filter->buffer_alloc + plane * plane_stride * plane_height;

// Manual aom_image_t plane dimension calculation:
const int w = plane == 0 ? (int)img->d_w : (int)((img->d_w + 1) / 2);
```

Good:

```cpp
// Rounding up raw subsampled dimensions and accumulating plane offsets:
int plane_offset = 0;
for (int plane = AOM_PLANE_Y; plane < num_planes; plane++) {
  int plane_height =
      plane ? (height + subsampling_y) >> subsampling_y : height;
  int plane_stride =
      plane ? (stride + subsampling_x) >> subsampling_x : stride;
  frame_to_filter->buffers[plane] =
      frame_to_filter->buffer_alloc + plane_offset;
  plane_offset += plane_height * plane_stride;
}

// Using dedicated aom_image_t helpers:
const int w = aom_img_plane_width(img, plane);
```
