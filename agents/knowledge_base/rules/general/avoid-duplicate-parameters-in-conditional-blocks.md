---
name: libaom/avoid-duplicate-parameters-in-conditional-blocks
description: When declaring function parameters across conditional compilation preprocessor blocks, ensure conditionally guarded parameters do not duplicate existing unconditional parameters
rubric: correctness
severity: blocking
---

**Rule.** When declaring or updating function signatures with parameters guarded
by conditional preprocessor blocks (such as #if CONFIG_... or #if HAVE_...),
verify that conditionally compiled parameters are not already declared in the
unconditional parameter list. Ensure each parameter appears exactly once across
active build configurations to prevent duplicate parameter declarations and
shadowing errors.

**Why.** In C and C++, declaring a parameter inside a conditional compilation
block when that same parameter is already present unconditionally causes
redefinition and shadowing compilation errors whenever that build configuration
flag is enabled. Because optional flags are often disabled during default local
builds, such duplicate parameters easily escape local testing and break
specialized CI or downstream builds.

## Examples

Bad:

```cpp
static AOM_FORCE_INLINE bool handle_inter_mode_nonrd(
    AV1_COMP *cpi, MACROBLOCK *x, InterModeSearchStateNonrd *search_state,
    PICK_MODE_CONTEXT *ctx, PRED_BUFFER **this_mode_pred,
    PRED_BUFFER *tmp_buffer, InterPredParams inter_pred_params_sr,
    int *best_early_term, unsigned int *sse_zeromv_norm, bool *check_globalmv,
#if CONFIG_AV1_TEMPORAL_DENOISING
    PICK_MODE_CONTEXT *ctx, int64_t *zero_last_cost_orig,
    int denoise_svc_pickmode,
#endif
    int *rate_mv) {
```

Good:

```cpp
static AOM_FORCE_INLINE bool handle_inter_mode_nonrd(
    AV1_COMP *cpi, MACROBLOCK *x, InterModeSearchStateNonrd *search_state,
    PICK_MODE_CONTEXT *ctx, PRED_BUFFER **this_mode_pred,
    PRED_BUFFER *tmp_buffer, InterPredParams inter_pred_params_sr,
    int *best_early_term, unsigned int *sse_zeromv_norm, bool *check_globalmv,
#if CONFIG_AV1_TEMPORAL_DENOISING
    int64_t *zero_last_cost_orig, int denoise_svc_pickmode,
#endif
    int *rate_mv) {
```
