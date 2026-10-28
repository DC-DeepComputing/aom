---
name: libaom/avoid-dangling-stack-pointers-in-persistent-structs
description: When storing stack variable addresses in persistent structs like MACROBLOCKD (e.g. xd->mi or scale factors), scope properly and reset to NULL or restore at exit
rubric: safety
severity: should_fix
---

**Rule.** When temporarily pointing members of persistent, shared, or
caller-owned structs (such as `MACROBLOCKD *xd`, `AV1_COMP *cpi`,
`CommonModeInfoParams`, or `TileDataEnc`) to stack-allocated variables (such as
`MB_MODE_INFO mbmi` assigned to `xd->mi` or `struct scale_factors` assigned to
`xd->block_ref_scale_factors`), scope the stack variables at function level to
span all usages, and explicitly reset the pointer to `NULL` (e.g. `xd->mi =
NULL;`) or restore persistent state (e.g. via `set_ref_ptrs()`) across all exit
paths before returning.

**Why.** Stack-allocated variables are invalidated when their enclosing block or
function returns. Leaving persistent or caller-owned structs pointing to popped
stack frames results in dangling pointers, causing undefined behavior, memory
corruption, and use-after-free crashes in subsequent encoder/decoder passes.
Declaring temporary stack variables at function scope prevents premature
lifetime expiration during loop iterations, while resetting pointers to NULL or
restoring state at function exit ensures memory safety.

## Examples

Bad:

```cpp
// Variant 1: Assigning local stack MB_MODE_INFO to xd->mi without resetting to NULL
void av1_calc_mb_wiener_var_row(AV1_COMP *const cpi, MACROBLOCKD *xd, ...) {
  MB_MODE_INFO mbmi;
  memset(&mbmi, 0, sizeof(mbmi));
  MB_MODE_INFO *mbmi_ptr = &mbmi;
  xd->mi = &mbmi_ptr;
  // ... processing using xd ...
  // Function returns without resetting xd->mi, leaving a dangling stack pointer.
}

// Variant 2: Block-scoped stack variable and missing restoration before return
for (int i = 0; i < num_refs; ++i) {
  struct scale_factors sf_tmp;
  if (search_state.use_scaled_ref_frame[ref_frame]) {
    av1_setup_scale_factors_for_frame(&sf_tmp, ...);
    xd->block_ref_scale_factors[0] = &sf_tmp;
  }
}
// Returns without restoring persistent scale factors via set_ref_ptrs(...)
```

Good:

```cpp
// Variant 1: Explicitly resetting xd->mi to NULL before function exit
void av1_calc_mb_wiener_var_row(AV1_COMP *const cpi, MACROBLOCKD *xd, ...) {
  MB_MODE_INFO mbmi;
  memset(&mbmi, 0, sizeof(mbmi));
  MB_MODE_INFO *mbmi_ptr = &mbmi;
  xd->mi = &mbmi_ptr;
  // ... processing using xd ...
  xd->mi = NULL;  // Reset pointer before return since mbmi is stack-allocated.
}

// Variant 2: Function-scoped stack variable and restoring state at exit
struct scale_factors sf_no_scale;
av1_setup_scale_factors_for_frame(&sf_no_scale, cm->width, cm->height,
                                  cm->width, cm->height);
if (search_state.use_scaled_ref_frame[ref_frame]) {
  xd->block_ref_scale_factors[0] = &sf_no_scale;
}
// ... processing ...
// Restore persistent pointers at function exit to prevent dangling references
set_ref_ptrs(cm, xd, mi->ref_frame[0], mi->ref_frame[1]);
```
