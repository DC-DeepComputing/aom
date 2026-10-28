---
name: libaom/apps/apps-validate-planar-format-before-planar-scaling
description: When scaling image buffers with planar scaling routines like libyuv's I420Scale, verify planar input formats and reject semi-planar NV12 buffers
rubric: correctness
severity: should_fix
---

**Rule.** Do not pass semi-planar image formats (such as NV12 with interleaved
UV planes) directly to planar scaling routines like libyuv's I420Scale or
I420Scale_16, which require separate planar U and V chroma buffers. Validate or
restrict input format checks to fully planar formats (such as AOM_IMG_FMT_I420
or AOM_IMG_FMT_YV12) before calling planar scaling functions.

**Why.** I420Scale and planar scaling routines access separate planar U and V
chroma buffers with distinct plane strides. Semi-planar formats like NV12 store
interleaved UV chroma samples in a single plane, so treating NV12 plane pointers
as independent U and V planar buffers corrupts chroma processing and causes
out-of-bounds memory accesses.

## Examples

Bad:

```cpp
if (img->fmt != AOM_IMG_FMT_I420 && img->fmt != AOM_IMG_FMT_YV12 &&
    img->fmt != AOM_IMG_FMT_NV12) {
  fprintf(stderr, "%s can only scale 4:2:0 8bpp inputs\n", exec_name);
  exit(EXIT_FAILURE);
}
I420Scale(img->planes[AOM_PLANE_Y], img->stride[AOM_PLANE_Y],
          img->planes[AOM_PLANE_U], img->stride[AOM_PLANE_U],
          img->planes[AOM_PLANE_V], img->stride[AOM_PLANE_V], ...);
```

Good:

```cpp
if (img->fmt != AOM_IMG_FMT_I420 && img->fmt != AOM_IMG_FMT_YV12) {
  fprintf(stderr, "%s can only scale 4:2:0 8bpp inputs\n", exec_name);
  exit(EXIT_FAILURE);
}
I420Scale(img->planes[AOM_PLANE_Y], img->stride[AOM_PLANE_Y],
          img->planes[AOM_PLANE_U], img->stride[AOM_PLANE_U],
          img->planes[AOM_PLANE_V], img->stride[AOM_PLANE_V], ...);
```
