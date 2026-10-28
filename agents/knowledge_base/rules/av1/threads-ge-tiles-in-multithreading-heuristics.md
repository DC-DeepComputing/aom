---
name: libaom/av1/threads-ge-tiles-in-multithreading-heuristics
description: When gating encoder multi-threading heuristics on thread count relative to tile count, use greater-or-equal comparisons (max_threads >= num_tiles) instead of exact equality
rubric: correctness
severity: should_fix
---

**Rule.** When gating multi-threading heuristics or speed feature decisions on
thread count relative to tile count, check if the thread count is greater than
or equal to the tile count (`max_threads >= num_tiles`) rather than testing
exact equality (`==`).

**Why.** Exact equality checks (`max_threads == num_tiles`) incorrectly disable
multi-threading heuristics when more worker threads than tiles are configured
(such as row multi-threading with 8 threads and 4 tiles). Using `>=` ensures
heuristics trigger whenever there are sufficient threads to cover all tiles.

## Examples

Bad:

```cpp
if (oxcf->row_mt == 1 && oxcf->max_threads >= 4 &&
    oxcf->max_threads == num_tiles &&
    (width * height) / (128 * 128 * num_tiles) <= 32) {
  return BLOCK_64X64;
}
```

Good:

```cpp
if (oxcf->row_mt == 1 && oxcf->max_threads >= 4 &&
    oxcf->max_threads >= num_tiles && AOMMIN(width, height) > 720 &&
    (width * height) / (128 * 128 * num_tiles) <= 32) {
  return BLOCK_64X64;
}
```
