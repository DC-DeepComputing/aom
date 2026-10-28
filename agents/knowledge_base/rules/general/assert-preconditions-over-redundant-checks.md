---
name: libaom/assert-preconditions-over-redundant-checks
description: Enforce caller-guaranteed preconditions, omitted special-case branches, and structural invariants in internal functions and DSP kernels using assertions instead of redundant runtime checks or fallbacks
rubric: readability
severity: should_fix
---

**Rule.** In internal library functions and architecture-optimized DSP routines
(such as convolve, filter, transform, warp, or rate control), enforce
caller-guaranteed preconditions (such as non-NULL pointer arguments, buffer
dimensions, alignment, strides, or upstream-handled filter cases) and structural
codec invariants using active debug assertions (`assert()` or `aom_assert()`).
Do not introduce redundant runtime `if` checks, defensive error returns (such as
`AOM_CODEC_INVALID_PARAM`), silent fallback branches, or fallback C function
calls (e.g., calling `*_c` routines) for conditions guaranteed by caller
contracts or preceding control flow. Additionally, do not wrap assertions in
defensive runtime `if` guards (e.g., `if (ptr) assert(...)`) when the resource
is expected to exist, and ensure all required pointer parameters are explicitly
asserted before dereference. When eliminating redundant runtime checks, bypass
branches, special-case branches (such as no-op or identity filter handling), or
dynamic fallback/reallocation logic because callers handle them upstream,
replace them with an `assert()` statement and explanatory comment documenting
the invariant rather than silently deleting checks or leaving dead fallback
branches. Reserve defensive runtime error checks for external public API
boundaries and untrusted user input.

**Why.** Internal library functions and optimized DSP routines operate within
trusted subsystem invariants where contract violations are programmer bugs
rather than recoverable runtime conditions. Defensive branching, dead C fallback
routines, and error returns add branch overhead in performance-critical paths,
obscure caller contracts, and mask bugs in production builds. Using `assert()`
or `aom_assert()` with explanatory comments validates preconditions and
structural invariants in debug builds with zero release-build overhead, verifies
why fallback or special-case logic is unnecessary, and documents caller
guarantees explicitly.

## Examples

Bad:

```cpp
// Variant 1: Defensive error returns in internal functions
aom_codec_err_t av1_extrc_send_tpl_stats(AOM_EXT_RATECTRL *ext_ratectrl,
                                         const AomTplGopStats *tpl_gop_stats) {
  if (ext_ratectrl == NULL || tpl_gop_stats == NULL) {
    return AOM_CODEC_INVALID_PARAM;
  }
  return AOM_CODEC_OK;
}

// Variant 2: Defensive branch to C fallback for impossible dimensions
if ((filter_params_x->taps > 8 || filter_params_y->taps > 8) && w < 4) {
  av1_convolve_x_sr_c(src, src_stride, dst, dst_stride, w, h,
                      filter_params_x, subpel_x_qn, conv_params);
} else {
  av1_convolve_x_sr(src, src_stride, dst, dst_stride, w, h, filter_params_x,
                    subpel_x_qn, conv_params);
}

// Variant 3: Silently omitting special-case check without asserting caller invariant
void convolve_dotprod(const int16_t *filter, ...) {
  // Caller guarantees filter[5] != 128 (no-op filter handled upstream),
  // but no assertion documents or verifies it.
  process_12tap_filter(filter, ...);
}
```

Good:

```cpp
// Variant 1: Assert non-NULL pointer parameters at internal function entry
aom_codec_err_t av1_extrc_send_tpl_stats(
    AOM_EXT_RATECTRL *ext_ratectrl, const AomTplGopStats *extrc_tpl_gop_stats) {
  assert(ext_ratectrl != NULL);
  assert(extrc_tpl_gop_stats != NULL);
  return AOM_CODEC_OK;
}

// Variant 2: Assert structural invariant and call primary dispatch directly
if (filter_params_x->taps > 8 || filter_params_y->taps > 8) {
  assert(w >= 4 && !(w % 8));
}
av1_convolve_x_sr(src, src_stride, dst, dst_stride, w, h, filter_params_x,
                  subpel_x_qn, conv_params);

// Variant 3: Assert caller invariant when omitting special-case branch
void convolve_dotprod(const int16_t *filter, ...) {
  assert(filter != NULL);
  // Caller handles identity/no-op filter upstream; should never reach kernel.
  assert(filter[5] != 128);
  process_12tap_filter(filter, ...);
}
```
