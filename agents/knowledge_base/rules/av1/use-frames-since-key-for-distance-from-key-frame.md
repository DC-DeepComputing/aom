---
name: libaom/av1/use-frames-since-key-for-distance-from-key-frame
description: When evaluating frame distance from the preceding key frame in encoder logic, use frames_since_key rather than frames_to_key which counts down to the next key frame
rubric: correctness
severity: should_fix
---

**Rule.** When checking elapsed distance from the preceding key frame or
detecting non-key frames, inspect rc->frames_since_key (or frame type helpers
like frame_is_intra_only()), not rc->frames_to_key. Do not use frames_to_key > 1
as a non-key frame check, as frames_to_key counts remaining frames until the
next key frame and is at its maximum at key frames.

**Why.** In RATE_CONTROL, frames_to_key represents the count of frames remaining
until the upcoming key frame (equal to the GOP interval at the start of a GOP),
whereas frames_since_key tracks the count of frames elapsed since the previous
key frame. Confusing the two causes keyframe proximity checks to evaluate to
true on key frames and false immediately before the next key frame, inverting
the intended logic.

## Examples

Bad:

```cpp
// Incorrectly assuming frames_to_key measures distance from the previous key frame:
sf->intra_sf.disable_smooth_intra =
    !frame_is_intra_only(&cpi->common) || (cpi->rc.frames_to_key > 1);
```

Good:

```cpp
// Use frames_since_key or explicit frame type checks:
sf->intra_sf.disable_smooth_intra =
    !frame_is_intra_only(&cpi->common) || (cpi->rc.frames_since_key > 1);
```
