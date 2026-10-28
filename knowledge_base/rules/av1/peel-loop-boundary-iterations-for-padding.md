---
name: libaom/av1/peel-loop-boundary-iterations-for-padding
description: When processing 2D blocks requiring edge padding or boundary adjustments, peel boundary iterations rather than evaluating conditional padding checks inside inner loops
rubric: safety
severity: should_fix
---

**Rule.** When implementing 2D block filtering, convolution, or DSP loops that
require special padding or boundary adjustments only on the first or last
iterations, peel the boundary iterations (e.g., initial and trailing rows or
columns) outside the loop. Do not evaluate conditional boundary checks or call
conditional padding helpers inside the inner loop body.

**Why.** Evaluating boundary checks or conditional padding helpers inside the
inner loop adds branching overhead and inhibits SIMD auto-vectorization across
all iterations. Peeling boundary rows/columns allows the main loop body to run
unconditionally with uniform SIMD vector loads and simple, branch-free
execution.

## Examples

Bad:

```cpp
// Anti-pattern: Conditional edge handling checked on every loop iteration
for (int row = 0; row < block_height; ++row) {
  for (int col = 0; col < block_width; col += 4) {
    if (row == 0 || row == block_height - 1 || col == 0) {
      load_and_pad_boundary(src, buf, row, col, block_width, block_height);
    } else {
      load_vector_unconditional(src, buf);
    }
    process_block_simd(buf);
  }
}
```

Good:

```cpp
// Recommended: Peel boundary rows/columns to keep inner loop branch-free
// 1. Process top boundary row(s) with dedicated padding
process_boundary_row(src, 0, block_width);

// 2. Main loop: process interior rows unconditionally without branch checks
for (int row = 1; row < block_height - 1; ++row) {
  for (int col = 0; col < block_width; col += 4) {
    load_vector_unconditional(src + row * stride + col, buf);
    process_block_simd(buf);
  }
}

// 3. Process bottom boundary row(s) with dedicated padding
process_boundary_row(src, block_height - 1, block_width);
```
