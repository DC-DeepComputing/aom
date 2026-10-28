---
name: libaom/derive-buffer-allocation-size-from-stride
description: When allocating 2D image buffers requiring a row stride, compute the stride first and derive the buffer allocation size as height * stride
rubric: readability
severity: nit
---

**Rule.** When allocating 2D image or pixel buffers where a row stride is also
needed, declare and compute the row stride first and derive the buffer
allocation size as `buffer_size = (size_t)height * stride`. Do not independently
compute `width * height * bytes_per_pixel` for the allocation size.

**Why.** Deriving allocation size directly from the row stride avoids redundant
calculations, ensures the allocated buffer strictly matches the row stride
passed to downstream functions, and prevents buffer size discrepancies if row
padding or alignment is introduced.

## Examples

Bad:

```cpp
// Redundant width multiplication and stride declared after allocation
const size_t buffer_size = width * height * 4;
uint8_t *src_argb = (uint8_t *)aom_malloc(buffer_size);
uint8_t *distorted_argb = (uint8_t *)aom_malloc(buffer_size);
const int stride_argb = width * 4;
```

Good:

```cpp
// Stride computed first and reused to derive total buffer allocation size
const int stride_argb = width * 4;
const size_t buffer_size = (size_t)height * stride_argb;
uint8_t *src_argb = (uint8_t *)aom_malloc(buffer_size);
uint8_t *distorted_argb = (uint8_t *)aom_malloc(buffer_size);
```
