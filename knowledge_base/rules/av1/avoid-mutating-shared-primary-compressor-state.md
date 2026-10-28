---
name: libaom/av1/avoid-mutating-shared-primary-compressor-state
description: When initializing per-frame or per-thread encoder speed features, do not modify shared primary compressor state (cpi->ppi) to avoid multi-threading data races
rubric: safety
severity: blocking
---

**Rule.** Do not modify shared primary compressor state (cpi->ppi) from
per-frame or per-thread functions such as speed feature initialization
(av1_set_speed_features_*). Initialize shared sequence-level or primary
rate-control fields during primary compressor setup (av1_create_compressor) or
protect them with explicit synchronization to prevent data races across parallel
worker threads.

**Why.** In libaom's multi-threaded encoder architecture, AV1_PRIMARY (cpi->ppi)
holds shared sequence-level and rate-control state across parallel frame or tile
workers. Mutating fields within cpi->ppi from per-frame or per-instance
functions introduces unsynchronized concurrent writes, causing ThreadSanitizer
data races and non-deterministic behavior.

## Examples

Bad:

```cpp
void av1_set_speed_features_framesize_independent(AV1_COMP *cpi, int speed) {
  // Bad: Modifying shared primary compressor state in per-frame speed features
  cpi->ppi->p_rc.rtc_mode = (cpi->oxcf.mode == REALTIME);
}
```

Good:

```cpp
// Good: Initialize shared primary state during compressor creation
void av1_create_compressor(AV1_PRIMARY *ppi, ...) {
  ppi->p_rc.rtc_mode = (oxcf->mode == REALTIME);
}

void av1_set_speed_features_framesize_independent(AV1_COMP *cpi, int speed) {
  // Only configure per-instance speed features
  cpi->sf.rt_sf.use_nonrd_pick_mode = 1;
}
```
