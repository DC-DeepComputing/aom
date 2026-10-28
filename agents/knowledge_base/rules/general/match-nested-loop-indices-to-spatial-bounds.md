---
name: libaom/match-nested-loop-indices-to-spatial-bounds
description: When writing nested loops for 2D image or block processing, ensure row/height indices are bounded by height limits and column/width indices by width limits
rubric: correctness
severity: blocking
---

**Rule.** When writing nested spatial loops for 2D image, block, plane, or
tensor processing, ensure that row and height index variables are bounded by
height limits and column and width index variables are bounded by width limits.
Verify that vertical and horizontal steps, filters, and termination bounds are
not inadvertently swapped across loop levels.

**Why.** Swapping width and height termination bounds in nested 2D loops leads
to incorrect iteration counts, out-of-bounds indexing, or buffer corruption
whenever dimensions are non-square.

## Examples

Bad:

```cpp
// Bug: 'h' is bounded by in_width, and 'w' is bounded by in_height.
for (int h = 0, u = 0; h < in_width; h += layer_config->skip_height, ++u) {
  for (int w = 0, v = 0; w < in_height;
       w += layer_config->skip_width, ++v) {
    process_block(input, h, w, u, v);
  }
}
```

Good:

```cpp
// Correct: 'h' is bounded by in_height, and 'w' is bounded by in_width.
for (int h = 0, u = 0; h < in_height; h += layer_config->skip_height, ++u) {
  for (int w = 0, v = 0; w < in_width;
       w += layer_config->skip_width, ++v) {
    process_block(input, h, w, u, v);
  }
}
```
