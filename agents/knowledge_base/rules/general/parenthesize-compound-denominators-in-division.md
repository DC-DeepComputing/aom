---
name: libaom/parenthesize-compound-denominators-in-division
description: When computing arithmetic expressions involving compound denominators or chained division, explicitly parenthesize divisor subexpressions to avoid unintended left-to-right evaluation
rubric: correctness
severity: should_fix
---

**Rule.** Explicitly parenthesize compound denominators and scaling factors in
division expressions. Because `/` is left-associative in C and C++, writing
chained division `a / b / c` evaluates as `(a / b) / c` rather than `a / (b /
c)`.

**Why.** In C and C++, division associates left-to-right. When the divisor term
is itself a quotient or compound scaling factor, omitting parentheses computes
repeated division instead of dividing by the intended net denominator, causing
arithmetic calculation errors.

## Examples

Bad:

```cpp
// Incorrect: left-associativity evaluates this as ((... / SCALE) / scaler) / scaler
B[jj * wiener_halfwin1 + ii] +=
    Hc[i * wiener_win + j][k * wiener_win2 + l] * a[k] /
    (scaler * WIENER_TAP_SCALE_FACTOR) * (a[l] / scaler) /
    WIENER_TAP_SCALE_FACTOR / scaler / scaler;
```

Good:

```cpp
// Correct: explicitly parenthesizes compound divisor expressions
B[jj * wiener_halfwin1 + ii] +=
    Hc[i * wiener_win + j][k * wiener_win2 + l] * a[k] /
    (scaler * WIENER_TAP_SCALE_FACTOR) * a[l] /
    (WIENER_TAP_SCALE_FACTOR / scaler);
```
