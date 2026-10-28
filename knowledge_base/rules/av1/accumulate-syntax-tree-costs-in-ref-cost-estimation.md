---
name: libaom/av1/accumulate-syntax-tree-costs-in-ref-cost-estimation
description: When estimating reference frame costs in encoder mode evaluation, accumulate all syntax tree branch costs from mode_costs into ref_costs rather than assigning only base costs
rubric: correctness
severity: should_fix
---

**Rule.** When estimating reference frame signaling costs (such as
ref_costs_single or ref_costs_comp) during RD or non-RD mode evaluation, ensure
that all active and newly enabled reference frames (including LAST2_FRAME,
LAST3_FRAME, BWDREF_FRAME, and ALTREF2_FRAME) accumulate their respective syntax
tree branch costs from mode_costs (e.g., single_ref_cost, comp_ref_cost). Do not
initialize reference costs with only the base cost without adding the syntax
tree signaling overhead.

**Why.** AV1 encodes reference frame selections using a decision tree where each
branch has a distinct probability and signaling cost. Underestimating the
signaling rate by omitting syntax tree costs skews rate-distortion optimization
and non-RD mode selection, causing the encoder to disproportionately favor
higher-overhead reference frames.

## Examples

Bad:

```cpp
if (is_one_pass_rt_lag) {
  ref_costs_single[LAST2_FRAME] = base_cost;
  ref_costs_single[LAST3_FRAME] = base_cost;
  ref_costs_single[BWDREF_FRAME] = base_cost;
  ref_costs_single[ALTREF2_FRAME] = base_cost;
}
```

Good:

```cpp
if (is_one_pass_rt_lag) {
  ref_costs_single[LAST2_FRAME] = base_cost;
  ref_costs_single[LAST3_FRAME] = base_cost;
  ref_costs_single[BWDREF_FRAME] = base_cost;
  ref_costs_single[ALTREF2_FRAME] = base_cost;
  ref_costs_single[LAST2_FRAME] += mode_costs->single_ref_cost[0][0][0];
  ref_costs_single[LAST3_FRAME] += mode_costs->single_ref_cost[0][0][0];
  ref_costs_single[BWDREF_FRAME] += mode_costs->single_ref_cost[0][0][1];
  ref_costs_single[BWDREF_FRAME] += mode_costs->single_ref_cost[0][1][0];
  ref_costs_single[ALTREF2_FRAME] += mode_costs->single_ref_cost[0][0][1];
  ref_costs_single[ALTREF2_FRAME] += mode_costs->single_ref_cost[0][2][0];
}
```
