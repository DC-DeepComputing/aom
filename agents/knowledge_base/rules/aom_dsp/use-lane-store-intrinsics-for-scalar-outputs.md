---
name: libaom/aom_dsp/use-lane-store-intrinsics-for-scalar-outputs
description: When writing 64-bit lanes of an SSE vector to separate scalar destination pointers, use direct lane store intrinsics (_mm_storel_pd / _mm_storeh_pd) instead of stack arrays
rubric: safety
severity: should_fix
---

**Rule.** When writing 64-bit lanes or scalar elements of an SSE vector (__m128d
or __m128i) into separate scalar destination pointers or variables, use direct
lane store intrinsics (such as _mm_storel_pd and _mm_storeh_pd for doubles, or
_mm_storel_epi64 / _mm_cvtsi128_si64 for integers). Do not buffer through an
intermediate stack array with _mm_storeu_pd or _mm_storeu_si128 followed by
scalar array indexing.

**Why.** Buffering vector registers through a temporary stack array introduces
unnecessary stack frame allocation, extra unaligned vector store operations, and
potential store-to-load forwarding stalls. Direct lane store intrinsics emit
single scalar or low/high quadword store instructions directly to target memory
locations without temporary array overhead.

## Examples

Bad:

```cpp
// Intermediate stack buffer with unaligned vector store
double result[2];
_mm_storeu_pd(result, reg_res);
*rate_f = result[0];
*distbysse_f = result[1];
```

Good:

```cpp
// Direct lane stores to scalar destination pointers
_mm_storel_pd(rate_f, reg_res);
_mm_storeh_pd(distbysse_f, reg_res);
```
