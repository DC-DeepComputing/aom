---
name: libaom/simplify-boolean-flag-computations
description: When computing boolean flags or decision conditions, evaluate compound logical expressions directly rather than tallying temporary condition counters in nested branches
rubric: readability
severity: nit
---

**Rule.** Simplify boolean flag and decision variable computations by evaluating
compound logical expressions (using && and ||) directly. Do not tally boolean
conditions into temporary counter variables across nested if branches when the
decision can be expressed as a single boolean expression.

**Why.** Evaluating compound logical expressions directly makes code more
concise and idiomatic while enabling compiler short-circuit evaluation.
Accumulating condition counts in temporary integer variables and branching on
the threshold introduces unnecessary local state, indentation, and visual
complexity.

## Examples

Bad:

```cpp
prune_sub_8x8 = 0;
if (xd->left_available && xd->up_available) {
  int num_neighbors_le_8x8 = 0;
  num_neighbors_le_8x8 += (xd->left_mbmi->bsize <= BLOCK_8X8);
  num_neighbors_le_8x8 += (xd->above_mbmi->bsize <= BLOCK_8X8);
  if (num_neighbors_le_8x8 < 2) {
    prune_sub_8x8 = 1;
  }
}
```

Good:

```cpp
prune_sub_8x8 = xd->left_available && xd->up_available &&
                (xd->left_mbmi->bsize > BLOCK_8X8 ||
                 xd->above_mbmi->bsize > BLOCK_8X8);
```
