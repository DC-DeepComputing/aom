---
name: libaom/av1/separate-speed-features-by-video-orientation
description: When configuring encoder speed features for different video orientations or categories, structure assignments into distinct top-level conditional blocks rather than intertwining compound conditions
rubric: design
severity: nit
---

**Rule.** When assigning encoder speed features across multiple video
orientations or categories (such as vertical videos versus horizontal/standard
videos), structure the assignments into distinct top-level conditional blocks
for each category. Do not intertwine shared speed features and category-specific
overrides using compound || or nested else-if conditions across different
categories.

**Why.** Intertwining compound boolean conditions and scattered overrides across
video orientations makes resolution- and speed-dependent heuristic tuning
difficult to read, benchmark, and maintain. Grouping speed feature assignments
into clean, dedicated conditional blocks per orientation isolates
category-specific presets and allows independent tuning without unintended side
effects.

## Examples

Bad:

```cpp
const bool is_vert_midres = is_vertical_video && is_between_480p_and_720p;
const bool is_horz_hdres = !is_vertical_video && is_720p_or_larger;

if (is_vert_midres || is_horz_hdres) {
  sf->lpf_sf.skip_loop_filter_using_filt_error = 1;
  sf->part_sf.split_partition_penalty_level = 1;
}

if (is_vert_midres) {
  sf->hl_sf.ref_frame_mvs_lvl = 2;
} else if (is_horz_hdres) {
  sf->hl_sf.ref_frame_mvs_lvl = 1;
}
```

Good:

```cpp
// Speed features for vertical videos.
if (is_vertical_video && is_between_480p_and_720p) {
  sf->hl_sf.ref_frame_mvs_lvl = 2;
  sf->lpf_sf.skip_loop_filter_using_filt_error = 1;
  sf->part_sf.split_partition_penalty_level = 1;
}

// Speed features for regular (horizontal) videos.
if (!is_vertical_video && is_720p_or_larger) {
  sf->hl_sf.ref_frame_mvs_lvl = 1;
  sf->lpf_sf.skip_loop_filter_using_filt_error = 1;
  sf->part_sf.split_partition_penalty_level = 1;
}
```
