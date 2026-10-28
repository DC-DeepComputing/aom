---
name: libaom/av1/simplify-block-dimension-expressions-in-specialized-simd-ker
description: When implementing size-specialized SIMD or DSP kernels for fixed block dimensions, simplify dimension-dependent expressions to compile-time constant literals instead of retaining variable-based calculations
rubric: readability
severity: blocking
---

**Rule.** When implementing size-specialized SIMD or DSP kernels for fixed block
dimensions (such as 4x4, 8x8, or 16x16), simplify block-dimension-dependent
expressions (such as bw * bh >> 4 or w * h) into compile-time constant literals
or named constants rather than retaining variable-based calculations from
generic implementations.

**Why.** In size-specialized block routines, dimension parameters like bw and bh
are omitted from the function signature. Carrying over variable-based
expressions from generic functions causes compilation errors when feature flags
or conditional paths are compiled, and misses compile-time constant arithmetic
evaluation.

## Examples

Bad:

```cpp
// In an 8x8 specialized SIMD kernel where bw and bh are fixed to 8
int rls_alpha = (bw * bh >> 4) << OPFL_RLS_PARAM_BITS;
```

Good:

```cpp
// In an 8x8 specialized SIMD kernel (8 * 8 >> 4 = 4)
int rls_alpha = 4 << OPFL_RLS_PARAM_BITS;
```
