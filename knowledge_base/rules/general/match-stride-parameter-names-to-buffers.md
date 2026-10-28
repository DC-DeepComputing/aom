---
name: libaom/match-stride-parameter-names-to-buffers
description: When declaring functions that accept buffer pointers and strides, name each stride parameter consistently with its corresponding buffer pointer parameter
rubric: readability
severity: nit
---

**Rule.** When declaring or defining functions that accept buffer pointers and
their associated stride values, name each stride parameter consistently with its
corresponding buffer parameter (e.g., `<buffer>_stride` for `<buffer>`, or
`stride1` for `frame1`/`src1`). Do not mix generic names (like `dst_stride` or
`stride`) with specific buffer names (like `frame_abs_diff`), or use mismatched
numbering across buffer-stride pairs.

**Why.** Consistent parameter naming pairs make multi-buffer function signatures
self-documenting and prevent confusion at call sites or during refactoring about
which stride applies to which buffer.

## Examples

Bad:

```cpp
static INLINE void get_abs_diff(const uint16_t *frame1, const uint32_t stride1,
                                const uint16_t *frame2, const uint32_t stride2,
                                const uint32_t block_width,
                                const uint32_t block_height,
                                uint16_t *frame_abs_diff,
                                const unsigned int dst_stride);
```

Good:

```cpp
static INLINE void get_abs_diff(const uint16_t *frame1, const uint32_t stride1,
                                const uint16_t *frame2, const uint32_t stride2,
                                const uint32_t block_width,
                                const uint32_t block_height,
                                uint16_t *frame_abs_diff,
                                const unsigned int frame_abs_diff_stride);
```
