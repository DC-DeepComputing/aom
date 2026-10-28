---
name: libaom/check-compiler-macros-with-defined-for-wundef
description: 'When checking compiler, platform, or architecture preprocessor macros, use defined(MACRO) in #if and #elif directives instead of bare #if/#elif MACRO to avoid -Wundef errors'
rubric: correctness
severity: should_fix
---

**Rule.** When testing compiler, platform, architecture, or optional
preprocessor macros in #if and #elif directives, check them using defined(MACRO)
(e.g., #if defined(MACRO) or #elif defined(MACRO)) rather than bare #if MACRO or
#elif MACRO. For standalone single-identifier conditions without #elif branches,
#ifdef MACRO or #ifndef MACRO is also acceptable.

**Why.** Under compiler warning flags like -Wundef, evaluating an undefined
preprocessor macro in #if or #elif directives generates a build error. Toolchain
and platform identification macros (such as __clang__, __GNUC__, __CC_ARM, or
_MSC_VER) are undefined on alternative compilers and must be checked with
defined() for clean multi-platform builds.

## Examples

Bad:

```cpp
#if defined(__clang__)
  // Clang-specific implementation
#elif __CC_ARM
  // ARM compiler implementation
#elif __GNUC__
  // GCC implementation
#endif
```

Good:

```cpp
#if defined(__clang__)
  // Clang-specific implementation
#elif defined(__CC_ARM)
  // ARM compiler implementation
#elif defined(__GNUC__)
  // GCC implementation
#endif
```
