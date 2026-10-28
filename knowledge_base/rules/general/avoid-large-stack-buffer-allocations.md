---
name: libaom/avoid-large-stack-buffer-allocations
description: When allocating large structures or scratch buffers (~16KB+), allocate dynamically on the heap or persistent context structures checked with CHECK_MEM_ERROR rather than on the stack
rubric: safety
severity: should_fix
---

**Rule.** Do not allocate large structures, arrays, or scratch buffers (e.g.,
~16KB or larger, or full-block/frame scratch arrays) as local variables on the
stack. Allocate large buffers and structures dynamically on the heap (using
aom_memalign(), aom_malloc(), or aom_calloc()) and verify allocation success
with CHECK_MEM_ERROR or AOM_CHECK_MEM_ERROR. Additionally, store dynamic
pointers in persistent context structures (such as ThreadData, MACROBLOCK, or
AV1_COMP) so top-level destruction routines can reliably free them if an early
exit or error recovery path (e.g., via aom_internal_error()) occurs.

**Why.** Worker thread stack space in libaom is finite (often as small as 64 KB
in multi-threaded worker pools), and large stack frames trigger compiler
warnings (such as MSVC C6262 or -Wstack-usage) and risk stack overflow crashes.
Furthermore, allocating local dynamic heap memory without tracking pointers in
persistent context or thread structures risks memory leaks when functions exit
early via longjmp-based error recovery (aom_internal_error).

## Examples

Bad:

```cpp
// Variant 1: Allocating large scratch array on stack (risks stack overflow)
void calc_wiener_var(...) {
  DECLARE_ALIGNED(32, uint8_t, pred_buffer[256 * 256]);
  process_block(..., pred_buffer);
}

// Variant 2: Large (~19KB) structure on stack triggers C6262 / -Wstack-usage
void backup_block(...) {
  SB_FIRST_PASS_STATS sb_org_stats;
  av1_backup_sb_state(&sb_org_stats, cpi, td, tile_data, mi_row, mi_col);
}
```

Good:

```cpp
// Variant 1: Allocate dynamically in worker thread context structure
CHECK_MEM_ERROR(
    cm, td->wiener_tmp_pred_buf,
    aom_memalign(32, buf_width * buf_height * sizeof(*td->wiener_tmp_pred_buf)));
calc_wiener_var(..., td->wiener_tmp_pred_buf);

// Variant 2: Allocate on heap and track in MACROBLOCK/ThreadData for safe cleanup
AOM_CHECK_MEM_ERROR(
    x->e_mbd.error_info, td->mb.sb_org_stats,
    (SB_FIRST_PASS_STATS *)aom_malloc(sizeof(*td->mb.sb_org_stats)));
av1_backup_sb_state(td->mb.sb_org_stats, cpi, td, tile_data, mi_row, mi_col);
```
