---
name: libaom/name-aom-tune-metric-tuning
description: When declaring variables, struct members, or function parameters of type aom_tune_metric, name the identifier tuning rather than tune or other variants
rubric: readability
severity: nit
---

**Rule.** Name variables, struct fields, and function parameters of type
`aom_tune_metric` consistently as `tuning` (e.g., `const aom_tune_metric
tuning`). Do not use abbreviated or alternative names like `tune`, `metric`, or
`tune_metric`.

**Why.** Consistent identifier naming across function signatures and struct
definitions reduces cognitive load, clarifies the parameter's role as the codec
tuning metric, and aligns with libaom's established conventions across the
encoder.

## Examples

Bad:

```cpp
void av1_set_quantizer(AV1_COMMON *const cm, int min_qmlevel, int max_qmlevel,
                       int q, int enable_chroma_deltaq, int enable_hdr_deltaq,
                       bool is_allintra, aom_tune_metric tune);
```

Good:

```cpp
void av1_set_quantizer(AV1_COMMON *const cm, int min_qmlevel, int max_qmlevel,
                       int q, int enable_chroma_deltaq, int enable_hdr_deltaq,
                       bool is_allintra, aom_tune_metric tuning);
```
