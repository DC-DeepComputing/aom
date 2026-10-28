---
name: libaom/match-buffer-count-and-size-names-to-buffer-identifier
description: Match buffer count and size variable names to the base buffer identifier using standard affixes like num_<buf> or <buf>_alloc_size rather than allocated_size_ prefixes or mismatched qualifiers
rubric: readability
severity: nit
---

**Rule.** When declaring struct members, parameters, or local variables that
store buffer counts, capacities, or allocated sizes, name them consistently with
the corresponding buffer identifier using standard affixes (such as num_<buffer>
for counts, or <buffer>_alloc_size / <buffer>_size for sizes and capacities).
Avoid adding extraneous or mismatched qualifiers not present in the buffer name
(e.g., avoid num_ref_frame_bufs for frame_bufs), and avoid prefixing with
allocated_size_ (e.g., use consec_zero_mv_alloc_size instead of
allocated_size_consec_zero_mv).

**Why.** Deriving count, size, and capacity variable names directly from the
base buffer name using standard prefixes and suffixes (num_*, *_alloc_size,
*_size) clarifies relationships between fields, prevents ambiguity in structs
with multiple buffers, and preserves consistent alphabetical and conceptual
grouping across libaom.

## Examples

Bad:

```cpp
struct BufferPool {
  RefCntBuffer *frame_bufs;
  uint8_t num_ref_frame_bufs;  // Extraneous qualifier 'ref_' not in buffer name
};

struct AV1_COMP {
  uint8_t *consec_zero_mv;
  int allocated_size_consec_zero_mv;  // Prefixed instead of suffixed
};
```

Good:

```cpp
struct BufferPool {
  RefCntBuffer *frame_bufs;
  uint8_t num_frame_bufs;  // Directly matches 'frame_bufs'
};

struct AV1_COMP {
  uint8_t *consec_zero_mv;
  int consec_zero_mv_alloc_size;  // Suffixes base buffer name
};
```
