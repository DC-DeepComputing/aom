---
name: libaom/avoid-speculative-generic-helper-names
description: When defining single-purpose helper functions, name them specifically for their exact operation rather than using speculatively generic names, and place feature condition guards at call sites
rubric: readability
severity: should_fix
---

**Rule.** Name helper functions specifically for the concrete operation or
subsystem they handle (e.g., init_encode_nonrd_sb_deltaq or setup_delta_q_nonrd)
rather than using overly broad or speculative names (e.g., init_encode_nonrd_sb)
in anticipation of hypothetical future logic. When a helper executes only under
a specific feature flag or condition, place the condition check at the call site
rather than wrapping the single conditional task inside a generically named
helper.

**Why.** Speculatively named helper functions mislead readers into assuming
broader initialization or multi-step processing is occurring. Naming functions
for what they actually execute clarifies program flow, and checking feature
conditions at call sites avoids unnecessary function call overhead and empty
wrapper indirection when features are disabled.

## Examples

Bad:

```cpp
// Generic helper name for a single feature setup, with condition hidden inside
static inline void init_encode_nonrd_sb(AV1_COMP *cpi, ThreadData *td,
                                        const TileDataEnc *tile_data,
                                        int mi_row, int mi_col) {
  if (cm->delta_q_info.delta_q_present_flag) {
    setup_delta_q_nonrd(cpi, td, x, tile_info, mi_row, mi_col, num_planes);
  }
}

// Call site obscures that only delta-Q is being initialized
init_encode_nonrd_sb(cpi, td, tile_data, mi_row, mi_col);
```

Good:

```cpp
// Accurately named for the specific operation it performs
static inline void init_encode_nonrd_sb_deltaq(AV1_COMP *cpi, ThreadData *td,
                                               const TileDataEnc *tile_data,
                                               int mi_row, int mi_col) {
  setup_delta_q_nonrd(cpi, td, x, tile_info, mi_row, mi_col, num_planes);
}

// Call site explicitly guarded by the relevant feature flag
if (cm->delta_q_info.delta_q_present_flag) {
  init_encode_nonrd_sb_deltaq(cpi, td, tile_data, mi_row, mi_col);
}
```
