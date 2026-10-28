---
name: libaom/document-non-obvious-simd-trade-offs
description: When using non-obvious SIMD initialization idioms or microarchitectural workarounds, add code comments explaining compiler optimization behavior and performance trade-offs
rubric: readability
severity: should_fix
---

**Rule.** When implementing non-obvious SIMD sequences, initialization idioms
(such as dummy zero-initialization before full lane overwrite), or
microarchitecture-specific workarounds, document the rationale in code comments.
Explain why the chosen idiom is preferred over seemingly cleaner alternatives
(such as broadcast loads or lane insertions) and note compiler optimization
assumptions or hardware latency/throughput trade-offs.

**Why.** Seemingly redundant operations (such as zero-broadcasting before
overwriting all lanes) or non-standard intrinsic choices often look like bugs or
sub-optimal code to reviewers and maintainers. Documenting that compilers
eliminate the redundant initializations and that alternative instructions incur
microarchitectural penalties prevents unnecessary refactoring churn.

## Examples

Bad:

```cpp
// Gathering elements with non-obvious zero initialization without explanation
static INLINE uint8x8_t load_u8_gather_s16_x8(const uint8_t *src,
                                              int16x8_t indices) {
  uint8x8_t ret = vdup_n_u8(0);
  ret = vld1_lane_u8(src + indices[0], ret, 0);
  ret = vld1_lane_u8(src + indices[1], ret, 1);
  return ret;
}
```

Good:

```cpp
static INLINE uint8x8_t load_u8_gather_s16_x8(const uint8_t *src,
                                              int16x8_t indices) {
  // Recent Clang and GCC versions correctly identify that this zero-broadcast
  // is redundant. Alternatively we could load and broadcast the zeroth element
  // and then replace the other lanes, however this is slower than loading a
  // single element without broadcast on some micro-architectures.
  uint8x8_t ret = vdup_n_u8(0);
  ret = vld1_lane_u8(src + indices[0], ret, 0);
  ret = vld1_lane_u8(src + indices[1], ret, 1);
  return ret;
}
```
