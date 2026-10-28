---
name: libaom/av1/reuse-working-buffers-across-frames
description: When managing frame- or superblock-level working buffers in per-frame encode routines, allocate once and reuse existing buffers across frames rather than repeatedly freeing and reallocating them
rubric: safety
severity: should_fix
---

**Rule.** Do not repeatedly free and reallocate frame- or superblock-level
working buffers inside per-frame encode routines when dimensions or
configurations remain unchanged. Allocate working buffers once on persistent
context structures (such as AV1_COMP or ext_ratectrl) and reuse them across
frames, reallocating only when dimensions or capacities change.

**Why.** Repeatedly invoking dynamic memory allocation routines (such as
aom_free and aom_calloc/aom_malloc) inside per-frame encode loops incurs
unnecessary CPU overhead and causes heap fragmentation. Reusing persistent
working buffers across frames keeps allocations off the critical path.

## Examples

Bad:

```cpp
// In per-frame encode routine:
aom_free(cpi->ext_ratectrl.sb_params_list);
CHECK_MEM_ERROR(
    cm, cpi->ext_ratectrl.sb_params_list,
    (aom_sb_params *)aom_calloc(
        sb_rows * sb_cols, sizeof(*cpi->ext_ratectrl.sb_params_list)));
```

Good:

```cpp
// In per-frame encode routine:
if (cpi->ext_ratectrl.sb_params_list == NULL) {
  CHECK_MEM_ERROR(
      cm, cpi->ext_ratectrl.sb_params_list,
      (aom_sb_params *)aom_calloc(
          sb_rows * sb_cols, sizeof(*cpi->ext_ratectrl.sb_params_list)));
}
```
