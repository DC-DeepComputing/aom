---
name: libaom/pass-struct-descriptors-by-pointer
description: When defining functions that accept struct buffer descriptors or multi-member structs, pass them by pointer rather than by value or single-element array notation
rubric: safety
severity: should_fix
---

**Rule.** When passing multi-member structs or buffer descriptors (such as
struct buf_2d) to functions, pass them by pointer (e.g., const struct buf_2d *
or struct buf_2d *) rather than by value or using single-element array notation
(such as buf[0] or buf[1]). Ensure callers pass the address (&buf or
&buf[index]) and callees access members using pointer dereference syntax
(ptr->member).

**Why.** Passing structs by value introduces unnecessary memory copying overhead
on every call. Using single-element array notation in parameter lists creates
misleading signatures that obscure pointer semantics and make caller and callee
contracts harder to understand.

## Examples

Bad:

```cpp
static void set_block_source_sad(AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
                                 struct buf_2d yv12_mb[0]) {
  struct macroblock_plane *const p = &x->plane[0];
  const int y_sad = cpi->ppi->fn_ptr[bsize].sdf(
      p->src.buf, p->src.stride, yv12_mb[0].buf, yv12_mb[0].stride);
  if (y_sad == 0) x->block_is_zero_sad = 1;
}
```

Good:

```cpp
static void set_block_source_sad(AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize,
                                 const struct buf_2d *yv12_mb) {
  struct macroblock_plane *const p = &x->plane[0];
  const int y_sad = cpi->ppi->fn_ptr[bsize].sdf(
      p->src.buf, p->src.stride, yv12_mb->buf, yv12_mb->stride);
  if (y_sad == 0) x->block_is_zero_sad = 1;
}
```
