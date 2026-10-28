---
name: libaom/check-non-zero-for-status-return-codes
description: For zero-on-success functions, check failure using non-zero (!= 0) instead of sentinel == -1, and propagate callee return codes directly rather than hardcoding -1
rubric: readability
severity: should_fix
---

**Rule.** When checking or propagating status/error return codes from functions
where zero indicates success and non-zero indicates failure:
1. Verify failure by checking for non-zero (`!= 0` or evaluating truthiness/boolean condition) rather than matching specific sentinel values like `== -1` or `< 0`, unless the API contract explicitly guarantees only that sentinel value.
2. When propagating failure upstream, forward the callee's status code directly (`if (ret != 0) return ret;` or `if (ret) return ret;`) rather than returning a hardcoded `-1` or other synthetic sentinel.

**Why.** Functions returning 0 on success and non-zero on failure may return or
propagate various positive/negative error codes or enum values to represent
specific failure modes (e.g., memory errors, invalid arguments). Comparing
specifically against `-1` risks silently ignoring other non-zero error
conditions, and returning a hardcoded `-1` swallows detailed error
classifications needed by upstream callers.

## Examples

Bad:

```cpp
// Variant 1: Checking specific sentinel ignores other error codes
if (av1_apply_selfguided_restoration(dat, w, h, stride, eps, xqd, dst,
                                     dst_stride, tmpbuf, bit_depth,
                                     highbd) == -1) {
  aom_internal_error(error_info, AOM_CODEC_MEM_ERROR,
                     "Error applying selfguided restoration");
}

// Variant 2: Hardcoding return sentinel swallows callee error codes
const int ret = av1_selfguided_restoration_c(
    dat8, width, height, stride, flt0, flt1, width, eps, bit_depth, highbd);
if (ret == -1) return -1;
```

Good:

```cpp
// Variant 1: Check for any non-zero failure code
if (av1_apply_selfguided_restoration(dat, w, h, stride, eps, xqd, dst,
                                     dst_stride, tmpbuf, bit_depth,
                                     highbd) != 0) {
  aom_internal_error(error_info, AOM_CODEC_MEM_ERROR,
                     "Error applying selfguided restoration");
}

// Variant 2: Propagate non-zero status code directly
const int ret = av1_selfguided_restoration_c(
    dat8, width, height, stride, flt0, flt1, width, eps, bit_depth, highbd);
if (ret != 0) return ret;
```
