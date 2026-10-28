---
name: libaom/test/read-config-from-cfg-in-test-hooks
description: When inspecting codec configuration values in encoder test hooks, read configured parameters directly from cfg_ instead of declaring uninitialized fixture member variables
rubric: correctness
severity: blocking
---

**Rule.** In test fixtures subclassing EncoderTest, read codec configuration
parameters directly from the inherited cfg_ member inside test hooks (such as
PreEncodeFrameHook or PostEncodeFrameHook) rather than declaring duplicate class
member variables. If custom fixture member variables are required for test
state, ensure they are explicitly initialized in SetUp() or at declaration to
prevent uninitialized memory reads and broken assertion conditions.

**Why.** Declaring duplicate fixture member variables that are never assigned in
SetUp() leaves them uninitialized. Reading these uninitialized variables in test
hooks leads to conditional branches or assertions evaluating garbage data, which
causes intermittent assertion failures and MemorySanitizer/Valgrind
uninitialized memory read errors.

## Examples

Bad:

```cpp
class FixedQpTest : public ::libaom_test::EncoderTest,
                    public ::testing::TestWithParam<int> {
 protected:
  void SetUp() override {
    InitializeConfig(GET_PARAM(0));
    cfg_.use_fixed_qp_offsets = GET_PARAM(1);
  }

  void PostEncodeFrameHook(::libaom_test::Encoder *encoder) override {
    // Uninitialized read: use_fixed_qp_offsets_ was never initialized in SetUp().
    if (use_fixed_qp_offsets_ == 2) {
      ASSERT_EQ(encoder->GetQP(), expected_qp_);
    }
  }

  int use_fixed_qp_offsets_;
  int expected_qp_ = 10;
};
```

Good:

```cpp
class FixedQpTest : public ::libaom_test::EncoderTest,
                    public ::testing::TestWithParam<int> {
 protected:
  void SetUp() override {
    InitializeConfig(GET_PARAM(0));
    cfg_.use_fixed_qp_offsets = GET_PARAM(1);
  }

  void PostEncodeFrameHook(::libaom_test::Encoder *encoder) override {
    // Read directly from cfg_ which was configured in SetUp().
    if (cfg_.use_fixed_qp_offsets == 2) {
      ASSERT_EQ(encoder->GetQP(), expected_qp_);
    }
  }

  int expected_qp_ = 10;
};
```
