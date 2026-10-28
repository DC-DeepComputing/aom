---
name: libaom/av1/document-or-adjust-thresholds-for-high-bit-depth
description: When adding empirical variance, SAD, or error metric thresholds tuned on 8-bit content, adjust thresholds for high bit depth or document with a TODO
rubric: readability
severity: nit
---

**Rule.** When introducing empirical thresholds based on pixel variance, SAD,
MSE, or block error metrics that have only been tuned for 8-bit (low bit depth)
content, explicitly adjust or scale the thresholds for high bit depth (10-bit or
12-bit) frames, or document the limitation with an attributed TODO comment.

**Why.** Pixel variance, SAD, and MSE metrics scale with dynamic range across
bit depths (e.g., 10-bit differences are 4× larger and variances are 16× larger
than 8-bit). Applying unadjusted 8-bit empirical thresholds to high bit depth
frames causes heuristics to behave inconsistently or misclassify blocks, while
documenting untuned thresholds with a TODO ensures high bit depth tuning is
tracked.

## Examples

Bad:

```cpp
// Empirical threshold tuned only on 8-bit content without scaling or documentation for HBD.
if ((blk_4x4_var_max - blk_4x4_var_min) <= 4.0) {
  allow_me_for_sub_blks = false;
}
```

Good:

```cpp
// Explicitly tracked with an attributed TODO for high bit depth tuning.
// TODO(author@aomedia.org): Experiment and adjust the threshold for high bit depth.
if ((blk_4x4_var_max - blk_4x4_var_min) <= 4.0) {
  allow_me_for_sub_blks = false;
}
```
