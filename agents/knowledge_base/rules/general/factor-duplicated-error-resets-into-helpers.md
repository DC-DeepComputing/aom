---
name: libaom/factor-duplicated-error-resets-into-helpers
description: When multiple validation or error return paths perform identical multi-field state resets or cleanup logic, extract the shared logic into a dedicated helper function
rubric: design
severity: should_fix
---

**Rule.** Extract repeated multi-statement state resets or cleanup sequences
across validation error exit paths into a dedicated static helper function. Do
not duplicate identical multi-field reset assignments before multiple error
returns.

**Why.** Duplicating multi-statement state resets across several error branches
increases boilerplate and risks inconsistencies when future maintenance updates
some reset paths but misses others. A centralized helper ensures consistent
state teardown across all error paths.

## Examples

Bad:

```cpp
if (params->max_quantizers[layer] > 63 ||
    params->min_quantizers[layer] < 0 ||
    params->min_quantizers[layer] > params->max_quantizers[layer]) {
  cpi->svc.number_spatial_layers = 1;
  cpi->svc.number_temporal_layers = 1;
  ppi->number_spatial_layers = 1;
  ppi->number_temporal_layers = 1;
  return AOM_CODEC_INVALID_PARAM;
}
...
if (params->scaling_factor_den[sl] < params->scaling_factor_num[sl]) {
  cpi->svc.number_spatial_layers = 1;
  cpi->svc.number_temporal_layers = 1;
  ppi->number_spatial_layers = 1;
  ppi->number_temporal_layers = 1;
  return AOM_CODEC_INVALID_PARAM;
}
```

Good:

```cpp
static void disable_svc(AV1_PRIMARY *ppi, AV1_COMP *cpi) {
  cpi->svc.number_spatial_layers = 1;
  cpi->svc.number_temporal_layers = 1;
  ppi->number_spatial_layers = 1;
  ppi->number_temporal_layers = 1;
  ppi->use_svc = 0;
}
...
if (params->max_quantizers[layer] > 63 ||
    params->min_quantizers[layer] < 0 ||
    params->min_quantizers[layer] > params->max_quantizers[layer]) {
  disable_svc(ppi, cpi);
  return AOM_CODEC_INVALID_PARAM;
}
...
if (params->scaling_factor_den[sl] < params->scaling_factor_num[sl]) {
  disable_svc(ppi, cpi);
  return AOM_CODEC_INVALID_PARAM;
}
```
