---
name: libaom/avoid-redundant-boolean-ternary
description: When evaluating, assigning, or returning boolean flags, use direct relational, logical, or inverted expressions instead of redundant ternary operators or procedural if-branch updates
rubric: readability
severity: nit
---

**Rule.** Do not use ternary conditional operators that redundantly yield
boolean literals or integer flags (e.g., `cond ? true : false`, `cond ? 1 : 0`,
`cond ? false : true`, `flag ? 1 : 0`) or procedural if-branch updates to set
boolean variables. Assign or return the direct relational, logical, or inverted
comparison expression (`cond`, `!cond`, `ptr != nullptr`, inverted relational
operators) instead, and initialize boolean variables directly (qualifying with
const when immutable).

**Why.** In C and C++, relational comparisons (`==`, `!=`, `<`, `>`, `<=`,
`>=`), logical operators (`!`, `&&`, `||`), pointer checks, and boolean flags
already evaluate directly to truth values (`bool` in C++, `0` or `1` in C).
Wrapping conditions in ternary operators yielding boolean/integer literals or
using mutable procedural if-blocks adds unnecessary visual noise, branching, and
mutable state without altering runtime behavior.

## Examples

Bad:

```cpp
// Redundant ternary operators yielding boolean literals or integer flags
bool is_var_based =
    (cpi->sf.part_sf.partition_search_type == VAR_BASED_PARTITION) ? true
                                                                   : false;
const int border = (rc->prev_frame_is_dropped) ? 1 : 0;
const bool is_sad_avg = second_pred == nullptr ? false : true;
return (offset >= max_offset) ? false : true;

// Procedural if-update instead of direct initialization
bool ref_is_scaled = false;
if (ref->y_crop_height != cm->height || ref->y_crop_width != cm->width)
  ref_is_scaled = true;
```

Good:

```cpp
// Direct boolean and integer flag assignment and return
const bool is_var_based =
    (cpi->sf.part_sf.partition_search_type == VAR_BASED_PARTITION);
const int border = rc->prev_frame_is_dropped;
const bool is_sad_avg = second_pred != nullptr;
return offset < max_offset;

// Direct const boolean initialization from condition expression
const bool ref_is_scaled =
    ref->y_crop_height != cm->height || ref->y_crop_width != cm->width;
```
