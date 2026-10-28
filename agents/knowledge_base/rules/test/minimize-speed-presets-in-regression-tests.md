---
name: libaom/test/minimize-speed-presets-in-regression-tests
description: When writing encoder regression tests or parameterized fixtures, test minimal speed presets (cpu_used), using the fastest setting that reproduces the issue.
rubric: safety
severity: should_fix
---

**Rule.** When writing encoder regression tests or parameterized test fixtures,
instantiate as few encoding speed presets (cpu_used / AOME_SET_CPUUSED) as
necessary to exercise the target bug or feature path. Prefer testing only the
single fastest speed setting (highest cpu_used value) that reproduces the issue
rather than parameterizing across multiple slow presets.

**Why.** Encoding at slower speed presets adds substantial CPU execution time to
the test suite and CI presubmits with little to no additional coverage benefit
for specific regression bugs. Restricting regression tests to the fastest
reproducing speed setting minimizes test suite run time while ensuring reliable
regression detection.

## Examples

Bad:

```cpp
// Testing multiple slow speed presets adds redundant test runtime.
AV1_INSTANTIATE_TEST_SUITE(ResizeCrashTest,
                           ::testing::Values(::libaom_test::kOnePassGood),
                           ::testing::Values(1, 3));
```

Good:

```cpp
// Test only the fastest speed preset that reproduces the bug.
AV1_INSTANTIATE_TEST_SUITE(ResizeCrashTest,
                           ::testing::Values(::libaom_test::kOnePassGood),
                           ::testing::Values(3));
```
