---
name: libaom/av1/verify-inter-mode-evaluated-before-pruning
description: When gating inter-mode pruning heuristics or assertions on prior inter evaluation, check the mode directly with is_inter_mode() rather than search_state->best_rd or best_y_rdcost
rubric: correctness
severity: should_fix
---

**Rule.** When gating candidate inter-mode pruning heuristics or asserting that
a valid inter mode has been tested for the current block, verify prior mode
evaluation directly using is_inter_mode(best_mode) or
is_inter_mode(search_state->best_mbmode.mode). Do not rely on indirect RD cost
fields or accumulators (such as search_state->best_rd or
search_state->best_y_rdcost.rdcost) or checks like best_rd != INT64_MAX as proof
of inter mode evaluation.

**Why.** In rate-distortion optimization (rdopt), search_state->best_rd is
pre-populated by prior partition evaluations or intra search passes, which can
falsely satisfy assertions and trigger inter mode pruning prematurely.
Intermediate block-local cost structures like search_state->best_y_rdcost are
not updated when do_tx_search is false. Inspecting is_inter_mode() directly on
the chosen best mode reliably guarantees that an inter prediction mode was
evaluated and selected without false positives or missed updates across
different transform search configurations.

## Examples

Bad:

```cpp
// Anti-pattern 1: Using search_state->best_rd (retains prior intra/partition costs)
if (skip_nearest_near_mv_using_refmv_weight(x, this_mode, ref_frame_type,
                                            args->search_state->best_rd)) {
  assert(args->search_state->best_rd != INT64_MAX);
  return 1;
}

// Anti-pattern 2: Using best_y_rdcost (not updated when do_tx_search is false)
if (ref_best_rd == INT64_MAX) return 0;
if (skip_nearest_near_mv_using_refmv_weight(
        x, this_mode, ref_frame_type,
        args->search_state->best_y_rdcost.rdcost)) {
  return 1;
}
```

Good:

```cpp
// Directly check whether the best evaluated mode is an inter mode
if (!is_inter_mode(best_mode)) return 0;

if (skip_nearest_near_mv_using_refmv_weight(
        x, this_mode, ref_frame_type,
        args->search_state->best_mbmode.mode)) {
  assert(is_inter_mode(args->search_state->best_mbmode.mode));
  return 1;
}
```
