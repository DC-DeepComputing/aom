---
name: libaom/avoid-incidental-diff-churn-in-unrelated-code
description: When preparing code changes, keep diffs minimal and focused on the intended scope by avoiding incidental whitespace edits, function relocations, and stale-base regressions
rubric: change_scope
severity: should_fix
---

**Rule.** Avoid incidental or out-of-scope edits—such as modifying whitespace in
untouched functions, relocating existing function definitions solely to avoid
forward declarations, or including unrelated hunks from an outdated base branch.
Keep changes minimal and strictly scoped to the intended fix or feature,
preferring forward declarations over function moves and rebasing on upstream to
avoid accidental regressions.

**Why.** Incidental modifications, large-scale function relocations, and
stale-branch diff hunks create review noise, obscure the intended functional
change, pollute git blame history, and risk inadvertently reverting upstream
improvements.

## Examples

Bad:

```cpp
// Deleting existing blank lines in untouched functions:
TileDataEnc *const this_tile = &cpi->tile_data[tile_index];
row_mt_sync_mem_dealloc(&this_tile->row_mt_sync);

// Moving existing function definitions across a file to avoid forward declarations:
static AOM_INLINE void restore_workers_after_fpmt(...) { ... }
static AOM_INLINE void prepare_fpmt_workers(...) {
  restore_workers_after_fpmt(...);
}

// Accidental revert of upstream struct assignment caused by outdated base branch:
memcpy(td->mb.e_mbd.tile_ctx, &this_tile->tctx, sizeof(FRAME_CONTEXT));
```

Good:

```cpp
// Preserving existing formatting to avoid unnecessary diff churn:
TileDataEnc *const this_tile = &cpi->tile_data[tile_index];

row_mt_sync_mem_dealloc(&this_tile->row_mt_sync);

// Keeping functions in place and adding a forward declaration:
static AOM_INLINE void restore_workers_after_fpmt(
    AV1_PRIMARY *ppi, int parallel_frame_count, int num_fpmt_workers_prepared);

static AOM_INLINE void prepare_fpmt_workers(...) {
  restore_workers_after_fpmt(...);
}

// Rebased change preserving upstream updates:
*td->mb.e_mbd.tile_ctx = this_tile->tctx;
```
