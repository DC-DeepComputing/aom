---
name: libaom/avoid-redundant-or-unused-using-declarations
description: When importing symbols with using declarations in C++ files, remove redundant or unused using-declarations if symbols are already fully qualified or unreferenced
rubric: readability
severity: nit
---

**Rule.** Do not add or retain redundant or unused `using` declarations (such as
`using namespace_name::SymbolName;`) when the referenced symbol is already fully
qualified at all usage sites or is not referenced in the file.

**Why.** Unused or redundant `using` declarations clutter namespace scopes
without providing any readability benefit, and they can mislead readers about
which symbols are actually required.

## Examples

Bad:

```cpp
using libaom_test::ACMRandom;

namespace {
class InterpCubicTest : public ::testing::TestWithParam<InterpCubicTestParam> {
 private:
  libaom_test::ACMRandom rnd_;
};
}  // namespace
```

Good:

```cpp
namespace {
class InterpCubicTest : public ::testing::TestWithParam<InterpCubicTestParam> {
 private:
  libaom_test::ACMRandom rnd_;
};
}  // namespace
```
