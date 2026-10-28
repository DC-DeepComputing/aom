---
name: libaom/aom_dsp/split-dsp-primitives-from-downstream-callers
description: When introducing new DSP or SIMD primitives and composite caller routines, split the base primitive and unit tests from downstream callers across separate patches
rubric: change_scope
severity: should_fix
---

**Rule.** When implementing new DSP or SIMD optimizations, split standalone
low-level primitives and their composite or downstream caller functions into
separate changes. First introduce and test the base DSP primitive alongside its
unit tests in test/, and then add composite caller functions and encoder/decoder
wiring in follow-up patches.

**Why.** Bundling new low-level primitives, composite caller variants, and
call-site wiring in a single change results in oversized patches that are
difficult to review. Separating primitives from downstream callers keeps reviews
focused, ensures isolated test verification, and simplifies bisection if
regressions occur.

## Examples

Bad:

```cpp
// Single CL introducing both base DSP primitive and composite caller functions
void aom_highbd_dist_wtd_comp_avg_pred_neon(...) { ... }
void aom_highbd_dist_wtd_comp_avg_upsampled_pred_neon(...) { ... }
```

Good:

```cpp
// CL 1: Implement base DSP primitive and unit tests
void aom_highbd_dist_wtd_comp_avg_pred_neon(...) { ... }

// CL 2: Implement composite callers and wiring in a follow-up patch
void aom_highbd_dist_wtd_comp_avg_upsampled_pred_neon(...) { ... }
```
