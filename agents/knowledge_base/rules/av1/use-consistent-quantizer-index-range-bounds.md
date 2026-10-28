---
name: libaom/av1/use-consistent-quantizer-index-range-bounds
description: When clamping or bounding quantizer indices (qindex), use consistent endpoints with either MINQ, MAXQ or 0, QINDEX_RANGE - 1 instead of mixing numeric literals with macros
rubric: readability
severity: nit
---

**Rule.** When clamping or bounding quantizer indices (`qindex`), specify
consistent range endpoints using either matching macro bounds (`MINQ`, `MAXQ`)
or offset range bounds (`0`, `QINDEX_RANGE - 1`). Do not mix numeric literals
with macro constants (such as `0, MAXQ` or `MINQ, QINDEX_RANGE - 1`).

**Why.** In AV1, `MINQ` and `MAXQ` are defined as the minimum and maximum
quantizer index endpoints (0 and 255), while `QINDEX_RANGE` represents the total
span (`MAXQ - MINQ + 1`). Mixing literal `0` with `MAXQ` creates inconsistent
boundary abstractions, whereas using paired macro endpoints (`MINQ, MAXQ`) or
zero-based range bounds (`0, QINDEX_RANGE - 1`) preserves readability and aligns
with quantizer clamping patterns across the codebase.

## Examples

Bad:

```cpp
const int sb_qindex = clamp(
    cm->delta_q_info.delta_q_present_flag
        ? cm->quant_params.base_qindex + x->delta_qindex
        : cm->quant_params.base_qindex,
    0, MAXQ);
```

Good:

```cpp
// Use 0, QINDEX_RANGE - 1 or MINQ, MAXQ consistently
const int sb_qindex = clamp(
    cm->delta_q_info.delta_q_present_flag
        ? cm->quant_params.base_qindex + x->delta_qindex
        : cm->quant_params.base_qindex,
    0, QINDEX_RANGE - 1);
```
