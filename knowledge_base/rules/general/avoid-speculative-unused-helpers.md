---
name: libaom/avoid-speculative-unused-helpers
description: When adding helper functions, macros, or local definitions, do not introduce speculative or unused code ahead of time; introduce helpers only when actively used.
rubric: readability
severity: should_fix
---

**Rule.** Do not add speculative, anticipated, or unused helper functions,
macros, or definitions to a patch. Introduce new helper functions and
abstractions only in the change where they are first actively called or
referenced.

**Why.** Adding unused helper functions in anticipation of future changes
introduces dead code, increases review overhead, and leaves functions that
cannot be validated against active call sites or test coverage. Introducing
helpers alongside their first callers ensures all code is immediately exercised
and reviewable.

## Examples

Bad:

```cpp
// Added in a multi-part patch series ahead of time without any active call sites:
static INLINE void load_u16_4x11(const uint16_t *s, ptrdiff_t p,
                                 uint16x4_t *const s0, uint16x4_t *const s1,
                                 uint16x4_t *const s2, uint16x4_t *const s3,
                                 uint16x4_t *const s4, uint16x4_t *const s5,
                                 uint16x4_t *const s6, uint16x4_t *const s7,
                                 uint16x4_t *const s8, uint16x4_t *const s9,
                                 uint16x4_t *const s10) {
  *s0 = vld1_u16(s);
  s += p;
  // ...
}
```

Good:

```cpp
// Introduce helper functions in the specific commit or patch where they are first called:
static INLINE void load_u16_4x11(const uint16_t *s, ptrdiff_t p,
                                 uint16x4_t *const s0, ...) { ... }

void convolve_2d_sr_horiz_neon(...) {
  // Active call site in the same patch:
  load_u16_4x11(src, src_stride, &s0, &s1, &s2, &s3, ...);
}
```
