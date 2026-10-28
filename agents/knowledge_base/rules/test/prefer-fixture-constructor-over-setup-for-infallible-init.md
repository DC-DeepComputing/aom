---
name: libaom/test/prefer-fixture-constructor-over-setup-for-infallible-init
description: In GoogleTest test fixtures, initialize member variables and test parameters in constructors rather than overriding SetUp() when initialization is infallible
rubric: readability
severity: should_fix
---

**Rule.** In GoogleTest test fixtures (`testing::Test` or
`testing::TestWithParam`), initialize member variables and test parameters
(e.g., `GetParam()`) directly in the constructor initializer list rather than
overriding `SetUp()`, unless initialization can fail or requires `ASSERT_*`
macros. Avoid overriding `TearDown()` if the destructor or base cleanup suffices
and the override is empty.

**Why.** Constructor initializer lists are the standard C++ idiom for object
initialization, enabling members to be const and preventing uninitialized state
between construction and SetUp(). SetUp() is only needed for fallible operations
or virtual calls, and empty TearDown() overrides add unnecessary boilerplate.

## Examples

Bad:

```cpp
class InterpCubicTest
    : public ::testing::TestWithParam<InterpCubicRateDistFunc> {
 public:
  void SetUp() override { target_func_ = GetParam(); }
  void TearDown() override {}

 protected:
  InterpCubicRateDistFunc target_func_;
};
```

Good:

```cpp
class InterpCubicTest
    : public ::testing::TestWithParam<InterpCubicRateDistFunc> {
 public:
  InterpCubicTest() : target_func_(GetParam()) {}

 protected:
  InterpCubicRateDistFunc target_func_;
};
```
