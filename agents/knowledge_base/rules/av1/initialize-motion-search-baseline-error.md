---
name: libaom/av1/initialize-motion-search-baseline-error
description: When implementing motion estimation or parameter search loops, initialize baseline distortion with the zero-motion or start predictor rather than an arbitrary magic number
rubric: readability
severity: should_fix
---

**Rule.** In motion search, spherical projection search, or parameter
optimization routines, initialize the baseline best error by computing the
distortion of the zero-motion predictor or starting vector rather than assigning
an arbitrary large magic number.

**Why.** Evaluating the default zero-motion or starting predictor upfront
establishes a valid baseline distortion so candidate offsets must outperform the
default to be selected, and avoids fragile magic constants that risk search
anomalies.

## Examples

Bad:

```cpp
// Initializing best error with an arbitrary large magic number
double best_ssd = 100000000;
for (delta_phi = -30 * search_step_phi; delta_phi <= 30 * search_step_phi;
     delta_phi += search_step_phi) {
  av1_get_pred_erp(block_x, block_y, block_width, block_height, delta_phi, 0,
                   ref_frame, frame_stride, frame_width, frame_height,
                   pred_block_stride, pred_block);
  const double ssd = av1_get_blocks_ssd(cur_block, pred_block, block_width,
                                        block_height, frame_stride,
                                        pred_block_stride);
  if (ssd < best_ssd) {
    best_ssd = ssd;
  }
}
```

Good:

```cpp
// Compute baseline distortion for zero-motion predictor up front
av1_get_pred_erp(block_x, block_y, block_width, block_height, 0, 0,
                 ref_frame, frame_stride, frame_width, frame_height,
                 pred_block_stride, pred_blocks[0]);
double best_ssd = av1_get_blocks_ssd(cur_block, pred_blocks[0], block_width,
                                     block_height, frame_stride,
                                     pred_block_stride);
for (int i = -30; i <= 30; i++) {
  const double delta_phi = i * search_step_phi;
  ...
}
```
