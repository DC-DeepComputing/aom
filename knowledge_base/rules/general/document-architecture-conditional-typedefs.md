---
name: libaom/document-architecture-conditional-typedefs
description: 'When conditionally defining different data types or typedefs across CPU architectures using #if AOM_ARCH_*, add a comment explaining why the types diverge'
rubric: readability
severity: nit
---

**Rule.** When using architecture guards (such as #if AOM_ARCH_*) to
conditionally define different data types or typedefs across CPU targets, add an
explanatory comment detailing why the types diverge. Document the
target-specific constraints or performance motivations, such as SIMD intrinsic
width requirements, generic C auto-vectorization benefits, or memory bandwidth
savings.

**Why.** Architecture-conditional type divergence is non-obvious to readers and
maintainers. Without explanatory comments, future contributors may inadvertently
harmonize types across architectures, regressing auto-vectorization performance
in generic C or breaking SIMD intrinsic compatibility.

## Examples

Bad:

```cpp
#if AOM_ARCH_ARM || AOM_ARCH_AARCH64 || AOM_ARCH_X86 || AOM_ARCH_X86_64
typedef int16_t WarpedFilterCoeff;
#else
typedef int8_t WarpedFilterCoeff;
#endif
```

Good:

```cpp
// Use 16-bit type for ARM/x86 SIMD intrinsics, but 8-bit type for generic C
// to match value range, enable compiler auto-vectorization, and reduce bandwidth.
#if AOM_ARCH_ARM || AOM_ARCH_AARCH64 || AOM_ARCH_X86 || AOM_ARCH_X86_64
typedef int16_t WarpedFilterCoeff;
#else
typedef int8_t WarpedFilterCoeff;
#endif
```
