---
name: libaom/test/exercise-non-zero-coordinate-offsets-in-dsp-tests
description: When writing or testing DSP/SIMD kernels accepting coordinate offset parameters, exercise non-zero offsets across the buffer rather than testing only origin (0, 0)
rubric: testing
severity: should_fix
---

**Rule.** When implementing or testing DSP and SIMD kernels that accept
coordinate offset parameters (such as x_idx, y_idx, col, row, or subpixel
offsets), ensure unit tests sweep multiple non-zero coordinate offsets across
the test buffer rather than evaluating only (0, 0). Verify that pointer offsets
and sub-block stepping match reference C implementations across varied buffer
locations.

**Why.** Testing only zero offsets (0, 0) conceals pointer arithmetic bugs where
SIMD kernels incorrectly ignore coordinate offsets on subsequent rows,
miscalculate row strides, or fail on sub-block boundaries. Exercising non-zero
offsets ensures offset arithmetic and buffer indexing are thoroughly validated
across all sub-blocks.

## Examples

Bad:

```cpp
void CheckAverages(int iterations) {
  // Only testing (0, 0) conceals offset indexing bugs
  int x16_idx = 0;
  int y16_idx = 0;
  CheckAveragesAt(iterations, x16_idx, y16_idx);
}
```

Good:

```cpp
void CheckAverages() {
  // Sweep non-zero coordinate offsets across the block
  for (int x16_idx = 0; x16_idx < this->kDataBlockWidth / 8; x16_idx += 2) {
    for (int y16_idx = 0; y16_idx < this->kDataBlockHeight / 8; y16_idx += 2) {
      CheckAveragesAt(1, x16_idx, y16_idx);
    }
  }
}
```
