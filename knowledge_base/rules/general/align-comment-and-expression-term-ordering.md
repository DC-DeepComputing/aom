---
name: libaom/align-comment-and-expression-term-ordering
description: When writing arithmetic expressions and comments for weighted combinations or multi-term ratios, order terms in natural sequence matching documented ratio descriptions
rubric: readability
severity: nit
---

**Rule.** When formulating weighted combinations, multi-term ratios, or
arithmetic operations across sequentially ordered elements (such as lower,
middle, and upper bounds or channels), keep the terms in both code comments and
arithmetic expressions ordered in their natural sequence. Ensure ratio
descriptions in comments and commit messages directly mirror the positional
order of terms in the implementation (e.g., ordering terms as lower, middle,
upper with a 1:2:1 ratio rather than 2:1:1).

**Why.** Mismatched term ordering between comments, commit messages, and
arithmetic expressions introduces confusing discrepancies and makes code harder
to audit. Aligning the sequence of terms across documentation and expressions
keeps mathematical notation intuitive and consistent.

## Examples

Bad:

```cpp
// Weigh the three variances in a 2:1:1 ratio. This allows for smoother
// delta-q transitions among superblocks with mixed-variance features.
const unsigned int variance =
    ((variances[middle_index] * 2) + variances[lower_index] +
     variances[upper_index]) /
    4;
```

Good:

```cpp
// Weigh the three variances in a 1:2:1 ratio, with rounding (the +2 term).
// This allows for smoother delta-q transitions among superblocks with
// mixed-variance features.
const unsigned int variance =
    (variances[lower_index] + (variances[middle_index] * 2) +
     variances[upper_index] + 2) /
    4;
```
