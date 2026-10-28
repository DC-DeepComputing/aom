---
name: libaom/av1/reject-unsupported-per-frame-flags-when-lagged
description: When handling per-frame encoding flags that modify unbuffered encoder state, verify g_lag_in_frames is zero and report AOM_CODEC_INCAPABLE via aom_internal_error
rubric: correctness
severity: should_fix
---

**Rule.** When adding or processing per-frame encoding flags (e.g. AOM_EFLAG_*)
that modify unbuffered encoder state, verify that frame lagging is disabled
(g_lag_in_frames == 0). If lagged frames are unsupported for the feature, check
ctx->cfg.g_lag_in_frames != 0 and report AOM_CODEC_INCAPABLE via
aom_internal_error() within setjmp scopes.

**Why.** When g_lag_in_frames is non-zero, frames enter a lookahead queue and
the frame encoded in the current call is not necessarily the frame just passed
in. Modifying unbuffered encoder state while frames are lagged causes flags to
apply to mismatched frames, and setjmp scopes require aom_internal_error() for
proper longjmp error unwinding.

## Examples

Bad:

```cpp
// Set up unbuffered internal flags without checking lag_in_frames compatibility
ppi->b_calculate_psnr = (flags & AOM_EFLAG_CALCULATE_PSNR) ||
                        (ctx->base.init_flags & AOM_CODEC_USE_PSNR);
```

Good:

```cpp
// Reject per-frame flag when lookahead buffering is enabled
if ((flags & AOM_EFLAG_CALCULATE_PSNR) && ctx->cfg.g_lag_in_frames != 0) {
  aom_internal_error(
      &ppi->error, AOM_CODEC_INCAPABLE,
      "Cannot calculate per-frame PSNR when g_lag_in_frames is nonzero");
}
ppi->b_calculate_psnr = (flags & AOM_EFLAG_CALCULATE_PSNR) ||
                        (ctx->base.init_flags & AOM_CODEC_USE_PSNR);
```
