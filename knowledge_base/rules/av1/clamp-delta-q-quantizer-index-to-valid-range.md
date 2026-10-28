---
name: libaom/av1/clamp-delta-q-quantizer-index-to-valid-range
description: When adjusting base quantizer indices with delta-q offsets (such as delta_qindex), clamp the calculated index to [0, QINDEX_RANGE - 1] or [0, MAXQ]
rubric: correctness
severity: should_fix
---

**Rule.** Clamp quantization indices calculated by adding delta-q offsets (such
as `base_qindex + delta_qindex` or `base_qindex + rdmult_delta_qindex`) to `[0,
QINDEX_RANGE - 1]` (or `[0, MAXQ]`) using `clamp()`. Do not use unclamped
delta-q sums when calculating superblock, block, or segment quantizer indices.

**Why.** Adding delta_qindex offsets to base_qindex can cause the resulting
quantizer index to fall below 0 or exceed the maximum allowed index (MAXQ /
QINDEX_RANGE - 1). Unclamped indices lead to out-of-bounds array lookups in
quantizer lookup tables (such as mode threshold or SAD LUTs) and invalid
quantization calculations.

## Examples

Bad:

```cpp
const int sb_qindex = cm->delta_q_info.delta_q_present_flag
                          ? cm->quant_params.base_qindex + x->delta_qindex
                          : cm->quant_params.base_qindex;
```

Good:

```cpp
const int sb_qindex =
    clamp(cm->delta_q_info.delta_q_present_flag
              ? cm->quant_params.base_qindex + x->delta_qindex
              : cm->quant_params.base_qindex,
          0, QINDEX_RANGE - 1);
```
