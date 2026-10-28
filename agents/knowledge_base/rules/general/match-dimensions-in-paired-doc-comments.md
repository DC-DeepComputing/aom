---
name: libaom/match-dimensions-in-paired-doc-comments
description: When documenting paired or symmetric struct members, variables, or parameters (such as rows/cols or width/height), verify comment text matches the specific dimension without copy-paste discrepancies.
rubric: readability
severity: should_fix
---

**Rule.** When documenting paired or symmetric struct members, variables,
parameters, or functions (such as rows vs. columns, width vs. height, x vs. y,
or horizontal vs. vertical), ensure all dimension and axis references in doc
comments accurately match the specific member. Check copied comments carefully
to prevent stale dimension names or copy-paste discrepancies between symmetric
fields.

**Why.** Doc comments copied across symmetric or paired fields (e.g., width vs.
height, row vs. column, luma vs. chroma) frequently retain stale dimension names
from the original field. Inaccurate dimension references in header documentation
mislead developers and callers regarding which axis or coordinate a field
configures.

## Examples

Bad:

```cpp
/*!
 * Max num of tile cols possible based on frame width.
 */
int max_log2_cols;
/*!
 * Max num of tile rows possible based on frame width.
 */
int max_log2_rows;
```

Good:

```cpp
/*!
 * Max num of tile cols possible based on frame width.
 */
int max_log2_cols;
/*!
 * Max num of tile rows possible based on frame height.
 */
int max_log2_rows;
```
