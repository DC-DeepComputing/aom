---
name: libaom/use-standard-distortion-metric-naming-sad-sse
description: When declaring block distortion metrics, accumulators, or functions, use standard abbreviations (sad, sse) and integer types instead of non-standard names (ssd) or floating-point types
rubric: readability
severity: should_fix
---

**Rule.** When declaring variables, accumulators, parameters, or return types
for block distortion metrics, use standard libaom abbreviations (`sad` for
absolute differences, `sse` for squared error) rather than non-standard names
(such as `ssd`), and use integer types (`int`, `unsigned int`, `uint32_t`,
`int64_t`, or `uint64_t`) instead of floating-point types (`double` or `float`).
Reserve floating-point types strictly for normalized metrics (such as per-sample
distortion) or continuous geometric transformations.

**Why.** libaom standardizes on integer-typed `sad` and `sse` throughout DSP
kernels, motion search, and rate-distortion optimization. Discrete pixel
differences (such as `uint8_t` samples) are integer quantities; using `double`
or `float` introduces unnecessary floating-point conversion overhead, inhibits
SIMD optimization, and violates codebase conventions. Non-standard abbreviations
like `ssd` harm code searchability and consistency across architecture-specific
DSP implementations.

## Examples

Bad:

```cpp
// Non-standard naming and floating-point types:
double temp_ssd;
double best_ssd;
uint64_t compute_block_ssd(const uint8_t *src, const uint8_t *ref);

// Floating-point accumulator and return type for discrete pixel SAD:
double compute_block_sad(const uint8_t *cur, int cur_stride,
                         const uint8_t *pred, int pred_stride,
                         int width, int height) {
  double ret_sad = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      ret_sad += abs(cur[x + y * cur_stride] - pred[x + y * pred_stride]);
    }
  }
  return ret_sad;
}
```

Good:

```cpp
// Sum of absolute differences using standard naming and integer types:
int temp_sad;
int best_sad;
int compute_block_sad(const uint8_t *cur, int cur_stride,
                      const uint8_t *pred, int pred_stride,
                      int width, int height) {
  int ret_sad = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      ret_sad += abs((int)cur[x + y * cur_stride] - (int)pred[x + y * pred_stride]);
    }
  }
  return ret_sad;
}

// Sum of squared error using standard naming and integer types:
unsigned int temp_sse;
uint64_t compute_block_sse(const uint8_t *src, const uint8_t *ref);
```
