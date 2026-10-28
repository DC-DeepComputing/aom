---
name: libaom/accumulate-directly-into-total-in-nested-loops
description: When computing reductions or summations across nested loops, accumulate directly into the total accumulator instead of tracking redundant inner-loop intermediate variables
rubric: readability
severity: nit
---

**Rule.** When performing summations, reductions, or metric calculations across
nested loops, accumulate values directly into the outer total accumulator
variable. Do not declare and track redundant inner-loop intermediate
accumulators or compute unused intermediate outputs when elements can be summed
straight into the final total.

**Why.** Declaring redundant inner-loop accumulators adds unnecessary local
state, requires extra variable tracking across loop boundaries, and complicates
loop maintenance without providing any performance or readability benefit.

## Examples

Bad:

```cpp
int64_t total_sse = 0;
for (int i = 0; i < h; ++i) {
  int32_t row_sse = 0;
  for (int j = 0; j < w; ++j) {
    const int diff = a[j] - b[j];
    row_sse += diff * diff;
  }
  total_sse += row_sse;
}
```

Good:

```cpp
int64_t total_sse = 0;
for (int i = 0; i < h; ++i) {
  for (int j = 0; j < w; ++j) {
    const int diff = a[j] - b[j];
    total_sse += diff * diff;
  }
}
```
