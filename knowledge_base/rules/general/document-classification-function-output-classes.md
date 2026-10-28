---
name: libaom/document-classification-function-output-classes
description: When implementing classification or binning functions that compute integer class IDs, document the valid output class range and total count in a function comment
rubric: readability
severity: nit
---

**Rule.** When implementing classification, binning, or feature-mapping
functions that calculate integer class IDs or bin indices from input metrics
(such as gradients, variance, or threshold crossings), document the complete
set, range, and total count of possible output classes in a function-level
comment. Clearly describe the physical or mathematical criteria separating each
class or bin.

**Why.** Feature classification and binning functions often condense
multidimensional metrics into a discrete integer class through non-obvious
arithmetic, bit shifts, or sign comparisons. Documenting the exact number of
possible classes and their valid range (e.g. 0 to 30) clarifies table lookup
bounds, prevents out-of-bounds indexing in downstream arrays, and makes the
classification logic transparent to reviewers and maintainers.

## Examples

Bad:

```cpp
static uint8_t get_class_from_grad(int gx, int gy, int bd) {
  const int thresh[] = { 3 << (bd - 8), 6 << (bd - 8), 20 << (bd - 8) };
  const int samesign = (gx * gy > 0);
  int cx = 0, cy = 0;
  // ...
  uint8_t c = cx * 4 + cy;
  if (c > 0) c = 2 * c - samesign;
  return c;
}
```

Good:

```cpp
// Classifies pixel gradients into 31 possible classes (0 to 30) based on
// thresholded horizontal and vertical gradient magnitudes and signs.
static uint8_t get_class_from_grad(int gx, int gy, int bd) {
  const int thresh[] = { 3 << (bd - 8), 6 << (bd - 8), 20 << (bd - 8) };
  const int samesign = (gx * gy > 0);
  int cx = 0, cy = 0;
  // ...
  uint8_t c = cx * 4 + cy;
  if (c > 0) c = 2 * c - samesign;
  return c;
}
```
