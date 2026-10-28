---
name: libaom/use-mm-shuffle-macro-for-shuffle-masks
description: When specifying mask arguments in x86 SIMD shuffle or permute intrinsics, use the _MM_SHUFFLE macro instead of magic integer literals
rubric: readability
severity: should_fix
---

**Rule.** When passing immediate shuffle mask constants to x86 SIMD shuffle or
permute intrinsics (such as _mm_shuffle_epi32, _mm_shuffle_ps,
_mm_shufflehi_epi16, _mm_shufflelo_epi16, or _mm256_shuffle_epi32), use the
_MM_SHUFFLE (or _MM_SHUFFLE2) macro rather than bare decimal or hexadecimal
integer literals.

**Why.** Raw integer literals (such as 78 or 0x4e) obscure which vector lanes
are being selected and permuted. The _MM_SHUFFLE(z, y, x, w) macro explicitly
specifies the source lane for each destination sub-lane, preventing
transposition bugs and improving code readability.

## Examples

Bad:

```cpp
// Obscure magic integer literals for shuffle mask arguments
p0q0 = _mm_shuffle_epi32(*q0p0, 78);
hi = _mm_shufflehi_epi16(val, 0x1b);
```

Good:

```cpp
// Explicit lane permutation using _MM_SHUFFLE macro
p0q0 = _mm_shuffle_epi32(*q0p0, _MM_SHUFFLE(1, 0, 3, 2));
hi = _mm_shufflehi_epi16(val, _MM_SHUFFLE(0, 1, 2, 3));
```
