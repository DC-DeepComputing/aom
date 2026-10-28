---
name: libaom/av1/place-tuning-speed-features-before-mode-early-returns
description: When configuring encoder speed features or tuning overrides across multiple modes, place shared tuning checks before mode-specific early returns like REALTIME
rubric: correctness
severity: should_fix
---

**Rule.** In encoder speed feature functions (such as
av1_set_speed_features_qindex_dependent or
av1_set_speed_features_framesize_dependent), ensure tuning-dependent overrides
or shared heuristics that apply across multiple encoding modes are evaluated
before mode-specific early returns (such as if (cpi->oxcf.mode == REALTIME) {
return; }). Do not place tuning-dependent logic after early exits of modes where
the tuning options are intended to take effect.

**Why.** Early returns for specific modes (such as REALTIME) bypass all
subsequent speed feature adjustments in the function. Placing tuning overrides
(such as AOM_TUNE_IQ or AOM_TUNE_SSIMULACRA2) after a mode-specific early return
prevents those tunings from taking effect in that mode, causing unexpected
quality or rate-distortion behavior.

## Examples

Bad:

```cpp
if (cpi->oxcf.mode == REALTIME) {
  if (speed >= 6) {
    sf->lpf_sf.cdef_strength_filter_level = 1;
  }
  return;
}

// Bug: REALTIME mode returns early above and never reaches this tuning override!
if (cpi->oxcf.mode == ALLINTRA || cpi->oxcf.tune_cfg.tuning == AOM_TUNE_IQ ||
    cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2) {
  if (cm->quant_params.base_qindex <= 140) {
    sf->lpf_sf.zero_low_cdef_strengths = 1;
  }
}
```

Good:

```cpp
// Evaluate multi-mode tuning overrides before mode-specific early returns
if (cpi->oxcf.mode == ALLINTRA || cpi->oxcf.tune_cfg.tuning == AOM_TUNE_IQ ||
    cpi->oxcf.tune_cfg.tuning == AOM_TUNE_SSIMULACRA2) {
  if (cm->quant_params.base_qindex <= 140) {
    sf->lpf_sf.zero_low_cdef_strengths = 1;
  }
}

if (cpi->oxcf.mode == REALTIME) {
  if (speed >= 6) {
    sf->lpf_sf.cdef_strength_filter_level = 1;
  }
  return;
}
```
