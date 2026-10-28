---
name: libaom/use-curly-braces-for-multi-line-conditionals
description: When writing conditionals (if/else) or loops (for/while/do), always enclose bodies in curly braces, including single-statement, multi-line, nested, or chained branches
rubric: readability
severity: should_fix
---

**Rule.** Always enclose conditional (`if`, `else if`, `else`) and loop (`for`,
`while`, `do-while`) bodies in curly braces (`{}`), even when the body consists
of a single statement, fits on a single line, spans multiple lines, or forms
nested control flow. Do not omit braces on outer or inner control-flow
constructs, multi-branch `if...else` chains, or when conditions span multiple
lines. If any branch in a conditional chain uses braces, enclose all branches in
curly braces.

**Why.** Omitting curly braces around control-flow bodies obscures block
boundaries, introduces dangling-else ambiguities, and impairs readability when
conditions or bodies span multiple lines. It risks silent logic bugs and
indentation errors when subsequent statements, refactorings, or debugging code
are added during maintenance.

## Examples

Bad:

```cpp
// Bad: Single-statement body and multi-line condition without braces
if ((blk_row >= num_rows - 1) || (blk_col >= num_cols - 1))
  return curr_sad;

// Bad: Nested loops without outer braces
for (int k = 0; k < in_channels; ++k)
  for (int l = -half; l < end; ++l) {
    sum += weights[k] * input[k][l];
  }

// Bad: Multi-branch if...else chain without braces
if (variance < 100)
  *q = (worst_q + *q) >> 1;
else
  *q = worst_q;
```

Good:

```cpp
// Good: Multi-line condition with braces
if ((blk_row >= num_rows - 1) || (blk_col >= num_cols - 1)) {
  return curr_sad;
}

// Good: Nested loops with explicit braces at each level
for (int k = 0; k < in_channels; ++k) {
  for (int l = -half; l < end; ++l) {
    sum += weights[k] * input[k][l];
  }
}

// Good: All branches in multi-branch chain enclosed in braces
if (variance < 100) {
  *q = (worst_q + *q) >> 1;
} else {
  *q = worst_q;
}
```
