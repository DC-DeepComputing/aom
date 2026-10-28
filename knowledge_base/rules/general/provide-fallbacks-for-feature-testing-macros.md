---
name: libaom/provide-fallbacks-for-feature-testing-macros
description: When using compiler feature-testing macros like __has_include, __has_feature, or __has_builtin, provide a fallback definition or guard with defined() for legacy toolchains
rubric: correctness
severity: blocking
---

**Rule.** When using compiler feature-testing macros such as __has_include,
__has_feature, __has_builtin, or __has_attribute, provide a fallback definition
(such as #ifndef __has_include / #define __has_include(path) 0) or guard with
#if defined(...) before evaluating them. Do not invoke feature-testing macros
directly in preprocessor expressions without ensuring they are defined.

**Why.** Older compiler toolchains and earlier language standard modes do not
natively support compiler feature-testing macros. Evaluating an undefined
feature-testing macro in preprocessor expressions causes syntax and build errors
on legacy compilers.

## Examples

Bad:

```cpp
// Preprocessor error on older compilers where __has_include is undefined
#if AOM_ARCH_AARCH64 && defined(__ARM_NEON_SVE_BRIDGE) && \
    defined(__ARM_FEATURE_SVE) && __has_include(<arm_neon_sve_bridge.h>)
#include <arm_neon_sve_bridge.h>
#define AOM_HAVE_NEON_SVE_BRIDGE 1
#endif
```

Good:

```cpp
#ifndef __has_include
#define __has_include(path) 0
#endif

#if AOM_ARCH_AARCH64 && defined(__ARM_NEON_SVE_BRIDGE) && \
    defined(__ARM_FEATURE_SVE) && __has_include(<arm_neon_sve_bridge.h>)
#include <arm_neon_sve_bridge.h>
#define AOM_HAVE_NEON_SVE_BRIDGE 1
#else
#define AOM_HAVE_NEON_SVE_BRIDGE 0
#endif
```
