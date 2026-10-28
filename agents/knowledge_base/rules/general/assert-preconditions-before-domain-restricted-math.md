---
name: libaom/assert-preconditions-before-domain-restricted-math
description: When executing domain-restricted mathematical operations like log2, log, sqrt, or division, assert input preconditions to document expected domain invariants and catch invalid values
rubric: correctness
severity: should_fix
---

**Rule.** When performing mathematical operations with restricted input domains
(such as log2(), log(), sqrt(), asin(), acos(), or division), add assert()
preconditions on input operands (e.g., verifying strictly positive values before
logarithms, non-negative values before square roots, or non-zero divisors).
Ensure edge cases or fallback clamps applied prior to the operation are
validated by the assertion.

**Why.** Mathematical functions invoked with values outside their valid domain
produce NaN, -Inf, or divide-by-zero errors that silently corrupt downstream
calculations and rate control. Asserting domain preconditions documents required
invariants and catches out-of-bounds inputs or flawed fallback branches in debug
builds.

## Examples

Bad:

```cpp
if (variance == 0) {
  variance = 1;
}
double qstep_ratio = 0.15 * strength * (-log2((double)variance) + 10.0) + 1.0;
```

Good:

```cpp
if (variance == 0) {
  variance = 1;
}
assert(variance >= 1);
double qstep_ratio = 0.15 * strength * (-log2((double)variance) + 10.0) + 1.0;
```
