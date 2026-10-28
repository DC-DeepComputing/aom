---
name: libaom/comments-document-control-flow-sentinels
description: When assigning sentinel values like INT_MAX or INT64_MAX to signal early exit or pruning across functions, document their meaning and caller-side checks.
rubric: readability
severity: nit
---

**Rule.** When setting struct fields or variables to extreme sentinel values
(such as INT_MAX, INT64_MAX, UINT_MAX, or -1) to signal early exit, error
states, or mode pruning across function boundaries, add an explanatory comment
describing the sentinel's purpose. Explicitly note which caller functions check
this sentinel value to skip subsequent evaluation or handle early returns.

**Why.** Using maximum or sentinel integer values for control-flow signaling
across callbacks and helper functions is not self-evident to readers.
Documenting the sentinel's meaning and the downstream functions that check it
clarifies control flow and prevents accidental omission or misuse during
refactoring.

## Examples

Bad:

```cpp
if (this_sad > sad_threshold) {
  args->rdc->rate = INT_MAX;
  args->rdc->dist = INT64_MAX;
  return;
}
```

Good:

```cpp
if (this_sad > sad_threshold) {
  // Set rate and distortion to sentinel maximums to prune current mode.
  // Note: args->rdc->rate is checked in av1_nonrd_pick_intra_mode() to skip
  // evaluation of this mode.
  args->rdc->rate = INT_MAX;
  args->rdc->dist = INT64_MAX;
  return;
}
```
