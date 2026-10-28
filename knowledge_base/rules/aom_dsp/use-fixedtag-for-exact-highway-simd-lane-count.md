---
name: libaom/aom_dsp/use-fixedtag-for-exact-highway-simd-lane-count
description: In Highway SIMD routines requiring an exact count of N vector lanes, use hn::FixedTag<T, N> instead of hn::CappedTag<T, N> to prevent truncation on narrower targets
rubric: correctness
severity: nit
---

**Rule.** In Highway SIMD routines, use hn::FixedTag<T, N> rather than
hn::CappedTag<T, N> when an exact count of N vector lanes is required (such as
storing output into fixed-size N-element arrays or buffers). Reserve
hn::CappedTag<T, N> for loops and operations that can safely process fewer than
N lanes on narrower vector architectures.

**Why.** hn::CappedTag<T, N> specifies a vector of at most N lanes and may
resolve to fewer lanes on hardware targets with narrower vector registers or
scalar emulation. Using CappedTag when code expects an exact N-element vector
causes partial writes, truncated results, and silent data corruption.

## Examples

Bad:

```cpp
// Incorrect: CappedTag may have < 4 lanes on narrow SIMD targets, truncating the store.
constexpr hn::CappedTag<uint32_t, 4> block_sum_tag;
hn::StoreU(block_sum, block_sum_tag, res);
```

Good:

```cpp
// Correct: FixedTag guarantees exactly 4 lanes across all target architectures.
constexpr hn::FixedTag<uint32_t, 4> block_sum_tag;
hn::StoreU(block_sum, block_sum_tag, res);
```
