---
name: libaom/av1/trim-lookahead-frames-from-tpl-gop-stats
description: When passing GOP-level TPL stats to external rate control callbacks, trim lookahead frames appended for ARF boost to match GOP boundaries
rubric: correctness
severity: should_fix
---

**Rule.** When passing GOP-level TPL statistics to external rate control (such
as via av1_extrc_send_tpl_stats or send_tpl_gop_stats), trim trailing lookahead
frames appended for ARF boost calculation. Ensure the external rate controller
receives stats strictly encompassing frames within the current GOP.

**Why.** Libaom's TPL analysis processes frames beyond the current GOP into the
lookahead buffer to calculate accurate ARF boost scores. Passing these untrimmed
trailing lookahead frames to external rate control causes frame count mismatches
and misallocates bitrate across GOP boundaries.

## Examples

Bad:

```cpp
if (cpi->ext_ratectrl.ready &&
    cpi->ext_ratectrl.funcs.send_tpl_gop_stats != NULL) {
  const aom_codec_err_t codec_status =
      av1_extrc_send_tpl_stats(&cpi->ext_ratectrl, &cpi->extrc_tpl_gop_stats);
}
```

Good:

```cpp
if (cpi->ext_ratectrl.ready &&
    cpi->ext_ratectrl.funcs.send_tpl_gop_stats != NULL) {
  trim_tpl_stats(cpi->common.error, &cpi->extrc_tpl_gop_stats,
                 extended_frame_count);
  const aom_codec_err_t codec_status =
      av1_extrc_send_tpl_stats(&cpi->ext_ratectrl, &cpi->extrc_tpl_gop_stats);
}
```
