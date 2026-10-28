---
name: libaom/unit-test-new-formulas-and-mode-behaviors
description: When introducing new algorithms, formulas, or mode-specific behaviors, add unit tests in test/ covering representative ranges, boundary conditions, and expected outputs
rubric: testing
severity: should_fix
---

**Rule.** Add unit test coverage (using GoogleTest in test/) when introducing
new algorithms, mathematical formulas, quantization/scaling models, or
mode-specific behaviors. Verify that the new logic produces expected results
across representative input values, monotonic ranges, and boundary conditions
(such as minimum/maximum limits or default configurations), protecting against
regressions.

**Why.** New mathematical formulas and mode-specific tunings often have subtle
boundary behaviors or monotonic progression requirements. Unit tests guarantee
that the logic takes effect as intended across valid parameter ranges and
prevent silent behavioral regressions during future refactorings.

## Examples

Bad:

```cpp
// av1/common/quant_common.h
static inline int aom_get_qmlevel_allintra(int qindex, int first, int last) {
  // New formula added for all-intra mode without corresponding unit tests
  return qindex * (first - last) / 255 + last;
}
```

Good:

```cpp
// test/av1_get_qmlevel_test.cc
TEST(GetQmLevelTest, AllIntra) {
  int qindex_1_qmlevel = aom_get_qmlevel_allintra(1, DEFAULT_QM_FIRST_ALLINTRA,
                                                  DEFAULT_QM_LAST_ALLINTRA);
  int qindex_255_qmlevel = aom_get_qmlevel_allintra(
      255, DEFAULT_QM_FIRST_ALLINTRA, DEFAULT_QM_LAST_ALLINTRA);
  EXPECT_EQ(qindex_1_qmlevel, DEFAULT_QM_LAST_ALLINTRA);
  EXPECT_EQ(qindex_255_qmlevel, DEFAULT_QM_FIRST_ALLINTRA);
  EXPECT_LT(qindex_255_qmlevel, qindex_1_qmlevel);
}
```
