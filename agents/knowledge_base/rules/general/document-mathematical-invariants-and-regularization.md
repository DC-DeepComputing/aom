---
name: libaom/document-mathematical-invariants-and-regularization
description: When implementing mathematical algorithms or choosing regularization parameters and tuning constants, document their theoretical rationale, expected magnitude ranges, and underlying invariants
rubric: readability
severity: should_fix
---

**Rule.** When introducing numerical tuning constants, thresholds, or
regularization parameters, document their theoretical derivation, expected input
magnitude ranges, and arithmetic constraints (such as integer math or SIMD
properties). Additionally, when algorithms rely on non-obvious mathematical
invariants (such as positive semi-definiteness or determinant lower bounds) to
assert validity or omit fallback error handling, explain why the property holds
in code comments.

**Why.** Reviewers and future maintainers cannot easily verify mathematical
assertions, omission of degenerate fallback paths, or whether chosen tuning
constants and regularization strengths are appropriate without explanations of
the underlying algebraic properties, input magnitude ranges, and arithmetic
constraints.

## Examples

Bad:

```cpp
// Example 1: Bare regularization constant without magnitude rationale
// Apply regularization
tmp[0] += 1;
tmp[3] += 1;

// Example 2: Mathematical assertion without explanation of invariant
// Note: det >= 1 due to regularization
assert(det >= 1);
```

Good:

```cpp
// Example 1: Regularization documented with theoretical basis and magnitude scale
// Apply regularization: we add `k * I` before inverting to ensure invertibility.
// Setting k = 1 works well because typical equation values are 1e5 to 1e6
// (upper limit ~6e7), and whole numbers preserve integerized SIMD operations.
tmp[0] += 1;
tmp[3] += 1;

// Example 2: Detailed explanation of positive semi-definiteness and bounds
// Due to how the least-squares matrix is constructed, all eigenvalues are >= 0
// (det M >= 0). The regularization term `+ k * I` further ensures det M >= k^2.
// With k = 1, det M >= 1, so singular matrix handling can safely be omitted.
assert(det >= 1);
```
