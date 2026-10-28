---
name: libaom/av1/omit-redundant-shift-rounding-ternary-in-high-bitdepth
description: When computing bit-shifts or rounding values in high-bitdepth functions where bit depth exceeds 8, omit redundant ternary checks like shift > 0
rubric: readability
severity: should_fix
---

**Rule.** In high-bitdepth-specific functions and code blocks where bit depth
(bd or xd->bd) is guaranteed to be greater than 8 (10 or 12 bits), compute
bit-shift offsets and rounding terms directly rather than guarding them with
redundant ternary expressions or conditionals like 'shift > 0 ? 1 << (shift - 1)
: 0'. Use direct expressions like '1 << (shift - 1)', '(1 << shift) >> 1', or
'ROUND_POWER_OF_TWO(val, shift)'.

**Why.** In AV1 high-bitdepth pipelines, bit depth is invariant at 10 or 12
bits, guaranteeing that (bd - 8) and shift = 2 * (bd - 8) are strictly positive
integers (>= 2). Adding runtime ternary checks like 'shift > 0' creates dead
branches and unnecessary visual clutter.

## Examples

Bad:

```cpp
// In high-bitdepth function where bd is 10 or 12
int shift = 2 * (bd - 8);
int rounding = shift > 0 ? 1 << (shift - 1) : 0;
error = (error + rounding) >> shift;
```

Good:

```cpp
// In high-bitdepth function where bd is 10 or 12
int shift = 2 * (bd - 8);
int rounding = 1 << (shift - 1);
error = (error + rounding) >> shift;
```
