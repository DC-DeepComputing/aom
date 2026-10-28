---
name: libaom/parenthesize-compound-column-offsets-in-2d-indexing
description: When computing 2D buffer offsets using strided arithmetic (`row * stride + col`), parenthesize multi-term horizontal column expressions for visual clarity
rubric: readability
severity: nit
---

**Rule.** When computing 2D buffer offsets or pointers using strided arithmetic
(`row_offset * stride + col_offset`), explicitly parenthesize the horizontal
column offset expression when it consists of multiple terms (e.g. `col + bx_pos`
or `c + x`). Do not leave compound horizontal expressions unparenthesized as
flat multi-term additions following multiplication.

**Why.** In 2D buffer offset calculations, parenthesizing compound horizontal
coordinate terms preserves the clear `(row) * stride + (col)` semantic
structure. This prevents visual ambiguity between row scaling and horizontal
displacement, making multidimensional index math easier to verify during code
reviews.

## Examples

Bad:

```cpp
// Unparenthesized compound column terms obscure 2D coordinate structure
const ptrdiff_t buf_offset =
    (ptrdiff_t)(row + by_pos) * ref_stride + col + bx_pos;
uint8_t *dst_ptr = dst + (r + y) * dst_stride + c + x;
```

Good:

```cpp
// Parenthesized compound column terms clearly separate row and col components
const ptrdiff_t buf_offset =
    (ptrdiff_t)(row + by_pos) * ref_stride + (col + bx_pos);
uint8_t *dst_ptr = dst + (r + y) * dst_stride + (c + x);
```
