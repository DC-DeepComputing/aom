---
name: libaom/av1/guard-rd-cost-arithmetic-against-sentinel-overflow
description: When performing arithmetic on RD cost variables or modifying stats, guard against INT64_MAX sentinel overflow and encapsulate checks inside calculation helpers
rubric: design
severity: should_fix
---

**Rule.** When performing direct arithmetic, scaling, or offset calculations on
rate-distortion (RD) cost, distortion, or SSE variables (such as this_yrd,
best_rd, or rd_stats->rdcost), verify that the value is strictly less than
INT64_MAX before the operation to prevent signed overflow on invalid candidate
sentinels. For calculation and adjustment helper functions (such as
adjust_rdcost()), encapsulate the INT64_MAX sentinel check directly inside the
helper as an early return rather than duplicating defensive guards across
individual call sites.

**Why.** In the encoder RDO framework, INT64_MAX represents invalid or pruned
modes. Direct addition or scaling on INT64_MAX causes signed 64-bit integer
overflow and undefined behavior. Encapsulating sentinel checks inside adjustment
helper functions eliminates boilerplate and prevents callers from accidentally
omitting required guards.

## Examples

Bad:

```cpp
// Direct arithmetic without sentinel guard:
best_rd = this_yrd + bias_offset;

// Duplicating defensive checks at every helper call site:
if (rd_stats->rdcost < INT64_MAX) {
  adjust_rdcost(cpi, x, rd_stats, /*is_inter_pred=*/true);
}
```

Good:

```cpp
// Guard direct arithmetic against sentinel overflow:
if (this_yrd < INT64_MAX) {
  best_rd = this_yrd + bias_offset;
}

// Encapsulate sentinel check inside helper rather than at call site:
// In adjust_rdcost():
if (rd_cost->rdcost == INT64_MAX) return;
// At call site:
adjust_rdcost(cpi, x, rd_stats, /*is_inter_pred=*/true);
```
