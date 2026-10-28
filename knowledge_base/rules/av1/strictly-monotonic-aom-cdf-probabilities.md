---
name: libaom/av1/strictly-monotonic-aom-cdf-probabilities
description: When defining CDF probability tables with AOM_CDF macros, ensure all argument thresholds are strictly monotonically increasing and below CDF_PROB_TOP
rubric: correctness
severity: should_fix
---

**Rule.** Ensure arguments passed to `AOM_CDF*` macros (such as `AOM_CDF2`
through `AOM_CDF16`) are strictly monotonically increasing probabilities
strictly less than `CDF_PROB_TOP` (32768). Do not pass individual symbol
probabilities, delta counts, or non-monotonic values.

**Why.** `AOM_CDF*` macros convert cumulative probability thresholds into
inverted CDF values (`AOM_ICDF(x) = CDF_PROB_TOP - x`) for arithmetic entropy
coding. Passing non-monotonic values or individual symbol probabilities produces
inverted intervals or negative symbol probabilities, corrupting entropy encoding
and decoding.

## Examples

Bad:

```cpp
// Non-monotonic values: passing symbol deltas or frequencies instead of cumulative sums
static const aom_cdf_prob default_interintra_mode_cdf[BLOCK_SIZE_GROUPS]
                                                     [CDF_SIZE(INTERINTRA_MODES)] = {
  { AOM_CDF5(8192, 16384, 24576, 2000) },
  { AOM_CDF5(1875, 11082, 27332, 2000) },
  { AOM_CDF5(2473, 9996, 26388, 2000) },
  { AOM_CDF5(4238, 11537, 25926, 2000) }
};
```

Good:

```cpp
// Strictly monotonically increasing cumulative probabilities (each < CDF_PROB_TOP / 32768)
static const aom_cdf_prob default_interintra_mode_cdf[BLOCK_SIZE_GROUPS]
                                                     [CDF_SIZE(INTERINTRA_MODES)] = {
  { AOM_CDF5(8192, 16384, 24576, 28672) },
  { AOM_CDF5(1875, 11082, 27332, 30000) },
  { AOM_CDF5(2473, 9996, 26388, 29500) },
  { AOM_CDF5(4238, 11537, 25926, 29000) }
};
```
