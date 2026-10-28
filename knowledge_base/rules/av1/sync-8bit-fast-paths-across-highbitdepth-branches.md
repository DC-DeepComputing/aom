---
name: libaom/av1/sync-8bit-fast-paths-across-highbitdepth-branches
description: 'When updating fast-path or scaling dispatch conditions partitioned by #if CONFIG_AV1_HIGHBITDEPTH, synchronize the 8-bit branch under high bit-depth with the #else branch'
rubric: correctness
severity: should_fix
---

**Rule.** When adding or updating fast-path, scaling, or optimization dispatch
conditions partitioned by `#if CONFIG_AV1_HIGHBITDEPTH` and `#else`, ensure the
8-bit condition under `CONFIG_AV1_HIGHBITDEPTH` (checking `bit_depth ==
AOM_BITS_8`) is updated symmetrically with the `#else` branch.

**Why.** High bit-depth builds (`CONFIG_AV1_HIGHBITDEPTH=1`) still process 8-bit
streams using standard 8-bit optimized routines. If an optimization condition is
added only to the `#else` branch, high bit-depth builds will miss the fast path
for 8-bit inputs and fall back to slower unoptimized paths.

## Examples

Bad:

```cpp
#if CONFIG_AV1_HIGHBITDEPTH
  if ((cm->width << 1) == unscaled->y_crop_width &&
      (cm->height << 1) == unscaled->y_crop_height &&
      cm->seq_params.bit_depth == AOM_BITS_8) {
    av1_resize_and_extend_frame(unscaled, scaled, filter, phase, num_planes);
  }
#else
  if (((cm->width << 1) == unscaled->y_crop_width &&
       (cm->height << 1) == unscaled->y_crop_height) ||
      ((cm->width << 2) == unscaled->y_crop_width &&
       (cm->height << 2) == unscaled->y_crop_height)) {
    av1_resize_and_extend_frame(unscaled, scaled, filter, phase, num_planes);
  }
#endif
```

Good:

```cpp
#if CONFIG_AV1_HIGHBITDEPTH
  if ((((cm->width << 1) == unscaled->y_crop_width &&
        (cm->height << 1) == unscaled->y_crop_height) ||
       ((cm->width << 2) == unscaled->y_crop_width &&
        (cm->height << 2) == unscaled->y_crop_height)) &&
      cm->seq_params.bit_depth == AOM_BITS_8) {
    av1_resize_and_extend_frame(unscaled, scaled, filter, phase, num_planes);
  }
#else
  if (((cm->width << 1) == unscaled->y_crop_width &&
       (cm->height << 1) == unscaled->y_crop_height) ||
      ((cm->width << 2) == unscaled->y_crop_width &&
       (cm->height << 2) == unscaled->y_crop_height)) {
    av1_resize_and_extend_frame(unscaled, scaled, filter, phase, num_planes);
  }
#endif
```
