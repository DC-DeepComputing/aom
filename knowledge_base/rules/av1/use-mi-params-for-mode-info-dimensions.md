---
name: libaom/av1/use-mi-params-for-mode-info-dimensions
description: When accessing mode-info grid boundaries and dimensions like mi_rows and mi_cols, retrieve them consistently from CommonModeInfoParams (mi_params) rather than mixing secondary sources like frame_info
rubric: readability
severity: should_fix
---

**Rule.** Access mode-info grid dimensions and loop boundaries (mi_rows,
mi_cols) consistently from CommonModeInfoParams (cpi->common.mi_params or
cm->mi_params), or alias a local const CommonModeInfoParams *const mi_params
pointer. Do not mix dimension sources within the same module by accessing row
boundaries from secondary structures like cpi->frame_info while taking column
boundaries from mi_params.

**Why.** CommonModeInfoParams maintains the authoritative frame mode-info grid
dimensions used for worker thread allocation and job scheduling across common,
encoder, and decoder modules. Mixing separate dimension structures creates
inconsistency and risks using mismatched or out-of-sync grid boundaries.

## Examples

Bad:

```cpp
const int mi_cols = cpi->common.mi_params.mi_cols;
const int mt_unit_cols = (mi_cols + (mt_unit_step >> 1)) / mt_unit_step;

for (int mi_row = 0, mt_thread_id = 0; mi_row < cpi->frame_info.mi_rows;
     mi_row += mb_step, ++mt_thread_id) {
  intra_mt->intra_sync_write_ptr(intra_row_mt_sync, mt_thread_id,
                                 mt_unit_cols - 1, mt_unit_cols);
}
```

Good:

```cpp
const CommonModeInfoParams *const mi_params = &cpi->common.mi_params;
const int mt_unit_cols =
    (mi_params->mi_cols + (mt_unit_step >> 1)) / mt_unit_step;

for (int mi_row = 0, mt_thread_id = 0; mi_row < mi_params->mi_rows;
     mi_row += mb_step, ++mt_thread_id) {
  intra_mt->intra_sync_write_ptr(intra_row_mt_sync, mt_thread_id,
                                 mt_unit_cols - 1, mt_unit_cols);
}
```
