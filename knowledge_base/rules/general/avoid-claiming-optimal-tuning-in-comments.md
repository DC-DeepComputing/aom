---
name: libaom/avoid-claiming-optimal-tuning-in-comments
description: When documenting tuning parameters, presets, or heuristic thresholds in code comments, avoid claiming settings are 'optimal'; describe them with qualified terms or empirical contexts.
rubric: readability
severity: nit
---

**Rule.** When writing comments explaining tuning parameters, presets,
heuristics, or default settings, avoid asserting that values or configurations
are 'optimal'. Use qualified terminology such as 'beneficial' or describe the
specific empirical evaluation context, metrics, or test conditions under which
the settings improve performance.

**Why.** Encoder presets and tuning heuristics are chosen empirically based on
specific datasets, quality metrics, and encoding modes. Claiming a setting is
globally 'optimal' overstates the empirical results and can mislead future
maintainers when trade-offs evolve across different content or metrics.

## Examples

Bad:

```cpp
// These QM settings have been found to be optimal for images, when used
// with an alternative QM formula (see aom_get_qmlevel_allintra()).
```

Good:

```cpp
// These QM min/max values have been found to be beneficial for images,
// when used with an alternative QM formula (see aom_get_qmlevel_allintra()).
```
