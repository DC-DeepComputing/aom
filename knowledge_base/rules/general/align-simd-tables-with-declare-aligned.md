---
name: libaom/align-simd-tables-with-declare-aligned
description: When declaring static lookup tables or arrays accessed by SIMD instructions or cast to wider integer pointers, use DECLARE_ALIGNED to ensure proper memory alignment
rubric: correctness
severity: should_fix
---

**Rule.** Declare static scan, filter, mask, and constant lookup tables (as well
as narrower integer arrays like uint8_t[] or stack buffers) with
DECLARE_ALIGNED(16, ...) or DECLARE_ALIGNED(32, ...) from aom_ports/mem.h
whenever they are accessed by SIMD/assembly vector loads or cast to wider
integer or vector pointers (such as uint32_t *, uint64_t *, or __m128i *).
Alternatively, use dedicated unaligned load helper functions when alignment
cannot be guaranteed.

**Why.** Assembly routines and SIMD intrinsics (such as SSE2/AVX2/NEON vector
loads like MOVDQA or _mm_load_si128) as well as multi-byte pointer dereferences
require strict address alignment. Accessing unaligned data triggers hardware
alignment faults, undefined behavior, or segmentation faults on platforms with
strict alignment enforcement.

## Examples

Bad:

```cpp
// Bad: Static scan table accessed by SIMD vector loads lacks alignment
static const int16_t default_scan_8x8_transpose[64] = {
  0, 8, 1, 2, 9, 16, 24, 17,
  // ...
};

// Bad: Unaligned byte array cast to wider 32-bit integer pointer
static const uint8_t smooth_weights[] = { 255, 149, 85, 43, 0 };
uint32_t read_weights(int idx) {
  return ((const uint32_t *)smooth_weights)[idx];
}
```

Good:

```cpp
// Good: Table aligned with DECLARE_ALIGNED for SIMD vector load instructions
DECLARE_ALIGNED(16, static const int16_t, default_scan_8x8_transpose[64]) = {
  0, 8, 1, 2, 9, 16, 24, 17,
  // ...
};

// Good: Byte array aligned to satisfy 32-bit (or SIMD) pointer read requirements
DECLARE_ALIGNED(4, static const uint8_t, smooth_weights[]) = {
  255, 149, 85, 43, 0
};
uint32_t read_weights(int idx) {
  return ((const uint32_t *)smooth_weights)[idx];
}
```
