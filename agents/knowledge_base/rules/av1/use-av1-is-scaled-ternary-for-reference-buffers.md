---
name: libaom/av1/use-av1-is-scaled-ternary-for-reference-buffers
description: When retrieving reference frame buffers in the AV1 encoder, use av1_is_scaled() in a ternary expression to select between av1_get_scaled_ref_frame() and get_ref_frame_yv12_buf()
rubric: readability
severity: nit
---

**Rule.** When resolving reference frame buffers in encoder routines, use
`av1_is_scaled(get_ref_scale_factors_const(cm, ref))` in a ternary expression to
select between `av1_get_scaled_ref_frame(cpi, ref)` and
`get_ref_frame_yv12_buf(cm, ref)`. Guard reference buffer resolution inside
`!frame_is_intra_only(cm)` blocks rather than speculatively retrieving scaled
buffers and falling back when null.

**Why.** Explicitly checking `av1_is_scaled()` makes the selection between
scaled and unscaled reference buffers clear and declarative via a ternary
operator, avoiding redundant speculative calls or NULL fallback logic. Confining
reference resolution inside non-intra guards prevents unnecessary reference
buffer lookups on intra-only frames.

## Examples

Bad:

```cpp
if (!frame_is_intra_only(cm)) {
  av1_scale_references(cpi, EIGHTTAP_REGULAR, 0, 0);
  last_frame = av1_get_scaled_ref_frame(cpi, LAST_FRAME);
}
if (!last_frame) {
  last_frame = get_ref_frame_yv12_buf(cm, LAST_FRAME);
}
```

Good:

```cpp
if (!frame_is_intra_only(cm)) {
  av1_scale_references(cpi, EIGHTTAP_REGULAR, 0, 0);
  last_frame = av1_is_scaled(get_ref_scale_factors_const(cm, LAST_FRAME))
                   ? av1_get_scaled_ref_frame(cpi, LAST_FRAME)
                   : get_ref_frame_yv12_buf(cm, LAST_FRAME);
}
```
