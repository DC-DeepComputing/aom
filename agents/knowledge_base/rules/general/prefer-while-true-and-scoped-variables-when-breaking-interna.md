---
name: libaom/prefer-while-true-and-scoped-variables-when-breaking-interna
description: When loop termination is handled by an internal break or return, use while (true) or while (1) and declare status variables inside the loop
rubric: readability
severity: nit
---

**Rule.** When a loop terminates via an explicit `break` or `return` inside its
body, write `while (1)` (or `while (true)`) and declare status or job variables
in the narrowest scope inside the loop body. Do not declare outer condition
variables with dummy initializations (such as `int has_jobs = 1; while
(has_jobs)`) when the loop condition is never used to terminate the loop.

**Why.** Declaring loop control variables at the outer function scope with dummy
initial state pollutes the enclosing scope with stale values and obscures
control flow. Using an infinite loop (`while (1)` or `while (true)`) with
internal break checks clearly communicates that loop exit occurs at that point
and keeps variable lifetimes tightly scoped to the iteration body.

## Examples

Bad:

```cpp
// Outer dummy variable initialized just to enter the loop
int has_jobs = 1;
while (has_jobs) {
  has_jobs = get_next_job(tile_data, &current_mi_row);
  if (!has_jobs) break;
  process_job(cpi, tile_data, current_mi_row);
}
```

Good:

```cpp
// Clear infinite loop with status variable scoped to the body
while (1) {
  const int has_jobs = get_next_job(tile_data, &current_mi_row);
  if (!has_jobs) break;
  process_job(cpi, tile_data, current_mi_row);
}
```
