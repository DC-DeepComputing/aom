---
name: libaom/av1/update-paired-dc-and-ac-quantization-functions
description: When modifying DC or AC quantization step calculations, lookup tables, or boundary conditions, update the corresponding AC or DC counterpart functions symmetrically
rubric: correctness
severity: should_fix
---

**Rule.** When modifying quantization step lookups, scaling formulas, or
boundary condition handling (such as lossless qindex == 0 or clamp limits) in DC
quantization functions (e.g., av1_dc_quant_QTX or dc_qlookup_*), update the
paired AC quantization functions (e.g., av1_ac_quant_QTX or ac_qlookup_*)
symmetrically, and vice versa. Ensure special-case branches and minimum/maximum
quantizer step limits remain consistent between DC and AC coefficient pipelines.

**Why.** DC and AC quantization calculations share paired lookup tables and
scaling logic for quantizer steps across the encoder and decoder. Applying a
boundary condition fix or scaling adjustment to only one half of the pair causes
inconsistent coefficient dequantization and broken rate-distortion behavior in
edge cases like lossless coding.

## Examples

Bad:

```cpp
// When updating DC quantizer handling for lossless mode (qindex == 0)...
int16_t av1_dc_quant_QTX(int qindex, int delta, aom_bit_depth_t bit_depth) {
  const int q_clamped = clamp(qindex + delta, 0, MAXQ);
  if (q_clamped == 0) return 4;
  return dc_qlookup_QTX[q_clamped];
}

// ...omitting the symmetrical check in AC quantizer:
int16_t av1_ac_quant_QTX(int qindex, int delta, aom_bit_depth_t bit_depth) {
  const int q_clamped = clamp(qindex + delta, 0, MAXQ);
  return ac_qlookup_QTX[q_clamped];
}
```

Good:

```cpp
// Update both DC and AC quantization functions symmetrically:
int16_t av1_dc_quant_QTX(int qindex, int delta, aom_bit_depth_t bit_depth) {
  const int q_clamped = clamp(qindex + delta, 0, MAXQ);
  if (q_clamped == 0) return 4;
  return dc_qlookup_QTX[q_clamped];
}

int16_t av1_ac_quant_QTX(int qindex, int delta, aom_bit_depth_t bit_depth) {
  const int q_clamped = clamp(qindex + delta, 0, MAXQ);
  if (q_clamped == 0) return 4;
  return ac_qlookup_QTX[q_clamped];
}
```
