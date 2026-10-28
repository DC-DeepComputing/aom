---
name: libaom/aom_dsp/encapsulate-simd-lane-stores-in-helpers
description: When storing SIMD vector lanes to strided memory destinations, encapsulate element extraction and stores in reusable inline helpers instead of ad-hoc extract-and-memcpy sequences
rubric: design
severity: should_fix
---

**Rule.** Encapsulate strided SIMD lane extraction and store patterns (such as
extracting vector elements and storing them to strided memory rows) into
reusable inline helper functions or shared memory macros (such as store_*_lane
utilities). Do not open-code ad-hoc vector element extraction and memcpy
sequences across multiple lines or call sites.

**Why.** Open-coding individual lane extraction and memcpy operations at each
strided destination duplicates boilerplate and clutters SIMD DSP routines.
Factoring lane stores into inline helpers or shared memory utilities improves
readability, reduces pointer and stride errors, and maintains consistent SIMD
styling across target architectures.

## Examples

Bad:

```cpp
// Ad-hoc vector element extraction and memcpy per strided row:
uint16_t val0 = (uint16_t)_mm_extract_epi16(reg, 0);
memcpy(dst + 0 * dst_stride, &val0, sizeof(val0));
uint16_t val1 = (uint16_t)_mm_extract_epi16(reg, 1);
memcpy(dst + 1 * dst_stride, &val1, sizeof(val1));
```

Good:

```cpp
// Encapsulated in a reusable inline helper or shared store_*_lane utility:
store_u8_2x1_lane(dst + 0 * dst_stride, reg, 0);
store_u8_2x1_lane(dst + 1 * dst_stride, reg, 1);
```
