---
name: libaom/consolidate-redundant-assertions
description: When writing debug assertions, consolidate adjacent assert statements and fold conditional prerequisites directly into assert expressions instead of wrapping asserts in if statements
rubric: readability
severity: should_fix
---

**Rule.** Consolidate multiple adjacent or closely related assert() (or
aom_assert()) statements into a single compound assertion using logical AND
(&&), and fold conditional checks into compound assert expressions using logical
disjunction (e.g., assert(!cond || check)) instead of wrapping assert() in an
outer if statement. Omit redundant assertions that re-check invariants or bounds
already validated by preceding helper functions or earlier guards.

**Why.** Fragmented assertion lines create visual clutter, and wrapping assert()
inside a runtime if statement leaves empty branching checks in non-assert or
NDEBUG builds. Folding conditions into compound assert expressions compiles out
the entire check when assertions are disabled while keeping invariant checks
concise.

## Examples

Bad:

```cpp
// Consolidate adjacent asserts
assert(output_num >= 0);
const int branch = layer_config->branch;
assert(branch >= 0 && branch < CNN_MAX_BRANCHES);

// Do not wrap assert in an outer if condition
if (filter_params_x->taps <= 8)
  assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
```

Good:

```cpp
// Consolidated compound assertion
const int branch = layer_config->branch;
assert(output_num >= 0 && branch >= 0 && branch < CNN_MAX_BRANCHES);

// Fold conditional prerequisite into assert expression
assert(filter_params_x->taps > 8 ||
       (0 <= sum && sum < (1 << (bd + FILTER_BITS + 1))));
```
