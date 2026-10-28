---
name: libaom/av1/document-lowbd-reuse-in-highbd-paths
description: When intentionally calling low-bitdepth functions or omitting high-bitdepth specializations in high-bitdepth code paths, document the dynamic range rationale in a comment
rubric: readability
severity: nit
---

**Rule.** When calling a low-bitdepth (8-bit) DSP, transform, or utility
function within high-bitdepth code paths (or when intentionally omitting a
high-bitdepth specialization), document the dynamic range or bit-width rationale
in an explanatory comment. Detail why low-bitdepth precision is sufficient
without risking coefficient overflow or precision loss.

**Why.** In high-bitdepth pipelines, maintainers expect dedicated high-bitdepth
routines. Reusing low-bitdepth implementations without explanation appears to be
an accidental oversight or bug, and may prompt redundant high-bitdepth
reimplementations or confusion during maintenance.

## Examples

Bad:

```cpp
static INLINE void highbd_wht_fwd_txfm(TX_SIZE tx_size, const int16_t *src_diff,
                                       ptrdiff_t src_stride,
                                       tran_low_t *coeff) {
  switch (tx_size) {
    case TX_4X4: aom_hadamard_4x4(src_diff, src_stride, coeff); break;
    case TX_8X8: aom_highbd_hadamard_8x8(src_diff, src_stride, coeff); break;
    default: assert(0);
  }
}
```

Good:

```cpp
static INLINE void highbd_wht_fwd_txfm(TX_SIZE tx_size, const int16_t *src_diff,
                                       ptrdiff_t src_stride,
                                       tran_low_t *coeff) {
  switch (tx_size) {
    // As the output transform coefficients of 4x4 Hadamard transform can be
    // represented using 15 bits (for 12-bit clip), use lowbd hadamard_4x4.
    case TX_4X4: aom_hadamard_4x4(src_diff, src_stride, coeff); break;
    case TX_8X8: aom_highbd_hadamard_8x8(src_diff, src_stride, coeff); break;
    default: assert(0);
  }
}
```
