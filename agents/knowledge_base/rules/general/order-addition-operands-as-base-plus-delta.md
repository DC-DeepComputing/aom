---
name: libaom/order-addition-operands-as-base-plus-delta
description: When computing adjusted values by adding a delta or offset to a base quantity, order addition operands as base + delta rather than delta + base
rubric: readability
severity: nit
---

**Rule.** When calculating an adjusted value, index, or parameter by adding a
delta, offset, or adjustment value to a base quantity (such as adding
delta_qindex to base_qindex), order the addition operands as `base + delta` (or
`base + offset`) rather than `delta + base`. Keep the primary baseline reference
as the left-hand operand and the modifier as the right-hand operand.

**Why.** Placing the baseline reference quantity before the delta or offset
modifier establishes the primary reference point first, aligning with standard
mathematical notation and codebase conventions across libaom.

## Examples

Bad:

```cpp
// Anti-pattern: Placing the delta modifier before the base value
const int sb_qindex = cm->delta_q_info.delta_q_present_flag
                          ? x->delta_qindex + cm->quant_params.base_qindex
                          : cm->quant_params.base_qindex;

const int target_rate = delta_rate + base_rate;
```

Good:

```cpp
// Idiomatic: Placing the base value first followed by the delta modifier
const int sb_qindex = cm->delta_q_info.delta_q_present_flag
                          ? cm->quant_params.base_qindex + x->delta_qindex
                          : cm->quant_params.base_qindex;

const int target_rate = base_rate + delta_rate;
```
