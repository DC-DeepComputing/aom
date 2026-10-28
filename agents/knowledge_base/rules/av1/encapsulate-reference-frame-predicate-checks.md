---
name: libaom/av1/encapsulate-reference-frame-predicate-checks
description: When evaluating compound predicate checks across multiple reference frames, encapsulate the check in a helper and chain with logical operators instead of nested if-else ladders
rubric: design
severity: nit
---

**Rule.** When checking multi-step conditions or flags across multiple reference
frames (such as LAST_FRAME, GOLDEN_FRAME, and ALTREF_FRAME), encapsulate the
per-frame predicate in a local helper macro or inline function and combine
candidate results with logical operators (|| or &&). Do not write chained
if/else if ladders with nested inner checks, as failing an inner condition on an
earlier reference frame skips evaluating subsequent candidate frames.

**Why.** Writing nested if/else if ladders to test compound conditions across
candidate reference frames introduces logic bugs where matching an outer flag
test but failing an inner property check prevents evaluation of remaining valid
reference frames. Encapsulating the compound check in a helper and chaining with
short-circuiting logical operators deduplicates logic and ensures all candidate
reference frames are evaluated correctly.

## Examples

Bad:

```cpp
if (cpi->ref_frame_flags & av1_ref_frame_flag_list[LAST_FRAME]) {
  if (av1_check_ref_is_low_spatial_res_super_frame(cpi, LAST_FRAME))
    inter_layer_pred_on = 1;
} else if (cpi->ref_frame_flags & av1_ref_frame_flag_list[GOLDEN_FRAME]) {
  if (av1_check_ref_is_low_spatial_res_super_frame(cpi, GOLDEN_FRAME))
    inter_layer_pred_on = 1;
} else if (cpi->ref_frame_flags & av1_ref_frame_flag_list[ALTREF_FRAME]) {
  if (av1_check_ref_is_low_spatial_res_super_frame(cpi, ALTREF_FRAME))
    inter_layer_pred_on = 1;
}
```

Good:

```cpp
#define CHECK_INTER_LAYER_PRED(ref_frame)                         \
  ((cpi->ref_frame_flags & av1_ref_frame_flag_list[ref_frame]) && \
   av1_check_ref_is_low_spatial_res_super_frame(cpi, ref_frame))

  inter_layer_pred_on = CHECK_INTER_LAYER_PRED(LAST_FRAME) ||
                        CHECK_INTER_LAYER_PRED(GOLDEN_FRAME) ||
                        CHECK_INTER_LAYER_PRED(ALTREF_FRAME);
#undef CHECK_INTER_LAYER_PRED
```
