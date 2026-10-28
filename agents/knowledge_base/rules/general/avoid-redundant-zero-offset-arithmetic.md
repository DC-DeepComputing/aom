---
name: libaom/avoid-redundant-zero-offset-arithmetic
description: When calculating indices, coordinates, or buffer offsets, omit redundant zero arithmetic such as adding zero-initialized fields or literal multiplications like `0 * stride`
rubric: readability
severity: should_fix
---

**Rule.** Do not perform redundant zero arithmetic when calculating indices,
coordinates, or buffer offsets. Specifically:
1. Do not add offset variables, delta fields, or index adjustments when the term was just assigned to zero or is statically known to be zero; use or assign the base value directly.
2. Do not write redundant literal multiplications by zero (such as `0 * stride + offset` or `offset + 0 * stride`); simplify the expression directly to the constant offset or term.

**Why.** Adding zero-valued terms immediately after initializing them to zero or
multiplying variables by literal zero introduces dead arithmetic and visual
clutter. It can also mislead readers into believing the offset or stride
contributes dynamically in that calculation.

## Examples

Bad:

```cpp
// Redundant addition of zero-initialized field
gf_group->arf_src_offset[*frame_ind] = 0;
gf_group->display_idx[*frame_ind] =
    *cur_disp_idx + gf_group->arf_src_offset[*frame_ind];

// Redundant literal multiplication by zero
rec_idx[0] = 0 * ccso_stride_ext - 1;
rec_idx[1] = 0 * ccso_stride_ext + 1;
```

Good:

```cpp
// Base value used directly
gf_group->arf_src_offset[*frame_ind] = 0;
gf_group->display_idx[*frame_ind] = *cur_disp_idx;

// Simplified directly to constant offsets
rec_idx[0] = -1;
rec_idx[1] = 1;
```
