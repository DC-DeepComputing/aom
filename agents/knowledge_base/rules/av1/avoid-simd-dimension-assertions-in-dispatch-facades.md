---
name: libaom/av1/avoid-simd-dimension-assertions-in-dispatch-facades
description: When implementing high-level convolution or filtering dispatch facades, do not assert SIMD block dimension constraints when delegating to functions with internal fallbacks
rubric: readability
severity: should_fix
---

**Rule.** In high-level convolution and filtering dispatch facades, do not
assert architecture- or SIMD-specific block dimension constraints (such as
assert(!(w % 8)) or assert(w >= 8)). Keep dispatch facades width- and
architecture-agnostic, and allow lower-level convolution and RTCD functions to
manage their own dimension checks and fallback to portable C routines for
unsupported block sizes.

**Why.** High-level dispatch facades should provide a uniform interface for all
valid block dimensions. Imposing backend-specific dimension assertions at the
facade layer causes false-positive assertion failures on valid block sizes and
leaks low-level SIMD implementation constraints into high-level dispatch logic.

## Examples

Bad:

```cpp
if (filter_params_x->taps > 8 || filter_params_y->taps > 8)
  assert(!(w % 8));
av1_convolve_x_sr(src, src_stride, dst, dst_stride, w, h, filter_params_x,
                  subpel_x_qn, conv_params);
```

Good:

```cpp
// Low-level av1_convolve_x_sr handles dimension limits and fallbacks internally.
av1_convolve_x_sr(src, src_stride, dst, dst_stride, w, h, filter_params_x,
                  subpel_x_qn, conv_params);
```
