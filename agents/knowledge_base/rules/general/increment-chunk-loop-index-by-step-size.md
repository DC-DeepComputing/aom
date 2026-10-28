---
name: libaom/increment-chunk-loop-index-by-step-size
description: When iterating over buffers or rows in fixed-size SIMD or block chunks, increment loop counters by chunk size rather than unit increments
rubric: readability
severity: should_fix
---

**Rule.** When iterating over buffer rows, blocks, or arrays in fixed-size SIMD
vectors or sub-block chunks (e.g., 8, 16, 32, or 64 elements, or constants like
STEP/CHUNK), step the loop index variable directly by the chunk size (for (int x
= 0; x < width; x += CHUNK)) rather than stepping by one with a divided bound
(for (int x = 0; x < width / CHUNK; ++x)). Use direct pointer offsets (src + x)
instead of multiplying or shifting by chunk size (src + x * CHUNK) inside the
loop body.

**Why.** Stepping the loop counter directly by the chunk size allows the index
to serve as an immediate element or byte offset (ptr + x). This eliminates
redundant multiplication or bit-shift arithmetic in inner SIMD loop bodies,
avoids division operations in the loop termination condition, and adheres to
libaom's standard SIMD loop indexing conventions.

## Examples

Bad:

```cpp
// Anti-pattern: Stepping by 1 with width division and repeated multiplications.
for (int x = 0; x < width / 32; x++) {
  const __m256i s0 = _mm256_loadu_si256((const __m256i *)(src0 + x * 32));
  const __m256i s1 = _mm256_loadu_si256((const __m256i *)(src1 + x * 32));
  for (int j = 0; j < 2; j++) {
    const __m128i h0 =
        _mm_loadu_si128((const __m128i *)(src0 + x * 32 + j * 16));
  }
}
```

Good:

```cpp
// Recommended: Stepping directly by vector chunk size with direct offsets.
for (int x = 0; x < width; x += 32) {
  const __m256i s0 = _mm256_loadu_si256((const __m256i *)(src0 + x));
  const __m256i s1 = _mm256_loadu_si256((const __m256i *)(src1 + x));
  for (int j = 0; j < 2; j++) {
    const __m128i h0 =
        _mm_loadu_si128((const __m128i *)(src0 + x + j * 16));
  }
}
```
