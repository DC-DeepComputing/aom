---
name: libaom/no-inline-on-non-static-c-functions
description: When defining functions in C source files, do not place AOM_INLINE or inline specifiers on non-static functions; use inline qualifiers only with static functions.
rubric: correctness
severity: blocking
---

**Rule.** Do not use AOM_INLINE, AOM_FORCE_INLINE, or inline on non-static
function definitions in .c source files. Restrict inline specifiers to
internal-linkage functions declared with static (static AOM_INLINE, static
AOM_FORCE_INLINE, or static inline), and define non-static functions exported
across translation units without inline specifiers.

**Why.** In MSVC and standard C, inline specifiers (such as AOM_INLINE expanding
to __forceinline or __inline) alter function linkage semantics and cause
compilation errors or unresolved external symbol linker failures when applied to
non-static function definitions exported across translation units. In C, inline
function definitions must have internal (static) linkage.

## Examples

Bad:

```cpp
// In .c source file: non-static function definition marked with inline macro.
AOM_INLINE bool av1_check_ref_is_low_spatial_res_super_frame(
    AV1_COMP *const cpi, int ref_frame) {
  // ...
  return true;
}
```

Good:

```cpp
// Non-static functions exported across translation units omit inline specifiers:
bool av1_check_ref_is_low_spatial_res_super_frame(
    AV1_COMP *const cpi, int ref_frame) {
  // ...
  return true;
}

// Internal-linkage inline helpers must be explicitly marked static:
static AOM_INLINE bool is_low_res_frame(const AV1_COMP *const cpi) {
  // ...
  return false;
}
```
