---
name: libaom/avoid-unsimplified-constant-expressions
description: When writing empirical constants or scaling formulas, use evaluated numeric literals directly instead of unsimplified arithmetic expressions of bare literals
rubric: readability
severity: nit
---

**Rule.** Do not write empirical constants or tuning coefficients as
unsimplified arithmetic expressions of bare literals (e.g., (255.0 + 1024.0)).
Use the evaluated constant value directly (e.g., 1279.0) unless the individual
operands represent distinct, meaningful domain parameters or named constants.

**Why.** Leaving an empirical constant as an unsimplified arithmetic expression
of bare numbers misleads readers into inferring false semantic significance
(such as a sum of two distinct limits or bit-depth bounds) where none exists.
Evaluating the constant directly prevents ambiguity, while distinct semantic
parameters should be expressed using clear named constants or explanatory
comments.

## Examples

Bad:

```cpp
// Misleading unsimplified sum of bare numeric literals
int boost = -(int)round((base_qindex + 544.0) *
                        (target_qindex - base_qindex) / (255.0 + 1024.0));
```

Good:

```cpp
// Evaluated empirical constant avoids false semantic inferences
int boost = (int)round((base_qindex + 544.0) *
                       (base_qindex - target_qindex) / 1279.0);
```
