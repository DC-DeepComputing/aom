---
name: libaom/av1/process-unconditional-luma-prior-to-chroma-plane-guards
description: When processing image planes with conditional chroma checks (num_planes > 1), execute unconditional luma operations independently first instead of duplicating luma logic across if-else branches
rubric: design
severity: nit
---

**Rule.** When processing image planes where luma (Y) operations apply
unconditionally and chroma (UV) operations are conditional on num_planes > 1,
execute the luma logic independently first. Do not nest unconditional luma
checks inside a num_planes > 1 block or duplicate luma logic across if
(num_planes > 1) and else or else if fallback branches for monochrome streams.

**Why.** Nesting unconditional luma operations inside multi-plane chroma checks
requires duplicating the luma operations in an else or else if branch for
single-plane (monochrome) streams. Decoupling the unconditional luma logic from
the conditional chroma guard simplifies control flow, eliminates code
duplication, and keeps plane handling clean and consistent.

## Examples

Bad:

```cpp
if (num_planes > 1) {
  if (is_low_luma_strength) {
    cdef_info->cdef_strengths[j] = 0;
    cdef_info->cdef_uv_strengths[j] = 0;
  }
  if (is_low_chroma_strength) {
    cdef_info->cdef_uv_strengths[j] = 0;
  }
} else if (is_low_luma_strength) {
  cdef_info->cdef_strengths[j] = 0;
}
```

Good:

```cpp
if (is_low_luma_strength) {
  cdef_info->cdef_strengths[j] = 0;
}
if (num_planes > 1) {
  if (is_low_luma_strength || is_low_chroma_strength) {
    cdef_info->cdef_uv_strengths[j] = 0;
  }
}
```
