---
name: libaom/av1/consolidate-bitdepth-buffer-pointers-in-branches
description: When setting up bitdepth-specific buffers across is_high_bitdepth branches, assign a unified byte pointer inside branches rather than repeating ternary expressions at call sites
rubric: readability
severity: nit
---

**Rule.** When preparing bitdepth-specific buffers across if (is_high_bitdepth)
or use_highbitdepth branches, assign a unified byte pointer (uint8_t * or const
uint8_t *, using CONVERT_TO_BYTEPTR for high bitdepth) directly inside each
branch. Do not repeat bitdepth checks with conditional ternary expressions
(is_high_bitdepth ? CONVERT_TO_BYTEPTR(buf16) : buf8) across downstream DSP or
kernel call sites.

**Why.** Consolidating bitdepth-dependent pointer assignment alongside buffer
initialization (such as memset) centralizes buffer setup in one place. This
eliminates redundant ternary condition evaluations and reduces visual clutter at
downstream function calls.

## Examples

Bad:

```cpp
if (is_high_bitdepth) {
  memset(dclevel16, 0, sizeof(dclevel16));
} else {
  memset(dclevel8, 0, sizeof(dclevel8));
}

src_var = cpi->ppi->fn_ptr[block_size].vf(
    mb->plane[0].src.buf, y_stride,
    is_high_bitdepth ? CONVERT_TO_BYTEPTR(dclevel16) : dclevel8,
    dclevel_stride, &sse);
```

Good:

```cpp
uint8_t *dclevel;
if (is_high_bitdepth) {
  memset(dclevel16, 0, sizeof(dclevel16));
  dclevel = CONVERT_TO_BYTEPTR(dclevel16);
} else {
  memset(dclevel8, 0, sizeof(dclevel8));
  dclevel = dclevel8;
}

src_var = cpi->ppi->fn_ptr[block_size].vf(
    mb->plane[0].src.buf, y_stride, dclevel, dclevel_stride, &sse);
```
