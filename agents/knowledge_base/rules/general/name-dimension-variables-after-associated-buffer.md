---
name: libaom/name-dimension-variables-after-associated-buffer
description: When defining or computing dimension, stride, or geometry variables for named buffers, qualify variable names with the corresponding buffer instead of generic terms
rubric: readability
severity: nit
---

**Rule.** When defining or calculating dimension, stride, or crop variables
(such as width, height, or stride) associated with specific buffers, frames, or
image objects, qualify the variable names with the corresponding buffer
identifier (e.g., `scaled_width`, `unscaled_height`, `src_stride`, `dst_width`).
Avoid generic or unanchored names (such as bare `target_width`, `target_height`,
or `width`) when multiple buffers or distinct source and destination objects are
processed.

**Why.** In functions operating on multiple image or frame buffers (such as
unscaled and scaled buffers, or source and destination planes), qualifying
dimension variables with their associated buffer explicitly communicates which
entity each variable configures, preventing ambiguity and parameter mismatches.

## Examples

Bad:

```cpp
const int target_width = for_psnr ? cm->superres_upscaled_width : cm->width;
const int target_height =
    for_psnr ? cm->superres_upscaled_height : cm->height;
const bool scaling_required = (target_width != unscaled->y_crop_width) ||
                              (target_height != unscaled->y_crop_height);

if (scaling_required) {
  if (aom_realloc_frame_buffer(
          scaled, target_width, target_height, ...))
```

Good:

```cpp
const int scaled_width = for_psnr ? cm->superres_upscaled_width : cm->width;
const int scaled_height =
    for_psnr ? cm->superres_upscaled_height : cm->height;
const bool scaling_required = (scaled_width != unscaled->y_crop_width) ||
                              (scaled_height != unscaled->y_crop_height);

if (scaling_required) {
  if (aom_realloc_frame_buffer(
          scaled, scaled_width, scaled_height, ...))
```
