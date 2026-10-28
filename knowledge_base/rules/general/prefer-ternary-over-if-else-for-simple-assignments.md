---
name: libaom/prefer-ternary-over-if-else-for-simple-assignments
description: Use ternary expressions for binary assignments, threshold initializations, and table pointer selection instead of mutable if-else branching or repeated member accesses
rubric: readability
severity: nit
---

**Rule.** Prefer direct ternary conditional expressions (target = condition ?
val_true : val_false;) over multi-line if-else branches or mutable default
variables overridden by if statements when conditionally assigning variables,
fields, or threshold flags. Qualify the resulting variable with const when it is
not modified further. Additionally, when accessing multiple fields or elements
from one of two alternative lookup tables, arrays, or structs based on a
condition, select the base pointer once into a const local pointer (const T
*const ptr = condition ? table_a : table_b;) instead of duplicating member
accesses across branches or repeating conditional checks.

**Why.** Using ternary conditional expressions and direct const initialization
eliminates unnecessary mutable state, reduces indentation nesting, avoids dead
stores or accidental override bugs, and makes conditional assignments concise.
Selecting table or struct pointers once into a const pointer avoids repetitive
conditional evaluations and ensures consistent, deduplicated member access
across branches.

## Examples

Bad:

```cpp
// Bad: Multi-line if-else or mutable default overrides for simple assignments
if (source_sad_nonrd >= kMedSad)
  x->color_sensitivity[COLOR_SENS_IDX(AOM_PLANE_U)] = 1;
else
  x->color_sensitivity[COLOR_SENS_IDX(AOM_PLANE_U)] = 0;

int allow_lower_qc = 1;
if (sharpness) {
  allow_lower_qc = abs_qc > 1;
  if (si <= 5) allow_lower_qc = abs_qc > 2;
}

// Bad: Duplicating field accesses or repeating ternary expressions for table selection
if (is_one_pass_rt_lag_params(cpi)) {
  *this_mode = comp_ref_mode_set_full[idx].pred_mode;
  *ref_frame = comp_ref_mode_set_full[idx].ref_frame;
} else {
  *this_mode = comp_ref_mode_set[idx].pred_mode;
  *ref_frame = comp_ref_mode_set[idx].ref_frame;
}
```

Good:

```cpp
// Good: Direct ternary assignment and const initialization
x->color_sensitivity[COLOR_SENS_IDX(AOM_PLANE_U)] =
    source_sad_nonrd >= kMedSad ? 1 : 0;

const int qc_threshold = (si <= 5) ? 2 : 1;
const int allow_lower_qc = sharpness ? (abs_qc > qc_threshold) : 1;

// Good: Assign active table pointer once using ternary operator
const COMP_REF_MODE *const comp_modes =
    is_one_pass_rt_lag_params(cpi) ? comp_ref_mode_set_full : comp_ref_mode_set;
*this_mode = comp_modes[idx].pred_mode;
*ref_frame = comp_modes[idx].ref_frame;
```
