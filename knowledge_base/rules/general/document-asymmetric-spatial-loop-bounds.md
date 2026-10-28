---
name: libaom/document-asymmetric-spatial-loop-bounds
description: When spatial processing ranges, loop bounds, or step sizes differ between horizontal and vertical dimensions, add an inline comment explaining why they are asymmetric
rubric: readability
severity: nit
---

**Rule.** When spatial processing dimensions, iteration ranges, or loop bounds
(such as x_range vs. y_range or width vs. height steps) use asymmetric
constants, scaling factors, or chunk sizes, add an inline comment explaining the
rationale for the dimensional differences. Document why horizontal and vertical
iteration chunk sizes diverge (e.g., fixed maximum block stride in one direction
versus dynamic superblock or partition height in the other) so reviewers and
maintainers do not assume the asymmetry is an accidental mismatch.

**Why.** In 2D grid, block-level, and loop-filtering operations, asymmetric
bounds (such as fixed column step limits alongside dynamic row heights) resemble
copy-paste errors or dimension mismatches. Explaining intentional differences in
stepping and chunk size prevents confusion, clarifies coordinate assumptions,
and avoids erroneous attempts to homogenize the loop bounds.

## Examples

Bad:

```cpp
const int y_range =
    AOMMIN((int)(plane_mi_rows - mi_row), (1 << mib_size_log2));
const int x_range = AOMMIN((int)(plane_mi_cols - mi_col), MAX_MIB_SIZE);
```

Good:

```cpp
// Height of the unit to filter (y_range) is based on active superblock size,
// whereas width (x_range) is based on maximum superblock size because columns
// are stepped by MAX_MIB_SIZE (e.g., mi_col = 0, MAX_MIB_SIZE, 2 * MAX_MIB_SIZE).
const int y_range =
    AOMMIN((int)(plane_mi_rows - mi_row), (1 << mib_size_log2));
const int x_range = AOMMIN((int)(plane_mi_cols - mi_col), MAX_MIB_SIZE);
```
