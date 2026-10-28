---
name: libaom/parenthesize-arithmetic-before-bitwise-shift
description: When combining arithmetic expressions (addition, subtraction, multiplication, or division) with bitwise shifts (<<, >>), explicitly parenthesize the arithmetic subexpression.
rubric: readability
severity: nit
---

**Rule.** Explicitly parenthesize arithmetic subexpressions (such as addition,
subtraction, multiplication, or division) when combining them with bitwise shift
operators (`>>` or `<<`). Do not rely on implicit operator precedence across
mixed arithmetic and shift operations.

**Why.** Although arithmetic operators have higher precedence than bitwise shift
operators in C and C++, omitting parentheses makes expressions harder to read
and increases the risk of subtle operator precedence errors.

## Examples

Bad:

```cpp
const int current_size = cm->mi_params.mi_rows * cm->mi_params.mi_cols >> 2;
int offset = base + index << 3;
```

Good:

```cpp
const int current_size = (cm->mi_params.mi_rows * cm->mi_params.mi_cols) >> 2;
int offset = (base + index) << 3;
```
