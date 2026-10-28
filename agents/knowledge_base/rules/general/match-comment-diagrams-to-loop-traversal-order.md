---
name: libaom/match-comment-diagrams-to-loop-traversal-order
description: When illustrating coordinate traversal orders or spatial grids in ASCII diagrams, verify axes match nested loop ordering and distinguish region dimensions from origin grids
rubric: readability
severity: should_fix
---

**Rule.** When illustrating coordinate traversal orders, spatial block layouts,
or iteration steps in comments using ASCII diagrams or grids, ensure the
documentation accurately matches the underlying algorithm:
1. Verify that coordinate axes (such as x vs. y or column vs. row) and sequence numbers match the code's nested loop ordering, ensuring outer and inner loop traversal directions are not transposed or inverted.
2. Accurately distinguish overall frame or region dimensions from candidate block origin (top-left corner) coordinate grids. Explicitly specify both the total region size (e.g., an 8x8 region) and the resulting origin coordinate range (e.g., a 5x5 grid of valid top-left positions for 4x4 blocks), rather than mislabeling the origin coordinate grid as the total canvas or image dimension.

**Why.** Transposed axes, mismatched iteration indices, or confusing valid block
origin coordinate ranges with total canvas dimensions mislead reviewers and
maintainers verifying boundary conditions, buffer sizes, and spatial traversal
logic. Clear and accurate ASCII diagrams prevent off-by-block-size errors,
clarify loop directionality, and ensure coordinate exploration algorithms are
easy to audit.

## Examples

Bad:

```cpp
// Variant 1: Diagram transposes axes relative to outer x_pos, inner y_pos loops
//    x  0  1  2  3  4
//  y +---------------
//  0 |  1 10  5 11  2
//  1 | 16 22 17 23 18
//  2 |  7 12  9 13  8
//  3 | 19 24 20 25 21
//  4 |  3 14  6 15  4
while (step > 1) {
  for (int x_pos = x_offset; x_pos < x_end; x_pos += step) {
    for (int y_pos = y_offset; y_pos < y_end; y_pos += step) {
      process_block(y_pos * pic_width + x_pos, x_pos, y_pos);
    }
  }
}

// Variant 2: Confusing 5x5 origin coordinate grid with overall canvas dimensions
// This is the coordinate exploration order example for a 5x5 image, with
// block_size = 4:
//    x  0  1  2  3  4
//  y +---------------
//  0 |  0  1  2  3  4
//  1 |  5  6  7  8  9
```

Good:

```cpp
// Variant 1: Diagram matches outer x_pos (column), inner y_pos (row) iteration
//    x  0  1  2  3  4
//  y +---------------
//  0 |  1 10  5 13  3
//  1 | 16 22 18 24 20
//  2 |  7 11  9 14  8
//  3 | 17 23 19 25 21
//  4 |  2 12  6 15  4
while (step > 1) {
  for (int x_pos = x_offset; x_pos < x_end; x_pos += step) {
    for (int y_pos = y_offset; y_pos < y_end; y_pos += step) {
      process_block(y_pos * pic_width + x_pos, x_pos, y_pos);
    }
  }
}

// Variant 2: Distinguishing total 8x8 region size from 5x5 candidate block origins
// This is the coordinate exploration order example for an 8x8 region, with
// block_size = 4. The top-left corner (x, y) coordinates of each candidate
// block are shown below. There are 5 * 5 (25) candidate blocks:
//    x  0  1  2  3  4
//  y +---------------
//  0 |  0  1  2  3  4
//  1 |  5  6  7  8  9
```
