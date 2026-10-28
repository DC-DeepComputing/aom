---
name: libaom/av1/document-search-pruning-rationale
description: When introducing encoder search pruning heuristics, early-exit shortcuts, speed features, or candidate exploration traversal orders, document the underlying video coding intuition, statistical rationale, and exploration invariants
rubric: readability
severity: nit
---

**Rule.** When adding or modifying encoder search pruning heuristics (such as
partition, prediction mode, or transform pruning), early-exit checks, speed
features, or custom candidate exploration orders in the encoder, document the
underlying domain rationale and video coding intuition in comments rather than
merely restating what syntax is skipped or looped over. For pruning decisions,
explain why the evaluated condition or metric correlates with skipping candidate
evaluations (such as directional pattern correlation, spatial variance, or
motion uniformity). For candidate exploration or traversal orders, explain how
the ordering optimizes coverage or dispersion under capacity limits or pruning,
and explicitly document invariants (such as visiting each entry once).

**Why.** Mechanical comments that merely restate what code prunes or describe
loop steps provide no insight into why heuristics are sound. Documenting the
underlying video coding, physical, or spatial intuition and invariants helps
future maintainers understand algorithm validity, diagnose unexpected coding
efficiency regressions, evaluate whether pruning heuristics remain valid during
refactoring, and adapt logic as neighboring encoder algorithms evolve.

## Examples

Bad:

```cpp
// Variant 1: Search pruning heuristic
// Prune horizontal partition if center angle of the prediction mode chosen
// by NONE partition is close to 90 degrees (i.e. vertical direction).
if (abs(angle - 90) < threshold) {
  part_state->prune_rect_part[HORZ] = 1;
}

// Variant 2: Candidate exploration traversal order
// Explore the entire frame hierarchically to hash intrabc candidate blocks,
// by starting with coarser steps, towards finer-grained steps.
while (step > 1) { ... }
```

Good:

```cpp
// Variant 1: Search pruning heuristic
// If the prediction mode chosen by NONE partition is close to 90 degrees,
// it implies a dominant vertical pattern, and the chance of choosing a
// vertical rectangular partition is high. Hence, horizontal partition is
// pruned in these cases.
if (abs(angle - 90) < threshold) {
  part_state->prune_rect_part[HORZ] = 1;
}

// Variant 2: Candidate exploration traversal order
// Doing a hierarchical block exploration helps maximize spatial dispersion
// of candidate blocks under the 256-entry cap while minimizing overlap.
// Also, the exploration is designed to visit each block candidate only once.
while (step > 1) { ... }
```
