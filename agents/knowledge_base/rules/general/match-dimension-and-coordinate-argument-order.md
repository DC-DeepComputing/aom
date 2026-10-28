---
name: libaom/match-dimension-and-coordinate-argument-order
description: When passing same-typed dimension or coordinate pairs (such as rows/cols, width/height, or x/y), match the callee's parameter order and use inline parameter comments.
rubric: correctness
severity: should_fix
---

**Rule.** When calling functions that accept adjacent same-typed parameters for
geometric dimensions, coordinates, or 2D grid/block bounds (such as rows and
cols, height and width, or x and y), verify that arguments match the callee's
exact parameter order (e.g., passing height to rows and width to cols, or width
before height). Do not assume a uniform ordering across subsystems. Use inline
parameter comments (such as /*rows=*/, /*cols=*/, /*width=*/, or /*height=*/) at
call sites where argument order could be ambiguous to prevent accidental
transposition.

**Why.** Because adjacent dimension and coordinate parameters share identical
primitive types (e.g., int, int32_t, double, size_t), transposing arguments
(such as passing width for rows/height or swapping x and y) compiles silently
without type-checker diagnostics or compiler warnings. While square blocks and
symmetric dimensions mask transposition bugs during testing, non-square
rectangular blocks (e.g., 16x8 or 4x16) or asymmetric frames cause inverted
buffer strides, incorrect loop bounds, distorted geometric projections, and
potential out-of-bounds memory accesses. Inline parameter comments make
positional mappings explicit and clarify argument intent.

## Examples

Bad:

```cpp
// 1. Transposing height and width when callee expects (width, height):
av1_sphere_to_plane_erp(phi + delta_phi, theta + delta_theta, frame_height,
                        frame_width, &x_ref, &y_ref);

// 2. Passing width then height to a function expecting rows then cols:
av1_count_colors_highbd(this_src, stride, kBlockWidth, kBlockHeight, bd, NULL,
                        count_buf, &n_colors, NULL);
```

Good:

```cpp
// 1. Correct argument order matching callee signature (width then height):
av1_sphere_to_plane_erp(phi + delta_phi, theta + delta_theta, frame_width,
                        frame_height, &x_ref, &y_ref);

// 2. Pass height to rows and width to cols with inline parameter comments:
av1_count_colors_highbd(this_src, stride, /*rows=*/kBlockHeight,
                        /*cols=*/kBlockWidth, bd, NULL, count_buf, &n_colors,
                        NULL);
```
