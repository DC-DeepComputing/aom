---
name: libaom/document-potential-library-substitutions-with-todo
description: When implementing custom or temporary logic that could be replaced by a shared library, add an attributed TODO comment documenting the potential substitution
rubric: readability
severity: nit
---

**Rule.** When implementing custom, fallback, or temporary routines (such as
color conversions, matrix operations, or utility transforms) that could be
replaced, unified, or accelerated by an existing shared library or common
subsystem (such as libyuv or internal helpers), add an attributed `//
TODO(<username_or_bug>): ...` comment documenting the potential substitution for
future refactoring.

**Why.** Leaving custom implementations without tracking comments obscures
opportunities for code reuse, SIMD optimization, and external library
integration, making future maintenance and refactoring harder to discover and
track.

## Examples

Bad:

```cpp
static void YUVtoRGB(const uint8_t *y, int y_stride, const uint8_t *u,
                     int u_stride, const uint8_t *v, int v_stride,
                     uint8_t *rgb, int width, int height) {
  // Custom conversion logic ...
}
```

Good:

```cpp
// TODO(username): We should be able to use libyuv for this conversion.
static void YUVtoRGB(const uint8_t *y, int y_stride, const uint8_t *u,
                     int u_stride, const uint8_t *v, int v_stride,
                     uint8_t *rgb, int width, int height) {
  // Custom conversion logic ...
}
```
