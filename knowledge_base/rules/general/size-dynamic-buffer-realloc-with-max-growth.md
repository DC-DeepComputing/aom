---
name: libaom/size-dynamic-buffer-realloc-with-max-growth
description: When dynamically resizing buffers to accommodate incoming data, size the reallocation using the maximum of geometric growth and total required capacity
rubric: safety
severity: nit
---

**Rule.** When resizing a dynamic buffer geometrically to accommodate incoming
data, calculate the new capacity as the maximum of the geometric expansion and
the total required size (e.g., using AOMMAX), rather than adding the chunk size
onto the expanded capacity.

**Why.** Adding incoming chunk size directly on top of a geometrically expanded
capacity over-allocates redundant memory on every resize. Taking AOMMAX of the
geometric expansion and the required size preserves amortized constant-time
growth for small additions while safely accommodating large chunks without
excess allocation.

## Examples

Bad:

```cpp
if (stats->buf.sz + len > stats->buf_alloc_sz) {
  size_t new_sz = (3 * stats->buf_alloc_sz) / 2 + len;
  void *new_buf = realloc(stats->buf.buf, new_sz);
  if (!new_buf) return 0;
  stats->buf.buf = new_buf;
  stats->buf_alloc_sz = new_sz;
}
```

Good:

```cpp
if (stats->buf.sz + len > stats->buf_alloc_sz) {
  size_t new_sz = AOMMAX((3 * stats->buf_alloc_sz) / 2, stats->buf.sz + len);
  void *new_buf = realloc(stats->buf.buf, new_sz);
  if (!new_buf) return 0;
  stats->buf.buf = new_buf;
  stats->buf_alloc_sz = new_sz;
}
```
