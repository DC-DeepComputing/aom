---
name: libaom/explicit-comparison-for-integer-error-codes
description: When checking return values of functions returning integer error or status codes, explicitly compare against expected values (such as == -1) instead of implicit boolean truthiness
rubric: readability
severity: nit
---

**Rule.** When evaluating return values of functions that return integer error
or status codes (such as 0 for success and -1 or non-zero for failure),
explicitly compare against the expected error value (e.g., `== -1`, `!= 0`, or
specific error enums) rather than relying on implicit boolean truthiness. Do not
use bare `if (func())` or `if (!func())` checks for integer status returns.

**Why.** In C, integer status conventions typically use 0 for success and
negative values (like -1) or non-zero codes for errors. Implicit truthiness
checks like `if (func())` invert standard boolean semantics (where true
indicates success), causing ambiguity for readers and risking subtle logic bugs
if positive values are valid or if return semantics change.

## Examples

Bad:

```cpp
// Ambiguous check on integer return code (0 = success, -1 = error)
if (smooth_filter_noise(first_stats, last_stats)) {
  aom_internal_error(error_info, AOM_CODEC_MEM_ERROR,
                     "Error allocating buffers in smooth_filter_noise()");
}
```

Good:

```cpp
// Explicit comparison clarifies the -1 failure contract
if (smooth_filter_noise(first_stats, last_stats) == -1) {
  aom_internal_error(error_info, AOM_CODEC_MEM_ERROR,
                     "Error allocating buffers in smooth_filter_noise()");
}
```
