---
name: libaom/av1/omit-2d-dimensions-in-1d-transform-types-and-tables
description: When defining 1D transform function pointer typedefs or multi-size dispatch tables indexed by TX_SIZES, omit specific 2D block dimensions from identifier names
rubric: readability
severity: should_fix
---

**Rule.** Do not embed specific 2D block dimensions (such as '8x8' or '16x16')
into function pointer typedefs or lookup dispatch array names when they
represent generic 1D transforms or tables indexed across multiple transform
sizes (TX_SIZES). Use generic 1D or vector-width descriptors (such as
transform_neon or _w_arr) instead of 2D dimension suffixes.

**Why.** Function pointer typedefs for 1D SIMD kernels and dispatch tables
indexed by TX_SIZES operate across multiple rectangular and square transform
sizes. Including 2D block dimensions like 8x8 in type or table identifiers is
misleading, obscuring that the function pointer or lookup table is shared across
various transform dimensions.

## Examples

Bad:

```cpp
// Bad: Embedding 2D block size (8x8) into 1D transform type and multi-size table
typedef void (*transform_8x8_neon)(int16x8_t *input, int16x8_t *output,
                                   int8_t cos_bit, int bit);

static const transform_8x8_neon
    lowbd_txfm_all_1d_zeros_w8x8_arr[TX_SIZES][ITX_TYPES_1D][4];
```

Good:

```cpp
// Good: Using generic 1D transform type and vector-width table name
typedef void (*transform_neon)(int16x8_t *input, int16x8_t *output,
                               int8_t cos_bit, int bit);

static const transform_neon
    lowbd_txfm_all_1d_zeros_w_arr[TX_SIZES][ITX_TYPES_1D][4];
```
