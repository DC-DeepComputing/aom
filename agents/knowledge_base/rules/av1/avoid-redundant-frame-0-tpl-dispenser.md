---
name: libaom/av1/avoid-redundant-frame-0-tpl-dispenser
description: When exporting or processing TPL model statistics in av1_tpl_setup_stats, do not manually invoke init_mc_flow_dispenser or mc_flow_dispenser for frame 0
rubric: safety
severity: should_fix
---

**Rule.** Do not manually invoke init_mc_flow_dispenser() and
mc_flow_dispenser() for frame 0 when configuring or exporting TPL statistics. In
libaom, av1_tpl_setup_stats() iterates over all GOP frames starting from
cpi->gf_frame_index (frame 0), making separate manual frame 0 dispenser calls
redundant.

**Why.** Unlike libvpx, libaom's TPL setup loop already processes frame 0 within
its main GOP frame iteration. Invoking motion compensation flow dispensers
separately for frame 0 duplicates expensive block-level motion flow calculations
and wastes encoder cycles.

## Examples

Bad:

```cpp
if (gf_group->update_type[0] != OVERLAY_UPDATE) {
  init_mc_flow_dispenser(cpi, 0, gf_group->q_val[0]);
  mc_flow_dispenser(cpi);
}
const aom_codec_err_t codec_status =
    av1_extrc_send_tpl_stats(&cpi->ext_ratectrl, &cpi->tpl_gop_stats);
```

Good:

```cpp
trim_tpl_stats(cpi->common.error, &cpi->extrc_tpl_gop_stats,
               extended_frame_count);
const aom_codec_err_t codec_status =
    av1_extrc_send_tpl_stats(&cpi->ext_ratectrl, &cpi->extrc_tpl_gop_stats);
```
