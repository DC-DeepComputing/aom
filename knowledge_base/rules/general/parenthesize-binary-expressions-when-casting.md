---
name: libaom/parenthesize-binary-expressions-when-casting
description: When casting the result of an arithmetic or bitwise binary operation, enclose the entire expression in parentheses rather than casting only the first operand
rubric: readability
severity: nit
---

**Rule.** When casting the evaluation result of an arithmetic or bitwise binary
expression (such as a bitshift, addition, subtraction, or bitwise AND/OR/XOR),
wrap the full binary expression in parentheses (e.g., `(int16_t)(1 << shift)` or
`static_cast<int16_t>(1 << shift)`). Do not omit parentheses around the binary
operation (e.g., `(int16_t)1 << shift`), as the cast binds only to the first
operand.

**Why.** In C and C++, cast operators have higher operator precedence than
binary arithmetic and bitwise operators like <<, >>, +, -, &, |, and ^. Casting
only the first operand converts that operand before evaluating the binary
operation, which then promotes it back to int during evaluation and leaves the
resulting expression uncast, potentially causing unexpected integer promotions
or compiler warnings.

## Examples

Bad:

```cpp
// Cast binds only to first operand due to precedence; shift result remains int
const __m256i rnd = _mm256_set1_epi16((int16_t)1 << (log_scale - 1));
uint16_t mask = (uint16_t)base_mask << bit_offset;
int32_t val = (int32_t)a * b + c;
```

Good:

```cpp
// Entire binary expression is enclosed in parentheses
const __m256i rnd = _mm256_set1_epi16((int16_t)(1 << (log_scale - 1)));
uint16_t mask = (uint16_t)(base_mask << bit_offset);
int32_t val = (int32_t)(a * b) + c;
```
