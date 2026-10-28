---
name: libaom/test/keep-config-consistent-across-analogous-tests
description: When implementing sets of parallel or analogous test cases across different resolutions or clips, maintain consistent configuration parameters across the suite unless intentionally varying them
rubric: readability
severity: should_fix
---

**Rule.** When implementing a set of parallel or analogous test cases (such as
rate-targeting tests across different resolutions, aspect ratios, or input
clips), keep configuration parameters (such as keyframe intervals kf_min_dist
and kf_max_dist, framerate, or speed presets) consistent across the suite. Avoid
unintended parameter discrepancies between sibling test variants unless a
specific divergence is intentional.

**Why.** Inconsistent configuration parameters between analogous test variants
create asymmetric test coverage across resolutions or input clips and can
introduce unintended rate-control or encoder behaviors that make test results
difficult to compare and maintain.

## Examples

Bad:

```cpp
// In BasicRateTargetingQvgaCBRKf (while sibling CIF and QVGA tests use 10):
cfg_.kf_max_dist = 5;
cfg_.kf_min_dist = 5;
```

Good:

```cpp
// In BasicRateTargetingQvgaCBRKf (consistent with sibling test cases):
cfg_.kf_max_dist = 10;
cfg_.kf_min_dist = 10;
```
