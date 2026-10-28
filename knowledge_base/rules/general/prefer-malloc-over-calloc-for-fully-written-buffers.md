---
name: libaom/prefer-malloc-over-calloc-for-fully-written-buffers
description: When dynamically allocating buffers that are immediately overwritten or fully initialized, use aom_malloc instead of aom_calloc to avoid redundant zeroing
rubric: safety
severity: should_fix
---

**Rule.** Prefer `aom_malloc()` (or `malloc()`) over `aom_calloc()` (or
`calloc()`) when dynamically allocating buffers that are immediately and
completely populated, initialized, or overwritten (e.g., via initialization
functions, `memset`, `memcpy`, or explicit assignment) before being read.
Reserve `aom_calloc()` for structures or buffers that genuinely require
zero-initialized fields or are partially written before reading.

**Why.** `aom_calloc()` unconditionally zero-initializes the allocated memory
block. When a buffer is immediately overwritten or populated by initialization
logic before being read, the initial zeroing performed by `aom_calloc()` is
redundant and incurs unnecessary CPU overhead, especially on
performance-sensitive encoding or decoding paths.

## Examples

Bad:

```cpp
// Bad: Redundant calloc zero-initialization before custom buffer initialization
CHECK_MEM_ERROR(cm, hbuf, (int16_t *)aom_calloc(search_width, sizeof(*hbuf)));
init_search_buffer(hbuf, search_width);

// Bad: calloc zeroing when buffer is immediately overwritten via memset/memcpy
CHECK_MEM_ERROR(cm, cpi->consec_zero_mv,
                aom_calloc(current_size, sizeof(*cpi->consec_zero_mv)));
memset(cpi->consec_zero_mv, 0, current_size * sizeof(*cpi->consec_zero_mv));
```

Good:

```cpp
// Good: Use aom_malloc when the buffer is immediately populated before reading
CHECK_MEM_ERROR(cm, hbuf, (int16_t *)aom_malloc(search_width * sizeof(*hbuf)));
init_search_buffer(hbuf, search_width);

// Good: Use aom_malloc when buffer is immediately initialized or overwritten
CHECK_MEM_ERROR(cm, cpi->consec_zero_mv,
                aom_malloc(current_size * sizeof(*cpi->consec_zero_mv)));
memset(cpi->consec_zero_mv, 0, current_size * sizeof(*cpi->consec_zero_mv));
```
