---
name: libaom/prefer-loop-index-offsets-over-mutating-base-pointers
description: When traversing adjacent sub-regions of strided 2D buffers across sequential loops, adjust loop index offsets rather than repeatedly mutating base buffer pointers
rubric: readability
severity: nit
---

**Rule.** When iterating over adjacent sub-regions, borders, or subsequent
blocks of a 2D buffer relative to a base buffer pointer, keep the base pointer
stable and adjust the loop index range (or compute explicit offsets from the
stable base) across sequential loops. Do not repeatedly mutate base buffer
pointers between loops with += or -= stride offsets.

**Why.** Mutating base pointers across sequential loops shifts the coordinate
origin back and forth, obscuring where pointers reference the original buffer
and increasing the risk of off-by-one or offset miscalculation errors. Keeping a
constant base pointer and adjusting loop indices makes coordinate spaces
explicit and easier to verify.

## Examples

Bad:

```cpp
// Anti-pattern: mutating base buffer pointer before sequential sub-region loop
intra_pred += INTERINTRA_ML_BORDER * intra_stride;
for (int j = 0; j < bh; ++j) {
  for (int i = 0; i < INTERINTRA_ML_BORDER; ++i) {
    intra_input[interpreter_offset++] = intra_pred[i + j * intra_stride];
  }
}
```

Good:

```cpp
// Idiomatic: stable base pointer with adjusted loop index bounds
for (int j = INTERINTRA_ML_BORDER; j < bh + INTERINTRA_ML_BORDER; ++j) {
  for (int i = 0; i < INTERINTRA_ML_BORDER; ++i) {
    intra_input[interpreter_offset++] = intra_pred[i + j * intra_stride];
  }
}
```
