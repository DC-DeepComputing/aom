---
name: libaom/av1/document-intentional-simd-overstore-or-overread
description: When SIMD or DSP kernels intentionally over-store or over-read beyond logical block dimensions to eliminate branches, document why the operation is memory-safe
rubric: readability
severity: nit
---

**Rule.** When SIMD or DSP vector kernels intentionally over-store or over-read
beyond logical block width or height to eliminate branches in hot loops, add
comments explaining that the operation avoids branching and why it is
memory-safe (e.g., intermediate buffer sizing or row padding).

**Why.** Storing or loading beyond logical block dimensions looks like an
out-of-bounds memory hazard. Explaining the buffer capacity guarantees and
branch-elimination intent prevents confusion during code review and stops future
maintainers from re-introducing unnecessary conditional branches.

## Examples

Bad:

```cpp
// Store 8 elements without explaining why over-storing is safe when block width < 8:
vst1q_s16(dst_ptr + i * dst_stride + j, d0);
```

Good:

```cpp
// Store 8 elements to avoid additional branches. This is safe if the
// actual block width is < 8 because the intermediate buffer is large
// enough to accommodate 128x128 blocks.
vst1q_s16(dst_ptr + i * dst_stride + j, d0);
```
