---
name: libaom/av1/test-simd-frame-routines-with-non-aligned-dimensions
description: When implementing or testing SIMD frame resizing, scaling, or filtering routines, include odd, arbitrary, and non-aligned dimensions to verify MSan safety
rubric: testing
severity: should_fix
---

**Rule.** When implementing or unit-testing SIMD frame resizing, scaling, and
whole-frame filtering routines that operate on arbitrary frame sizes in
fixed-width vector chunks (e.g., 8-byte or 16-byte loads/stores), include odd,
arbitrary, and non-aligned dimensions in the test matrix rather than only
power-of-two or vector-aligned multiples. Verify that SIMD kernels handle
remainder dimensions correctly without uninitialized memory reads under
MemorySanitizer (MSan) or buffer overflows.

**Why.** Video codecs and interfaces (such as WebCodecs) frequently supply
arbitrary frame dimensions. Although frame buffers allocate border padding and
aligned strides that permit bounded vector overreading, testing non-aligned and
odd dimensions under MSan ensures vector loops and tail processing do not read
uninitialized padding or exceed buffer bounds.

## Examples

Bad:

```cpp
// Testing only power-of-two or vector-aligned dimensions
static const int kSizesToTest[] = { 16, 32, 64, 128 };
for (int w : kSizesToTest) {
  for (int h : kSizesToTest) {
    TestResizeFrame(w, h);
  }
}
```

Good:

```cpp
// Testing arbitrary, odd, and non-aligned dimensions for MSan safety
static const int kSizesToTest[] = { 1,  2,  3,  4,  6,   8,  10, 12,
                                    14, 16, 18, 20, 22,  24, 26, 28,
                                    30, 32, 34, 68, 128, 134 };
for (int w : kSizesToTest) {
  for (int h : kSizesToTest) {
    TestResizeFrame(w, h);
  }
}
```
