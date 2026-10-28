---
name: libaom/arm-neon-32bit-fallback-unpack-both-halves
description: When implementing 32-bit ARM (ARMv7) fallbacks for AArch64 NEON code, unpack both halves using vget_high_* and test with 32-bit cross-compilation
rubric: correctness
severity: blocking
---

**Rule.** When writing 32-bit ARM (ARMv7) fallbacks for AArch64 NEON
optimizations under '#if defined(__aarch64__)' / '#else' blocks, ensure vector
unpacking routines process both low and high halves (using vget_high_* for the
upper half rather than accidentally duplicating vget_low_*). Validate changes
across 32-bit ARM branches using cross-compilation CMake toolchain files (such
as armv7-linux-gcc.cmake).

**Why.** AArch64 NEON provides instructions and 128-bit operations (such as
128-bit table lookups or high-half widenings) not natively available in 32-bit
ARMv7 NEON, requiring 32-bit paths to manually unpack vectors using low and high
halves. Because native x86 and AArch64 test environments skip 32-bit
preprocessor branches, copy-paste typos like repeated vget_low_* calls easily go
unnoticed without 32-bit cross-compilation testing.

## Examples

Bad:

```cpp
// 32-bit ARM fallback inadvertently unpacks the lower half twice
int16x8_t pre_s16 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pre_u8)));
obmc_variance_8x1_s16_neon(pre_s16, &wsrc[0], &mask[0], &ssev, &sumv);

pre_s16 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pre_u8)));  // Duplicate vget_low_u8
obmc_variance_8x1_s16_neon(pre_s16, &wsrc[8], &mask[8], &ssev, &sumv);
```

Good:

```cpp
// Correctly unpacks low half, then high half for the remaining lanes
int16x8_t pre_s16 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pre_u8)));
obmc_variance_8x1_s16_neon(pre_s16, &wsrc[0], &mask[0], &ssev, &sumv);

pre_s16 = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(pre_u8)));  // Correctly unpacks high half
obmc_variance_8x1_s16_neon(pre_s16, &wsrc[8], &mask[8], &ssev, &sumv);
```
