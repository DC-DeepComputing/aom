---
name: libaom/av1/drop-overlay-frame-when-altref-dropped
description: When encoding OVERLAY_UPDATE or INTNL_OVERLAY_UPDATE frames whose paired alt-ref frame was dropped, mark the overlay frame as dropped via cpi->is_dropped_frame
rubric: correctness
severity: should_fix
---

**Rule.** When preparing or encoding an overlay frame (OVERLAY_UPDATE or
INTNL_OVERLAY_UPDATE) whose corresponding earlier alt-ref frame (ARF_UPDATE or
INTNL_ARF_UPDATE) was dropped by rate control (gf_group->is_frame_dropped[...]),
explicitly mark the overlay frame as dropped (cpi->is_dropped_frame = true) and
reset show_existing_frame to 0. Do not allow the overlay frame to fall back to
being encoded as a regular non-dropped frame when its reference was never
encoded.

**Why.** Overlay frames depend on the reconstructed reference from their
corresponding alt-ref frame. If rate control dropped the alt-ref frame, encoding
the overlay frame normally will reference missing or uninitialized frame
buffers, corrupting rate control budgets and visual output.

## Examples

Bad:

```cpp
// When the alt-ref frame was dropped, only reset show_existing_frame.
if (gf_group->is_frame_dropped[idx]) {
  frame_params.show_existing_frame = 0;
}
```

Good:

```cpp
// When the alt-ref frame was dropped, reset show_existing_frame and drop the overlay frame.
if (gf_group->is_frame_dropped[idx]) {
  frame_params.show_existing_frame = 0;
  assert(!cpi->is_dropped_frame);
  cpi->is_dropped_frame = true;
}
```
