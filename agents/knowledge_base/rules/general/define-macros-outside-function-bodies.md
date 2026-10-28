---
name: libaom/define-macros-outside-function-bodies
description: When defining preprocessor macros (#define), declare them at file scope outside function bodies rather than placing macro definitions inside functions
rubric: readability
severity: should_fix
---

**Rule.** Do not define preprocessor macros (#define) inside function bodies;
declare them at file scope outside functions (such as immediately preceding the
functions that use them) or in appropriate headers. When a helper macro is used
locally within a source file, define it before the using function and undefine
it with #undef after use if appropriate.

**Why.** In C and C++, preprocessor directives do not respect lexical block or
function scoping; macros defined inside a function remain visible for the
remainder of the translation unit. Defining macros inside function bodies
creates a misleading impression of local scope and violates codebase style
conventions.

## Examples

Bad:

```cpp
int av1_encodedframe_overshoot_cbr(AV1_COMP *cpi, int *q) {
  if (cpi->svc.spatial_layer_id > 0) {
#define CHECK_INTER_LAYER_PRED(ref_frame)                         \
  ((cpi->ref_frame_flags & av1_ref_frame_flag_list[ref_frame]) && \
   (av1_check_ref_is_low_spatial_res_super_frame(cpi, ref_frame)))
    inter_layer_pred_on = CHECK_INTER_LAYER_PRED(LAST_FRAME) || ...;
  }
}
```

Good:

```cpp
#define CHECK_INTER_LAYER_PRED(ref_frame)                         \
  ((cpi->ref_frame_flags & av1_ref_frame_flag_list[ref_frame]) && \
   (av1_check_ref_is_low_spatial_res_super_frame(cpi, ref_frame)))

int av1_encodedframe_overshoot_cbr(AV1_COMP *cpi, int *q) {
  if (cpi->svc.spatial_layer_id > 0) {
    inter_layer_pred_on = CHECK_INTER_LAYER_PRED(LAST_FRAME) || ...;
  }
}
#undef CHECK_INTER_LAYER_PRED
```
