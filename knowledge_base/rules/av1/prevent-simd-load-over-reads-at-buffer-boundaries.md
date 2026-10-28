---
name: libaom/av1/prevent-simd-load-over-reads-at-buffer-boundaries
description: When implementing SIMD vector loads near row or block boundaries in 2D processing kernels, ensure loads do not read past buffer bounds on the final row
rubric: safety
severity: should_fix
---

**Rule.** In 2D SIMD kernels (such as image resizing, convolution, or filtering
routines), ensure that fixed-width vector loads near row or block boundaries do
not read past allocated buffer bounds, especially on the final row where reading
beyond valid width accesses unallocated memory. Adjust load offsets, clamp
boundary indices, or handle trailing boundary columns with dedicated scalar
fallbacks or masked loads.

**Why.** While vector over-reads at intermediate row ends may safely land in
subsequent row padding or next-line pixels, over-reading past the end of the
final row reads beyond the buffer's allocated memory boundary. This causes
memory safety violations, address sanitizer (ASan) crashes, and undefined
behavior.

## Examples

Bad:

```cpp
// On the final row (i == height - 1), a 16-byte vector load with positive offset
// at the last block column over-reads past the end of the input buffer.
const int in_idx = i * in_stride + j - filter_offset;
__m128i row01 = _mm_loadu_si128((const __m128i *)&input[in_idx + 5 + filter_offset]);
```

Good:

```cpp
// Clamp offset or branch on boundary columns to avoid loading beyond buffer end.
const int is_last_cols16 = (j == filtered_length - 16);
const int row01_offset = is_last_cols16 ? 0 : 5;
__m128i row01 = _mm_loadu_si128((const __m128i *)&input[in_idx + row01_offset + filter_offset]);
```
