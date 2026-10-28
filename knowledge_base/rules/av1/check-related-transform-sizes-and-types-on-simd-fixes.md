---
name: libaom/av1/check-related-transform-sizes-and-types-on-simd-fixes
description: When fixing precision, overflow, or rounding mismatches in SIMD transform or DSP kernels, check all transposed sizes, related dimensions, and 1D transform types
rubric: correctness
severity: should_fix
---

**Rule.** When fixing a precision, overflow, rounding, or reference-mismatch
defect in a SIMD transform or DSP kernel for a specific block dimension or
transform type (e.g., 4x16 SSSE3 identity transform), inspect all sister
functions, transposed block dimensions (e.g., 16x4), and sibling 1D transform
types (DCT, ADST, FLIPADST, IDTX) that share the same kernel structures or
bit-depth assumptions. Ensure unit tests in test/ cover all affected block size
and transform type combinations.

**Why.** Transform and DSP kernels share common algorithmic pipelines,
intermediate bit-depth bounds, and rounding helper logic across transposed
dimensions and sibling transform families. Patching a mismatch in only one size
or type leaves sibling kernels with identical defects.

## Examples

Bad:

```cpp
// Only fixing 4x16 SSSE3 kernel while leaving transposed 16x4 unpatched:
static void lowbd_inv_txfm2d_add_16x4_ssse3(const int32_t *input,
                                            uint8_t *output, int stride,
                                            TX_TYPE tx_type, ...) {
  ...
  row_txfm(buf, buf, cos_bit_row);
  round_shift_16bit_ssse3(buf, txfm_size_col, shift[0]);
  ...
}
```

Good:

```cpp
// Patching both 4x16 and transposed 16x4 transform kernels for identity:
static void lowbd_inv_txfm2d_add_16x4_ssse3(const int32_t *input,
                                            uint8_t *output, int stride,
                                            TX_TYPE tx_type, ...) {
  ...
  if (row_txfm == iidentity16_new_ssse3) {
    const __m128i scale = pair_set_epi16(2 * NewSqrt2, 3 << (NewSqrt2Bits - 1));
    const __m128i ones = _mm_set1_epi16(1);
    for (int j = 0; j < 16; ++j) {
      const __m128i buf_lo = _mm_unpacklo_epi16(buf[j], ones);
      const __m128i buf_hi = _mm_unpackhi_epi16(buf[j], ones);
      const __m128i buf_32_lo =
          _mm_srai_epi32(_mm_madd_epi16(buf_lo, scale), (NewSqrt2Bits + 1));
      const __m128i buf_32_hi =
          _mm_srai_epi32(_mm_madd_epi16(buf_hi, scale), (NewSqrt2Bits + 1));
      buf[j] = _mm_packs_epi32(buf_32_lo, buf_32_hi);
    }
  } else {
    row_txfm(buf, buf, cos_bit_row);
    round_shift_16bit_ssse3(buf, txfm_size_col, shift[0]);
  }
  ...
}
```
