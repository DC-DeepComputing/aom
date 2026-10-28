---
name: libaom/av1/avoid-redundant-quantizer-conversions
description: When computing quantizer index deltas from an available base qindex, convert target quantizer values directly instead of round-tripping through av1_compute_qdelta
rubric: safety
severity: should_fix
---

**Rule.** When computing quantizer index deltas or adjusted qindices from an
already-known base qindex, convert only the target quantizer value back to an
index (e.g. using av1_convert_q_to_qindex()) and compute the index difference
directly. Do not convert the base qindex into a float quantizer value only to
pass both into av1_compute_qdelta() or helper functions that perform redundant
table searches to re-invert the base value.

**Why.** Inverting float or double quantizer values back to a qindex (via
av1_convert_q_to_qindex or av1_find_qindex) requires iterating over quantizer
lookup tables. When the base qindex is already available, converting it to a
float and re-searching for its index wastes CPU cycles on redundant table
lookups.

## Examples

Bad:

```cpp
// base_qindex is already known in scope
double base_q = av1_convert_qindex_to_q(base_qindex, bit_depth);
double target_q = base_q / qstep_ratio;
int delta_q = -av1_compute_qdelta(&cpi->rc, base_q, target_q, bit_depth);
int qindex = base_qindex + delta_q;
```

Good:

```cpp
// base_qindex is already known in scope
double base_q = av1_convert_qindex_to_q(base_qindex, bit_depth);
double target_q = base_q / qstep_ratio;
int target_qindex = av1_convert_q_to_qindex(target_q, bit_depth);
int delta_q = target_qindex - base_qindex;
int qindex = target_qindex;
```
