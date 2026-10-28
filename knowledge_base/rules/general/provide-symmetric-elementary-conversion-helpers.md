---
name: libaom/provide-symmetric-elementary-conversion-helpers
description: When mapping domain values or units, provide symmetric composable elementary conversion helpers rather than compound delta or fused arithmetic functions
rubric: design
severity: should_fix
---

**Rule.** When designing conversion routines between representations, domain
values, or units (such as quantizer floats and index levels `q` / `qindex`),
provide symmetric, elementary 1-to-1 conversion helper functions (converting A
to B and inverse B to A) rather than compound functions that fuse conversions
with arithmetic (such as computing multi-step differences or deltas internally).
Callers needing differences or deltas should compose the elementary conversion
functions directly.

**Why.** Symmetric, elementary conversion functions provide clean two-way
mappings that maximize reusability across callers. Ad-hoc composite helpers that
fuse conversion with arithmetic (like delta calculation or multi-step loops)
duplicate internal conversion logic across parameters, increase API bloat, and
prevent callers that already hold one converted value from reusing the
conversion logic.

## Examples

Bad:

```cpp
// Anti-pattern: Composite helper fusing conversion and delta calculation
int av1_compute_qdelta_fp(double qstart, double qtarget, aom_bit_depth_t bit_depth) {
  int start_index = MAXQ, target_index = MAXQ;
  for (int i = MINQ; i <= MAXQ; ++i) {
    if (av1_convert_qindex_to_q(i, bit_depth) >= qstart) {
      start_index = i;
      break;
    }
  }
  for (int i = MINQ; i <= MAXQ; ++i) {
    if (av1_convert_qindex_to_q(i, bit_depth) >= qtarget) {
      target_index = i;
      break;
    }
  }
  return target_index - start_index;
}
```

Good:

```cpp
// Preferred: Provide symmetric, elementary conversion pair
double av1_convert_qindex_to_q(int qindex, aom_bit_depth_t bit_depth);

int av1_convert_q_to_qindex(double q, aom_bit_depth_t bit_depth) {
  int qindex;
  for (qindex = MINQ; qindex <= MAXQ; ++qindex) {
    if (av1_convert_qindex_to_q(qindex, bit_depth) >= q) break;
  }
  return qindex;
}

// Callers compose conversions and arithmetic directly as needed:
int qdelta = av1_convert_q_to_qindex(qtarget, bit_depth) -
             av1_convert_q_to_qindex(qstart, bit_depth);
```
