---
name: libaom/av1/document-joint-effects-of-coupled-tuning-adjustments
description: When tuning encoder parameters with coupled or opposing multi-stage adjustments (such as rdmult vs. trellis), document their joint synergistic effect and cross-reference related functions
rubric: readability
severity: nit
---

**Rule.** When introducing coupled, complementary, or seemingly counterintuitive
tuning adjustments across different encoder stages (such as opposing rdmult
biases between mode evaluation and trellis quantization), add a comment
explaining their joint synergistic effect and cross-referencing the
corresponding adjustment function or stage.

**Why.** Tuning adjustments that pull parameters in opposing directions across
different encoder stages appear contradictory or counterintuitive in isolation.
Documenting their joint interaction clarifies that the opposing trends are
intentional and helps future maintainers understand and preserve the tuning
synergy.

## Examples

Bad:

```cpp
// For the SSIMULACRA 2 tune, increase rshift from 2 to 4.
// This biases trellis quantization towards keeping more coefficients, helping
// preserve image features (like repeating patterns and camera noise/film
// grain), which improves SSIMULACRA 2 scores.
const int rshift =
    cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2 ? 4 : 2;
```

Good:

```cpp
// For the SSIMULACRA 2 tune, increase rshift from 2 to 4.
// This biases trellis quantization towards keeping more coefficients, and
// together with the SSIMULACRA2 rdmult adjustment in
// av1_compute_rd_mult_based_on_qindex(), this helps preserve image
// features (like repeating patterns and camera noise/film grain), which
// improves SSIMULACRA 2 scores.
const int rshift =
    cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2 ? 4 : 2;
```
