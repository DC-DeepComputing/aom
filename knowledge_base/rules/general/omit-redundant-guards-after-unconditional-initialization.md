---
name: libaom/omit-redundant-guards-after-unconditional-initialization
description: When local variables, struct fields, or loop bounds are guaranteed valid or non-zero by unconditional initialization or assertions, omit redundant runtime if checks
rubric: readability
severity: should_fix
---

**Rule.** Do not add or retain redundant defensive if checks (such as non-zero,
null, positivity guards, or always-true flags) for conditions already guaranteed
by unconditional initialization, constant initial values, preceding assert()
statements, or loop bounds. Eliminate dead branches, obsolete preconditions, and
unused scaffolding variables when data flow invariants ensure the condition is
always met.

**Why.** Defensive checks for conditions already guaranteed by initialization
constants, assertions, or loop execution add dead branches, unnecessary
indentation, and visual clutter. Removing redundant checks and their unused
scaffolding variables clarifies data flow invariants and makes code paths
explicit.

## Examples

Bad:

```cpp
// Redundant check when struct fields are unconditionally initialized
if (initial_dimensions->width && initial_dimensions->height &&
    (cm->width > initial_dimensions->width ||
     cm->height > initial_dimensions->height)) {
  realloc_buffers(cpi);
}

// Redundant check when preceding assertion and loop guarantee num_samples > 0
assert(mi_rows > 0 && mi_cols > 0);
for (int r = 0; r < mi_rows; r += 4) {
  for (int c = 0; c < mi_cols; c += 4) {
    num_samples++;
  }
}
if (num_samples > 0)
  cpi->rc.percent_blocks_inactive = (num_blocks_inactive * 100) / num_samples;

// Redundant check and dead flag variable initialized to constant and never modified
int valid_ref = 1;
if (ref_frame[i] == GOLDEN_FRAME && cpi->rc.frames_since_golden == 0)
  ref_frame[i] = LAST_FRAME;
if (valid_ref) {
  av1_enable_segfeature(seg, i, SEG_LVL_REF_FRAME);
  av1_set_segdata(seg, i, SEG_LVL_REF_FRAME, ref_frame[i]);
}
```

Good:

```cpp
// Direct comparison relying on initialization invariant
if (cm->width > initial_dimensions->width ||
    cm->height > initial_dimensions->height) {
  realloc_buffers(cpi);
}

// Unconditional computation relying on asserted non-zero bounds
assert(mi_rows > 0 && mi_cols > 0);
for (int r = 0; r < mi_rows; r += 4) {
  for (int c = 0; c < mi_cols; c += 4) {
    num_samples++;
  }
}
cpi->rc.percent_blocks_inactive = (num_blocks_inactive * 100) / num_samples;

// Unconditional execution without dead variable or redundant condition check
if (ref_frame[i] == GOLDEN_FRAME && cpi->rc.frames_since_golden == 0)
  ref_frame[i] = LAST_FRAME;
av1_enable_segfeature(seg, i, SEG_LVL_REF_FRAME);
av1_set_segdata(seg, i, SEG_LVL_REF_FRAME, ref_frame[i]);
```
