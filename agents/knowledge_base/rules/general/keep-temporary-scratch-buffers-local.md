---
name: libaom/keep-temporary-scratch-buffers-local
description: When declaring function signatures, keep temporary scratchpad arrays and intermediate search buffers local to the function instead of exposing them as parameters
rubric: design
severity: should_fix
---

**Rule.** Do not expose temporary intermediate buffers, candidate evaluation
arrays, or search scratchpads in function parameter lists. Keep scratchpad
storage local to the function (allocated on the stack or scoped locally) unless
caller-managed buffer reuse across invocations is explicitly required for
performance.

**Why.** Exposing internal scratch buffers in function signatures leaks
implementation details, clutters call sites with unnecessary buffer pointers and
counts, and complicates interface maintenance when intermediate search logic
changes.

## Examples

Bad:

```cpp
// Bad: Exposing temporary candidate evaluation arrays and scratchpad pointers in parameters
void av1_motion_search_erp(int block_x, int block_y, int block_width,
                           int block_height, const uint8_t *cur_frame,
                           const uint8_t *ref_frame, int frame_stride,
                           int frame_width, int frame_height,
                           int pred_block_stride,
                           uint8_t *pred_blocks[128 * 128],
                           uint8_t *best_pred_block, int *pred_block_num,
                           double *best_mv_phi, double *best_mv_theta);
```

Good:

```cpp
// Good: Encapsulate scratch buffers locally and pass only input buffers and final output results
void av1_motion_search_brute_force_erp(int block_x, int block_y,
                                       int block_width, int block_height,
                                       const uint8_t *cur_frame,
                                       const uint8_t *ref_frame,
                                       int frame_stride, int frame_width,
                                       int frame_height,
                                       struct SphereMV *best_mv) {
  uint8_t pred_blocks[MAX_CANDIDATES][MAX_BLOCK_SIZE];
  // Internal search logic populates and evaluates local pred_blocks...
}
```
