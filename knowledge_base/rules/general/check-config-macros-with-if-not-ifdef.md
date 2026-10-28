---
name: libaom/check-config-macros-with-if-not-ifdef
description: 'When conditionally compiling code based on build configuration macros (CONFIG_* or HAVE_*), use #if CONFIG_* rather than #ifdef or #if defined()'
rubric: correctness
severity: should_fix
---

**Rule.** Check build configuration and feature macros using #if CONFIG_* or #if
!CONFIG_* (and #if HAVE_* / #if !HAVE_*) rather than #ifdef, #ifndef, or #if
defined(...). In libaom, config/aom_config.h defines all configuration macros as
integer 0 (disabled) or 1 (enabled), so defined(...) always evaluates to true.

**Why.** Because config/aom_config.h explicitly defines all CONFIG_* and HAVE_*
flags as either 0 or 1, #ifdef and #if defined(...) evaluate to true even when a
feature is disabled (0), causing disabled feature code to execute or #if
!defined(...) fallback paths to be incorrectly bypassed.

## Examples

Bad:

```cpp
// INCORRECT: defined() is always true because CONFIG_ macros are defined as 0 or 1
#if defined(CONFIG_INTERNAL_STATS)
frame_flags_ ^= AOM_EFLAG_CALCULATE_PSNR;
#endif

#if !defined(CONFIG_AV1_HIGHBITDEPTH)
Process8BitOnly();
#endif
```

Good:

```cpp
// CORRECT: Evaluate the macro value (0 or 1) directly
#if CONFIG_INTERNAL_STATS
frame_flags_ ^= AOM_EFLAG_CALCULATE_PSNR;
#endif

#if !CONFIG_AV1_HIGHBITDEPTH
Process8BitOnly();
#endif
```
