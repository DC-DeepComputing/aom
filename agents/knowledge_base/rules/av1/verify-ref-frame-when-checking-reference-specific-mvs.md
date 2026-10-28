---
name: libaom/av1/verify-ref-frame-when-checking-reference-specific-mvs
description: When checking if a reference-specific candidate motion vector has been evaluated in inter mode search, verify that ref_frame matches in addition to MV coordinates
rubric: correctness
severity: should_fix
---

**Rule.** When checking whether a reference-specific candidate motion vector
(such as a superblock motion estimation vector computed against LAST_FRAME) has
already been tested or evaluated during inter mode search, verify that the
current ref_frame matches the candidate's reference frame in addition to
checking that the motion vector coordinates match.

**Why.** Motion vectors are defined relative to a specific reference frame.
Comparing only motion vector coordinates without checking ref_frame causes
false-positive matches against searches for other reference frames that happen
to produce identical displacement values, incorrectly triggering early
termination or skipping required mode evaluations.

## Examples

Bad:

```cpp
if (x->sb_me_block &&
    search_state->frame_mv[this_best_mode][ref_frame].as_mv.row ==
        x->sb_me_mv_row &&
    search_state->frame_mv[this_best_mode][ref_frame].as_mv.col ==
        x->sb_me_mv_col)
  *sb_me_is_tested = 1;
```

Good:

```cpp
if (x->sb_me_block && ref_frame == LAST_FRAME &&
    search_state->frame_mv[this_best_mode][ref_frame].as_int ==
        x->sb_me_mv.as_int)
  *sb_me_has_been_tested = 1;
```
