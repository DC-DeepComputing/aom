---
name: libaom/av1/no-const-cast-cpi-store-stats-in-macroblock
description: When recording statistics or counters during encoder search, do not cast away const from const AV1_COMP *cpi; store thread-local metrics in MACROBLOCK *x
rubric: safety
severity: blocking
---

**Rule.** Do not cast away const from `const AV1_COMP *cpi` to mutate compressor
state or record metrics during frame, block, or mode search. Instead, store
thread-local statistics, counters, and temporary buffers in the thread-local
`MACROBLOCK *x` context, and aggregate them across workers after parallel
encoding completes.

**Why.** `AV1_COMP` (`cpi`) is shared across worker threads without
synchronization during multi-threaded encoding, which is why search routines
receive it as `const`. Casting away `const` to modify `cpi` causes data races
and undefined behavior, whereas `MACROBLOCK` (`x`) is independently allocated
per worker thread and safe for unsynchronized local updates.

## Examples

Bad:

```cpp
// Mutating shared cpi by casting away const in worker search functions:
void av1_rd_pick_palette_intra_sby(const AV1_COMP *cpi, MACROBLOCK *x, ...) {
  // Bug: modifying shared cpi directly causes data races across workers
  ((AV1_COMP *)cpi)->palette_pixel_num += (block_width * block_height);
}
```

Good:

```cpp
// Accumulate stats in thread-local MACROBLOCK and aggregate after search:
void av1_rd_pick_palette_intra_sby(const AV1_COMP *cpi, MACROBLOCK *x, ...) {
  // Thread-safe: x is thread-local to this worker
  x->palette_pixels += (block_width * block_height);
}
```
