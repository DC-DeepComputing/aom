---
name: libaom/av1/use-standard-q-value-and-qindex-terminology
description: When documenting rate control and quantization logic, refer to floating-point quantizer values as 'Q value' and integer table indices as 'qindex'
rubric: readability
severity: nit
---

**Rule.** In rate control comments, docstrings, and explanatory text, refer to
continuous floating-point quantizer values as 'Q value' and integer quantization
table indices as 'qindex'. Avoid ambiguous descriptors such as 'quantizer (q)',
'real Q value', or 'quantizer index'.

**Why.** In AV1 rate control, 'qindex' specifically represents the integer index
into the quantization lookup tables (ranging from MINQ to MAXQ), whereas 'Q
value' represents the continuous floating-point quantizer scale. Using
standardized terminology ('Q value' vs. 'qindex') eliminates ambiguity about
whether an explanation or conversion refers to the table index or the underlying
floating-point scale.

## Examples

Bad:

```cpp
// Converts a quantizer (q) to a qindex.
int av1_convert_q_to_qindex(double q, aom_bit_depth_t bit_depth);

// Convert the index to a real Q value.
double q_val = av1_convert_qindex_to_q(qindex, bit_depth);
```

Good:

```cpp
// Converts a Q value to a qindex.
int av1_convert_q_to_qindex(double q, aom_bit_depth_t bit_depth);

// Convert the qindex to a Q value.
double q_val = av1_convert_qindex_to_q(qindex, bit_depth);
```
