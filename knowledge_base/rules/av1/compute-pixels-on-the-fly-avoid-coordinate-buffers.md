---
name: libaom/av1/compute-pixels-on-the-fly-avoid-coordinate-buffers
description: When implementing pixel or block prediction and interpolation loops, compute output samples directly on the fly rather than buffering intermediate coordinates in temporary arrays
rubric: safety
severity: should_fix
---

**Rule.** In pixel prediction, interpolation, and geometric projection loops,
compute and write output pixel values directly on the fly in a single pass
rather than buffering intermediate coordinates or sample grids in temporary
arrays for a subsequent pass. Avoid allocating intermediate coordinate buffers
(such as subpel_x/subpel_y arrays) when coordinates can be mapped and sampled
inline within the output loop.

**Why.** Allocating and storing per-pixel intermediate coordinates across
separate passes introduces allocation overhead, bloats memory footprint, and
degrades data cache locality. Computing pixel values on the fly in a single pass
eliminates redundant buffer management and halves iteration passes over the
block.

## Examples

Bad:

```cpp
// Multi-pass coordinate buffering with separate prediction pass
double *subpel_x = (double *)malloc(w * h * sizeof(double));
double *subpel_y = (double *)malloc(w * h * sizeof(double));
for (int y = 0; y < h; y++) {
  for (int x = 0; x < w; x++) {
    subpel_x[y * w + x] = compute_coord_x(x, y);
    subpel_y[y * w + x] = compute_coord_y(x, y);
  }
}
inter_pred_block(ref_frame, subpel_x, subpel_y, pred_block, w, h);
free(subpel_x);
free(subpel_y);
```

Good:

```cpp
// Single-pass inline coordinate evaluation and pixel interpolation
for (int y = 0; y < h; y++) {
  for (int x = 0; x < w; x++) {
    const double subpel_x = compute_coord_x(x, y);
    const double subpel_y = compute_coord_y(x, y);
    pred_block[y * pred_stride + x] =
        interp_pixel(ref_frame, subpel_x, subpel_y);
  }
}
```
