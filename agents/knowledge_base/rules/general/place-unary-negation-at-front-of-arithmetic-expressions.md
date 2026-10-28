---
name: libaom/place-unary-negation-at-front-of-arithmetic-expressions
description: When negating the result of a multi-factor arithmetic expression, place unary minus at the front of the expression rather than on an internal factor
rubric: readability
severity: nit
---

**Rule.** When negating the result of a multi-factor or multi-term arithmetic
expression, place the unary minus operator (-) at the front of the entire
expression or cast (e.g., -(a * b / c) or -(int)(...)) rather than on an
internal factor (such as a * -b / c).

**Why.** Burying a unary minus inside internal multiplicative or divisor terms
makes the overall sign of the calculation easy to miss or misread. Placing the
negation at the front of the expression makes the intended polarity immediately
visible to the reader.

## Examples

Bad:

```cpp
// Bad: Unary negation is buried inside an internal multiplicative factor.
int boost = (int)((base_qindex + 544) * -compute_delta(cpi, x) / (255 + 1024));
```

Good:

```cpp
// Good: Leading unary minus makes the expression's sign immediately obvious.
int boost = -(int)((base_qindex + 544.0) * (target_qindex - base_qindex) /
                   (255.0 + 1024.0));
```
