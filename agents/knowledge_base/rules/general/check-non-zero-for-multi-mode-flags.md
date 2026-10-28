---
name: libaom/check-non-zero-for-multi-mode-flags
description: When testing whether a multi-mode configuration parameter or flag is active across non-zero modes, check != 0 or > 0 instead of chaining equality checks
rubric: readability
severity: nit
---

**Rule.** When checking whether a multi-value flag or configuration parameter is
enabled across all non-zero operational modes (where 0 indicates disabled and
positive values represent active modes), test param != 0 (or param > 0) rather
than chaining equality comparisons for every individual active mode (param == 1
|| param == 2).

**Why.** Testing != 0 or > 0 concisely captures the enabled state across all
active modes without enumerating every valid integer value, avoids condition
clutter, and remains correct automatically if new positive modes are added.

## Examples

Bad:

```cpp
// Chaining equality checks for each active non-zero mode
if (cpi->oxcf.q_cfg.use_fixed_qp_offsets == 1 ||
    cpi->oxcf.q_cfg.use_fixed_qp_offsets == 2) {
  process_fixed_qp(cpi);
}
```

Good:

```cpp
// Testing non-zero to cover all enabled modes concisely
if (cpi->oxcf.q_cfg.use_fixed_qp_offsets != 0) {
  process_fixed_qp(cpi);
}
```
