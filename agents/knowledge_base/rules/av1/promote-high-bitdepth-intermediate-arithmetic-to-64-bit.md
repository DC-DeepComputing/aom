---
name: libaom/av1/promote-high-bitdepth-intermediate-arithmetic-to-64-bit
description: When computing high bit-depth (10-bit or 12-bit) pixel differences, variances, or squared accumulations, promote intermediate variables to int64_t to prevent 32-bit integer overflow
rubric: correctness
severity: should_fix
---

**Rule.** When implementing or updating high bit-depth (HBD) algorithms handling
10-bit or 12-bit pixels (such as variance, distortion, SSE, or spatial
prediction calculations), promote intermediate variables and accumulators to
int64_t or uint64_t. Ensure squared pixel differences, sums, and block-level
accumulations do not use 32-bit signed integers (int or int32_t) that can
overflow with pixel values up to 1023 or 4095.

**Why.** While squared pixel differences and block accumulations fit comfortably
within 32-bit integer limits for 8-bit pixels (max 255), 10-bit (max 1023) and
12-bit (max 4095) pixels generate squared differences up to ~1.68×10^7 per
pixel, which quickly exceed INT32_MAX (~2.15×10^9) when summed over blocks or
frames, causing undefined integer overflow.

## Examples

Bad:

```cpp
// 10-bit/12-bit diff squared and summed over block can overflow 32-bit signed int
int diff = pred_ptr[idy * bw + idx] - sum;
*rec_var += diff * diff;
```

Good:

```cpp
// Promote difference and accumulator to int64_t for high bit-depth safety
int64_t diff = pred_ptr[idy * bw + idx] - sum;
*rec_var += diff * diff;
```
