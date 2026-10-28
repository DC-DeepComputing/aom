---
name: libaom/use-inequality-when-asserting-bounds
description: When asserting that a variable satisfies a lower or upper bound after clamping or defaulting, use inequality comparisons (>=, <=) rather than equality (==)
rubric: correctness
severity: blocking
---

**Rule.** When asserting that a non-negative, clamped, or defaulted variable
satisfies an invariant bound, verify that the assertion checks an inequality
condition (such as `>=` or `<=`) rather than an equality comparison (`==`). Do
not assert equality against the minimum or maximum clamp value when the variable
may legitimately take larger or smaller valid values.

**Why.** Using equality comparisons (`==`) instead of inequality operators (`>=`
or `<=`) when verifying invariant bounds causes false assertion failures
whenever a variable takes a valid value beyond the clamp threshold, terminating
debug builds on valid inputs.

## Examples

Bad:

```cpp
if (variance == 0) {
  variance = 1;
}
assert(variance == 1);
```

Good:

```cpp
if (variance == 0) {
  variance = 1;
}
assert(variance >= 1);
```
