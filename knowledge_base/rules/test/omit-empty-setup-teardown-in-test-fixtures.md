---
name: libaom/test/omit-empty-setup-teardown-in-test-fixtures
description: When defining or modifying GoogleTest test fixtures, omit empty, no-op SetUp() or TearDown() overrides and rely on base class default implementations
rubric: readability
severity: nit
---

**Rule.** Do not define empty, no-op SetUp(), TearDown(), SetUpTestSuite(), or
TearDownTestSuite() overrides in GoogleTest test fixtures (subclasses of
::testing::Test or ::testing::TestWithParam). Omit redundant method definitions
and rely on the base class default implementations instead.

**Why.** GoogleTest's base test classes already provide empty default
implementations for lifecycle methods like SetUp() and TearDown(). Overriding
them with empty function bodies adds redundant boilerplate code and clutters
fixture declarations.

## Examples

Bad:

```cpp
class TransformTest : public ::testing::TestWithParam<TxParam> {
 public:
  void SetUp() override { tx_func_ = GetParam(); }
  void TearDown() override {}

 private:
  TxFunc tx_func_;
};
```

Good:

```cpp
class TransformTest : public ::testing::TestWithParam<TxParam> {
 public:
  void SetUp() override { tx_func_ = GetParam(); }

 private:
  TxFunc tx_func_;
};
```
