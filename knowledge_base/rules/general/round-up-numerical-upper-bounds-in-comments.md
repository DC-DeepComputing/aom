---
name: libaom/round-up-numerical-upper-bounds-in-comments
description: When documenting numerical upper bounds or worst-case factors in comments to justify overflow safety, ensure values are rounded up rather than truncated
rubric: readability
severity: nit
---

**Rule.** When writing comments that document numerical upper bounds or
worst-case scaling factors to prove arithmetic or overflow safety (such as
safety against DBL_MAX or INT_MAX), round up (ceiling) rather than truncate
floating-point upper bound estimates. A truncated upper bound understates the
worst-case value and renders the safety justification mathematically invalid.

**Why.** Truncating or rounding down an upper bound produces a figure smaller
than the actual worst-case mathematical value, misleading readers and
invalidating comments that justify why calculations will not overflow data type
limits.

## Examples

Bad:

```cpp
// geom_mean_of_scale can go up to 4.8322^256, which is within DBL_MAX
// (maximum value a double data type can hold).
```

Good:

```cpp
// geom_mean_of_scale can go up to 4.8323^256, which is within DBL_MAX
// (maximum value a double data type can hold).
```
