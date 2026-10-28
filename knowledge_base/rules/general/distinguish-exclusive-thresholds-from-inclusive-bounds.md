---
name: libaom/distinguish-exclusive-thresholds-from-inclusive-bounds
description: When naming threshold and boundary variables, reserve min_ or max_ for inclusive bounds (>= or <=) and use _thresh for exclusive thresholds (> or <)
rubric: readability
severity: should_fix
---

**Rule.** Do not name variables or struct members min_* or max_* when they
represent exclusive thresholds evaluated with strict inequality (> or <).
Reserve min_* and max_* prefixes for inclusive boundary limits tested with >= or
<=, and name exclusive cutoff values with a *_thresh or *_threshold suffix (or
adjust the comparison to inclusive >= or <=).

**Why.** Naming an exclusive threshold variable min_* or max_* implies an
inclusive bound, making the stored value appear off-by-one when compared with
strict inequality (val > min_*) and causing confusion in assertions, comments,
and condition logic.

## Examples

Bad:

```cpp
// part4_min_bsize_allowed implies inclusive minimum, but comparison is strict inequality
BLOCK_SIZE part4_min_bsize_allowed = cpi->sf.part_sf.ext_partition_eval_thresh;
assert(part4_min_bsize_allowed >= BLOCK_8X8);
bool partition4_allowed = bsize > part4_min_bsize_allowed;
```

Good:

```cpp
// Use *_thresh suffix for exclusive threshold comparisons (bsize > thresh)
BLOCK_SIZE part4_bsize_thresh = cpi->sf.part_sf.ext_partition_eval_thresh;
assert(part4_bsize_thresh >= BLOCK_8X8);
bool partition4_allowed = bsize > part4_bsize_thresh;
```
