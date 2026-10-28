---
name: libaom/consistent-simd-accumulator-half-suffixes
description: In SIMD vector routines where operand vectors use _l and _h (or low/high) suffixes, name accumulator variables with matching suffixes instead of indexed arrays
rubric: readability
severity: nit
---

**Rule.** In SIMD vector routines where operand vectors are split into low and
high halves using _l and _h (or _low and _high) suffixes, name corresponding
accumulator variables consistently with matching _l/_h (or _low/_high) suffixes
(such as acc_l and acc_h). Avoid using indexed arrays (such as acc[2]) or
un-suffixed accumulators when accumulating split vector halves.

**Why.** Using matching _l and _h suffixes for accumulator variables clarifies
data flow and makes it immediately obvious which vector half is being
accumulated into which variable. Replacing indexed arrays with distinct named
variables also avoids unnecessary array indexing syntax and keeps variable
naming uniform across SIMD implementations.

## Examples

Bad:

```cpp
int64x2_t acc[2] = { vdupq_n_s64(0), vdupq_n_s64(0) };
do {
  const int16x8_t ds_l = vld1q_s16(ds);
  const int16x8_t ds_h = vld1q_s16(ds + 8);
  const int16x8_t m_l = svget_neonq_s16(svld1ub_s16(pattern, m));
  const int16x8_t m_h = svget_neonq_s16(svld1ub_s16(pattern, m + 8));
  acc[0] = aom_sdotq_s16(acc[0], ds_l, m_l);
  acc[1] = aom_sdotq_s16(acc[1], ds_h, m_h);
} while (...);
```

Good:

```cpp
int64x2_t acc_l = vdupq_n_s64(0);
int64x2_t acc_h = vdupq_n_s64(0);
do {
  const int16x8_t ds_l = vld1q_s16(ds);
  const int16x8_t ds_h = vld1q_s16(ds + 8);
  const int16x8_t m_l = svget_neonq_s16(svld1ub_s16(pattern, m));
  const int16x8_t m_h = svget_neonq_s16(svld1ub_s16(pattern, m + 8));
  acc_l = aom_sdotq_s16(acc_l, ds_l, m_l);
  acc_h = aom_sdotq_s16(acc_h, ds_h, m_h);
} while (...);
```
