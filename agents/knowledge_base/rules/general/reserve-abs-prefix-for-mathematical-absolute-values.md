---
name: libaom/reserve-abs-prefix-for-mathematical-absolute-values
description: When naming variables or identifiers representing combined totals, sums, or aggregate quantities, use total, sum, or combined rather than abs or abs_
rubric: readability
severity: nit
---

**Rule.** Do not use 'abs' or the 'abs_' prefix when naming variables, struct
members, or functions that represent combined totals, sums, or aggregate
quantities. Use 'total', 'sum', or 'combined' instead (e.g.,
'total_luma_strength'), reserving 'abs' and 'abs_' exclusively for mathematical
absolute values or magnitudes (such as 'abs_diff' or 'abs_cost').

**Why.** In C and C++ conventions, 'abs' universally denotes mathematical
absolute value (e.g., abs() or magnitude calculations). Using 'abs' or 'abs_'
for combined or aggregate quantities misleads readers into expecting
absolute-value logic or non-negative deltas rather than summation.

## Examples

Bad:

```cpp
// 'abs_' misleadingly suggests mathematical absolute value for a sum.
const int abs_luma_strength =
    new_pri_luma_strength + new_sec_luma_strength;
```

Good:

```cpp
// Explicitly name total or aggregate quantities with 'total' or 'sum'.
const int total_luma_strength =
    new_pri_luma_strength + new_sec_luma_strength;
```
