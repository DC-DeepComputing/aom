---
name: libaom/avoid-duplicate-macros-in-preprocessor-conditionals
description: When writing compound preprocessor conditionals with logical operators, verify each operand checks a distinct feature macro rather than duplicating identifiers from copy-paste errors
rubric: correctness
severity: blocking
---

**Rule.** When writing compound preprocessor #if or #elif directives with
logical operators (|| or &&), verify that each operand tests a distinct intended
macro or configuration flag without duplicate identifier names. Check that
copy-pasted conditional clauses are updated to reference their intended target
flags (such as CONFIG_*, HAVE_*, or ARCH_*).

**Why.** Duplicating macro operands in compound conditionals (such as #if
CONFIG_A || CONFIG_A instead of #if CONFIG_A || CONFIG_B) creates dead logic or
prevents guarded code from compiling and executing under valid build
configurations where the omitted flag is active.

## Examples

Bad:

```cpp
// Bug: CONFIG_EXT_RECUR_PARTITIONS is duplicated instead of checking CONFIG_SDP
#if CONFIG_EXT_RECUR_PARTITIONS || CONFIG_EXT_RECUR_PARTITIONS
  BLOCK_SIZE plane_bsize =
      get_mb_plane_block_size(xd, xd->mi[0], plane, ssx, ssy);
#else
  BLOCK_SIZE plane_bsize = get_plane_block_size(xd->mi[0]->sb_type, plane_pd);
#endif
```

Good:

```cpp
// Correct: tests both distinct configuration flags
#if CONFIG_EXT_RECUR_PARTITIONS || CONFIG_SDP
  BLOCK_SIZE plane_bsize =
      get_mb_plane_block_size(xd, xd->mi[0], plane, ssx, ssy);
#else
  BLOCK_SIZE plane_bsize = get_plane_block_size(xd->mi[0]->sb_type, plane_pd);
#endif
```
