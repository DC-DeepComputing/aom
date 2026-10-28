---
name: libaom/prefer-break-over-mutating-loop-counter
description: When handling errors or matches in loops, terminate early via explicit break or helper return rather than mutating loop counters, checking header flags, or wrapping bodies in if (!error)
rubric: readability
severity: should_fix
---

**Rule.** When an error, allocation failure (such as from aom_malloc), or early
exit condition is detected inside a loop, terminate the loop immediately using
an explicit break (or return) statement. Do not allow loops to continue
iterating across remaining passes while guarding loop bodies with if (!error)
checks, testing error flags in the loop header condition (&& !error), or
mutating loop induction variables/counters to their terminal bounds (e.g.,
ref_frame = ALTREF_FRAME or x = row_max; y = col_max). Keep loop header
conditions focused strictly on iteration bounds. For nested search loops or
multi-level iterations, factor the search into a dedicated static or static
inline helper function that returns early upon finding a match or encountering
an error rather than mutating outer loop counters or propagating multi-level
sentinel exit flags.

**Why.** Guarding loop bodies with if (!error) executes wasteful empty loop
cycles once an error has occurred, increases nesting depth, and obscures control
flow. Testing error flags in loop condition headers causes redundant checks on
every iteration and risks continuing loop execution past the failure point if
statements follow the check. Mutating loop induction variables inside loop
bodies obscures control flow, complicates loop invariants, and risks off-by-one
errors or unintended post-loop variable states. Using break for single loops or
extracting nested loops into dedicated helper functions with early return makes
early-exit intent explicit, fails fast, keeps loop headers strictly focused on
iteration bounds, and isolates local search state.

## Examples

Bad:

```cpp
// Bad: wrapping loop body in if (!error) or checking error in header
for (int j = 0; j < num_buffers; ++j) {
  if (!error) {
    buf[k][j] = (uint8_t *)aom_malloc(size);
    if (!buf[k][j]) error = true;
  }
}
for (j = 0; j < 2 && !error; ++j) {
  block_hash_values[k][j] = (uint32_t *)aom_malloc(size);
  if (!block_hash_values[k][j]) error = true;
}

// Bad: mutating loop counters to force termination
for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
  if (cpi->ppi->rtc_ref.reference[ref_frame - 1] == 1) {
    first_ref_map_idx = cpi->ppi->rtc_ref.ref_idx[ref_frame - 1];
    ref_frame = ALTREF_FRAME;
  }
}

// Bad: mutating outer loop indices across nested loops
for (int x = 0; x < row_max; x += 4) {
  for (int y = 0; y < col_max; y += 4) {
    if (active_map_4x4[(r + x) * mi_cols + (c + y)] == AM_SEGMENT_ID_ACTIVE) {
      block_is_active = 1;
      x = row_max; y = col_max;
    }
  }
}
```

Good:

```cpp
// Good: break immediately on error or allocation failure
for (int j = 0; j < num_buffers; ++j) {
  buf[k][j] = (uint8_t *)aom_malloc(size);
  if (!buf[k][j]) {
    error = true;
    break;
  }
}

// Good: explicit break statement
for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
  if (cpi->ppi->rtc_ref.reference[ref_frame - 1] == 1) {
    first_ref_map_idx = cpi->ppi->rtc_ref.ref_idx[ref_frame - 1];
    break;
  }
}

// Good: factor nested loops into helper function with early return
static int is_block_active(const unsigned char *active_map_4x4, int mi_cols,
                           int r, int c, int row_max, int col_max) {
  for (int x = 0; x < row_max; x += 4) {
    for (int y = 0; y < col_max; y += 4) {
      if (active_map_4x4[(r + x) * mi_cols + (c + y)] == AM_SEGMENT_ID_ACTIVE) {
        return 1;
      }
    }
  }
  return 0;
}
```
