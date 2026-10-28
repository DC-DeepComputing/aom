---
name: libaom/match-buffer-pointer-bit-depth-suffixes
description: When declaring buffer pointers or stride variables with bit-depth suffixes, ensure suffixes like dst8 vs dst16 match the underlying element type
rubric: readability
severity: should_fix
---

**Rule.** When declaring buffer pointers, strides, or function parameters with
width or bit-depth suffixes (such as src8/src16 or dst8/dst16), ensure the
suffix matches the pointed-to element type (e.g., uint8_t * vs. uint16_t * or
CONV_BUF_TYPE *). Do not use 8-bit suffixes on 16-bit or intermediate-depth
buffer variables, or vice versa.

**Why.** Mismatched bit-depth suffixes mislead maintainers about element sizes
during pointer arithmetic, pitch calculations, and SIMD vectorization,
increasing the risk of buffer overflow or incorrect stride stepping.

## Examples

Bad:

```cpp
CONV_BUF_TYPE *dst8 = conv_params->dst;
int dst8_stride = conv_params->dst_stride;
```

Good:

```cpp
CONV_BUF_TYPE *dst16 = conv_params->dst;
int dst16_stride = conv_params->dst_stride;
```
