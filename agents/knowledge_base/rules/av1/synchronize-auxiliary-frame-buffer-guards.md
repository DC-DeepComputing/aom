---
name: libaom/av1/synchronize-auxiliary-frame-buffer-guards
description: When modifying conditions that gate preparing auxiliary frame buffers in the encoder, update matching restoration and consumption guards across downstream stages
rubric: correctness
severity: should_fix
---

**Rule.** When adding or modifying conditional guards that control allocating,
preparing, or copying auxiliary frame buffers (such as cpi->orig_source or
cpi->unfiltered_source for temporal filtering or PSNR calculation), ensure
matching conditional expressions gate the corresponding restoration or
consumption logic in downstream encoder pipeline stages (such as
encode_strategy.c).

**Why.** If the conditions gating auxiliary buffer setup in encoder.c and buffer
restoration in encode_strategy.c diverge, downstream stages may restore
unallocated or stale buffer pointers, or fail to restore original source
pointers, leading to memory corruption, assertion failures, or incorrect
distortion computations.

## Examples

Bad:

```cpp
// In av1/encoder/encoder.c:
if (is_psnr_calc_enabled(cpi) && cpi->sf.rt_sf.use_rtc_tf) {
  aom_yv12_copy_y(cpi->source, &cpi->orig_source, 1);
}

// In av1/encoder/encode_strategy.c (mismatched condition):
if (is_psnr_calc_enabled(cpi) && cpi->sf.rt_sf.use_rtc_tf &&
    cpi->common.current_frame.frame_type != KEY_FRAME) {
  cpi->source = &cpi->orig_source;
}
```

Good:

```cpp
// In av1/encoder/encoder.c:
if (is_psnr_calc_enabled(cpi) && cpi->sf.rt_sf.use_rtc_tf) {
  aom_yv12_copy_y(cpi->source, &cpi->orig_source, 1);
}

// In av1/encoder/encode_strategy.c (synchronized condition):
if (is_psnr_calc_enabled(cpi) && cpi->sf.rt_sf.use_rtc_tf) {
  cpi->source = &cpi->orig_source;
}
```
