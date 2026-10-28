---
name: libaom/av1/recompute-sliding-window-accumulations
description: When implementing sliding-window or rolling-buffer algorithms, recalculate accumulations inside the loop rather than hoisting initial sum calculations across buffer updates
rubric: correctness
severity: blocking
---

**Rule.** In sliding-window or rolling-buffer algorithms (such as 2D spatial or
temporal filter kernels), recalculate accumulation and reduction values within
the loop body whenever subsequent iterations shift or update window buffer
contents. Do not hoist accumulation calculations outside the loop or reuse
precalculated initial values across rolling updates.

**Why.** Hoisting an accumulation calculation outside a sliding-window loop
causes all subsequent iterations to reuse the initial value, ignoring updates to
the rolling buffer and generating corrupted filter outputs while rendering
buffer shifts ineffective.

## Examples

Bad:

```cpp
uint32_t sum = horizontal_add_u32x4(vsum);
for (unsigned int row = 0; row < block_height; row++) {
  for (int i = 0; i < 4; i++) {
    acc_5x5_neon[row][col + i] = sum;
  }
  // Sliding window shifts vsrc, but sum is hoisted and never updated!
  for (int i = 0; i < 4; i++) {
    vsrc[i][0] = vsrc[i + 1][0];
  }
}
```

Good:

```cpp
do {
  // Recompute accumulation for each row using the updated sliding window
  acc_5x5_neon[row][col - half_window] =
      sum_kernel5x5_mask_double(vsrc, vmask1[0], vmask2[0]);
  // Update sliding window buffer for the next iteration
  for (int i = 0; i < 4; i++) {
    vsrc[i][0] = vsrc[i + 1][0];
  }
  row++;
} while (row < block_height);
```
