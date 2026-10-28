---
name: libaom/bundle-coupled-parameters-into-struct
description: When functions take or return multiple tightly coupled scalar parameters, bundle related fields into a dedicated struct rather than passing separate arguments
rubric: design
severity: should_fix
---

**Rule.** Bundle tightly coupled parameters that are consistently passed or
returned together (such as multi-dimensional coordinates, spatial dimensions, or
motion vector components) into a dedicated struct. Avoid bloating function
signatures or creating data clumps with multiple individual scalar or pointer
arguments.

**Why.** Passing related parameters as individual scalar variables creates
bloated signatures, increases the risk of argument transpositions at call sites,
and makes APIs harder to maintain and refactor when new fields are needed.

## Examples

Bad:

```cpp
void av1_motion_search_erp(int block_x, int block_y, int block_width,
                           int block_height, uint8_t *cur_frame,
                           uint8_t *ref_frame, int frame_stride,
                           int frame_width, int frame_height,
                           int pred_block_stride,
                           uint8_t *pred_blocks[128 * 128],
                           uint8_t *best_pred_block, int *pred_block_num,
                           double *best_mv_phi, double *best_mv_theta);
```

Good:

```cpp
struct SphereMV {
  double phi;
  double theta;
};

void av1_motion_search_brute_force_erp(int block_x, int block_y,
                                       int block_width, int block_height,
                                       uint8_t *cur_frame, uint8_t *ref_frame,
                                       int frame_stride, int frame_width,
                                       int frame_height,
                                       struct SphereMV *best_mv);
```
