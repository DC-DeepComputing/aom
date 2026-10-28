---
name: libaom/av1/use-sad-and-helpers-for-motion-search-distortion
description: When computing candidate block distortion in motion search, use Sum of Absolute Differences (SAD) and dedicated helper functions instead of inlining squared-error loops
rubric: safety
severity: should_fix
---

**Rule.** In motion search, candidate evaluation, and block matching routines,
use Sum of Absolute Differences (SAD) instead of Sum of Squared Differences or
Squared Error (SSD/SSE) for candidate block distortion metrics. Encapsulate
pixel difference and distortion computations into dedicated helper functions or
reuse libaom's architecture-optimized DSP SAD routines (such as aom_sad*) rather
than inlining scalar pixel comparison loops.

**Why.** SAD is computationally significantly cheaper than SSD/SSE during
iterative candidate evaluation in motion search. Factoring distortion
calculations into dedicated helper functions keeps search loops clean, promotes
modularity, and facilitates reusing SIMD-optimized aom_sad kernels across target
CPU architectures.

## Examples

Bad:

```cpp
// Inlining SSD calculation in motion search loop
temp_ssd = 0;
for (int idx_y = 0; idx_y < block_height; idx_y++) {
  for (int idx_x = 0; idx_x < block_width; idx_x++) {
    int pos_curr = block_x + idx_x + (block_y + idx_y) * frame_stride;
    int pos_pred = idx_x + idx_y * pred_block_stride;
    int diff = cur_frame[pos_curr] - pred_blocks[idx][pos_pred];
    temp_ssd += diff * diff;
  }
}
```

Good:

```cpp
// Encapsulate SAD computation in a helper or reuse aom_sad DSP routines
static int get_block_sad(const uint8_t *cur_block, const uint8_t *pred_block,
                         int block_width, int block_height,
                         int cur_stride, int pred_stride) {
  int total_sad = 0;
  for (int y = 0; y < block_height; y++) {
    for (int x = 0; x < block_width; x++) {
      total_sad += abs(cur_block[x + y * cur_stride] -
                       pred_block[x + y * pred_stride]);
    }
  }
  return total_sad;
}

// In motion search loop:
int temp_sad = get_block_sad(cur_block, pred_block, width, height,
                             frame_stride, pred_block_stride);
```
