---
name: libaom/av1/avoid-redundant-highbd-pointer-conversions
description: When initializing high-bitdepth pixel pointers from raw memory buffers, cast directly to uint16_t* instead of round-tripping through CONVERT_TO_BYTEPTR and CONVERT_TO_SHORTPTR
rubric: readability
severity: nit
---

**Rule.** Do not round-trip a raw allocated memory buffer through
CONVERT_TO_BYTEPTR() and CONVERT_TO_SHORTPTR() when preparing a 16-bit
high-bitdepth buffer. Cast the raw unshifted buffer directly with (uint16_t *)
before wrapping the resulting pointer with CONVERT_TO_BYTEPTR() for downstream
interfaces.

**Why.** CONVERT_TO_BYTEPTR() and CONVERT_TO_SHORTPTR() pack and unpack 16-bit
pointers by bit-shifting their addresses for generic 8-bit pointer interfaces.
Applying both macros sequentially on a freshly allocated raw buffer is a
redundant identity round-trip that obscures code intent; raw buffer addresses
are unshifted and only require a direct pointer cast.

## Examples

Bad:

```cpp
// Redundant round-trip conversion on raw unshifted buffer
uint8_t *tmp_pred = CONVERT_TO_BYTEPTR(pred_buffer);
uint16_t *pred_buffer_16 = CONVERT_TO_SHORTPTR(tmp_pred);
dst_buffer = CONVERT_TO_BYTEPTR(pred_buffer_16);
```

Good:

```cpp
// Cast raw buffer directly to uint16_t *
uint16_t *pred_buffer_16 = (uint16_t *)pred_buffer;
dst_buffer = CONVERT_TO_BYTEPTR(pred_buffer_16);
```
