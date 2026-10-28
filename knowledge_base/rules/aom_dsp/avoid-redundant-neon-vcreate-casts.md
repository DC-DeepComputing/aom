---
name: libaom/aom_dsp/avoid-redundant-neon-vcreate-casts
description: When passing scalar arguments to ARM NEON vector creation intrinsics like vcreate_s64, avoid redundant casts and pass uint64_t values directly
rubric: readability
severity: nit
---

**Rule.** When initializing ARM NEON vectors with scalar creation intrinsics
such as vcreate_s64, vcreate_u64, or other vcreate_* variants, pass existing
uint64_t arguments directly without redundant type casts (such as (int64_t)).
Verify intrinsic parameter types against ARM NEON specifications rather than
inferring argument types from the vector return type.

**Why.** In the ARM NEON intrinsics specification, all vcreate_* intrinsics take
a uint64_t scalar argument regardless of whether they return signed (int64x1_t),
unsigned (uint64x1_t), or floating-point vector types. Adding explicit casts
like (int64_t) when passing a uint64_t value is redundant, clutters code, and
introduces unnecessary type conversions.

## Examples

Bad:

```cpp
// Redundant cast: vcreate_s64 takes uint64_t, not int64_t
SIMD_INLINE v128 v128_from_64(uint64_t a, uint64_t b) {
  return vcombine_s64(vcreate_s64((int64_t)b), vcreate_s64((int64_t)a));
}
```

Good:

```cpp
// Pass uint64_t arguments directly to vcreate_s64 without casting
SIMD_INLINE v128 v128_from_64(uint64_t a, uint64_t b) {
  return vcombine_s64(vcreate_s64(b), vcreate_s64(a));
}
```
