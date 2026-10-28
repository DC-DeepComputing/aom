---
name: libaom/add-regression-tests-for-bug-fixes-and-edge-cases
description: When fixing bugs, correcting edge cases, or landing codec fixes, add new unit tests or re-enable existing DISABLED_ regression tests in test/
rubric: testing
severity: should_fix
---

**Rule.** When fixing bugs, handling special edge cases, or adding logic for
specific codec controls and configuration modes, add corresponding regression
unit tests under test/. If disabled regression tests (such as googletest cases
prefixed with DISABLED_ added for bug or fuzz reproducers) already exist for the
issue, re-enable them in the same CL by removing the prefix.

**Why.** Bug fixes and edge-case guards without active regression unit tests are
vulnerable to accidental removal or regression during subsequent refactorings.
Dedicated unit tests and re-enabled test suites ensure that targeted
configurations and bug reproducers remain continuously verified in CI and make
backporting and cherry-picking fixes to release branches verifiable.

## Examples

Bad:

```cpp
// 1. Bug fix in rate control / frame config submitted without an accompanying test in test/
if (cpi->ppi->rtc_ref.set_ref_frame_config && no_references_set) {
  *frame_type = INTRA_ONLY_FRAME;
}

// 2. Bug fix lands while existing regression reproducer remains disabled:
TEST(SearchWienerTest, DISABLED_8bitSignedIntegerOverflowInLinsolveWiener) {
  ...
}
```

Good:

```cpp
// 1. Add dedicated regression unit test for the edge case:
TEST_P(DatarateTestSVC, BasicRateTargetingSVC1TL1SLIntraOnly) {
  BasicRateTargetingSVC1TL1SLIntraOnlyTest();
}

// 2. Re-enable existing regression test when the underlying bug is fixed:
TEST(SearchWienerTest, 8bitSignedIntegerOverflowInLinsolveWiener) {
  ...
}
```
