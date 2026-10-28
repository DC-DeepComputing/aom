---
name: libaom/consolidate-identical-dispatch-tables
description: When defining function dispatch or lookup tables with identical entries, consolidate them into a single table instead of maintaining redundant arrays under separate names
rubric: design
severity: should_fix
---

**Rule.** Consolidate duplicate function dispatch or lookup tables into a single
shared table when their entries are identical across configurations, pixel
widths, or stages. Do not maintain separate redundant arrays under distinct
names or comments when a single table can serve all call sites.

**Why.** Maintaining separate tables with identical function pointer entries
creates redundant data, wastes binary size, and confuses readers by suggesting
different implementations or specializations are being selected.

## Examples

Bad:

```cpp
// 1D functions process 4 pixels at one time.
static const transform_1d_neon
    lowbd_txfm_all_1d_w4_arr[TX_SIZES][ITX_TYPES_1D] = {
      { av1_idct4_new, av1_iadst4_new, av1_iidentity4_c },
      { av1_idct8_new, av1_iadst8_new, av1_iidentity8_c },
      { av1_idct16_new, av1_iadst16_new, av1_iidentity16_c },
    };

// 1D functions process 8 pixels at one time.
static const transform_1d_neon
    lowbd_txfm_all_1d_w8_arr[TX_SIZES][ITX_TYPES_1D] = {
      { av1_idct4_new, av1_iadst4_new, av1_iidentity4_c },
      { av1_idct8_new, av1_iadst8_new, av1_iidentity8_c },
      { av1_idct16_new, av1_iadst16_new, av1_iidentity16_c },
    };
```

Good:

```cpp
// 1D functions
static const transform_1d_neon lowbd_txfm_all_1d_arr[TX_SIZES][ITX_TYPES_1D] = {
  { av1_idct4_new, av1_iadst4_new, av1_iidentity4_c },
  { av1_idct8_new, av1_iadst8_new, av1_iidentity8_c },
  { av1_idct16_new, av1_iadst16_new, av1_iidentity16_c },
  { av1_idct32_new, NULL, NULL },
  { av1_idct64_new, NULL, NULL },
};
```
