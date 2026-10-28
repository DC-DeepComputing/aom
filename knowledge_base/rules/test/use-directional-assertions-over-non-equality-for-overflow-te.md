---
name: libaom/test/use-directional-assertions-over-non-equality-for-overflow-te
description: When writing tests for arithmetic operations or overflow safety on bounded or positive inputs, use directional assertions like EXPECT_GT instead of EXPECT_NE
rubric: testing
severity: should_fix
---

**Rule.** When testing arithmetic calculations, transform stages, or overflow
bounds on positive or bounded inputs, use specific directional assertions (such
as EXPECT_GT(val, 0) or EXPECT_LT) rather than loose non-equality checks
(EXPECT_NE(val, 0)). Ensure test assertions verify that the computed value has
the expected sign and direction rather than merely checking that it is non-zero.

**Why.** Arithmetic operations on bounded or positive inputs can experience
signed integer overflow or erroneous sign inversion that wraps to negative
non-zero values. A loose non-equality check like EXPECT_NE(val, 0) falsely
passes on negative wrapped outputs, whereas a directional assertion like
EXPECT_GT(val, 0) guarantees that the calculation produced a valid positive
result without overflow.

## Examples

Bad:

```cpp
// Loose non-equality check falsely passes if calculation overflows and wraps negative
EXPECT_NE(output[0], 0);
```

Good:

```cpp
// Directional assertion ensures the positive input calculation did not overflow and wrap
EXPECT_GT(output[0], 0);
```
