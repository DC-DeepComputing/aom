---
name: libaom/avoid-static-lookup-tables-for-formulaic-simd-masks
description: When generating formulaic SIMD masks or boundary vectors, compute them dynamically using vector comparisons or arithmetic intrinsics instead of static lookup tables
rubric: safety
severity: should_fix
---

**Rule.** In SIMD implementations, avoid declaring static lookup tables for
formulaic masks, padding vectors, or sequential index patterns. Compute them
dynamically using vector comparisons, broadcasts, or arithmetic intrinsics (such
as vcleq_u8 with vdupq_n_u8 in NEON or _mm_cmplt_epi8 in SSE/AVX) to minimize
compiled library binary size.

**Why.** Large static lookup tables accumulate across SIMD specializations and
bloat the .rodata section and library binary size. Computing formulaic masks
on-the-fly using vector instructions avoids binary bloat while executing with
negligible performance overhead.

## Examples

Bad:

```cpp
// Bad: Declaring large static tables for formulaic mask sequences bloats binary size.
static const uint8_t warp_pad_left_neon[14][16] = {
  { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  // ...
};

uint8x16_t mask_val = vld1q_u8(warp_pad_left_neon[out_of_boundary_left]);
src_1 = vbslq_u8(mask_val, vec_dup, src_1);
```

Good:

```cpp
// Good: Dynamically compute formulaic masks using vector comparison intrinsics.
const uint8x16_t indx_vec = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
uint8x16_t cmp_vec = vdupq_n_u8(out_of_boundary_left);
uint8x16_t mask_val = vcleq_u8(indx_vec, cmp_vec);
src_1 = vbslq_u8(mask_val, vec_dup, src_1);
```
