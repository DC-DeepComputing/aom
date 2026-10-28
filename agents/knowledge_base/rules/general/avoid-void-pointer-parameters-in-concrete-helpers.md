---
name: libaom/avoid-void-pointer-parameters-in-concrete-helpers
description: When defining concrete internal helper functions, declare parameters with concrete pointer types instead of accepting void pointers and creating local cast aliases
rubric: readability
severity: nit
---

**Rule.** When defining internal or static helper functions that operate on
concrete buffer types (such as uint8_t * or const uint16_t *), declare
parameters with the concrete pointer type instead of void * or const void *. Do
not introduce redundant local shadow or alias pointer variables with casts;
operate on the typed parameter variables directly.

**Why.** Accepting void * in concrete helpers obscures expected buffer data
types, bypasses compiler type checking at call sites, and forces function bodies
to introduce unnecessary local shadow pointer variables and type casts.
Declaring concrete pointer types directly in signatures simplifies function
bodies and makes data flow explicit.

## Examples

Bad:

```cpp
static INLINE void paeth_4or8_x_h_neon(uint8_t *const dest, ptrdiff_t stride,
                                       const void *const top_row,
                                       const void *const left_column, int width,
                                       int height) {
  uint8_t *dest_u8 = dest;
  const uint8_t *const top_row_u8 = (const uint8_t *)top_row;
  const uint8_t *const left_col_u8 = (const uint8_t *)left_column;
  // ...
}
```

Good:

```cpp
static INLINE void paeth_4or8_x_h_neon(uint8_t *dest, ptrdiff_t stride,
                                       const uint8_t *top_row,
                                       const uint8_t *left_column,
                                       int width, int height) {
  // Operate directly on dest, top_row, and left_column without local alias pointers.
}
```
