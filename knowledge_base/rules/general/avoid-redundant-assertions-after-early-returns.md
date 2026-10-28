---
name: libaom/avoid-redundant-assertions-after-early-returns
description: When writing assertions, omit redundant or tautological assert() or AOM_CHECK() checks for conditions already guaranteed by enclosing if branches, preceding early returns, or variable types
rubric: readability
severity: nit
---

**Rule.** Do not add or retain redundant or tautological assertion checks
(`assert()`, `aom_assert()`, `AOM_ASSERT()`, `AOM_CHECK()`, `ASSERT()`) for
invariants already guaranteed by preceding control flow, enclosing conditional
branches, variable assignments, or variable types (such as unsigned
non-negativity). Specifically:
1. If an earlier `if` block returns, breaks, continues, or exits on a condition, do not assert the negated condition immediately following the block.
2. Inside an `if` block, do not assert conditions that are already trivially tested or implied by the enclosing branch condition. Rely on branch guards directly, or place precondition assertions inside downstream callee functions that require them.
3. If a variable type or prior assignment structurally guarantees a condition, omit tautological checks (e.g., asserting an `unsigned` integer is `>= 0` or `>= 1` immediately after clamping).

**Why.** Assertions that duplicate enclosing branch conditions, restate the
negation of a preceding early-return guard, or check tautological type
properties add visual noise and maintenance overhead without providing debugging
value, runtime safety, or bug prevention. They can also mislead readers into
believing a variable might hold an impossible state. Moving necessary
precondition assertions into downstream callee functions ensures invariants are
enforced consistently across all call sites rather than cluttering callers.

## Examples

Bad:

```cpp
// Redundant assertion after early return
if (use_palette) {
  build_palette_pred(xd, plane, ...);
  return;
}
assert(use_palette == 0);

// Redundant assertion inside matching if branch
if (cpi->gf_frame_index < gf_group->size && !(frame_flags & FRAMEFLAGS_KEY)) {
  assert(cpi->gf_frame_index < gf_group->size);
  av1_setup_target_rate(cpi);
}

// Tautological assertion guaranteed by type or prior assignment
unsigned int variance = av1_get_block_variance_boost(cpi, x);
if (variance == 0) variance = 1;
assert(variance >= 1);  // Redundant: variance is unsigned and guaranteed >= 1
```

Good:

```cpp
// Proceed directly after early return
if (use_palette) {
  build_palette_pred(xd, plane, ...);
  return;
}
build_intra_pred(xd, plane, ...);

// Rely directly on enclosing branch condition
if (cpi->gf_frame_index < gf_group->size && !(frame_flags & FRAMEFLAGS_KEY)) {
  av1_setup_target_rate(cpi);
}

// Rely on type guarantees and clamping without redundant assertions
unsigned int variance = av1_get_block_variance_boost(cpi, x);
if (variance == 0) variance = 1;
```
