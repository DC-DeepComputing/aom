---
name: libaom/av1/no-zero-based-plane-index-for-chroma
description: When iterating over chroma-only planes, do not use a 0-based index named plane; use plane=1 to MAX_MB_PLANE or rename to uv_idx or comp
rubric: readability
severity: should_fix
---

**Rule.** When iterating over chroma-only planes (U and V), do not name a
0-based loop variable `plane` (e.g., `for (int plane = 0; plane < 2; plane++)`).
Instead, iterate using standard plane indices (e.g., `for (int plane = 1; plane
< MAX_MB_PLANE; ++plane)` and index 2-element arrays with `plane - 1`), or
rename the 0-based variable descriptively (such as `uv_idx` or `comp`).

**Why.** In libaom, plane indices 0, 1, and 2 conventionally represent Y (Luma),
U (Chroma), and V (Chroma) respectively. Naming a 0-based chroma loop variable
`plane` is misleading because `plane == 0` suggests Luma, whereas explicit
1-based plane indexing or dedicated chroma index names preserve clear plane
semantics.

## Examples

Bad:

```cpp
// Misleading: plane == 0 suggests Luma, but loop iterates over chroma
for (int plane = 0; plane < 2; plane++) {
  cm->ccso_info.ccso_enable[plane] = aom_rb_read_literal(rb, 1);
}
```

Good:

```cpp
// Option 1: Standard 1-based plane indices (1 = U, 2 = V)
for (int plane = 1; plane < MAX_MB_PLANE; ++plane) {
  cm->ccso_info.ccso_enable[plane - 1] = aom_rb_read_literal(rb, 1);
}

// Option 2: Descriptive 0-based chroma component index
for (int uv_idx = 0; uv_idx < 2; ++uv_idx) {
  cm->ccso_info.ccso_enable[uv_idx] = aom_rb_read_literal(rb, 1);
}
```
