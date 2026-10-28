---
name: libaom/av1/assume-minimum-block-width-4-in-cdef-simd
description: In AV1 CDEF SIMD routines, leverage the minimum 4x4 block invariant by asserting width == 4 on small-block fallbacks and using fixed-size vector operations instead of scalar loops
rubric: safety
severity: should_fix
---

**Rule.** In AV1 CDEF SIMD and DSP routines, leverage the specification
invariant that block dimensions are at least 4 pixels (8x8 blocks subsampled
down to at most 4:2:0 chroma). On small-block fallback paths where larger vector
widths (such as 8 or 16) do not apply, assert that the remaining width equals 4
(e.g., `assert(width == 4);` or `assert(width - w == 4);`) and process with
fixed-size vector intrinsics (such as 64-bit `uint16x4_t` loads/stores) or
constant loop bounds rather than generic scalar fallback loops.

**Why.** Per the AV1 specification, CDEF filters 8x8 blocks, which scale down to
a minimum dimension of 4x4 for 4:2:0 subsampled chroma; dimensions smaller than
4 are invalid. Asserting `width == 4` and using fixed-size vector operations
eliminates variable-length scalar loop overhead and enables compiler inlining
and vectorization across small block dimensions.

## Examples

Bad:

```cpp
} else {
  // Scalar fallback assuming arbitrary remaining widths
  for (int w = 0; w < width; w++) {
    dst_ptr[w] = src_ptr[w];
  }
}
```

Good:

```cpp
} else {
  // In CDEF, chroma subsampling allows at minimum a 4x4 block
  assert(width == 4);
  uint16x4_t row = vld1_u16(src_ptr);
  vst1_u16(dst_ptr, row);
}
```
