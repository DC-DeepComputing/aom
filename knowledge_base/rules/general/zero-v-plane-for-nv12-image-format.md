---
name: libaom/zero-v-plane-for-nv12-image-format
description: When configuring or allocating an aom_image_t with AOM_IMG_FMT_NV12 format, set planes[AOM_PLANE_V] to NULL and stride[AOM_PLANE_V] to 0
rubric: correctness
severity: should_fix
---

**Rule.** When configuring, populating, or allocating an aom_image_t struct with
format AOM_IMG_FMT_NV12, ensure planes[AOM_PLANE_V] is set to NULL (or nullptr)
and stride[AOM_PLANE_V] is set to 0. Do not assign separate chroma plane
pointers or non-zero strides to the V plane for NV12 images, as interleaved UV
data is accessed entirely through AOM_PLANE_U.

**Why.** In NV12 format, UV chroma samples are interleaved into a single
continuous buffer starting at planes[AOM_PLANE_U]. The aom_image_t specification
and internal codec invariants require planes[AOM_PLANE_V] to be NULL and
stride[AOM_PLANE_V] to be 0; assigning a non-zero stride or pointer to the V
plane violates struct invariants and causes incorrect buffer handling or
validation errors.

## Examples

Bad:

```cpp
img.planes[AOM_PLANE_U] = uv_buf;
img.planes[AOM_PLANE_V] = uv_buf + 1;
img.stride[AOM_PLANE_U] = uv_stride;
img.stride[AOM_PLANE_V] = uv_stride;
```

Good:

```cpp
img.planes[AOM_PLANE_U] = uv_buf;
img.planes[AOM_PLANE_V] = (img.fmt == AOM_IMG_FMT_NV12) ? NULL : v_buf;
img.stride[AOM_PLANE_U] = uv_stride;
img.stride[AOM_PLANE_V] = (img.fmt == AOM_IMG_FMT_NV12) ? 0 : v_stride;
```
