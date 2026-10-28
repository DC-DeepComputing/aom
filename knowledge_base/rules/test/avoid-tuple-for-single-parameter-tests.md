---
name: libaom/test/avoid-tuple-for-single-parameter-tests
description: When defining Googletest parameterized tests with a single parameter, pass the type directly to TestWithParam without std::tuple and use GetParam()
rubric: readability
severity: nit
---

**Rule.** When defining a Googletest parameterized test suite that takes only a
single parameter (such as a function pointer, enum, or config struct), do not
wrap that single type in a 1-element `std::tuple<T>`. Parameterize
`::testing::TestWithParam<T>` directly with the type and retrieve it using
`GetParam()` rather than `GET_PARAM(0)` or `std::get<0>`.

**Why.** Wrapping a single parameter in `std::tuple` adds unnecessary type
boilerplate and requires tuple index unpacking via `GET_PARAM(0)` or
`std::get<0>(GetParam())`. Passing the parameter type directly simplifies
fixture declarations and allows direct access via `GetParam()`.

## Examples

Bad:

```cpp
using InterpCubicTestParam = std::tuple<const InterpCubicRateDistFunc>;

class InterpCubicTest : public ::testing::TestWithParam<InterpCubicTestParam> {
 public:
  void SetUp() override { target_func_ = GET_PARAM(0); }
  // ...
};
```

Good:

```cpp
using InterpCubicTestParam = InterpCubicRateDistFunc;

class InterpCubicTest : public ::testing::TestWithParam<InterpCubicTestParam> {
 public:
  void SetUp() override { target_func_ = GetParam(); }
  // ...
};
```
