---
name: libaom/av1/gate-orientation-dependent-speed-features
description: When configuring resolution-dependent encoder speed features tuned for horizontal or vertical datasets, check video orientation (such as !is_vertical_video) alongside min-dimension thresholds
rubric: correctness
severity: should_fix
---

**Rule.** When configuring resolution-dependent encoder speed features or
heuristics tuned specifically for landscape/horizontal or portrait/vertical
video datasets, explicitly check video orientation (such as !is_vertical_video
or is_vertical_video) alongside minimum-dimension resolution thresholds (such as
is_720p_or_larger). Do not rely solely on AOMMIN(cm->width, cm->height)
thresholds when heuristics are not orientation-agnostic.

**Why.** Resolution checks based on AOMMIN(cm->width, cm->height) match both
landscape (1280x720) and vertical/portrait (720x1280) orientations equally.
Omitting orientation checks causes speed feature heuristics tuned specifically
on horizontal HD datasets to unintentionally activate on vertical videos (or
vice versa), degrading rate-distortion performance or speed trade-offs.

## Examples

Bad:

```cpp
// Bad: min-dimension resolution check matches both landscape and portrait video
if (is_720p_or_larger && speed <= 2 && leaf_and_overlay_frames) {
  sf->lpf_sf.min_lr_unit_size = RESTORATION_UNITSIZE_MAX >> 1;
  sf->lpf_sf.max_lr_unit_size = RESTORATION_UNITSIZE_MAX >> 1;
}
```

Good:

```cpp
// Good: explicitly guard with !is_vertical_video for horizontal-tuned heuristics
if (!is_vertical_video && is_720p_or_larger && speed <= 2 &&
    leaf_and_overlay_frames) {
  sf->lpf_sf.min_lr_unit_size = RESTORATION_UNITSIZE_MAX >> 1;
  sf->lpf_sf.max_lr_unit_size = RESTORATION_UNITSIZE_MAX >> 1;
}
```
