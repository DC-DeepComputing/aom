---
name: libaom/av1/avoid-state-dependent-early-exit-in-shared-helpers
description: When writing shared common helpers reused across encoder search and decoding, avoid embedding state-dependent early-exit checks; move mode-specific guards to caller sites
rubric: design
severity: should_fix
---

**Rule.** Do not place state-dependent early-exit checks in shared common helper
functions if other callers (such as encoder search or RDO routines) need to
invoke the helper before that state is decided or finalized. Place mode- or
feature-gating early exits at the caller sites that require them instead of
temporarily mutating state to bypass the helper.

**Why.** Embedding state-dependent early-exit guards in shared common helpers
forces callers executing prior to state selection (such as encoder search or
parameter optimization passes) to artificially mutate struct state to bypass the
bailout. Moving early-exit checks to specific caller sites preserves helper
reusability and prevents fragile state-mutation workarounds.

## Examples

Bad:

```cpp
// In shared common helper av1_loop_restoration_corners_in_sb():
if (cm->rst_info[plane].frame_restoration_type == RESTORE_NONE) return 0;

// In encoder search routine before frame_restoration_type is chosen:
cm->rst_info[plane].frame_restoration_type = RESTORE_SWITCHABLE;  // Hack to bypass helper's early exit
int has_lr = av1_loop_restoration_corners_in_sb(cm, plane, ...);
```

Good:

```cpp
// In callers requiring the early exit (decodeframe.c, bitstream.c):
if (cm->rst_info[plane].frame_restoration_type == RESTORE_NONE) continue;
if (av1_loop_restoration_corners_in_sb(cm, plane, ...)) { ... }

// In encoder search routine:
int has_lr = av1_loop_restoration_corners_in_sb(cm, plane, ...);
```
