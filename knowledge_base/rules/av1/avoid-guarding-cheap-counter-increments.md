---
name: libaom/av1/avoid-guarding-cheap-counter-increments
description: When updating simple statistic counters or scalar metric accumulators, increment them directly without adding sentinel checks or conditional guards
rubric: design
severity: should_fix
---

**Rule.** Do not guard simple scalar counter increments or statistic
accumulations behind sentinel value checks (such as checking `!= -1`) or
conditional branches unless profiling demonstrates a measurable bottleneck.
Accumulate trivial statistics directly to avoid unnecessary branching and state
management across worker threads and encoder passes.

**Why.** Trivial integer additions and scalar counter increments have negligible
execution overhead. Adding conditional branches and sentinel states introduces
unnecessary complexity and state management across mode decision passes and
multi-threading workers without providing meaningful CPU savings.

## Examples

Bad:

```cpp
// Bad: Guarding a trivial integer accumulation behind a sentinel check.
if (cpi->palette_pixel_num != -1) {
  x->palette_pixels += (block_width * block_height);
}
```

Good:

```cpp
// Good: Accumulating simple statistics unconditionally without unnecessary branching.
x->palette_pixels += (block_width * block_height);
```
