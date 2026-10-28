---
name: libaom/av1/save-and-restore-coding-context-for-speculative-encodes
description: When implementing speculative, trial, or state-freezing frame encodes, save and restore encoder state with av1_save_all_coding_context() and restore_all_coding_context() instead of solely bypassing post-encode updates
rubric: correctness
severity: should_fix
---

**Rule.** When implementing speculative, trial, or state-freezing frame encodes
that must leave encoder state unmodified, save and restore encoder state using
av1_save_all_coding_context() and restore_all_coding_context(). Do not rely
solely on skipping av1_post_encode_updates(), and ensure all frame counters and
sequence indices (such as frame_number or frame_index_set) are preserved in
CODING_CONTEXT.

**Why.** Encoding passes mutate internal encoder state across rate control, loop
filtering, segmentation, and frame counters. Skipping post-encode updates alone
fails to revert these in-pass mutations, causing state corruption and bitstream
inconsistencies on subsequent frames.

## Examples

Bad:

```cpp
// Relying solely on skipping post-encode updates leaves internal frame counters and context mutated
if (!(flags & AOM_EFLAG_FREEZE_INTERNAL_STATE)) {
  av1_post_encode_updates(cpi, &cpi_data);
}
```

Good:

```cpp
if (cpi->ppi->b_freeze_internal_state) {
  av1_save_all_coding_context(cpi);
}
...
av1_post_encode_updates(cpi, &cpi_data);
if (cpi->ppi->b_freeze_internal_state) {
  restore_all_coding_context(cpi);
}
```
