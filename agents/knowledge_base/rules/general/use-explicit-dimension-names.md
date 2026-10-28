---
name: libaom/use-explicit-dimension-names
description: When declaring parameters or variables representing spatial dimensions, use explicit names like width and height instead of ambiguous single-letter abbreviations like h and v
rubric: readability
severity: nit
---

**Rule.** When declaring function parameters, struct members, or local variables
that represent rectangular spatial dimensions or extents, name them explicitly
as width and height (or hsize and vsize) rather than ambiguous single-letter
abbreviations such as h (which can ambiguously mean horizontal or height) and v
(vertical). Keep parameter names consistent across function prototypes,
implementations, and internal tracking variables.

**Why.** Single-letter abbreviations like h and v are ambiguous and error-prone
because h is easily conflated with height rather than horizontal (width).
Explicitly spelling out width and height prevents transposition bugs, keeps
function signatures consistent across architecture backends, and aligns
parameter names with local variables.

## Examples

Bad:

```cpp
void cdef_copy_rect8_8bit_to_16bit_avx2(uint16_t *dst, int dstride,
                                        const uint8_t *src, int sstride, int v,
                                        int h) {
  int remaining_width = h;
  for (int r = 0; r < v; ++r) {
    // Process rows...
  }
}
```

Good:

```cpp
void cdef_copy_rect8_8bit_to_16bit_avx2(uint16_t *dst, int dstride,
                                        const uint8_t *src, int sstride,
                                        int height, int width) {
  int remaining_width = width;
  for (int r = 0; r < height; ++r) {
    // Process rows...
  }
}
```
