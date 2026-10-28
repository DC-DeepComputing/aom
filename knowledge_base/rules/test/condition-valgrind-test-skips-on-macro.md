---
name: libaom/test/condition-valgrind-test-skips-on-macro
description: 'When skipping test parameters or reducing iterations to prevent slow Valgrind timeouts, guard skips with #ifdef AOM_VALGRIND_BUILD instead of skipping unconditionally'
rubric: testing
severity: should_fix
---

**Rule.** When skipping test cases, reducing parameter matrices, or truncating
frame counts to avoid timeouts during slow Valgrind runs, enclose the skip or
parameter reduction in #ifdef AOM_VALGRIND_BUILD (or #if
!defined(AOM_VALGRIND_BUILD)). Do not skip test configurations unconditionally.

**Why.** Valgrind execution has substantial performance overhead and requires
reduced test matrices to complete within CI timeout limits. Unconditionally
skipping test configurations removes test coverage from standard presubmit and
release builds where execution overhead is minimal.

## Examples

Bad:

```cpp
// Unconditionally skipping reduces coverage in fast standard builds.
if (cfg_.rc_target_bitrate == 2000) {
  GTEST_SKIP() << "No need to run this test for 2 bitrates, the issue occurs "
                  "at first bitrate = 1000.";
}
```

Good:

```cpp
#ifdef AOM_VALGRIND_BUILD
// Guard skip to preserve full matrix in fast builds while avoiding Valgrind timeout.
if (cfg_.rc_target_bitrate == 2000) {
  GTEST_SKIP() << "Skip second bitrate in slow Valgrind builds; first bitrate "
                  "is sufficient.";
}
#endif  // AOM_VALGRIND_BUILD
```
