---
name: libaom/use-aom-arch-macros-for-architecture-guards
description: When writing architecture-dependent conditional compilation guards, use project-defined AOM_ARCH_* macros instead of compiler-specific predefined macros
rubric: readability
severity: should_fix
---

**Rule.** When guarding architecture-dependent code or optimizations, use
project-defined architecture macros (such as #if AOM_ARCH_AARCH64, #if
AOM_ARCH_ARM, or #if AOM_ARCH_X86_64) rather than compiler-specific predefined
macros (such as __aarch64__, __arm__, __x86_64__, _M_ARM64, or _M_X64). Ensure
guards use #if AOM_ARCH_* rather than #ifdef because libaom defines architecture
configuration macros to 0 or 1.

**Why.** Libaom configures canonical architecture macros (AOM_ARCH_*) through
CMake build configuration and header definitions. Using standard project macros
instead of raw compiler- or toolchain-specific symbols guarantees consistent
behavior across diverse compilers (GCC, Clang, MSVC) and cross-compilation
targets.

## Examples

Bad:

```cpp
#ifdef __aarch64__
  const uint32x4_t c = vpaddq_u32(b, b);
  return vpaddq_u32(c, c);
#else
  const uint32x2_t c = vpadd_u32(vget_low_u32(b), vget_high_u32(b));
  return vget_lane_u32(vpadd_u32(c, c), 0);
#endif
```

Good:

```cpp
#if AOM_ARCH_AARCH64
  const uint32x4_t c = vpaddq_u32(b, b);
  return vpaddq_u32(c, c);
#else
  const uint32x2_t c = vpadd_u32(vget_low_u32(b), vget_high_u32(b));
  return vget_lane_u32(vpadd_u32(c, c), 0);
#endif
```
