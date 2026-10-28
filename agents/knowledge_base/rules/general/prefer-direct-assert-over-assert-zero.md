---
name: libaom/prefer-direct-assert-over-assert-zero
description: When asserting invariants, bounds, or return values, assert expected conditions directly with assert(expr) rather than if (...) assert(0), casting assert-only variables to (void)
rubric: readability
severity: nit
---

**Rule.** Assert invariants, bounds, and return values directly using
`assert(condition)` (e.g., `assert(!ret);`, `assert(status == AOM_CODEC_OK);`,
or `assert(width > 0);`) rather than branching with `if (...)` to call
`assert(0)` or `assert(false)`. Reserve `assert(0)` strictly for structurally
unreachable code paths (such as impossible `default:` cases in `switch`
statements). Additionally, when a variable or return value is evaluated solely
inside assertions, cast it explicitly to void (e.g., `(void)ret;`) to prevent
unused variable warnings in release (`NDEBUG`) builds.

**Why.** Direct assertions state the required invariant clearly without inverted
branch logic, print the actual failed expression in assertion diagnostics
instead of a generic `0` or `false`, and ensure debug checks are eliminated
cleanly in release builds without leaving dead branches. Because assertions are
compiled out when `NDEBUG` is defined, casting assert-only variables to `(void)`
ensures clean release compilation across compilers.

## Examples

Bad:

```cpp
// Bad: branching to assert(0) on error return value
const int ret = av1_selfguided_restoration_c(
    dat8, width, height, stride, flt0, flt1, width, eps, bit_depth, highbd);
if (ret) assert(0);

// Bad: branching on inverted condition/bounds check
if (subpel_search_method > SUBPEL_TREE_PRUNED_MORE) assert(0);
if (width <= 0 || height <= 0) {
  assert(0);
}
```

Good:

```cpp
// Good: direct assertion with (void) cast to prevent release build warnings
const int ret = av1_selfguided_restoration_c(
    dat8, width, height, stride, flt0, flt1, width, eps, bit_depth, highbd);
(void)ret;
assert(!ret);

// Good: directly asserting expected bounds and invariants
assert(subpel_search_method <= SUBPEL_TREE_PRUNED_MORE);
assert(width > 0 && height > 0);
```
