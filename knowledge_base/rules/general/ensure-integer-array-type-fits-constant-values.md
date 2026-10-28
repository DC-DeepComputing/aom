---
name: libaom/ensure-integer-array-type-fits-constant-values
description: When defining constant lookup tables or arrays initialized with numeric literals, choose an integer element type with sufficient bit depth to prevent overflow.
rubric: correctness
severity: blocking
---

**Rule.** Ensure constant lookup tables, arrays, and matrices use an integer
element type with adequate bit depth and signedness to accommodate all
initializer values without overflow or truncation. When table values exceed the
range of smaller integer types (such as values over 32,767 for int16_t or 255
for uint8_t), widen the array type to int32_t or uint32_t.

**Why.** Initializing integer arrays with constants exceeding the element type's
range causes compile errors (such as narrowing conversion warnings treated as
errors) or silent signed overflow and data truncation, resulting in corrupted
lookup values at runtime.

## Examples

Bad:

```cpp
// Values exceed INT16_MAX (32767), causing narrowing/overflow errors.
static const int16_t dc_qlookup_12_QTX[QINDEX_RANGE] = {
  // ...
  52713, 54821, 57014, 59295, 61667, 64133, 66699, 69367, 72141, 75027,
};
```

Good:

```cpp
// int32_t accommodates values exceeding 32767 without overflow.
static const int32_t dc_qlookup_12_QTX[QINDEX_RANGE] = {
  // ...
  52713, 54821, 57014, 59295, 61667, 64133, 66699, 69367, 72141, 75027,
};
```
