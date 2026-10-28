---
name: libaom/av1/declare-local-variables-for-intra-mode-neighbor-flags
description: When querying intra mode neighbor bitmasks in reconstruction routines, declare named local flag variables consistently rather than mixing locals with inline table lookups
rubric: readability
severity: nit
---

**Rule.** When querying intra mode neighbor requirement bitmasks (such as
extend_modes[mode] & NEED_*) in intra prediction routines, declare named const
local flag variables for all required neighbor checks (e.g., need_left,
need_above, need_above_left). Do not mix local flag variables with inline table
lookup expressions across related conditional branches.

**Why.** Consistently declaring named local constants for all evaluated neighbor
requirements improves code readability, prevents duplicate table lookups across
multiple branch conditions, and aligns with the established convention across
sibling intra prediction functions.

## Examples

Bad:

```cpp
const int need_left = extend_modes[mode] & NEED_LEFT;
const int need_above = extend_modes[mode] & NEED_ABOVE;
...
if (extend_modes[mode] & NEED_ABOVELEFT) {
  // Use above-left predictor
}
```

Good:

```cpp
const int need_left = extend_modes[mode] & NEED_LEFT;
const int need_above = extend_modes[mode] & NEED_ABOVE;
const int need_above_left = extend_modes[mode] & NEED_ABOVELEFT;
...
if (need_above_left) {
  // Use above-left predictor
}
```
