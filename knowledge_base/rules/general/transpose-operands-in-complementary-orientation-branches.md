---
name: libaom/transpose-operands-in-complementary-orientation-branches
description: When branching on complementary orientations or dimensions (such as col vs row or width vs height), verify that dimension operands are transposed in complementary branches
rubric: correctness
severity: should_fix
---

**Rule.** When implementing symmetric logic for complementary orientations or
dimensions (such as width vs. height, col vs. row, or horizontal vs. vertical
branches), ensure that dimension operands are properly transposed in the
complementary branch rather than reusing or copy-pasting the forward branch
condition.

**Why.** Reusing condition expressions across complementary orientation branches
without transposing compared dimension variables (e.g., checking 'col == row *
2' inside an 'else' / 'col < row' branch) creates unsatisfiable conditions, dead
code branches, and incorrect aspect ratio or transform calculations.

## Examples

Bad:

```cpp
if (col > row) {
  if (col == row * 2) return 1;
  if (col == row * 4) return 2;
} else {
  // Bug: Unsatisfiable condition when col < row
  if (col == row * 2) return -1;
  if (col == row * 4) return -2;
}
```

Good:

```cpp
if (col > row) {
  if (col == row * 2) return 1;
  if (col == row * 4) return 2;
} else {
  // Transpose operands for complementary orientation (col < row)
  if (row == col * 2) return -1;
  if (row == col * 4) return -2;
}
```
