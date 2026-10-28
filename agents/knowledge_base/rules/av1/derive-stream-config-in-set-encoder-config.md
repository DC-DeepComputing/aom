---
name: libaom/av1/derive-stream-config-in-set-encoder-config
description: When deriving stream-level encoder configuration parameters from resolution and thread counts, compute them in set_encoder_config rather than recalculating per frame in encoder_encode
rubric: safety
severity: should_fix
---

**Rule.** Compute and configure stream-level or codec control parameters (such
as automatic tile rows and columns derived from frame resolution and thread
count) in set_encoder_config() during initialization or reconfiguration. Do not
recalculate stream-level or static control settings on the per-frame encoding
hot path in encoder_encode() or av1_update_frame_size().

**Why.** Stream-level encoder settings derived from dimensions, threading
limits, and user configurations remain constant across frames unless explicitly
updated via codec controls. Re-evaluating these parameters per frame introduces
unnecessary runtime overhead on hot encoding paths and blurs the architectural
boundary between interface configuration and core frame encoding.

## Examples

Bad:

```cpp
void av1_update_frame_size(AV1_COMP *cpi) {
  // Anti-pattern: recalculating stream-level tile config per frame.
  if (cpi->auto_tiles_enabled && cpi->oxcf.max_threads > 1) {
    av1_set_auto_tiles(&cpi->oxcf.tile_cfg, cm->width, cm->height,
                       cpi->oxcf.max_threads);
  }
}
```

Good:

```cpp
static void set_encoder_config(AV1EncoderConfig *oxcf,
                               const aom_codec_enc_cfg_t *cfg,
                               struct av1_extracfg *extra_cfg) {
  // Good: compute stream-level configuration during init / reconfiguration.
  if (extra_cfg->auto_tiles) {
    set_auto_tiles(&oxcf->tile_cfg, cfg->g_w, cfg->g_h, cfg->g_threads);
    extra_cfg->tile_columns = oxcf->tile_cfg.tile_columns;
    extra_cfg->tile_rows = oxcf->tile_cfg.tile_rows;
  }
}
```
