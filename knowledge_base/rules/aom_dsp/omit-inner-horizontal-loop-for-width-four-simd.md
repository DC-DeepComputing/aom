---
name: libaom/aom_dsp/omit-inner-horizontal-loop-for-width-four-simd
description: In SIMD and DSP kernels with power-of-two block dimensions, handle minimum width 4 in an else branch without inner horizontal loops or redundant checks
rubric: readability
severity: should_fix
---

**Rule.** In SIMD and DSP kernels processing power-of-two block dimensions (4,
8, 16, 32, 64, 128), after handling widths of 8 or greater, handle width 4
directly in an else branch rather than writing a redundant `if (w == 4)` check.
Omit inner horizontal loops, width subtractions (such as `w_tmp -= 4`), and
multi-chunk iterations for width 4, processing each row in a single fixed-width
pass.

**Why.** In AV1 and libaom DSP kernels, block dimensions are strictly powers of
two and at least 4 pixels wide. Once widths >= 8 are handled, width 4 is the
only remaining valid dimension; writing an explicit conditional check is
redundant, and writing an inner horizontal loop introduces dead logic and
unnecessary branching overhead.

## Examples

Bad:

```cpp
if (w_tmp > 7) {
  // Process widths >= 8 with horizontal unrolling
  // ...
}
// Redundant conditional check and redundant inner horizontal loop for width 4
if (w_tmp == 4) {
  do {
    int w_inner = w_tmp;
    do {
      blend_w4_neon(dst, src0, src1, mask);
      w_inner -= 4;
    } while (w_inner > 3);
    dst += dst_stride;
  } while (--h != 0);
}
```

Good:

```cpp
if (w_tmp > 7) {
  // Process widths >= 8 with horizontal unrolling
  // ...
} else {
  // Width is guaranteed to be 4; process directly without inner horizontal loop
  do {
    blend_w4_neon(dst, src0, src1, mask);
    dst += dst_stride;
  } while (--h != 0);
}
```
