---
name: libaom/inline-single-use-simd-loads
description: When writing SIMD or NEON intrinsics, inline single-use vector loads directly into consuming operations rather than creating intermediate temporary variables
rubric: readability
severity: nit
---

**Rule.** In SIMD intrinsics code, inline single-use vector loads directly as
arguments into consuming operations (such as accumulation intrinsics like vpadal
/ vpadalq) rather than storing them in intermediate temporary variables. Reserve
named vector variables for values reused across multiple operations.

**Why.** Passing single-use vector loads directly to consuming operations avoids
declaring and tracking unnecessary temporary vector variables, keeping SIMD
loops and arithmetic pipelines concise and clean.

## Examples

Bad:

```cpp
// Redundant temporary variable for single-use load
uint16x8_t tmp_q = vld1q_u16(above);
sum_q = vpadalq_u16(sum_q, tmp_q);
```

Good:

```cpp
// Single-use load inlined into consuming intrinsic
sum_q = vpadalq_u16(sum_q, vld1q_u16(above));
```
