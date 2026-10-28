---
name: libaom/order-multi-component-parameters-by-canonical-name
description: When declaring multi-component functions (such as RGB, YUV, or XYB), order parameters by their canonical name sequence rather than caller layouts
rubric: readability
severity: should_fix
---

**Rule.** Order multi-component function parameters (such as color plane
pointers, channel strides, or coordinates) to match the canonical sequence
implied by the function name (e.g., X, Y, B for XYB, or R, G, B for RGB). Do not
reorder parameter lists to match the internal layout or plane ordering of a
specific caller; instead, map or reorder arguments at the call site.

**Why.** Aligning parameter order with the canonical name prevents confusing API
contracts where component arguments appear out of order. Keeping multi-component
APIs canonical isolates caller-specific plane mappings to call sites and
eliminates bugs where callers mistakenly pass arguments in the expected name
order.

## Examples

Bad:

```cpp
// Anti-pattern: Parameter order (Y, B, X) does not match function name (XYB).
void RGBToXYB(const uint8_t *rgb, int stride_rgb,
              uint8_t *y, int y_stride,
              uint8_t *b, int b_stride,
              uint8_t *x, int x_stride,
              int width, int height);
```

Good:

```cpp
// Conforming: Parameters follow the canonical X, Y, B order implied by the name.
static void RGBToXYB(const uint8_t *rgb, int stride_rgb,
                     uint8_t *x, int x_stride,
                     uint8_t *y, int y_stride,
                     uint8_t *b, int b_stride,
                     int width, int height);
```
