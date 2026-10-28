---
name: libaom/factor-complex-branch-logic-into-helpers
description: When implementing feature logic inside preprocessor (#if/#elif) or complex conditional branches, extract multi-step blocks into dedicated helper functions rather than inlining them in dispatch routines
rubric: design
severity: should_fix
---

**Rule.** Extract lengthy, self-contained feature logic or multi-step processing
within preprocessor branches (#if/#elif) and complex conditional blocks into
dedicated helper functions. Do not inline large feature-specific implementations
directly within top-level dispatch or control flow functions.

**Why.** Inlining multi-step conditional feature logic directly inside
preprocessor branches obscures the top-level dispatch flow, mixes unrelated
feature concerns, and makes functions difficult to read and maintain across
different build configurations. Extracting the logic into dedicated helper
functions isolates feature complexity and keeps dispatch routines clean.

## Examples

Bad:

```cpp
void av1_inverse_transform_block(const MACROBLOCKD *xd, ...) {
#elif CONFIG_DSPL_RESIDUAL
  DSPL_TYPE dspl_type = xd->mi[0]->dspl_type;
  if (plane == 0 && dspl_type == DSPL_XY) {
    const TX_SIZE new_tx_size = half_tx_size_map[tx_size];
    // Inlined multi-step buffer allocation, unpack, inverse diff, upsample...
    av1_unpack_and_inverse_diff(...);
    av1_upsample_and_add(...);
  } else {
    av1_inv_txfm_add(dqcoeff, dst, stride, &txfm_param);
  }
#else
  av1_inv_txfm_add(dqcoeff, dst, stride, &txfm_param);
#endif
}
```

Good:

```cpp
#if CONFIG_DSPL_RESIDUAL
static void inverse_dspl_transform_block(const MACROBLOCKD *xd, ...) {
  // Encapsulated buffer allocation, unpack, inverse diff, upsample...
  av1_unpack_and_inverse_diff(...);
  av1_upsample_and_add(...);
}
#endif

void av1_inverse_transform_block(const MACROBLOCKD *xd, ...) {
#elif CONFIG_DSPL_RESIDUAL
  if (plane == 0 && xd->mi[0]->dspl_type == DSPL_XY && xd->bd == 8) {
    inverse_dspl_transform_block(xd, dqcoeff, dst, stride, ...);
  } else {
    av1_inv_txfm_add(dqcoeff, dst, stride, &txfm_param);
  }
#else
  av1_inv_txfm_add(dqcoeff, dst, stride, &txfm_param);
#endif
}
```
