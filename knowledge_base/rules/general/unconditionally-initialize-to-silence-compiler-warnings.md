---
name: libaom/unconditionally-initialize-to-silence-compiler-warnings
description: 'When suppressing compiler warnings such as uninitialized variables or SIMD vectors, unconditionally initialize at declaration instead of adding compiler-version #ifdef guards'
rubric: readability
severity: should_fix
---

**Rule.** When silencing compiler diagnostics (such as false-positive
uninitialized variable or vector warnings when populating SIMD registers
lane-by-lane), unconditionally initialize variables or SIMD vectors to zero at
declaration. Do not introduce compiler-version-specific #if or #ifdef guards or
custom suppression macros to selectively initialize variables for specific
toolchain versions.

**Why.** False-positive compiler warnings frequently affect multiple compiler
versions and toolchains rather than a single specific release. Compiler-version
#ifdef guards are fragile, add unnecessary preprocessor clutter, and break
across compiler updates, whereas unconditional zero-initialization cleanly
eliminates warnings across all compilers with negligible or optimized-away
runtime overhead.

## Examples

Bad:

```cpp
// Fragile compiler-version guard to silence false-positive uninitialized warning
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ == 6 && \
    __GNUC_MINOR__ == 3
#define AOM_UNINITIALIZED_GCC
#endif

void load_u32_lane(uint32x2_t *tu0, const uint32_t *src) {
#ifdef AOM_UNINITIALIZED_GCC
  *tu0 = vdup_n_u32(0);
#endif
  *tu0 = vld1_lane_u32(src, *tu0, 0);
}
```

Good:

```cpp
// Unconditionally initialize vector at declaration to silence warnings across all compilers
void load_u32_lane(uint32x2_t *tu0, const uint32_t *src) {
  uint32x2_t tmp = vdup_n_u32(0);
  *tu0 = vld1_lane_u32(src, tmp, 0);
}
```
