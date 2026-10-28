---
name: libaom/vectorize-simd-block-copies-not-memcpy
description: When implementing direct block copy or zero-motion prediction paths in architecture-specific SIMD routines, use vectorized load and store loops instead of row-by-row memcpy calls
rubric: safety
severity: should_fix
---

**Rule.** In architecture-specific SIMD routines (such as ARM NEON or x86
SSE/AVX2), implement direct 2D block copy and zero-motion prediction paths using
vectorized SIMD load and store loops (e.g., vld1q_*/vst1q_* or
_mm_loadu_*/_mm_storeu_*). Do not use row-by-row memcpy() calls in a loop.

**Why.** Calling memcpy() inside per-row loops incurs function call overhead and
defeats the throughput benefits of architecture-specific SIMD implementations.
Vectorizing 2D block copies directly with SIMD load and store intrinsics
maximizes memory bandwidth utilization and maintains consistency across standard
and high-bitdepth pipelines.

## Examples

Bad:

```cpp
if (!subpel_x_q3 && !subpel_y_q3) {
  const uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
  for (int i = 0; i < height; i++) {
    memcpy(comp_pred, ref, width * sizeof(*comp_pred));
    comp_pred += width;
    ref += ref_stride;
  }
}
```

Good:

```cpp
if (!subpel_x_q3 && !subpel_y_q3) {
  const uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
  if (width > 4) {
    int i = height;
    do {
      int j = 0;
      do {
        uint16x8_t r = vld1q_u16(ref + j);
        vst1q_u16(comp_pred + j, r);
        j += 8;
      } while (j < width);
      ref += ref_stride;
      comp_pred += width;
    } while (--i != 0);
  }
}
```
