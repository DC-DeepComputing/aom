---
name: libaom/contrast-allocation-and-reallocation-semantics
description: When documenting memory or buffer allocation functions that have paired reallocation variants, explicitly contrast unconditional freeing and allocation against memory reuse semantics
rubric: readability
severity: nit
---

**Rule.** When documenting memory or buffer allocation functions that have
corresponding reallocation variants (such as `aom_alloc_frame_buffer` and
`aom_realloc_frame_buffer`), explicitly contrast their memory lifecycle
semantics. Document whether the allocation helper unconditionally frees and
reallocates memory, and explain that the reallocation variant reuses existing
buffers where possible so callers know when to prefer each function.

**Why.** Vague comments advising callers to consider a reallocation function
without stating why cause confusion and risk suboptimal performance. Clearly
contrasting unconditional reallocation with memory reuse enables callers to
choose the right helper and avoid unnecessary memory fragmentation and
reallocation overhead.

## Examples

Bad:

```cpp
// If ybf currently contains an image, all associated memory is freed first.
// If this is likely, consider using aom_realloc_frame_buffer() instead.
int aom_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf, int width, int height,
                           int ss_x, int ss_y, int use_highbitdepth,
                           int border, int byte_alignment);
```

Good:

```cpp
// If ybf currently contains an image, all associated memory will be freed and
// then reallocated. In contrast, aom_realloc_frame_buffer() will reuse any
// existing allocations where possible. So, if ybf is likely to already be
// set up, please consider aom_realloc_frame_buffer() instead.
int aom_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf, int width, int height,
                           int ss_x, int ss_y, int use_highbitdepth,
                           int border, int byte_alignment);
```
