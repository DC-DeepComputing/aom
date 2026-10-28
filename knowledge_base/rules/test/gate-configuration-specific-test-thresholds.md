---
name: libaom/test/gate-configuration-specific-test-thresholds
description: 'When adjusting test assertion thresholds for specific build configurations like CONFIG_REALTIME_ONLY, gate relaxed thresholds behind #if CONFIG_* rather than relaxing them globally'
rubric: correctness
severity: should_fix
---

**Rule.** When adjusting test thresholds, tolerances, or assertions to
accommodate behavior differences under a specific build configuration (such as
CONFIG_REALTIME_ONLY), do not relax the threshold globally for all builds. Gate
the configuration-specific threshold behind '#if CONFIG_...' (or the appropriate
build macro) and add a comment documenting why the expectation differs in that
build mode.

**Why.** Relaxing test assertions globally weakens regression detection in
standard builds when only a specialized build configuration exhibits different
behavior or higher variance. Gating and documenting configuration-specific
adjustments preserves strict test coverage across default builds while
preventing false test failures in specialized configurations.

## Examples

Bad:

```cpp
// Lowering threshold globally to accommodate CONFIG_REALTIME_ONLY failures
ASSERT_GE(effective_datarate_, cfg_.rc_target_bitrate * 0.5)
    << " The datarate for the file is lower than target by too much!";
```

Good:

```cpp
#if CONFIG_REALTIME_ONLY
// The threshold needs to be lowered under CONFIG_REALTIME_ONLY due to rate control variance.
ASSERT_GE(effective_datarate_, cfg_.rc_target_bitrate * 0.65)
    << " The datarate for the file is lower than target by too much!";
#else
ASSERT_GE(effective_datarate_, cfg_.rc_target_bitrate * 0.85)
    << " The datarate for the file is lower than target by too much!";
#endif
```
