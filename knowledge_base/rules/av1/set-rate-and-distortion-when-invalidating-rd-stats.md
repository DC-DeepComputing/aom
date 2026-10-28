---
name: libaom/av1/set-rate-and-distortion-when-invalidating-rd-stats
description: When invalidating or accumulating RD_STATS candidates with sentinel maximum values, set both rate and dist sentinels and assert accumulators are not sentinels before adding
rubric: correctness
severity: should_fix
---

**Rule.** When invalidating or marking an RD_STATS candidate structure as pruned
or skipped with sentinel values, set both rate and dist (e.g., rate = INT_MAX
and dist = INT64_MAX), or call av1_invalid_rd_stats(), rather than updating rate
alone. Additionally, when accumulating RD metrics across sub-blocks where
callbacks may prune paths with sentinels, assert that accumulator fields have
not been set to sentinels before adding to them, or use av1_merge_rd_stats() to
guard against integer overflow.

**Why.** RD_STATS tracks paired rate and distortion metrics, and leaving dist
uninitialized when setting rate to INT_MAX can cause downstream cost evaluation
to operate on stale values. Furthermore, accumulating additional sub-block
metrics onto an accumulator that already holds INT_MAX or INT64_MAX leads to
integer overflow.

## Examples

Bad:

```cpp
// Anti-pattern 1: Setting only rate sentinel on early prune
if (this_sad > sad_threshold) {
  args->rdc->rate = INT_MAX;
  return;
}

// Anti-pattern 2: Accumulating without checking for sentinel values
args->rdc->rate += this_rdc.rate;
args->rdc->dist += this_rdc.dist;
```

Good:

```cpp
// Pattern 1: Set both rate and distortion sentinels (or call av1_invalid_rd_stats)
if (this_sad > sad_threshold) {
  args->rdc->rate = INT_MAX;
  args->rdc->dist = INT64_MAX;
  return;
}

// Pattern 2: Assert accumulators are not sentinels before adding
assert(args->rdc->rate != INT_MAX && args->rdc->dist != INT64_MAX);
args->rdc->rate += this_rdc.rate;
args->rdc->dist += this_rdc.dist;
```
