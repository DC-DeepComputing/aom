---
name: libaom/test/use-inv-cos-bit-for-inverse-transform-tests
description: When writing or updating unit and regression tests for AV1 inverse transforms, use cos_bit = 12 (INV_COS_BIT) rather than forward-transform values
rubric: testing
severity: should_fix
---

**Rule.** When writing unit or regression tests for AV1 inverse transforms (such
as av1_iadst*, av1_idct*, av1_iidentity*, or 2D inverse transforms), pass
cos_bit = 12 (or the INV_COS_BIT constant) rather than forward-transform
precision values (cos_bit = 13). Ensure input test vectors, range bitdepths, and
overflow test cases are constructed for valid 12-bit cosine precision.

**Why.** Per section 7.13.2.6 of the AV1 specification and libaom's INV_COS_BIT
constant, all inverse transform stages operate with 12-bit cosine precision,
whereas forward transforms use 13-bit precision. Testing inverse transforms with
cos_bit = 13 exercises an invalid, non-spec-compliant configuration that cannot
occur during decoding and can produce false overflow reports or mask real
precision issues.

## Examples

Bad:

```cpp
// Bad: Using forward-transform cos_bit (13) in an inverse transform test
int32_t input[4] = { 300000, 0, 100000, 160000 };
int32_t output[4];
const int8_t cos_bit = 13;
av1_iadst4(input, output, cos_bit, range_bit);
```

Good:

```cpp
// Good: Inverse transforms in AV1 always use cos_bit = 12 (INV_COS_BIT)
int32_t input[4] = { 300000, 0, 300000, 300000 };
int32_t output[4];
const int8_t cos_bit = 12;  // or INV_COS_BIT
av1_iadst4(input, output, cos_bit, range_bit);
```
