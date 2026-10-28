---
name: libaom/match-rtcd-prototype-parameter-order-2
description: When declaring or updating RTCD prototypes (*_rtcd_defs.pl), C reference functions, and SIMD specializations, ensure parameter names, ordering, count, and types match exactly
rubric: correctness
severity: should_fix
---

**Rule.** When declaring, implementing, or updating RTCD prototype definitions
(*_rtcd_defs.pl), C reference functions (_c), and architecture-specific SIMD
specializations (e.g., _neon, _sve, _sse2, _ssse3, _avx2, _rvv), ensure that
parameter names, ordering, count, and types match identically across all
declarations and definitions. Never transpose adjacent parameters of the same
type across architecture variants. When renaming, reordering, adding, or
modifying parameters, synchronize the change consistently across RTCD
prototypes, the C reference function, and all SIMD specializations.

**Why.** Mismatched parameter order or types between architecture-specific SIMD
functions and their RTCD prototypes—especially for adjacent parameters of
identical types—causes silent argument swapping and subtle runtime or visual
corruption when dispatched through RTCD function tables. Inconsistent parameter
names between RTCD prototype headers and function implementations trigger
clang-tidy warnings (readability-inconsistent-declaration-parameter-name) and
hinder cross-architecture code maintainability.

## Examples

Bad:

```cpp
// 1. Transposed parameter order vs av1_rtcd_defs.pl (which expects: const int phase, const int num_planes):
void av1_resize_and_extend_frame_neon(const YV12_BUFFER_CONFIG *src,
                                      YV12_BUFFER_CONFIG *dst,
                                      const InterpFilter filter,
                                      const int num_planes, const int phase);

// 2. Mismatched parameter names vs av1_rtcd_defs.pl (which expects: int v, int h):
void cdef_copy_rect8_8bit_to_16bit_avx2(uint16_t *dst, int dstride,
                                        const uint8_t *src, int sstride,
                                        int height, int width);
```

Good:

```cpp
// 1. Parameter order matching RTCD prototype and C reference:
void av1_resize_and_extend_frame_neon(const YV12_BUFFER_CONFIG *src,
                                      YV12_BUFFER_CONFIG *dst,
                                      const InterpFilter filter,
                                      const int phase, const int num_planes);

// 2. Parameter names matching RTCD prototype and C reference:
void cdef_copy_rect8_8bit_to_16bit_avx2(uint16_t *dst, int dstride,
                                        const uint8_t *src, int sstride,
                                        int v, int h);
```
