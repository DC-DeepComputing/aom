---
name: libaom/parenthesize-ternary-conditions
description: 'When writing ternary conditional expressions, enclose the condition in parentheses (such as `(condition) ? a : b`) instead of leaving it unparenthesized'
rubric: readability
severity: nit
---

**Rule.** Enclose the condition of a ternary conditional expression in
parentheses (e.g., `(condition) ? a : b`) rather than leaving it
unparenthesized. This visually groups the test and clearly separates the
condition from the consequent and alternative expressions.

**Why.** Unparenthesized conditions in ternary expressions can be ambiguous or
harder to scan, particularly when conditions contain comparison operators or
member accesses. Enclosing the condition in parentheses provides clear visual
grouping and distinguishes the condition from the branch values.

## Examples

Bad:

```cpp
const int offset = height < 8 ? 4 : 8;
__m128i rep = is_second_half ? _mm_set1_epi16(0x8008) : _mm_setzero_si128();
```

Good:

```cpp
const int offset = (height < 8) ? 4 : 8;
__m128i rep = (is_second_half) ? _mm_set1_epi16(0x8008) : _mm_setzero_si128();
```
