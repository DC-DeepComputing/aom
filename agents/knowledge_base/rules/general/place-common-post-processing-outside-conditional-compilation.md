---
name: libaom/place-common-post-processing-outside-conditional-compilation
description: 'When applying common post-processing, clamping, or validation to values computed in conditional compilation blocks, place shared logic after #endif rather than within individual branches'
rubric: correctness
severity: should_fix
---

**Rule.** When performing post-processing, bounds checking, clamping,
saturation, or validation on values derived within preprocessor conditional
compilation blocks (#if, #ifdef, #elif, #else), place the common logic after
#endif. Do not duplicate or isolate shared adjustments inside a single
configuration branch.

**Why.** Placing shared post-processing or bug fixes inside only one #if or
#else branch leaves other build configurations, architectures, or feature flags
unpatched and leads to divergent behavior. Moving common adjustments after
#endif guarantees all configurations receive the fix and avoids duplicate code.

## Examples

Bad:

```cpp
// BAD: Clamping is applied only to the #else branch, leaving CONFIG_FPMT_TEST unpatched.
#if CONFIG_FPMT_TEST
fast_extra_bits = AOMMIN(vbr_bits_off_target_fast, one_frame_bits);
#else
fast_extra_bits = AOMMIN(p_rc->vbr_bits_off_target_fast, one_frame_bits);
fast_extra_bits = (fast_extra_bits < INT_MAX) ? fast_extra_bits : INT_MAX;
#endif
```

Good:

```cpp
// GOOD: Common clamping logic runs after #endif for all configurations.
#if CONFIG_FPMT_TEST
fast_extra_bits = AOMMIN(vbr_bits_off_target_fast, one_frame_bits);
#else
fast_extra_bits = AOMMIN(p_rc->vbr_bits_off_target_fast, one_frame_bits);
#endif
fast_extra_bits = (fast_extra_bits < INT_MAX) ? fast_extra_bits : INT_MAX;
```
