---
name: libaom/reuse-temporary-buffers-across-loop-iterations
description: When evaluating candidates or intermediate data sequentially across loop iterations, reuse a single scratch buffer rather than allocating arrays or per-iteration buffers
rubric: safety
severity: should_fix
---

**Rule.** When performing search, filtering, or candidate evaluations where
intermediate results are processed sequentially in a loop, allocate and reuse a
single temporary scratch buffer across all iterations. Do not allocate an array
of buffers for all candidates or perform dynamic allocations inside the loop
when data from prior iterations is not needed for subsequent evaluation.

**Why.** Allocating an array of buffers or invoking dynamic memory allocation on
every loop iteration incurs significant allocation overhead, increases peak
memory consumption, and degrades cache locality. Reusing a single pre-allocated
scratch buffer minimizes memory footprint and eliminates redundant allocation
churn.

## Examples

Bad:

```cpp
// Bad: Allocating an array of scratch buffers for all search iterations
uint8_t *pred_blocks[61 * 61];
for (int i = 0; i < 61 * 61; i++) {
  pred_blocks[i] = aom_malloc(block_width * block_height);
}
for (int i = -30; i <= 30; i++) {
  for (int j = -30; j <= 30; j++) {
    av1_get_pred_block(ref_frame, pred_blocks[idx], i, j);
    sad = get_sad_of_blocks(cur_block, pred_blocks[idx], block_width, block_height);
    if (sad < best_sad) {
      best_sad = sad;
      best_mv = (MV){ i, j };
    }
    idx++;
  }
}
```

Good:

```cpp
// Good: Allocate a single scratch buffer and reuse it across all loop iterations
uint8_t *pred_block = aom_malloc(block_width * block_height);
for (int i = -30; i <= 30; i++) {
  for (int j = -30; j <= 30; j++) {
    av1_get_pred_block(ref_frame, pred_block, i, j);
    sad = get_sad_of_blocks(cur_block, pred_block, block_width, block_height);
    if (sad < best_sad) {
      best_sad = sad;
      best_mv = (MV){ i, j };
    }
  }
}
aom_free(pred_block);
```
