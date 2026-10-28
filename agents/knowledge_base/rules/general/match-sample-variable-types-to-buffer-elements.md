---
name: libaom/match-sample-variable-types-to-buffer-elements
description: When declaring local variables storing pixel or sample values from uint8_t or uint16_t image buffers, use matching uint8_t or uint16_t rather than signed int.
rubric: readability
severity: nit
---

**Rule.** When declaring local variables or helper function return types that
store pixel or sample values read from or written to image buffers (such as raw
8-bit image data or high-bit-depth blocks), declare them with the matching
buffer element type (`uint8_t` or `uint16_t`, qualified with `const` when
unmodified) rather than defaulting to `int` or `int32_t`. Reserve wider signed
integer types (like `int` or `int32_t`) for intermediate arithmetic,
differences, accumulation, or filtering calculations that can exceed sample
bit-depth bounds.

**Why.** Typing local pixel sample variables as `uint8_t` or `uint16_t`
accurately documents valid sample ranges ([0, 255] or [0, 1023]/[0, 4095]),
prevents implicit signedness or width conversions (such as when indexing
frequency or lookup arrays), eliminates unnecessary integer promotion clutter,
and clearly signals to readers and compilers that the value is an in-range pixel
sample rather than an accumulator or signed difference.

## Examples

Bad:

```cpp
// Variant 1: Storing 8-bit sample in int across frequency histogram indexing
static uint8_t find_dominant_value(const uint8_t *src, int stride, int rows,
                                   int cols) {
  uint32_t value_freq[256] = { 0 };
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const int value = src[r * (ptrdiff_t)stride + c];
      value_freq[value]++;
    }
  }
  ...
}

// Variant 2: Reading/writing 8-bit or high-bitdepth samples as int
const int value = src[r * src_stride + c];
dilated_block[r * dilated_stride + c] = value;
```

Good:

```cpp
// Variant 1: Using uint8_t matching buffer element type
static uint8_t find_dominant_value(const uint8_t *src, int stride, int rows,
                                   int cols) {
  uint32_t value_freq[256] = { 0 };
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      const uint8_t value = src[r * (ptrdiff_t)stride + c];
      value_freq[value]++;
    }
  }
  ...
}

// Variant 2: Using matching uint8_t (or uint16_t for high-bitdepth)
const uint8_t value = src[r * src_stride + c];
dilated_block[r * dilated_stride + c] = value;
```
