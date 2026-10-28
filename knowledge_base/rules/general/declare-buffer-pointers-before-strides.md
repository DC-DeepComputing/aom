---
name: libaom/declare-buffer-pointers-before-strides
description: When declaring local variables or function parameters for image buffers and strides, declare each buffer pointer before its associated stride
rubric: readability
severity: nit
---

**Rule.** When declaring local variables, struct members, or function arguments
for image buffers and their strides, declare each buffer pointer before its
associated stride (e.g., src_buffer before src_stride, dst_buffer before
dst_stride). Maintain this (buffer, stride) pair ordering consistently across
consecutive buffer declarations.

**Why.** In libaom image, scaling, and DSP routines, functions and data
structures consistently take buffer pointers immediately followed by their
associated strides. Declaring local buffer and stride pairs in (buffer, stride)
order maintains consistency with standard API signatures and improves code
readability.

## Examples

Bad:

```cpp
const int src_stride = src->strides[is_uv];
const uint8_t *src_buffer = src->buffers[i];
const int dst_stride = dst->strides[is_uv];
uint8_t *dst_buffer = dst->buffers[i];
```

Good:

```cpp
const uint8_t *src_buffer = src->buffers[i];
const int src_stride = src->strides[is_uv];
uint8_t *dst_buffer = dst->buffers[i];
const int dst_stride = dst->strides[is_uv];
```
