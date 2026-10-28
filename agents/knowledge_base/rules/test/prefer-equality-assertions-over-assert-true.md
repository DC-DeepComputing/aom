---
name: libaom/test/prefer-equality-assertions-over-assert-true
description: When asserting equality or comparison conditions in unit tests, use specific macros like EXPECT_EQ or ASSERT_EQ instead of EXPECT_TRUE or ASSERT_TRUE
rubric: readability
severity: nit
---

**Rule.** In unit tests, use specific equality and comparison assertion macros
(such as EXPECT_EQ, ASSERT_EQ, EXPECT_NE, or ASSERT_NE) rather than evaluating
equality or relational expressions inside boolean assertions like
EXPECT_TRUE(actual == expected) or ASSERT_TRUE(...).

**Why.** When a comparison inside EXPECT_TRUE or ASSERT_TRUE fails, GoogleTest
only reports that the expression evaluated to false without printing the
compared values. Dedicated comparison macros print both actual and expected
values in failure output, making test failures much easier to diagnose.

## Examples

Bad:

```cpp
ASSERT_TRUE(rc_api_->ComputeQP(frame_params_) ==
            aom::kFrameDropDecisionDrop);
EXPECT_TRUE(num_frames == 10);
```

Good:

```cpp
ASSERT_EQ(rc_api_->ComputeQP(frame_params_),
          aom::kFrameDropDecisionDrop);
EXPECT_EQ(num_frames, 10);
```
