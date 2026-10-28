---
name: libaom/av1/use-loop-for-reference-frame-bitmasks
description: When mapping reference frame bitmasks from ref_frame_flags, iterate from LAST_FRAME to ALTREF_FRAME in a loop using (1 << (ref_frame - 1)) instead of unrolling assignments
rubric: readability
severity: nit
---

**Rule.** When initializing, mapping, or checking reference frame masks against
ref_frame_flags (such as cpi->ref_frame_flags), iterate over reference frames in
a loop from LAST_FRAME to ALTREF_FRAME and derive each frame's bitmask using (1
<< (ref_frame - 1)) or av1_ref_frame_flag_list[ref_frame]. Do not write out
repetitive unrolled assignments for each individual AOM_*_FLAG macro.

**Why.** In libaom, reference frame enum values from LAST_FRAME (1) through
ALTREF_FRAME (7) map sequentially to the bitflags AOM_LAST_FLAG through
AOM_ALT_FLAG (1 << 0 to 1 << 6). Looping over the enum range reduces
boilerplate, prevents copy-paste omissions, and keeps reference mask derivation
consistent across encoder routines.

## Examples

Bad:

```cpp
search_state->use_ref_frame_mask[LAST_FRAME] =
    cpi->ref_frame_flags & AOM_LAST_FLAG;
search_state->use_ref_frame_mask[LAST2_FRAME] =
    cpi->ref_frame_flags & AOM_LAST2_FLAG;
search_state->use_ref_frame_mask[LAST3_FRAME] =
    cpi->ref_frame_flags & AOM_LAST3_FLAG;
search_state->use_ref_frame_mask[GOLDEN_FRAME] =
    cpi->ref_frame_flags & AOM_GOLD_FLAG;
search_state->use_ref_frame_mask[BWDREF_FRAME] =
    cpi->ref_frame_flags & AOM_BWD_FLAG;
search_state->use_ref_frame_mask[ALTREF2_FRAME] =
    cpi->ref_frame_flags & AOM_ALT2_FLAG;
search_state->use_ref_frame_mask[ALTREF_FRAME] =
    cpi->ref_frame_flags & AOM_ALT_FLAG;
```

Good:

```cpp
for (MV_REFERENCE_FRAME ref_frame_iter = LAST_FRAME;
     ref_frame_iter <= ALTREF_FRAME; ++ref_frame_iter) {
  search_state->use_ref_frame_mask[ref_frame_iter] =
      cpi->ref_frame_flags & (1 << (ref_frame_iter - 1));
}
```
