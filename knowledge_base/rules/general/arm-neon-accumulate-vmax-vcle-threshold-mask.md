---
name: libaom/arm-neon-accumulate-vmax-vcle-threshold-mask
description: When computing ARM NEON threshold masks over multiple differences, accumulate differences with vmax and compare once with vcle rather than chaining vcgt, vorr, and vmvn
rubric: safety
severity: should_fix
---

**Rule.** When generating ARM NEON masks where multiple values or absolute
differences must all be within a threshold (such as `diff <= thresh`),
accumulate intermediate values using `vmax[q]_*` and perform a single
`vcle[q]_*` comparison against the threshold. Do not perform individual
`vcgt[q]_*` comparisons, combine them with `vorr[q]_*`, and invert the result
with `vmvn[q]_*`.

**Why.** Accumulating maximum absolute differences with `vmax` and performing a
single `vcle` threshold check eliminates multiple vector comparisons,
intermediate boolean OR operations, and bitwise inversions (`vmvn`),
significantly reducing instruction count and register pressure in inner SIMD
kernels.

## Examples

Bad:

```cpp
// Compares each difference against threshold, ORs masks, and inverts
flat_8x8 = vcgt_u8(vabd_u8(p1q1, p0q0), thresh_8x8);
temp_8x8 = vcgt_u8(vabd_u8(p2q2, p0q0), thresh_8x8);
flat_8x8 = vorr_u8(flat_8x8, temp_8x8);
temp_8x8 = vcgt_u8(vabd_u8(p3q3, p0q0), thresh_8x8);
flat_8x8 = vorr_u8(flat_8x8, temp_8x8);
flat_8x8 = vmvn_u8(flat_8x8);
```

Good:

```cpp
// Accumulates max differences first, then performs a single threshold test
flat_8x8 = vabd_u8(p1q1, p0q0);
flat_8x8 = vmax_u8(flat_8x8, vabd_u8(p2q2, p0q0));
flat_8x8 = vmax_u8(flat_8x8, vabd_u8(p3q3, p0q0));
flat_8x8 = vcle_u8(flat_8x8, thresh_8x8);
```
