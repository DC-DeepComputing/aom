---
name: libaom/av1/null-check-reference-frame-buffers
description: When retrieving reference frame buffers using get_ref_frame_yv12_buf, get_ref_frame_buf, or related lookups, verify returned pointers are non-null before accessing struct members
rubric: safety
severity: blocking
---

**Rule.** When retrieving reference frame buffers using
get_ref_frame_yv12_buf(), get_ref_frame_buf(), get_primary_ref_frame_buf(), or
av1_get_scaled_ref_frame(), verify that the returned pointer is non-null before
dereferencing or accessing buffer fields (such as dimensions or crop
properties).

**Why.** get_ref_frame_yv12_buf() and related lookups query cm->ref_frame_map
and return NULL when a reference frame (such as golden or altref) is unmapped,
unpopulated, or disabled. Accessing struct members without a null check causes
null pointer dereferences and crashes.

## Examples

Bad:

```cpp
const YV12_BUFFER_CONFIG *yv12_alt = get_ref_frame_yv12_buf(cm, ALTREF_FRAME);
if (yv12_alt->y_crop_height != cm->height ||
    yv12_alt->y_crop_width != cm->width) {
  yv12_alt = av1_get_scaled_ref_frame(cpi, ALTREF_FRAME);
  scaled_ref_alt = true;
}
```

Good:

```cpp
const YV12_BUFFER_CONFIG *yv12_alt = get_ref_frame_yv12_buf(cm, ALTREF_FRAME);
if (yv12_alt && (yv12_alt->y_crop_height != cm->height ||
                 yv12_alt->y_crop_width != cm->width)) {
  yv12_alt = av1_get_scaled_ref_frame(cpi, ALTREF_FRAME);
  scaled_ref_alt = true;
}
```
