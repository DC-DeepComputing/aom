---
name: libaom/initialize-sentinel-fields-at-setup
description: When struct fields use non-zero sentinel values like -1 for invalid or unset state, initialize them in struct setup functions rather than relying on runtime resets
rubric: correctness
severity: should_fix
---

**Rule.** Explicitly initialize struct and buffer fields that use non-zero
sentinel values (such as -1 for invalid indices, display orders, or unset
coordinates) in dedicated setup or allocation routines. Do not rely solely on
downstream runtime execution paths or conditional resets (such as keyframe or
scene-change handlers) to set sentinel states before first use.

**Why.** Default memory allocation (such as aom_calloc or memset(0)) zeroes all
fields. If zero is a valid index or frame identifier, an uninitialized non-zero
sentinel field will hold 0 until a runtime reset path executes, leading to false
matches and logic bugs on early frames or unvisited branches.

## Examples

Bad:

```cpp
// Only resetting sentinel on a conditional runtime path:
if (rc->frames_to_key <= 0) {
  cpi->ppi->tpl_data.prev_gop_arf_disp_order = -1;
}
```

Good:

```cpp
// In av1_setup_tpl_buffers() or struct allocation/init:
tpl_data->prev_gop_arf_disp_order = -1;

// And update or reset at runtime when conditions trigger:
if (rc->frames_to_key <= 0) {
  cpi->ppi->tpl_data.prev_gop_arf_disp_order = -1;
}
```
