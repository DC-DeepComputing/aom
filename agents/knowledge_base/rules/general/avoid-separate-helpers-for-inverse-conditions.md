---
name: libaom/avoid-separate-helpers-for-inverse-conditions
description: When implementing complementary or inverse conditions, define a single predicate helper and negate its return value instead of writing duplicate inverse functions
rubric: design
severity: should_fix
---

**Rule.** Do not create separate helper functions for mutually inverse predicate
conditions (such as checking above versus below a threshold) with duplicate
iteration or traversal logic. Define a single predicate helper function and
invert or negate its return value at call sites when evaluating the
complementary condition.

**Why.** Defining multiple helper functions for inverse boolean conditions
duplicates traversal, threshold comparison, and boundary logic. Consolidating
them into a single predicate reduces code duplication, lowers maintenance
overhead, and ensures consistent condition evaluation.

## Examples

Bad:

```cpp
static int check_buffer_above_thresh(AV1_COMP *cpi, int64_t buffer_level, int drop_mark) {
  for (int i = 0; i < cpi->svc.number_spatial_layers; ++i) {
    if (buffer_level > drop_mark) return 1;
  }
  return 0;
}

static int check_buffer_below_thresh(AV1_COMP *cpi, int64_t buffer_level, int drop_mark) {
  for (int i = 0; i < cpi->svc.number_spatial_layers; ++i) {
    if (buffer_level <= drop_mark) return 1;
  }
  return 0;
}
```

Good:

```cpp
static bool check_buffer_below_thresh(AV1_COMP *cpi, int64_t buffer_level, int drop_mark) {
  for (int i = 0; i < cpi->svc.number_spatial_layers; ++i) {
    if (buffer_level <= drop_mark) return true;
  }
  return false;
}

// Call sites invert the result when checking for above threshold:
if (!check_buffer_below_thresh(cpi, buffer_level, drop_mark)) {
  // Complementary condition logic here without duplicate helper
}
```
