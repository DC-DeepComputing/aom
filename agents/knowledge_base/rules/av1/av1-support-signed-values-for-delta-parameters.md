---
name: libaom/av1/av1-support-signed-values-for-delta-parameters
description: When declaring bitstream syntax fields or parameters for delta offsets, use signed integer types to support bidirectional adjustments relative to base values
rubric: correctness
severity: should_fix
---

**Rule.** When introducing bitstream syntax fields, struct members, or function
parameters representing delta offsets (such as quantization, loop filter, or
transform deltas), use signed integer types (such as int8_t or int16_t) and
signed bitstream decoding instead of unsigned types. Ensure bitstream reading
and writing logic accounts for negative values (e.g., via offset baselines or
sign bits) so adjustments can increase or decrease the reference value.

**Why.** Delta parameters represent relative adjustments from a nominal base or
reference value. Restricting deltas to unsigned types prevents negative offsets,
limiting encoder flexibility, tuning experiments, and rate-distortion
optimization.

## Examples

Bad:

```cpp
// In SequenceHeader:
uint8_t base_dc_delta_q;

// Reading from bitstream:
seq_params->base_dc_delta_q = aom_rb_read_literal(rb, DELTA_DCQUANT_BITS);
```

Good:

```cpp
// In SequenceHeader:
int8_t base_dc_delta_q;

// Reading from bitstream:
seq_params->base_dc_delta_q =
    DELTA_DCQUANT_MIN + aom_rb_read_literal(rb, DELTA_DCQUANT_BITS);
```
