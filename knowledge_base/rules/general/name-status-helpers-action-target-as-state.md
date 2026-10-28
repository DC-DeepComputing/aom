---
name: libaom/name-status-helpers-action-target-as-state
description: When naming state-transition or status-marking helper functions that update tracking flags, use explicit <action>_<target>_as_<state> phrasing (e.g., mark_block_as_coded) instead of ambiguous <action>_<state>_<target> phrasing (e.g., mark_c
rubric: readability
severity: should_fix
---

**Rule.** Name helper functions that update status flags, tracking states, or
entity transitions using explicit '<action>_<target>_as_<state>' phrasing (e.g.,
mark_block_as_coded, mark_block_as_not_coded, set_unit_as_processed) rather than
ambiguous '<action>_<state>_<target>' or '<action>_<target>' phrasing (such as
mark_coded_block or reset_coded_block). Ensure helper names clearly distinguish
modifying an entity's status flag from operating on an already-classified entity
or resetting underlying block/entity data.

**Why.** Phrasing like mark_coded_block or reset_coded_block is grammatically
ambiguous because it can be misinterpreted as executing an algorithm on a block
that is already coded or resetting block buffer data. Using
mark_<target>_as_<state> or set_<target>_as_<state> explicitly conveys that the
function transitions a status tracking flag on the target entity.

## Examples

Bad:

```cpp
// Ambiguous: sounds like operating on already-coded blocks or resetting block data
void av1_mark_coded_block(MACROBLOCKD *xd, int mi_row, int mi_col,
                          BLOCK_SIZE bsize, BLOCK_SIZE sb_size);
void av1_reset_coded_block(MACROBLOCKD *xd, int mi_row, int mi_col,
                           BLOCK_SIZE bsize, BLOCK_SIZE sb_size);
```

Good:

```cpp
// Clear: explicitly conveys updating status flags on the target block
void av1_mark_block_as_coded(MACROBLOCKD *xd, int mi_row, int mi_col,
                             BLOCK_SIZE bsize, BLOCK_SIZE sb_size);
void av1_mark_block_as_not_coded(MACROBLOCKD *xd, int mi_row, int mi_col,
                                 BLOCK_SIZE bsize, BLOCK_SIZE sb_size);
```
