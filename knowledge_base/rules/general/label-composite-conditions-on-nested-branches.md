---
name: libaom/label-composite-conditions-on-nested-branches
description: When writing complex, nested, or multi-way conditional dispatch ladders, label each branch and fallback else with comments summarizing composite active conditions and variable ranges
rubric: readability
severity: nit
---

**Rule.** In complex multi-variable dispatch ladders, nested switches, or deeply
nested if-else structures (such as DSP functions dispatching on combinations of
subpixel offsets xoffset/yoffset, filter dimensions, block sizes bsize, or
transform modes tx_size), label each if, else if, and fallback else branch with
an inline or block comment stating the full composite condition and active
variable ranges (e.g., `// xoffset == 0 && yoffset == 0` or `// xoffset == 0 &&
yoffset == 1, 2, 3, 5, 6, 7`).

**Why.** In long functions with deeply nested conditional trees and
multi-variable ladders spanning many lines, readers easily lose track of outer
conditions, and else if / else branches implicitly represent composite negations
of prior branches. Explicitly documenting active composite conditions and
variable ranges clarifies control flow, prevents incorrect assumptions during
maintenance and refactoring, and simplifies auditing against reference
specifications.

## Examples

Bad:

```cpp
// Bad: Multi-variable nested branches and fallback else branches lack composite condition comments.
if (xoffset == 0) {
  if (yoffset == 0) {
    highbd_variance8x8_c(src, src_stride, dst, dst_stride, sse, sum);
  } else if (yoffset == 4) {
    highbd_sub_pixel_variance8x8_c(src, src_stride, 0, 4, dst, dst_stride, sse, sum);
  } else {
    highbd_sub_pixel_variance8x8_c(src, src_stride, 0, yoffset, dst, dst_stride, sse, sum);
  }
} else if (xoffset == 4) {
  if (yoffset == 0) {
    compute_subpel_variance_40(src, dst, w, h);
  } else {
    variance_kernel_general(src, dst, w, h);
  }
} else if (yoffset == 0) {
  switch (output_width) {
    case 8:
      compute_subpel_variance_04(src, dst, w, h);
      break;
    default:
      variance_kernel_general(src, dst, w, h);
      break;
  }
}
```

Good:

```cpp
// Good: Each branch explicitly documents the full active condition and variable ranges.
if (xoffset == 0) {
  if (yoffset == 0) {  // xoffset == 0 && yoffset == 0
    highbd_variance8x8_c(src, src_stride, dst, dst_stride, sse, sum);
  } else if (yoffset == 4) {  // xoffset == 0 && yoffset == 4
    highbd_sub_pixel_variance8x8_c(src, src_stride, 0, 4, dst, dst_stride, sse, sum);
  } else {  // xoffset == 0 && yoffset == 1, 2, 3, 5, 6, 7
    highbd_sub_pixel_variance8x8_c(src, src_stride, 0, yoffset, dst, dst_stride, sse, sum);
  }
} else if (xoffset == 4) {
  if (yoffset == 0) {  // xoffset == 4 && yoffset == 0
    compute_subpel_variance_40(src, dst, w, h);
  } else {  // xoffset == 4 && yoffset != 0
    variance_kernel_general(src, dst, w, h);
  }
// xoffset is in {1, 2, 3, 5, 6, 7}
} else if (yoffset == 0) {  // xoffset != 0, 4 && yoffset == 0
  switch (output_width) {
    case 8:
      compute_subpel_variance_04(src, dst, w, h);
      break;
    default:
      variance_kernel_general(src, dst, w, h);
      break;
  }
}
```
