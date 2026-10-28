---
name: libaom/av1/account-for-encoder-mode-in-rdmult-tuning
description: When computing rate-distortion multipliers (rdmult) or designing mode-specific tuning adjustments, ensure helper functions condition on the encoder mode (oxcf.mode) rather than applying tunings unconditionally
rubric: correctness
severity: should_fix
---

**Rule.** When designing visual tuning metrics or adjusting rate-distortion
multipliers (such as in av1_compute_rd_mult or
av1_compute_rd_mult_based_on_qindex), ensure calculations explicitly account for
the encoder mode (cpi->oxcf.mode or MODE) rather than assuming tuning
configurations (such as AOM_TUNE_* adjustments) apply unconditionally across all
pipeline stages. Specifically, verify that mode-specific adjustments targeting
intra-only modes (e.g., ALLINTRA) or realtime modes are not erroneously applied
in inter-only analysis modules like TPL (temporal dependency model).

**Why.** The TPL model evaluates temporal dependencies strictly for inter-frame
coding and is inactive in all-intra mode. If helper functions adjust
rate-distortion multipliers based on a global tuning setting without verifying
the active encoder mode, inter-frame modules like TPL can receive inappropriate
intra-specific or mode-specific RD multiplier offsets, degrading rate-distortion
optimization.

## Examples

Bad:

```cpp
// Missing encoder mode parameter; tuning adjustments apply unconditionally in TPL.
int rdmult = av1_compute_rd_mult(
    base_qindex, cm->seq_params->bit_depth,
    cpi->ppi->gf_group.update_type[cpi->gf_frame_index], layer_depth,
    boost_index, frame_type, cpi->oxcf.q_cfg.use_fixed_qp_offsets,
    is_stat_consumption_stage(cpi), cpi->oxcf.tune_cfg.tuning);
```

Good:

```cpp
// Passes oxcf.mode so mode-specific tuning adjustments (e.g. ALLINTRA) are gated.
int rdmult = av1_compute_rd_mult(
    base_qindex, cm->seq_params->bit_depth,
    cpi->ppi->gf_group.update_type[cpi->gf_frame_index], layer_depth,
    boost_index, frame_type, cpi->oxcf.q_cfg.use_fixed_qp_offsets,
    is_stat_consumption_stage(cpi), cpi->oxcf.tune_cfg.tuning,
    cpi->oxcf.mode);
```
