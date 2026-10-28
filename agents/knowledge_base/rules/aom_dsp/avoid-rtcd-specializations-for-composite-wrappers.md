---
name: libaom/aom_dsp/avoid-rtcd-specializations-for-composite-wrappers
description: When creating composite wrapper functions that sequence calls to other RTCD functions without SIMD-specific logic, do not add RTCD specializations; implement them as plain C helpers
rubric: design
severity: should_fix
---

**Rule.** Do not declare RTCD (runtime CPU detection) function prototypes or
architecture-specific specializations for composite wrapper functions that
merely sequence calls to other RTCD functions without dedicated SIMD operations.
Implement composite wrappers as plain C helper functions that call the
underlying RTCD-dispatched primitives directly.

**Why.** Registering composite wrapper functions in RTCD definitions causes
dispatch table bloat, redundant indirect call overhead, and duplicate wrapper
code across architecture backends. Calling RTCD primitives directly from a
single C helper ensures optimal execution of each component operation without
unnecessary RTCD dispatch layers.

## Examples

Bad:

```cpp
// In aom_dsp/aom_dsp_rtcd_defs.pl:
// add_proto qw/void aom_comp_mask_upsampled_pred/, "...";
// specialize qw/aom_comp_mask_upsampled_pred ssse3/;

void aom_comp_mask_upsampled_pred_ssse3(uint8_t *comp_pred, const uint8_t *pred,
                                        int width, int height, int subpel_x_q3,
                                        int subpel_y_q3, const uint8_t *ref,
                                        int ref_stride, const uint8_t *mask,
                                        int mask_stride, int invert_mask) {
  if (subpel_x_q3 || subpel_y_q3) {
    aom_upsampled_pred(comp_pred, width, height, subpel_x_q3, subpel_y_q3, ref,
                       ref_stride);
    ref = comp_pred;
    ref_stride = width;
  }
  aom_comp_mask_pred_ssse3(comp_pred, pred, width, height, ref, ref_stride,
                           mask, mask_stride, invert_mask);
}
```

Good:

```cpp
// Plain C helper calling RTCD-dispatched primitives directly without RTCD specialization:
void aom_comp_mask_upsampled_pred(uint8_t *comp_pred, const uint8_t *pred,
                                  int width, int height, int subpel_x_q3,
                                  int subpel_y_q3, const uint8_t *ref,
                                  int ref_stride, const uint8_t *mask,
                                  int mask_stride, int invert_mask) {
  if (subpel_x_q3 || subpel_y_q3) {
    aom_upsampled_pred(comp_pred, width, height, subpel_x_q3, subpel_y_q3, ref,
                       ref_stride);
    ref = comp_pred;
    ref_stride = width;
  }
  aom_comp_mask_pred(comp_pred, pred, width, height, ref, ref_stride,
                     mask, mask_stride, invert_mask);
}
```
