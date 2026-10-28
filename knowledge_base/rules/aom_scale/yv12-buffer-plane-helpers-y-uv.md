---
name: libaom/aom_scale/yv12-buffer-plane-helpers-y-uv
description: When declaring helper functions, parameters, or doc comments for YV12_BUFFER_CONFIG planes, use 'y' and 'uv' naming instead of 'luma' and 'chroma'
rubric: readability
severity: nit
---

**Rule.** When naming helper functions, parameters, local variables, or
documentation comments that operate on YV12_BUFFER_CONFIG planes (such as
stride, width, height, or buffer calculations), use 'y' and 'uv' prefixes (e.g.,
aom_calc_y_stride, y_stride, uv_stride) rather than 'luma' and 'chroma'.
Maintain naming consistency with the y_* and uv_* fields defined in
YV12_BUFFER_CONFIG.

**Why.** YV12_BUFFER_CONFIG consistently defines its plane dimensions, strides,
and buffer pointers using y_ and uv_ prefixes (such as y_width, uv_width,
y_stride, uv_stride, y_buffer). Using 'luma' and 'chroma' in helper functions or
comments creates naming inconsistency across the codebase and struct accessors.

## Examples

Bad:

```cpp
/*!
 * Calculates the stride value for an image from aligned width and border.
 * Returns the luma stride value.
 */
static AOM_INLINE int aom_calc_luma_stride(int aligned_width, int border) {
  return aligned_width + (border * 2);
}
```

Good:

```cpp
/*!
 * Calculates the stride value for an image from aligned width and border.
 * Returns the y stride value.
 */
static AOM_INLINE int aom_calc_y_stride(int aligned_width, int border) {
  return aligned_width + (border * 2);
}
```
