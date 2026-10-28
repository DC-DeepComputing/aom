---
name: libaom/split-state-initialization-from-validation
description: When callers require only state population without validation, extract state initialization into a dedicated helper rather than calling check functions and discarding return values
rubric: design
severity: should_fix
---

**Rule.** When a function combines output state population with validation or
condition checking, extract the state initialization into a dedicated helper
(e.g., set_*()) if callers only require setting state. Do not invoke validation
or check functions (e.g., check_*() or is_*()) and discard their return values
when the validation result is unneeded.

**Why.** Invoking a validation function and ignoring its return value misleads
readers into suspecting an unhandled error or missed condition. Separating state
population from validation clarifies intent at call sites and allows validation
helpers to cleanly reuse the initialization helper.

## Examples

Bad:

```cpp
// Caller needs to populate planes_to_lf but ignores the validation result:
if (lpf_mt_with_enc_enabled(cpi->mt_info.pipeline_lpf_mt_with_enc,
                            lf->filter_level)) {
  check_planes_to_loop_filter(lf, planes_to_lf, plane_start, plane_end);
}
```

Good:

```cpp
// State population is extracted into set_*() so caller does not discard check_*() return value:
if (lpf_mt_with_enc_enabled(cpi->mt_info.pipeline_lpf_mt_with_enc,
                            lf->filter_level)) {
  set_planes_to_loop_filter(lf, planes_to_lf, plane_start, plane_end);
}
```
