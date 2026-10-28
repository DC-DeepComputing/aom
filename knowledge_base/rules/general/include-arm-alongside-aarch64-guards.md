---
name: libaom/include-arm-alongside-aarch64-guards
description: When guarding types, constants, or code paths shared across 32-bit and 64-bit Arm architectures, check AOM_ARCH_ARM alongside AOM_ARCH_AARCH64
rubric: correctness
severity: blocking
---

**Rule.** When writing preprocessor `#if` guards for type definitions,
constants, data structures, or code paths shared across Arm architectures (such
as Neon SIMD routines), include `AOM_ARCH_ARM` alongside `AOM_ARCH_AARCH64`. Do
not guard shared Arm definitions with `AOM_ARCH_AARCH64` alone unless the
feature or assembly is strictly 64-bit AArch64-specific.

**Why.** In libaom, 32-bit Arm targets define `AOM_ARCH_ARM` while 64-bit Arm
targets define `AOM_ARCH_AARCH64`. Guarding shared Arm types or implementations
with only `AOM_ARCH_AARCH64` causes 32-bit Arm builds to hit generic C fallbacks
or miss required type definitions, leading to compilation errors or type
mismatches.

## Examples

Bad:

```cpp
// Bad: Omitting AOM_ARCH_ARM leaves 32-bit Arm with int8_t while Arm Neon expects int16_t
#if AOM_ARCH_AARCH64 || AOM_ARCH_X86 || AOM_ARCH_X86_64
typedef int16_t WarpedFilterCoeff;
#else
typedef int8_t WarpedFilterCoeff;
#endif
```

Good:

```cpp
// Good: Includes both 32-bit and 64-bit Arm architecture macros
#if AOM_ARCH_ARM || AOM_ARCH_AARCH64 || AOM_ARCH_X86 || AOM_ARCH_X86_64
typedef int16_t WarpedFilterCoeff;
#else
typedef int8_t WarpedFilterCoeff;
#endif
```
