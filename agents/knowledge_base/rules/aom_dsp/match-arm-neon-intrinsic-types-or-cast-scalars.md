---
name: libaom/aom_dsp/match-arm-neon-intrinsic-types-or-cast-scalars
description: When passing or extracting ARM NEON vectors with mismatched signedness or types, use vreinterpret_*, matching intrinsics, or explicit scalar casts
rubric: correctness
severity: should_fix
---

**Rule.** When passing ARM NEON vector expressions to functions, helper macros,
or intrinsics expecting a different signedness or type (such as passing
uint64x1_t to an int64x1_t/v64 parameter), explicitly convert the vector using
vreinterpret_* or use signedness-matching intrinsics. When extracting vector
lanes or converting element types before extraction, ensure the vector argument
matches the intrinsic's expected signedness (e.g., using vreinterpret_u64_* with
vget_lane_u64), or call the matching signed extraction intrinsic (e.g.,
vget_lane_s64) and cast the returned scalar to unsigned. Do not rely on implicit
vector conversions across signedness boundaries.

**Why.** ARM NEON vector types (such as int64x1_t and uint64x1_t) are distinct
types under the ARM C Language Extensions (ACLE) specification. Relying on
implicit vector conversions across signedness or type boundaries causes
compilation failures on strict toolchains like MSVC on Windows Arm64. Explicitly
converting via vreinterpret_* or casting extracted scalar values ensures
portability across all supported compilers.

## Examples

Bad:

```cpp
// Variant 1: Passing mismatched vector signedness into vector helper function
return v64_low_u32(v64_ziplo_8(vget_high_u64(m), vget_low_u64(m)));

// Variant 2: Mismatched signedness in vreinterpret before lane extraction
v256 r1 = v256_from_64(vget_lane_u64(vreinterpret_u64_u8(shuffle1_hi), 0),
                       vget_lane_u64(vreinterpret_s64_u8(shuffle1_lo), 0));

// Variant 3: Passing signed vector directly to unsigned lane extraction
return c ? v64_from_64((vget_lane_u64(b, 0) >> c * 8) |
                       (vget_lane_u64(a, 0) << (8 - c) * 8))
         : b;
```

Good:

```cpp
// Variant 1: Explicitly convert vector types using vreinterpret_*
return v64_low_u32(v64_ziplo_8(vreinterpret_s64_u64(vget_high_u64(m)),
                               vreinterpret_s64_u64(vget_low_u64(m))));

// Variant 2: Match vreinterpret signedness with lane extraction intrinsic
v256 r1 = v256_from_64(vget_lane_u64(vreinterpret_u64_u8(shuffle1_hi), 0),
                       vget_lane_u64(vreinterpret_u64_u8(shuffle1_lo), 0));

// Variant 3: Match signed intrinsic and cast scalar return value to unsigned
return c ? v64_from_64(((uint64_t)vget_lane_s64(b, 0) >> c * 8) |
                       ((uint64_t)vget_lane_s64(a, 0) << (8 - c) * 8))
         : b;
```
