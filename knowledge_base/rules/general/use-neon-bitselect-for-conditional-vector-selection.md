---
name: libaom/use-neon-bitselect-for-conditional-vector-selection
description: When conditionally selecting vector elements in ARM NEON code, use bitselect intrinsics (vbslq_* / vbsl_*) instead of manual bitwise logic
rubric: safety
severity: should_fix
---

**Rule.** In ARM NEON SIMD implementations, use dedicated bitselect intrinsics
(vbslq_* / vbsl_*) rather than manually combining bitwise negation, bitwise AND,
bitwise OR, or bit clear intrinsics (vmvnq_*, vandq_*, vorrq_*, vbicq_*) for
conditional vector selection.

**Why.** Dedicated vbslq_* and vbsl_* intrinsics map directly to NEON bitselect
instructions (bsl, bit, bif). Hand-rolled bitwise selections using separate NOT,
AND, and OR intrinsics force compilers that fail to fold the expression to emit
multiple redundant vector instructions, degrading SIMD performance.

## Examples

Bad:

```cpp
const uint16x8_t cmp = vcgtq_u16(dist1, dist0);
ind = vorrq_u16(vandq_u16(vmvnq_u16(cmp), ind), vandq_u16(cmp, ind1));
```

Good:

```cpp
const uint16x8_t cmp = vcgtq_u16(dist1, dist0);
ind = vbslq_u16(cmp, ind1, ind);
```
