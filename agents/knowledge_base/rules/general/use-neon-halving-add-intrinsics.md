---
name: libaom/use-neon-halving-add-intrinsics
description: When computing vector averages or halving additions in ARM NEON routines, use halving add intrinsics (vhadd/vrhadd) instead of separate addition and right shifts
rubric: safety
severity: should_fix
---

**Rule.** In ARM NEON SIMD routines, use halving add intrinsics (`vhadd_*` /
`vhaddq_*` or rounding `vrhadd_*` / `vrhaddq_*`) or halving subtract intrinsics
(`vhsub_*` / `vhsubq_*`) when computing vector averages, `(a + b) >> 1`, `(a + b
+ 1) >> 1`, or halving differences rather than separate addition/subtraction and
right-shift instructions.

**Why.** NEON halving add and subtract instructions perform the arithmetic
operation and division by 2 (or rounding division by 2) in a single instruction
without intermediate overflow. Using a single halving instruction eliminates
redundant shift instructions, reduces register pressure, and decreases cycle
count in performance-critical SIMD loops.

## Examples

Bad:

```cpp
// Separate addition and right shift by 1 to compute (a + b) >> 1
int32x4_t sum = vaddq_s32(a, b);
int32x4_t avg = vshrq_n_s32(sum, 1);
```

Good:

```cpp
// Halving add computes (a + b) >> 1 in a single instruction
int32x4_t avg = vhaddq_s32(a, b);
```
