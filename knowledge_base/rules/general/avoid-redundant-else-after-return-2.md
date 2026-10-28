---
name: libaom/avoid-redundant-else-after-return-2
description: Use early-return guard clauses for preconditions or fallbacks, and omit redundant else or else-if blocks after return, break, continue, or goto
rubric: readability
severity: should_fix
---

**Rule.** Use early-return guard clauses for input validation, null checks
(e.g., `ptr == NULL` or `!cfg`), fast paths, and fallback handlers rather than
wrapping subsequent logic in `else` blocks. When an `if` block unconditionally
terminates control flow via `return`, `break`, `continue`, or `goto`, do not
attach redundant `else` or `else-if` blocks. Similarly, omit redundant `else {
return ...; }` branches when control flow falls through to an identical trailing
default return statement. Flatten subsequent statements and keep the primary
execution path unindented at the base scope.

**Why.** Wrapping main function logic in `else` blocks or nesting statements
after early exits increases indentation depth, introduces unnecessary cognitive
load, and creates diff churn during refactoring. Using early returns isolates
preconditions and fallback logic upfront, keeps git blame intact for the primary
execution path, and conforms to standard readability guidelines (such as
clang-tidy `readability-else-after-return`).

## Examples

Bad:

```cpp
// Guard clause nested in else
if (cfg == NULL) {
  return;
} else {
  cfg->taps = taps;
}

// Redundant else-if and else after return
if (skip_lf > 1) {
  return 1;
} else if (refresh > 0) {
  return 1;
} else {
  return 0;
}
return 0;
```

Good:

```cpp
// Guard clause returns early; main path unindented
if (cfg == NULL) return;
cfg->taps = taps;

// Flattened control flow without redundant else / else-if
if (skip_lf > 1) {
  return 1;
}
if (refresh > 0) {
  return 1;
}
return 0;
```
