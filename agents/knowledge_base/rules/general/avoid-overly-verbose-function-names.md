---
name: libaom/avoid-overly-verbose-function-names
description: When declaring or naming helper functions, avoid excessively verbose compound identifiers; use concise names referencing established domain abbreviations and mode categories
rubric: readability
severity: nit
---

**Rule.** Do not use excessively verbose compound function names that spell out
every parameter or condition in full. Use concise identifiers that leverage
established domain abbreviations (such as orip for orientation intra prediction,
bsize for block size, or ctx for context) and concise category descriptors to
keep signatures readable and prevent unnecessary line wrapping.

**Why.** Excessively long, sprawling identifier names clutter call sites, harm
readability, and cause awkward multi-line parameter wrapping without adding
clarity over standard domain abbreviations.

## Examples

Bad:

```cpp
static INLINE int av1_signal_intra_pred_filter_for_dir_mode_bsize(
    const AV1_COMMON *cm, const MB_MODE_INFO *mbmi, PLANE_TYPE plane,
    BLOCK_SIZE bsize);
```

Good:

```cpp
static INLINE int av1_signal_orip_for_horver_modes(
    const AV1_COMMON *cm, const MB_MODE_INFO *mbmi, PLANE_TYPE plane,
    BLOCK_SIZE bsize);
```
