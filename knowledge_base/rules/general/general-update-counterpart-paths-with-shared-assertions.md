---
name: libaom/general-update-counterpart-paths-with-shared-assertions
description: When adding assertions, bounds checks, or validity guards to duplicated or parallel logic in one function, update all counterpart functions sharing the pattern consistently
rubric: correctness
severity: should_fix
---

**Rule.** When adding assertions, invariant checks, or error guards to
duplicated or parallel logic in one function, verify whether counterpart
functions or sibling branches perform the same operations. Update all related
code paths to include the corresponding invariant assertions and validity checks
consistently.

**Why.** Duplicated or cloned logic across sibling functions often shares the
same data structures and validity assumptions. Updating assertions or error
guards in only one location leaves counterpart paths vulnerable to silent
corruption, invalid state propagation, and divergent behavior.

## Examples

Bad:

```cpp
// In prepare_features_after_part_ab:
BLOCK_SIZE horz_4_bs = get_partition_subsize(bsize, PARTITION_HORZ_4);
BLOCK_SIZE vert_4_bs = get_partition_subsize(bsize, PARTITION_VERT_4);
assert(horz_4_bs != BLOCK_INVALID);
assert(vert_4_bs != BLOCK_INVALID);

// In counterpart function av1_ml_prune_4_partition (duplicated logic left without assertions):
BLOCK_SIZE horz_4_bs = get_partition_subsize(bsize, PARTITION_HORZ_4);
BLOCK_SIZE vert_4_bs = get_partition_subsize(bsize, PARTITION_VERT_4);
```

Good:

```cpp
// In both prepare_features_after_part_ab and counterpart function av1_ml_prune_4_partition:
BLOCK_SIZE horz_4_bs = get_partition_subsize(bsize, PARTITION_HORZ_4);
BLOCK_SIZE vert_4_bs = get_partition_subsize(bsize, PARTITION_VERT_4);
assert(horz_4_bs != BLOCK_INVALID);
assert(vert_4_bs != BLOCK_INVALID);
```
