---
name: libaom/factor-identical-function-calls-out-of-conditional-branches
description: When conditional branches duplicate function calls differing only by callee or arguments, select the function pointer or arguments upfront and invoke shared logic once
rubric: design
severity: should_fix
---

**Rule.** Do not duplicate identical function calls or multi-statement execution
flows across `if`/`else` or `switch` branches when only the arguments or the
invoked helper function variant differ. Instead, conditionally prepare, compute,
or reassign differing arguments, or select the appropriate function variant
upfront using a local function pointer or ternary expression, and execute the
shared call flow once.

**Why.** Duplicating identical call sequences, field assignments, or loop logic
across branches solely to pass different arguments or call different helper
variants bloats code, obscures data flow, and creates maintenance hazards where
updates are applied to one branch but omitted in another. Isolating the
differing inputs or function selection keeps the common workflow DRY, clean, and
maintainable.

## Examples

Bad:

```cpp
// Case 1: Duplicated branch sequence differing only by helper function called
if (is_allintra) {
  quant_params->qmatrix_level_y = aom_get_qmlevel_allintra(
      quant_params->base_qindex, min_qmlevel, max_qmlevel);
  quant_params->qmatrix_level_u = aom_get_qmlevel_allintra(
      quant_params->base_qindex + quant_params->u_ac_delta_q, min_qmlevel,
      max_qmlevel);
} else {
  quant_params->qmatrix_level_y =
      aom_get_qmlevel(quant_params->base_qindex, min_qmlevel, max_qmlevel);
  quant_params->qmatrix_level_u =
      aom_get_qmlevel(quant_params->base_qindex + quant_params->u_ac_delta_q,
                      min_qmlevel, max_qmlevel);
}

// Case 2: Duplicated function invocation differing only by arguments
if (!subpel_x_q3 && !subpel_y_q3) {
  aom_comp_mask_pred_ssse3(comp_pred, pred, width, height, ref, ref_stride,
                           mask, mask_stride, invert_mask);
} else {
  aom_upsampled_pred(comp_pred, width, height, subpel_x_q3, subpel_y_q3, ref,
                     ref_stride);
  aom_comp_mask_pred_ssse3(comp_pred, pred, width, height, comp_pred, width,
                           mask, mask_stride, invert_mask);
}
```

Good:

```cpp
// Case 1: Select function pointer upfront and execute shared flow once
int (*get_qmlevel)(int, int, int) =
    is_allintra ? aom_get_qmlevel_allintra : aom_get_qmlevel;

quant_params->qmatrix_level_y =
    get_qmlevel(quant_params->base_qindex, min_qmlevel, max_qmlevel);
quant_params->qmatrix_level_u =
    get_qmlevel(quant_params->base_qindex + quant_params->u_ac_delta_q,
                min_qmlevel, max_qmlevel);

// Case 2: Conditionally prepare arguments and invoke function once
if (subpel_x_q3 || subpel_y_q3) {
  aom_upsampled_pred(comp_pred, width, height, subpel_x_q3, subpel_y_q3, ref,
                     ref_stride);
  ref = comp_pred;
  ref_stride = width;
}
aom_comp_mask_pred_ssse3(comp_pred, pred, width, height, ref, ref_stride,
                         mask, mask_stride, invert_mask);
```
