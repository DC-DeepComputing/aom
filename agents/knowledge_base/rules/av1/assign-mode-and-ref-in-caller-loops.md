---
name: libaom/av1/assign-mode-and-ref-in-caller-loops
description: When evaluating or dispatching encoder prediction modes, assign mode and reference frame state directly in caller loops rather than mutating out-parameters inside setup sub-helpers
rubric: design
severity: should_fix
---

**Rule.** In encoder mode evaluation and iteration routines (such as inter,
compound, or intra mode search), assign mode (PREDICTION_MODE) and reference
frame (MV_REFERENCE_FRAME) variables directly in the caller loop or dispatcher
function managing mode selection. Do not pass mode and reference frame
out-parameters into sub-helpers (such as compound parameter setup or validation
routines) to mutate caller iteration state as a side effect.

**Why.** Assigning mode and reference frame variables directly in the caller
loop keeps mode iteration and data flow transparent, aligns compound mode
evaluation with single-reference mode handling, and prevents sub-helpers from
concealing mutations to caller iteration state.

## Examples

Bad:

```cpp
// Sub-helper mutates caller's iteration mode and ref_frame as out-parameters:
static inline int setup_compound_params_from_comp_idx(
    const AV1_COMP *cpi, int comp_index, PREDICTION_MODE *this_mode,
    MV_REFERENCE_FRAME *ref_frame, MV_REFERENCE_FRAME *ref_frame2, ...) {
  const COMP_REF_MODE_SET *comp_set =
      is_one_pass_rt_lag_params(cpi) ? comp_ref_mode_set_full : comp_ref_mode_set;
  *this_mode = comp_set[comp_index].pred_mode;
  *ref_frame = comp_set[comp_index].ref_frame[0];
  *ref_frame2 = comp_set[comp_index].ref_frame[1];
  ...
}
```

Good:

```cpp
// Caller managing mode iteration sets mode and reference frames directly:
if (idx >= num_inter_modes) {
  const int comp_index = idx - num_inter_modes;
  const COMP_REF_MODE_SET *comp_set =
      is_one_pass_rt_lag_params(cpi) ? comp_ref_mode_set_full : comp_ref_mode_set;
  *this_mode = comp_set[comp_index].pred_mode;
  *ref_frame = comp_set[comp_index].ref_frame[0];
  *ref_frame2 = comp_set[comp_index].ref_frame[1];
  if (!setup_compound_params_from_comp_idx(cpi, x, comp_index, ...)) {
    return true;
  }
}
```
