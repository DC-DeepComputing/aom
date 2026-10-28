---
name: libaom/av1/size-txfm-stage-range-using-max-txfm-stage-num
description: When declaring or sizing stage_range buffers in AV1 transform implementations, use MAX_TXFM_STAGE_NUM instead of hardcoded integer constants
rubric: readability
severity: should_fix
---

**Rule.** In AV1 forward and inverse transform implementations, declare and size
local stage_range arrays (including stage_range_col and stage_range_row) using
MAX_TXFM_STAGE_NUM instead of hardcoded numbers. Avoid magic numbers when sizing
buffers passed to 1D and 2D transform helpers.

**Why.** AV1 transform pipelines across different block sizes and transform
types support up to MAX_TXFM_STAGE_NUM stages. Sizing buffers consistently with
MAX_TXFM_STAGE_NUM adheres to subsystem conventions, prevents magic numbers, and
guarantees uniform buffer dimensions across transform functions.

## Examples

Bad:

```cpp
// Hardcoded buffer sizes
const int8_t stage_range[32] = { 16, 16, 16, 16, 16 };
int8_t stage_range_col[12];
int8_t stage_range_row[12];
```

Good:

```cpp
// Sized using canonical MAX_TXFM_STAGE_NUM macro
const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16, 16, 16, 16, 16 };
int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
```
