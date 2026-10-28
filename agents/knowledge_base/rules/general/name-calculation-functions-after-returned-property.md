---
name: libaom/name-calculation-functions-after-returned-property
description: When naming calculation and getter helper functions, name them after the specific property or metric they return rather than just the enclosing feature name
rubric: readability
severity: nit
---

**Rule.** Name calculation and getter helper functions after the specific
entity, metric, or property they compute and return rather than solely after the
feature name (e.g., av1_get_<feature>_<property> instead of av1_get_<feature>).
Ensure the identifier explicitly reflects what value is produced.

**Why.** Naming a function only after a feature or module (such as naming a
variance calculator av1_get_block_variance_boost) misleadingly implies that it
returns the feature's final multiplier or factor rather than an underlying
metric. Specifying the exact returned property clarifies value semantics and
return types at call sites.

## Examples

Bad:

```cpp
// Misleading: implies the function returns a boost factor rather than block variance.
unsigned int av1_get_block_variance_boost(const AV1_COMP *cpi,
                                          const MACROBLOCK *x);
```

Good:

```cpp
// Clear: explicitly specifies the computed property (block variance) for the feature.
unsigned int av1_get_variance_boost_block_variance(const AV1_COMP *cpi,
                                                   const MACROBLOCK *x);
```
