---
name: libaom/test/avoid-underscores-in-googletest-test-names
description: When naming Googletest test suites or test cases, use PascalCase without underscores and use alphanumeric suffixes like V2 instead of underscore suffixes
rubric: readability
severity: nit
---

**Rule.** Do not use underscores in Googletest test suite names or test case
names (excluding framework prefixes like DISABLED_ or MAYBE_). Use PascalCase
and alphanumeric suffixes (such as 'V2') instead of numeric underscore suffixes
(such as '_2').

**Why.** Googletest concatenates test suite names and test names with
underscores internally (forming identifiers like TestSuite_TestName_Test) and
uses underscores for test filtering. Underscores in suite or test names can
cause symbol collisions or break test filtering expressions.

## Examples

Bad:

```cpp
// Bad: underscores in test name or test suite name.
TEST(EncodeAPI, Buganizer503810640_2) {
  // Test body...
}
```

Good:

```cpp
// Good: PascalCase with alphanumeric suffix (V2) without underscores.
TEST(EncodeAPI, Buganizer503810640V2) {
  // Test body...
}
```
