---
name: libaom/av1/check-av1-is-valid-scale-after-setup
description: When verifying reference frame dimensions after calling av1_setup_scale_factors_for_frame, inspect the initialized struct with av1_is_valid_scale rather than re-evaluating valid_ref_frame_size
rubric: readability
severity: should_fix
---

**Rule.** After setting up frame scale factors with
av1_setup_scale_factors_for_frame(), check scale validity by inspecting the
initialized scale_factors struct with av1_is_valid_scale(sf). Do not separately
re-evaluate valid_ref_frame_size() on the input dimensions at the call site.

**Why.** av1_setup_scale_factors_for_frame() already invokes
valid_ref_frame_size() internally and marks invalid dimensions as
REF_INVALID_SCALE. Calling valid_ref_frame_size() again at the call site is
redundant and duplicates frame dimension checks.

## Examples

Bad:

```cpp
av1_setup_scale_factors_for_frame(sf, buf->buf.y_crop_width,
                                  buf->buf.y_crop_height, cm->width,
                                  cm->height);
has_valid_ref_frame |= valid_ref_frame_size(
    buf->buf.y_crop_width, buf->buf.y_crop_height, cm->width, cm->height);
```

Good:

```cpp
av1_setup_scale_factors_for_frame(sf, buf->buf.y_crop_width,
                                  buf->buf.y_crop_height, cm->width,
                                  cm->height);
has_valid_ref_frame |= av1_is_valid_scale(sf);
```
