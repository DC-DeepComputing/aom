---
name: libaom/avoid-side-effects-in-const-getters
description: When implementing query, context, or getter functions taking const pointers, keep them side-effect-free and avoid mutating pointed-to struct fields
rubric: readability
severity: should_fix
---

**Rule.** Do not introduce side-effect mutations to input structures or their
referenced sub-structures inside query, context calculation, or getter functions
(especially functions taking const pointers). Keep query functions pure and
read-only; perform any necessary state adjustments explicitly outside the
function or pass required parameters directly into helper routines.

**Why.** Getter and context calculation functions taking const pointers are
expected to be pure and side-effect-free. Mutating sub-structures through
non-const member pointers violates const correctness contracts, breaks caller
assumptions, and can introduce subtle state desynchronization bugs across
encoding and decoding pipelines.

## Examples

Bad:

```cpp
static INLINE int get_tx_size_context(const MACROBLOCKD *xd) {
  MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  // Mutating nested struct state inside a const query function
  if (has_above) above_mbmi->tree_type = xd->tree_type;
  return is_inter_block(above_mbmi);
}
```

Good:

```cpp
static INLINE int get_tx_size_context(const MACROBLOCKD *xd) {
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  // Pass required values explicitly without mutating input state
  return is_inter_block(above_mbmi, xd->tree_type);
}
```
