---
name: libaom/merge-consecutive-loops-over-same-range
description: When consecutive loops iterate over the same range, collection, or equal-sized independent buffers, merge them into a single loop to eliminate overhead and interleave operations
rubric: safety
severity: should_fix
---

**Rule.** When multiple consecutive loops iterate over identical bounds, ranges,
or collections without cross-element data dependencies, merge them into a single
loop pass. Avoid separate sequential passes that re-iterate over the same
collection or redundantly incur loop branch and counter overhead. Additionally,
in SIMD and performance-sensitive routines processing independent equal-sized
input buffers (such as 'above' and 'left' intra predictor boundary arrays),
combine the sequential vector loops into a single loop and interleave the vector
loads and arithmetic operations (e.g., vld1q_*, vpadalq_*, _mm_*).

**Why.** Fusing consecutive loops over the same range or equal-sized independent
buffers eliminates redundant loop counter and branch overhead, improves cache
locality by processing elements while still warm in cache, and enables
instruction-level parallelism (ILP) and vector load pipelining in SIMD execution
units.

## Examples

Bad:

```cpp
// Variant 1: Redundant consecutive passes over the same collection
for (int j = 0; j < cdef_info->nb_cdef_strengths; j++) {
  cdef_info->cdef_strengths[j] = new_pri_luma * CDEF_SEC_STRENGTHS + new_sec_luma;
}
if (should_zero_cdef_strengths) {
  for (int j = 0; j < cdef_info->nb_cdef_strengths; j++) {
    const int pri_luma = cdef_info->cdef_strengths[j] / CDEF_SEC_STRENGTHS;
    if (pri_luma <= 4) cdef_info->cdef_strengths[j] = 0;
  }
}

// Variant 2: Separate sequential loops over independent equal-sized buffers
for (int i = 0; i < bw; i += 8) {
  sum_q = vpadalq_u16(sum_q, vld1q_u16(above));
  above += 8;
}
for (int i = 0; bh < 8; i += 8) {
  sum_q = vpadalq_u16(sum_q, vld1q_u16(left));
  left += 8;
}
```

Good:

```cpp
// Variant 1: Merged single pass over collection
for (int j = 0; j < cdef_info->nb_cdef_strengths; j++) {
  cdef_info->cdef_strengths[j] = new_pri_luma * CDEF_SEC_STRENGTHS + new_sec_luma;
  if (should_zero_cdef_strengths && new_pri_luma <= 4) {
    cdef_info->cdef_strengths[j] = 0;
  }
}

// Variant 2: Combined SIMD loop interleaving vector loads
for (int i = 0; i < bw; i += 8) {
  sum_q = vpadalq_u16(sum_q, vld1q_u16(above));
  sum_q = vpadalq_u16(sum_q, vld1q_u16(left));
  above += 8;
  left += 8;
}
```
