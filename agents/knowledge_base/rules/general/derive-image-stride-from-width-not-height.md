---
name: libaom/derive-image-stride-from-width-not-height
description: When computing, sizing, or defining test vectors for image strides or row pitch, derive values from image width rather than height
rubric: correctness
severity: should_fix
---

**Rule.** When computing, manipulating, or constructing test cases for image
strides and row pitch, derive values and offsets from image width rather than
image height. Do not use height constants or variables to define horizontal
stride variations or padding, even when test dimensions are square.

**Why.** Image stride represents the horizontal line pitch (sample or byte
offset between successive vertical rows), which depends on row width and
horizontal padding. Using image height to construct or test stride values
creates semantic confusion and masks dimension mismatch bugs if test
configurations become non-square.

## Examples

Bad:

```cpp
constexpr int kWidth = 64;
constexpr int kHeight = 64;
// Stride variations erroneously derived using kHeight.
static constexpr std::array<UVStride, 4> kUVStrides = {
  UVStride{ kWidth, kHeight - 1 }, UVStride{ kWidth, kHeight + 1 },
  UVStride{ kWidth - 1, kHeight }, UVStride{ kWidth + 1, kHeight }
};
```

Good:

```cpp
constexpr int kWidth = 64;
constexpr int kHeight = 64;
// Stride variations correctly derived using horizontal dimension (kWidth).
static constexpr std::array<UVStride, 4> kUVStrides = {
  UVStride{ kWidth, kWidth - 1 }, UVStride{ kWidth, kWidth + 1 },
  UVStride{ kWidth - 1, kWidth }, UVStride{ kWidth + 1, kWidth }
};
```
