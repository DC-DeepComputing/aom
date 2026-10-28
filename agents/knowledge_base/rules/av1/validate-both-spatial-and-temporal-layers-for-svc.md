---
name: libaom/av1/validate-both-spatial-and-temporal-layers-for-svc
description: When validating or guarding encoder configurations for SVC mode, check both spatial and temporal layers or the use_svc flag instead of spatial layers alone
rubric: correctness
severity: should_fix
---

**Rule.** When validating encoder configurations or guarding features against
unsupported Scalable Video Coding (SVC) modes, check both spatial scalability
(number_spatial_layers > 1) and temporal scalability (number_temporal_layers >
1), or inspect the consolidated use_svc flag. Do not check number_spatial_layers
alone when a restriction applies to all SVC configurations.

**Why.** SVC in libaom supports temporal scalability in addition to spatial
scalability. Restricting checks solely to number_spatial_layers > 1 overlooks
temporal-only SVC streams (number_temporal_layers > 1), allowing unsupported
configurations or incorrect encoder behavior to proceed when temporal layers are
active.

## Examples

Bad:

```cpp
// Incomplete check: ignores temporal-only SVC configurations
if (ppi->number_spatial_layers > 1) {
  if (cpi->oxcf.gf_cfg.lag_in_frames > 0 && cpi->oxcf.mode == GOOD) {
    return AOM_CODEC_INVALID_PARAM;
  }
}
```

Good:

```cpp
// Complete check: accounts for both spatial and temporal scalability
if (ppi->number_spatial_layers > 1 || ppi->number_temporal_layers > 1) {
  if (cpi->oxcf.gf_cfg.lag_in_frames > 0 && cpi->oxcf.mode == GOOD) {
    return AOM_CODEC_INVALID_PARAM;
  }
}
```
