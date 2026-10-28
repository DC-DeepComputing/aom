---
name: libaom/test/use-descriptive-test-names-over-numeric-suffixes
description: When defining test cases or test fixtures, name tests using their distinguishing parameters and conditions rather than appending arbitrary numeric suffixes
rubric: readability
severity: nit
---

**Rule.** When adding or modifying unit tests, include distinguishing test
conditions, parameters, or configurations (such as framerate, bitrate,
resolution, or layer count) in the test name. Avoid using generic names with
arbitrary numeric suffixes (such as Test2 or Qvga2) to differentiate test
variations.

**Why.** Descriptive test names clearly communicate what scenario or
configuration is being exercised, making test failure logs easy to diagnose and
test suites straightforward to navigate without inspecting test body
implementations.

## Examples

Bad:

```cpp
// Check basic rate targeting for CBR, for 3TL, 1SL, QVGA, low framerate.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL1SLQvga) {
  BasicRateTargetingSVC3TL1SLQvgaTest();
}

// Check basic rate targeting for CBR, for 3TL, 1SL, QVGA, high bitrate.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL1SLQvga2) {
  BasicRateTargetingSVC3TL1SLQvga2Test();
}
```

Good:

```cpp
// Check basic rate targeting for CBR, for 3TL, 1SL, QVGA, low framerate.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL1SLQvgaLowFramerate) {
  BasicRateTargetingSVC3TL1SLQvgaLowFramerateTest();
}

// Check basic rate targeting for CBR, for 3TL, 1SL, QVGA, high bitrate and low framerate.
TEST_P(DatarateTestSVC, BasicRateTargetingSVC3TL1SLQvgaHighBitrateLowFramerate) {
  BasicRateTargetingSVC3TL1SLQvgaHighBitrateLowFramerateTest();
}
```
