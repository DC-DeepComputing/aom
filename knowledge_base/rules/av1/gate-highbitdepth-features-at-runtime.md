---
name: libaom/av1/gate-highbitdepth-features-at-runtime
description: When gating encoder features or heuristics for bit depth, check runtime flags like cpi->oxcf.use_highbitdepth rather than compile-time CONFIG_AV1_HIGHBITDEPTH or bit_depth
rubric: correctness
severity: should_fix
---

**Rule.** When conditionally enabling, disabling, or branching encoder features,
search heuristics, or speed features based on stream bit depth, check runtime
configuration flags (such as cpi->oxcf.use_highbitdepth,
!cpi->oxcf.use_highbitdepth, cm->seq_params->use_highbitdepth,
is_cur_buf_hbd(xd), or local use_hbd / !use_hbd). Do not use compile-time
preprocessor guards (#if CONFIG_AV1_HIGHBITDEPTH or #if
!CONFIG_AV1_HIGHBITDEPTH) or sequence bit depth checks
(cm->seq_params->bit_depth == AOM_BITS_8).

**Why.** CONFIG_AV1_HIGHBITDEPTH is a build-time configuration flag indicating
that the binary was compiled with high-bitdepth support; high-bitdepth builds
still process and encode standard 8-bit streams. Using #if
CONFIG_AV1_HIGHBITDEPTH or #if !CONFIG_AV1_HIGHBITDEPTH causes 8-bit streams in
high-bitdepth builds to incorrectly disable low-bitdepth optimizations or
execute high-bitdepth logic paths. Furthermore, checking bit_depth == 8 is
insufficient because an 8-bit stream can still be routed through the
high-bitdepth pipeline (use_highbitdepth == 1). Checking
cpi->oxcf.use_highbitdepth guarantees heuristics match the active pipeline.

## Examples

Bad:

```cpp
// Gating high-bitdepth heuristic:
#if CONFIG_AV1_HIGHBITDEPTH
  return 0;
#endif

// Gating low-bitdepth speed feature:
#if !CONFIG_AV1_HIGHBITDEPTH
if (!cpi->rc.rtc_external_ratectrl &&
    AOMMIN(cm->width, cm->height) <= 720) {
  sf->hl_sf.accurate_bit_estimate = cpi->oxcf.q_cfg.aq_mode == NO_AQ;
}
#endif
```

Good:

```cpp
// Gating high-bitdepth heuristic:
  if (cpi->oxcf.use_highbitdepth) return 0;

// Gating low-bitdepth speed feature:
if (!cpi->rc.rtc_external_ratectrl &&
    AOMMIN(cm->width, cm->height) <= 720 && !cpi->oxcf.use_highbitdepth) {
  sf->hl_sf.accurate_bit_estimate = cpi->oxcf.q_cfg.aq_mode == NO_AQ;
}
```
