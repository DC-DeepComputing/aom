---
name: libaom/batch-neon-horizontal-reductions-with-horizontal-add-4d
description: When computing horizontal reductions for multiple contiguous outputs in ARM NEON routines, batch partial sums with horizontal_add_4d_* and vector stores instead of individual scalar reductions
rubric: safety
severity: should_fix
---

**Rule.** In ARM NEON routines, when computing horizontal reductions for
multiple contiguous outputs (such as 2 or 4 rows/columns), return unreduced
partial sum vectors from helper kernels and reduce them in batch using
multi-vector horizontal reduction helpers (such as horizontal_add_4d_* or
horizontal_add_2d_* from aom_dsp/arm/sum_neon.h) followed by a vector store
(such as vst1q_*). Do not perform separate scalar horizontal reductions and
individual scalar stores for each output.

**Why.** Performing independent horizontal reductions to scalars and storing
them with scalar assignments incurs extra cross-lane shuffling latency and
repeated memory store instructions. Batching partial sum vectors with
horizontal_add_4d_* computes pairwise reductions in parallel across registers
and allows writing all outputs at once using a single vector store (vst1q_*).

## Examples

Bad:

```cpp
static uint32_t sum_kernel5x5_mask_double(...) {
  uint32x4_t vsums = ...;
  // Individually reducing vector to scalar
  return horizontal_add_u32x4(vsums);
}

// Storing scalar outputs one by one
acc[row][col + 0] = sum_kernel5x5_mask_double(vsrc, vmask1[0], vmask2[0]);
acc[row][col + 1] = sum_kernel5x5_mask_double(vsrc, vmask1[1], vmask2[1]);
acc[row][col + 2] = sum_kernel5x5_mask_double(vsrc, vmask1[2], vmask2[2]);
acc[row][col + 3] = sum_kernel5x5_mask_double(vsrc, vmask1[3], vmask2[3]);
```

Good:

```cpp
static uint32x4_t sum_kernel5x5_mask_double(...) {
  uint32x4_t vsums = ...;
  // Return unreduced partial sum vector
  return vsums;
}

// Batch 4 partial sums and reduce/store in a single vector operation
uint32x4_t sums[4];
sums[0] = sum_kernel5x5_mask_double(vsrc, vmask1[0], vmask2[0]);
sums[1] = sum_kernel5x5_mask_double(vsrc, vmask1[1], vmask2[1]);
sums[2] = sum_kernel5x5_mask_double(vsrc, vmask1[2], vmask2[2]);
sums[3] = sum_kernel5x5_mask_double(vsrc, vmask1[3], vmask2[3]);
vst1q_u32(&acc[row][col], horizontal_add_4d_u32x4(sums));
```
