---
name: libaom/chain-mutually-exclusive-checks-with-else-if
description: When dispatching across mutually exclusive states, enum cases, or dimensions, chain with positive equality else-if or switch, explicitly handle valid cases, and verify terminal invariants
rubric: readability
severity: should_fix
---

**Rule.** When dispatching logic across mutually exclusive configuration fields,
enum values, filter tap sizes (such as vert_tap or horiz_tap), scaling ratios
(such as dst_w/src_w), or discrete block dimensions (such as height, width, bw,
bh, bsize, or tx_size):
1. Chain conditions using else if (including across #if/#else preprocessor directives) or a switch statement rather than writing consecutive independent if statements.
2. Use explicit positive equality comparisons (==) or explicit case labels for each supported variant rather than negative exclusion checks (such as else if (val != excluded_val)) or omitting valid no-op/default enum cases (such as NONE) by relying on variable pre-initialization.
3. In open-ended or non-exhaustive chains and switch statements, provide an explicit fallback else or default branch that falls back to standard behavior or traps invalid/unhandled values with an assertion (such as assert(0), aom_assert(0), or assert(0 && "...")) .
4. In exhaustive chains covering a fixed set of valid states, handle the final expected case directly inside the terminal else or #else block guarded by an inline comment and invariant assertion (e.g., assert(height == 16) or assert(bw == 4)) rather than evaluating a redundant trailing else if condition followed by else { assert(0); } or leaving a bare trailing else if that risks silent fallthrough.

**Why.** Independent if statements falsely imply multiple branches can execute
simultaneously and cause redundant condition checks or unintended execution if
state mutates. Negative exclusion checks (e.g., !=) and omitted switch cases
obscure which concrete variant is executed, bypass switch exhaustiveness checks,
and mask corrupt or unexpected inputs. Terminating non-exhaustive ladders with
fallback else or default branches with debug assertions traps invalid states
early, while handling the final expected case in exhaustive ladders directly in
else with an invariant assertion avoids runtime comparison overhead in release
builds while preserving strict debug-time contract verification.

## Examples

Bad:

```cpp
// Anti-pattern 1: Independent if statements and negative exclusion checks
if (tune_cfg.tuning == AOM_TUNE_SSIM) {
  set_ssim_scaling(cpi);
}
if (tune_cfg.tuning == AOM_TUNE_VMAF) {
  set_vmaf_scaling(cpi);
}

if (vert_tap == 4) {
  apply_filter_4tap(src, dst);
} else if (vert_tap != 12) {  // Implicitly handles 8-tap filter via exclusion
  apply_filter_8tap(src, dst);
} else {
  apply_filter_12tap(src, dst);
}

// Anti-pattern 2: Omitting valid enum cases and relying on pre-initialization
activation_fn activation = identity;
switch (layer_config->activation) {
  case RELU: activation = relu; break;
  case SOFTSIGN: activation = softsign; break;
}

// Anti-pattern 3: Redundant trailing else-if comparison in exhaustive chain
if (height == 64) {
  s0 = _mm_srai_epi16(s0, 5);
} else if (height == 32) {
  s0 = _mm_srai_epi16(s0, 4);
} else if (height == 16) {
  s0 = _mm_srai_epi16(s0, 3);
} else {
  assert(0);
}
```

Good:

```cpp
// Variant 1: Chained else if with explicit positive equality comparisons
if (tune_cfg.tuning == AOM_TUNE_SSIM) {
  set_ssim_scaling(cpi);
} else if (tune_cfg.tuning == AOM_TUNE_VMAF) {
  set_vmaf_scaling(cpi);
}

if (vert_tap == 4) {
  apply_filter_4tap(src, dst);
} else if (vert_tap == 8) {
  apply_filter_8tap(src, dst);
} else if (vert_tap == 12) {
  apply_filter_12tap(src, dst);
} else {
  assert(0 && "Unsupported filter tap size");
}

// Variant 2: Explicitly enumerated enum cases with asserting default
activation_fn activation = identity;
switch (layer_config->activation) {
  case NONE: activation = identity; break;
  case RELU: activation = relu; break;
  case SOFTSIGN: activation = softsign; break;
  default: assert(0 && "Invalid activation type"); break;
}

// Variant 3: Exhaustive chain terminating in else with invariant assertion
if (height == 64) {
  s0 = _mm_srai_epi16(s0, 5);
} else if (height == 32) {
  s0 = _mm_srai_epi16(s0, 4);
} else {
  assert(height == 16);
  s0 = _mm_srai_epi16(s0, 3);
}
```
