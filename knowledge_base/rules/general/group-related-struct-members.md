---
name: libaom/group-related-struct-members
description: When declaring or adding struct members, place fields adjacent to logically related members or shared-prefix groups rather than appending or scattering them
rubric: readability
severity: should_fix
---

**Rule.** When defining structs or adding new members, place fields adjacent to
logically related existing members or within their corresponding functional
subgroup (such as members sharing feature prefixes, dimension tracking, or rate
control parameters). Do not scatter related fields across the struct or append
new fields to the end of the struct definition.

**Why.** Structs in codec codebases (such as MACROBLOCK, AV1_COMP, and
AV1_COMMON) often manage extensive state. Grouping related members contiguously
and clustering fields with shared prefixes keeps data structures coherent,
prevents duplicate tracking variables, and makes struct definitions easier to
navigate and maintain.

## Examples

Bad:

```cpp
typedef struct macroblock {
  int sb_me_partition;
  int sb_col_scroll;
  int sb_row_scroll;
  int sb_me_block;
  int_mv sb_me_mv;
} MACROBLOCK;

typedef struct AV1_COMP {
  // Dimension and sizing tracking
  InitialDimensions initial_dimensions;
  int initial_mbs;
  ResizePendingParams resize_pending_params;
  // Frame status and rate control
  int scaled_last_source_available;
  int last_coded_width;
  int last_coded_height;
} AV1_COMP;
```

Good:

```cpp
typedef struct macroblock {
  int sb_me_partition;
  int sb_me_block;
  int_mv sb_me_mv;
  int sb_col_scroll;
  int sb_row_scroll;
} MACROBLOCK;

typedef struct AV1_COMP {
  // Dimension and sizing tracking
  InitialDimensions initial_dimensions;
  int initial_mbs;
  int last_coded_width;
  int last_coded_height;
  ResizePendingParams resize_pending_params;
  // Frame status and rate control
  int scaled_last_source_available;
} AV1_COMP;
```
