---
name: libaom/validate-bounds-before-mutating-state
description: When validating buffer capacity or bounds before copying data, perform error checks before updating destination struct fields to fail fast and avoid partial state mutation
rubric: readability
severity: nit
---

**Rule.** When performing buffer bounds validation or fallible error checks
prior to copying data, execute the capacity and validity checks before modifying
destination struct fields or assigning intermediate state. Do not update struct
metadata or timestamps before validating that destination buffers can
accommodate the payload.

**Why.** Performing bounds checks and validation upfront fails fast before
executing unnecessary assignments. It also ensures destination data structures
remain uncorrupted and do not retain partial mutations if the operation
encounters a buffer overflow or error exit.

## Examples

Bad:

```cpp
// Anti-pattern: mutating struct fields before validating buffer capacity
dst_data->lib_flags = src_data->lib_flags;
dst_data->ts_frame_start = src_data->ts_frame_start;
dst_data->ts_frame_end = src_data->ts_frame_end;
if (src_data->frame_size > dst_data->cx_data_sz) {
  aom_internal_error(&ppi->error, AOM_CODEC_ERROR, "cx_data buffer full");
}
memcpy(dst_data->cx_data, src_data->cx_data, src_data->frame_size);
```

Good:

```cpp
// Validating buffer capacity before mutating struct fields
if (src_data->frame_size > dst_data->cx_data_sz) {
  aom_internal_error(&ppi->error, AOM_CODEC_ERROR, "cx_data buffer full");
}
dst_data->lib_flags = src_data->lib_flags;
dst_data->ts_frame_start = src_data->ts_frame_start;
dst_data->ts_frame_end = src_data->ts_frame_end;
memcpy(dst_data->cx_data, src_data->cx_data, src_data->frame_size);
```
