---
name: libaom/return-early-from-loops-on-termination-conditions
description: When searching, matching, or validating in loops without cleanup requirements, return early on termination conditions instead of setting flags and breaking
rubric: readability
severity: nit
---

**Rule.** When a loop searches for an item, matches elements, validates
conditions, or detects an early termination condition where no resource cleanup
or post-processing is required, return directly (e.g., return false;, return
true;, or return result;) from inside the loop. Avoid declaring intermediate
boolean or integer flag variables (such as valid, found, existing, or match)
that are set inside the loop followed by break and a post-loop conditional
check.

**Why.** Returning immediately upon condition match or validation failure
eliminates intermediate flag variables, avoids unnecessary state tracking across
loop boundaries, reduces post-loop branching and nesting, and makes the exit
point and return value explicit.

## Examples

Bad:

```cpp
// Searching/matching:
int existing = 0;
for (int i = 0; i < refmv_count; ++i) {
  if (ref_mv_stack[i].this_mv.as_int == cand_mv.this_mv.as_int &&
      (!is_comp ||
       ref_mv_stack[i].comp_mv.as_int == cand_mv.comp_mv.as_int)) {
    existing = 1;
    break;
  }
}
if (existing) return false;
return true;

// Bounds validation:
int mv_valid = 1;
for (int i = 0; i < 1 + is_comp; ++i) {
  const int ref_x = mi_col * MI_SIZE + mv_col[i];
  const int ref_y = mi_row * MI_SIZE + mv_row[i];
  if (ref_x <= -block_width || ref_y <= -block_height ||
      ref_x >= frame_width || ref_y >= frame_height) {
    mv_valid = 0;
    break;
  }
}
if (!mv_valid) return false;
```

Good:

```cpp
// Searching/matching:
for (int i = 0; i < refmv_count; ++i) {
  if (ref_mv_stack[i].this_mv.as_int == cand_mv.this_mv.as_int &&
      (!is_comp ||
       ref_mv_stack[i].comp_mv.as_int == cand_mv.comp_mv.as_int)) {
    return false;
  }
}
return true;

// Bounds validation:
for (int i = 0; i < 1 + is_comp; ++i) {
  const int ref_x = mi_col * MI_SIZE + mv_col[i];
  const int ref_y = mi_row * MI_SIZE + mv_row[i];
  if (ref_x <= -block_width || ref_y <= -block_height ||
      ref_x >= frame_width || ref_y >= frame_height) {
    return false;
  }
}
return true;
```
