---
name: libaom/aom/nv12-chroma-shift-and-interleaved-plane-width
description: When handling semi-planar 4:2:0 formats like AOM_IMG_FMT_NV12, preserve x_chroma_shift as 1 and double plane 1 width or stride for interleaved UV data
rubric: correctness
severity: should_fix
---

**Rule.** Maintain standard 4:2:0 chroma shift parameters (x_chroma_shift = 1,
y_chroma_shift = 1) for semi-planar formats such as AOM_IMG_FMT_NV12 in
aom_image_t rather than setting horizontal shift to zero. When allocating,
reading, or writing interleaved UV chroma plane data (plane 1), explicitly
double the row width (w *= 2) and stride, and terminate plane iteration before
plane 2.

**Why.** NV12 represents 4:2:0 subsampled chroma where U and V samples are
interleaved into a single buffer (plane 1). Artificially zeroing x_chroma_shift
distorts logical image plane dimension queries (such as aom_img_plane_width()),
whereas keeping correct subsampling metadata and adjusting byte widths and
strides during raw plane I/O ensures consistency across image descriptors.

## Examples

Bad:

```cpp
// Incorrectly clearing xcs to fit packed chroma byte width
img->x_chroma_shift = (fmt == AOM_IMG_FMT_NV12) ? 0 : 1;

// Reading raw NV12 frame assuming unadjusted plane width
if (yuv_frame->fmt == AOM_IMG_FMT_NV12 && plane == 1) {
  w = (w + 1) & ~1;
}
```

Good:

```cpp
// Retaining standard 4:2:0 chroma shift for NV12
img->x_chroma_shift = 1;
img->y_chroma_shift = 1;

// Doubling width for interleaved UV data in plane 1
if (yuv_frame->fmt == AOM_IMG_FMT_NV12 && plane > 1) break;
if (yuv_frame->fmt == AOM_IMG_FMT_NV12 && plane == 1) w *= 2;
```
