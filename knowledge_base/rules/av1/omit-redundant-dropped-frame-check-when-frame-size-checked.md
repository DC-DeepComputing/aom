---
name: libaom/av1/omit-redundant-dropped-frame-check-when-frame-size-checked
description: When checking cpi_data->frame_size > 0 in encoder post-encode routines, omit redundant !cpi->is_dropped_frame checks because dropped frames always produce zero size
rubric: readability
severity: nit
---

**Rule.** In AV1 encoder post-encode and packet output routines, do not add
redundant !cpi->is_dropped_frame condition checks when frame_size > 0 (or
cpi_data->frame_size > 0) is already tested. In libaom's encoder pipeline,
dropped frames are guaranteed to have a frame size of zero, making explicit
dropped-frame guards redundant when frame size is validated.

**Why.** Dropped frames are assigned a frame size of zero during encoding.
Checking frame_size > 0 inherently ensures that the frame was not dropped, so
adding an extra !is_dropped_frame check adds unnecessary condition clutter.

## Examples

Bad:

```cpp
if (ppi->b_calculate_psnr && cpi_data->frame_size > 0 &&
    !cpi->is_dropped_frame) {
  if (cm->show_existing_frame ||
      (!is_stat_generation_stage(cpi) && cm->show_frame)) {
    generate_psnr_packet(cpi);
  }
}
```

Good:

```cpp
if (ppi->b_calculate_psnr && cpi_data->frame_size > 0) {
  if (cm->show_existing_frame ||
      (!is_stat_generation_stage(cpi) && cm->show_frame)) {
    generate_psnr_packet(cpi);
  }
}
```
