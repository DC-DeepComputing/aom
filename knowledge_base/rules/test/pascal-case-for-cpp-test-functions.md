---
name: libaom/test/pascal-case-for-cpp-test-functions
description: When defining functions, helper routines, or fixture methods in C++ test files, use UpperCamelCase instead of C-style snake_case naming
rubric: readability
severity: nit
---

**Rule.** In C++ test files, fixtures, and test utilities (test/**), name helper
functions, static routines, and fixture methods in UpperCamelCase (PascalCase)
rather than C-style snake_case. Reserve snake_case function naming for C
production source files.

**Why.** While libaom's core C codebase uses snake_case, test suites and helper
fixtures are written in C++ and follow Google C++ Style conventions where
function and method names use UpperCamelCase. Using C-style snake_case in C++
tests causes stylistic inconsistency with surrounding test fixtures and
utilities.

## Examples

Bad:

```cpp
// Static helper function
static void fill_image(aom_image_t *img, uint8_t val) {
  memset(img->planes[0], val, img->stride[0] * img->d_h);
}

// Test fixture method
class InterpCubicTest : public ::testing::TestWithParam<InterpCubicTestParam> {
 public:
  double generate_random_double(double min, double max) {
    return min + (static_cast<double>(rnd_.Rand31()) / ((1U << 31) - 1)) *
                     (max - min);
  }
};
```

Good:

```cpp
// Static helper function
static void FillImage(aom_image_t *img, uint8_t val) {
  memset(img->planes[0], val, img->stride[0] * img->d_h);
}

// Test fixture method
class InterpCubicTest : public ::testing::TestWithParam<InterpCubicTestParam> {
 public:
  double GenerateRandomDouble(double min, double max) {
    return min + (static_cast<double>(rnd_.Rand31()) / ((1U << 31) - 1)) *
                     (max - min);
  }
};
```
