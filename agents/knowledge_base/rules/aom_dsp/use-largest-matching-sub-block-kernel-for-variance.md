---
name: libaom/aom_dsp/use-largest-matching-sub-block-kernel-for-variance
description: When decomposing block variance or SSE calculations with sub-block helper kernels, choose the largest matching sub-block kernel dividing both dimensions
rubric: safety
severity: should_fix
---

**Rule.** When implementing or decomposing block variance, SSE, or subpel
variance functions across larger blocks using sub-block helper kernels (such as
variance_sse2, variance_avx2, or NEON variants), select the largest sub-block
kernel and step size that evenly divides both the block width and height (e.g.,
use a 16x16 sub-block kernel with step size 16 rather than an 8x8 kernel when
width and height are multiples of 16).

**Why.** Using the largest matching sub-block kernel minimizes loop iteration
overhead and helper function invocations while maximizing SIMD vector width
utilization and throughput across the block.

## Examples

Bad:

```cpp
// In a 64x16 variance function, decomposing into 8x8 sub-blocks adds unnecessary loop overhead
variance_sse2(src, src_stride, ref, ref_stride, 64, 16, sse, &sum,
              aom_get8x8var_sse2, 8);
```

Good:

```cpp
// Use the largest sub-block kernel (16x16) that evenly divides both 64 and 16
variance_sse2(src, src_stride, ref, ref_stride, 64, 16, sse, &sum,
              aom_get16x16var_sse2, 16);
```
