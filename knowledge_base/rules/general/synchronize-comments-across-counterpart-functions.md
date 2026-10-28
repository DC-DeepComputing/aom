---
name: libaom/synchronize-comments-across-counterpart-functions
description: When documenting algorithmic rationale, boundary invariants, or non-obvious logic in functions, synchronize explanatory comments across counterpart implementations like highbd_ and SIMD variants
rubric: readability
severity: nit
---

**Rule.** When adding or updating comments that explain algorithmic rationale,
mathematical formulas, non-obvious tweaks (such as matrix regularization or
numerical clamping), boundary invariants, or edge-case handling in reference
code or functions, replicate and synchronize those comments across all
corresponding counterpart implementations. This includes standard 8-bit vs.
highbd_ variants, C reference code vs. architecture-specific SIMD
implementations (e.g., SSE, AVX, NEON), and conditional debug blocks.

**Why.** libaom maintains parallel implementations across bitdepths (standard
vs. highbd_) and architectures (C reference vs. SSE/AVX/NEON SIMD kernels) that
share the same underlying mathematical invariants and boundary conditions.
Allowing explanatory comments to diverge causes documentation drift and makes
optimized or high-bitdepth variants harder to maintain or safely refactor
without constantly cross-referencing other files.

## Examples

Bad:

```cpp
// Variant 1: Diverged comments between standard and highbd_ functions
// In filter_selectively_horiz():
// Next block's thresholds, when it is within current 64x64 block.
// If it is out of bound, its mask is zero, and it points to current edge's
// filter parameters, instead of next edge's.
int next_edge = step;
if (offset + next_edge >= MI_SIZE_64X64) next_edge = 0;

// In highbd_filter_selectively_horiz():
// Next block's thresholds.
int next_edge = step;
if (offset + next_edge >= MI_SIZE_64X64) next_edge = 0;

// Variant 2: Missing algorithmic context in SIMD counterpart
// Apply regularization
result = _mm_add_epi32(result, _mm_set_epi32(1, 0, 0, 1));
```

Good:

```cpp
// Variant 1: Synchronized comments across standard and highbd_ counterparts
// In both filter_selectively_horiz() and highbd_filter_selectively_horiz():
// Next block's thresholds, when it is within current 64x64 block.
// If it is out of bound, its mask is zero, and it points to current edge's
// filter parameters, instead of next edge's.
int next_edge = step;
if (offset + next_edge >= MI_SIZE_64X64) next_edge = 0;

// Variant 2: Mathematical rationale replicated in SIMD counterpart
// Apply regularization: we add k * I (with k = 1) to ensure det M >= k^2 > 0
// and avoid singular matrices.
result = _mm_add_epi32(result, _mm_set_epi32(1, 0, 0, 1));
```
