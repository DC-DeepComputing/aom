---
name: libaom/document-spatial-boundary-checks
description: When implementing non-obvious spatial coordinate calculations or boundary checks, add inline comments clarifying the geometric cases handled by each branch
rubric: readability
severity: nit
---

**Rule.** Add clarifying inline comments to non-obvious coordinate calculations,
partition checks, and spatial boundary branches (such as superblock, tile, or
frame boundary conditions). Explicitly describe the geometric cases or neighbor
relationships evaluated by each branch condition.

**Why.** Complex spatial arithmetic and block-relative coordinate comparisons
are difficult to decipher at a glance. Explicit inline comments prevent
misunderstandings about which geometric regions or availability conditions each
branch handles, reducing bugs during maintenance and refactoring.

## Examples

Bad:

```cpp
if (tr_mask_row < 0) {
  has_tr = 1;
} else if (tr_mask_col >= sb_mi_size) {
  has_tr = 0;
}
```

Good:

```cpp
// Above super block row
if (tr_mask_row < 0) {
  // Later the tile boundary checker will figure out whether the top-right
  // block is available.
  has_tr = 1;
// Same super block row, but column of tr block is over the right boundary of current super block
} else if (tr_mask_col >= sb_mi_size) {
  has_tr = 0;
}
```
