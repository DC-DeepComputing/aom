---
name: libaom/name-desired-dimensions-target-not-scaled-before-scaling
description: When computing desired buffer dimensions before determining if scaling is required, name variables target_width and target_height rather than scaled_width and scaled_height
rubric: readability
severity: nit
---

**Rule.** When computing desired dimensions for conditional scaling, resizing,
or buffer reallocation before determining whether scaling is required, name the
variables target_width and target_height (or target_w/target_h) rather than
scaled_width and scaled_height. Reserve scaled_width and scaled_height for
dimensions of buffers that have already been scaled or when scaling is
unconditionally performed.

**Why.** Before checking whether scaling is necessary, the computed dimensions
represent the desired target size (which may match the unscaled buffer size if
no scaling is performed). Naming them scaled_width and scaled_height prematurely
implies that scaling has already occurred or is guaranteed, obscuring the
conditional nature of the operation.

## Examples

Bad:

```cpp
const int scaled_width = for_psnr ? cm->superres_upscaled_width : cm->width;
const int scaled_height =
    for_psnr ? cm->superres_upscaled_height : cm->height;
const bool scaling_required = (scaled_width != unscaled->y_crop_width) ||
                              (scaled_height != unscaled->y_crop_height);
if (scaling_required) {
  // Perform scaling to scaled_width x scaled_height...
}
```

Good:

```cpp
const int target_width = for_psnr ? cm->superres_upscaled_width : cm->width;
const int target_height =
    for_psnr ? cm->superres_upscaled_height : cm->height;
const bool scaling_required = (target_width != unscaled->y_crop_width) ||
                              (target_height != unscaled->y_crop_height);
if (scaling_required) {
  // Perform scaling to target_width x target_height...
}
```
