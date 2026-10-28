---
name: libaom/test/prefer-static-constexpr-std-array-for-constant-test-data
description: When defining fixed-size compile-time constant buffers or test data in C++ tests, use static constexpr std::array instead of C-style arrays
rubric: readability
severity: should_fix
---

**Rule.** In C++ test code, declare fixed-size compile-time constant buffers,
test vectors, and mock payload data using `static constexpr std::array<T, N>`
rather than raw C-style arrays (`static const T[]` or `static const T[N]`). Pair
the declaration with a named constant for the array dimension or size.

**Why.** `std::array` provides a type-safe container that avoids pointer decay,
preserves compile-time size checking, and works seamlessly with modern container
interfaces, while `static constexpr` ensures compile-time evaluation without
redundant runtime allocation or initialization.

## Examples

Bad:

```cpp
TEST_F(InvalidFilePoCTest, CrashTest) {
  static const unsigned char kPoCData[] = {
    0x00, 0x00, 0xe7, 0xff, 0x00, 0x01,
  };
  static const unsigned int kPoCDataLen = 6;
  InMemoryVideoSource decode_video(kPoCData, kPoCDataLen);
  ...
}
```

Good:

```cpp
TEST_F(InvalidFilePoCTest, CrashTest) {
  static const unsigned int kPoCDataLen = 6;
  static constexpr std::array<unsigned char, kPoCDataLen> kPoCData = {
    0x00, 0x00, 0xe7, 0xff, 0x00, 0x01,
  };
  InMemoryVideoSource decode_video(kPoCData);
  ...
}
```
