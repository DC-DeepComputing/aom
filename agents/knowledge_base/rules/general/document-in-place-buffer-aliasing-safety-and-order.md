---
name: libaom/document-in-place-buffer-aliasing-safety-and-order
description: When reusing a buffer in place where source and destination alias or overlap, document why writes do not overwrite unread inputs and state required processing order
rubric: readability
severity: nit
---

**Rule.** When reusing a buffer in place where source and destination arguments
alias or overlap within the same memory region (such as strided filtering or
transform passes), add an explanatory comment detailing why output writes will
not overwrite unread inputs (e.g., stride and offset relationships, or
read-before-write timing). Explicitly document any processing direction or loop
ordering constraints (such as required top-to-bottom or left-to-right
evaluation) necessary to preserve data integrity.

**Why.** In-place buffer reuse across multi-tap filtering or 2D transform
operations creates subtle data hazard risks. If source and destination buffers
alias with differing offsets or strides, documenting the safety invariant and
loop direction dependencies prevents future optimizations or refactorings from
inadvertently reordering loops, changing stride assumptions, or corrupting
unread inputs.

## Examples

Bad:

```cpp
// Intermediate filter result stored directly into comp_pred8.
uint16_t *temp = CONVERT_TO_SHORTPTR(comp_pred8);
...
aom_highbd_convolve8_vert(CONVERT_TO_BYTEPTR(temp_start_vert), MAX_SB_SIZE,
                          comp_pred8, width, NULL, -1, kernel_y, 16, width,
                          height, bd);
```

Good:

```cpp
// The src and dst parameters of 'aom_highbd_convolve8_vert()' share the same
// buffer ('comp_pred8') with different offsets. For any row 'y', the 8-tap
// vertical filter reads rows 'y-3' to 'y+4' (stride = MAX_SB_SIZE) and writes
// result to row 'y-3' (stride = width). Since width <= MAX_SB_SIZE, output is
// written only after its input is read. Processing must proceed strictly from
// top to bottom in increasing row index order to avoid overwriting unread inputs.
uint16_t *temp = CONVERT_TO_SHORTPTR(comp_pred8);
...
aom_highbd_convolve8_vert(CONVERT_TO_BYTEPTR(temp_start_vert), MAX_SB_SIZE,
                          comp_pred8, width, NULL, -1, kernel_y, 16, width,
                          height, bd);
```
