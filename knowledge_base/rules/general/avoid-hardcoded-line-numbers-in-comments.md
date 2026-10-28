---
name: libaom/avoid-hardcoded-line-numbers-in-comments
description: When referencing code locations or operations in comments, name concrete variables, macros (e.g., AOMMAX), or logic directly instead of citing line numbers, relative positions, or generic prose
rubric: readability
severity: nit
---

**Rule.** When writing comments that explain code locations, calculations, or
operations, refer directly to concrete identifiers, macro/function names (such
as AOMMAX, AOMMIN, or clamp), variables, and semantic conditions. Do not cite
hardcoded line numbers (e.g., 'see line 123'), ambiguous relative spatial
positions (e.g., 'the equation below' or 'the following line'), or generic prose
descriptions of operations (e.g., 'the maximum operation') that can be conflated
with surrounding domain limits or values.

**Why.** Hardcoded line numbers and spatial references become stale and
inaccurate as surrounding code is refactored, reordered, or expanded. Similarly,
generic phrases like 'the maximum operation' or 'the clamp step' can easily be
confused with adjacent domain parameters, limits, or thresholds (such as 'max
columns'). Directly naming the target variable, macro, function, or logic
ensures comments remain clear, unambiguous, and resilient to refactoring.

## Examples

Bad:

```cpp
// Variant 1: Hardcoded line numbers and spatial references become stale across edits.
// Set src_sad_blk_64x64 to NULL for number_spatial_layers > 1, as
// it is never allocated for number_spatial_layers > 1 (see line 3247).
cpi->src_sad_blk_64x64 = NULL;

// Variant 2: Ambiguous relative references when multiple statements follow.
// The weight part of the equation below was determined by iteratively
// increasing weight on CID22 and Daala's subset1.
if (mode == REALTIME) {
  weight = 32;
} else {
  weight = clamp(((255 - qindex) * 3) / 4, 0, 72) + 128;
}
rdmult = (int64_t)((double)rdmult * weight / 128.0);

// Variant 3: Generic prose operation descriptions cause ambiguity with domain limits.
// When a thread encounters an error, num_finished_cols[row] is set to max
// cols. In this case, the maximum operation here ensures that
// num_finished_cols[row] is not overwritten with a smaller value.
row_mt_sync->num_finished_cols[r] =
    AOMMAX(row_mt_sync->num_finished_cols[r], cur);
```

Good:

```cpp
// Variant 1: Directly describe the target condition or allocation logic.
// Set src_sad_blk_64x64 to NULL for number_spatial_layers > 1, as
// it is never allocated for number_spatial_layers > 1 (see the condition
// under which cpi->src_sad_blk_64x64 is allocated later in this function).
cpi->src_sad_blk_64x64 = NULL;

// Variant 2: Directly name the specific variable or calculation being explained.
// Weight terms were determined by iteratively testing various weights
// on CID22 and Daala's subset1.
if (mode == REALTIME) {
  weight = 32;
} else {
  weight = clamp(((255 - qindex) * 3) / 4, 0, 72) + 128;
}
rdmult = (int64_t)((double)rdmult * weight / 128.0);

// Variant 3: Explicitly name the concrete macro identifier (AOMMAX).
// When a thread encounters an error, num_finished_cols[r] is set to maximum
// column number. In this case, the AOMMAX operation here ensures that
// num_finished_cols[r] is not overwritten with a smaller value.
row_mt_sync->num_finished_cols[r] =
    AOMMAX(row_mt_sync->num_finished_cols[r], cur);
```
