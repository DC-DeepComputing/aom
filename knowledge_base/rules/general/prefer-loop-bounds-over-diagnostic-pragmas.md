---
name: libaom/prefer-loop-bounds-over-diagnostic-pragmas
description: When suppressing compiler array-bounds or overflow warnings in bounded loops, add explicit upper-bound conditions instead of diagnostic pragmas
rubric: readability
severity: should_fix
---

**Rule.** When suppressing false-positive compiler array-bounds or
buffer-overflow warnings (such as -Wstringop-overflow or -Warray-bounds) in
loops operating on fixed-size arrays, add explicit upper-bound checks (e.g., i <
MAX_TXFM_STAGE_NUM) directly to the loop condition with an explanatory comment
rather than disabling compiler diagnostics with #pragma GCC diagnostic ignored.

**Why.** Explicit loop bound checks avoid compiler-specific diagnostic pragmas,
keep warning suppression consistent across modules, and ensure bounds invariants
remain visible to both compilers and static analyzers.

## Examples

Bad:

```cpp
#if defined(__GNUC__) && __GNUC__ >= 7
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
for (int i = 0; i < stage_num_col; ++i) {
  cfg->stage_range_col[i] = (range_mult2_col[i] + 1) >> 1;
}
#if defined(__GNUC__) && __GNUC__ >= 7
#pragma GCC diagnostic pop
#endif
```

Good:

```cpp
// i < MAX_TXFM_STAGE_NUM will quiet -Wstringop-overflow.
for (int i = 0; i < stage_num_col && i < MAX_TXFM_STAGE_NUM; ++i) {
  cfg->stage_range_col[i] = (range_mult2_col[i] + 1) >> 1;
}
```
