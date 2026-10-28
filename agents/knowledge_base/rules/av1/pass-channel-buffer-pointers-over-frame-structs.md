---
name: libaom/av1/pass-channel-buffer-pointers-over-frame-structs
description: When writing helper functions, pass specific sub-structures (e.g., RATE_CONTROL) or channel buffer pointers rather than monolithic structs like AV1_COMP or EncodeFrameInput
rubric: design
severity: should_fix
---

**Rule.** When a helper function only accesses a specific sub-structure (such as
RATE_CONTROL, PRIMARY_RATE_CONTROL, or AV1_COMMON) or operates on a single image
plane/channel, pass the specific sub-structure or explicit channel buffer
pointers and strides (such as const RATE_CONTROL *rc or const uint8_t *src_y,
int src_ystride). Do not pass broad monolithic context structs (such as
AV1_COMP) or aggregate frame containers (such as EncodeFrameInput or
YV12_BUFFER_CONFIG) when only a narrow subset of fields is used.

**Why.** Passing monolithic context structs or aggregate frame containers to
narrow helper functions creates unnecessary coupling, obscures actual data
dependencies, and hinders function reusability with arbitrary buffers or
sub-components.

## Examples

Bad:

```cpp
// Anti-pattern 1: Passing monolithic AV1_COMP when only rc is needed
static int adjust_rtc_keyframe(const AV1_COMP *cpi, int enumerator) {
  const RATE_CONTROL *rc = &cpi->rc;
  if (rc->perc_flat_blocks_keyframe > 70) return enumerator;
  return enumerator * 2;
}

// Anti-pattern 2: Passing aggregate frame struct for a single plane
static void rc_spatial_act_keyframe(AV1_COMP *cpi,
                                    const EncodeFrameInput *frame_input) {
  const YV12_BUFFER_CONFIG *unscaled_src = frame_input->source;
  const uint8_t *src_y = unscaled_src->y_buffer;
  int src_ystride = unscaled_src->y_stride;
  // Operates only on luma plane...
}
```

Good:

```cpp
// Narrowed to the specific sub-structure
static int adjust_rtc_keyframe(const RATE_CONTROL *rc, int enumerator) {
  if (rc->perc_flat_blocks_keyframe > 70) return enumerator;
  return enumerator * 2;
}

// Narrowed to explicit channel buffer pointer and stride
static void rc_spatial_act_keyframe(AV1_COMP *cpi, const uint8_t *src_y,
                                    int src_ystride) {
  // Operates directly on passed luma buffer and stride...
}
```
