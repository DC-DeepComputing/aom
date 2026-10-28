---
name: libaom/cache-redundant-function-calls-across-branches
description: When evaluating identical non-trivial function calls or queries across conditional branches, cache the result in a local variable before branching
rubric: safety
severity: should_fix
---

**Rule.** When evaluating identical non-trivial function calls, expensive
queries, or complex expressions across consecutive conditional branches (such as
if and else-if), evaluate and store the result in a local const variable before
branching. Do not re-evaluate identical function calls with identical parameters
in subsequent branch conditions.

**Why.** Re-evaluating identical non-trivial helper functions across multiple
conditional branches wastes CPU cycles on redundant computations (such as
repeated loops, lookups, or calculations) and clutters branch conditions.
Caching the result in a well-named local variable eliminates duplicate execution
and improves code readability.

## Examples

Bad:

```cpp
// Redundant evaluation of check_buffer_below_thresh in both branches
if (!check_buffer_below_thresh(cpi, buffer_level, drop_mark) &&
    rc->decimation_factor > 0) {
  --rc->decimation_factor;
} else if (check_buffer_below_thresh(cpi, buffer_level, drop_mark) &&
           rc->decimation_factor == 0) {
  rc->decimation_factor = 1;
}
```

Good:

```cpp
// Cache function result before branching to avoid redundant computation
const bool buffer_below_thresh =
    check_buffer_below_thresh(cpi, buffer_level, drop_mark);
if (!buffer_below_thresh && rc->decimation_factor > 0) {
  --rc->decimation_factor;
} else if (buffer_below_thresh && rc->decimation_factor == 0) {
  rc->decimation_factor = 1;
}
```
