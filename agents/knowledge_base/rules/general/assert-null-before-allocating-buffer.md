---
name: libaom/assert-null-before-allocating-buffer
description: When dynamically allocating memory into struct pointer members, assert that destination pointers are NULL before allocation to prevent buffer aliasing and leaks
rubric: safety
severity: should_fix
---

**Rule.** In dynamic buffer allocation functions, assert (e.g. with assert(!ptr)
or assert(ptr == NULL)) that destination struct pointer members are NULL before
allocating new memory to them. Ensure corresponding deallocation routines reset
freed pointer members to NULL so debug assertions detect stale, unreleased, or
aliased buffers.

**Why.** When structs containing dynamically allocated buffers are
shallow-copied, reused across worker threads, or repeatedly initialized,
allocating memory over non-NULL pointers silently leaks memory or causes
double-free errors if aliased pointers are freed elsewhere. Precondition
assertions catch missing deallocations and accidental buffer aliasing early in
debug builds.

## Examples

Bad:

```cpp
void av1_alloc_src_diff_buf(const struct AV1Common *cm, struct macroblock *mb) {
  const int num_planes = av1_num_planes(cm);
  for (int plane = 0; plane < num_planes; ++plane) {
    // Missing NULL assertion: stale or aliased pointers will be overwritten
    CHECK_MEM_ERROR(cm, mb->plane[plane].src_diff,
                    aom_memalign(32, sizeof(*mb->plane[plane].src_diff) * sb_size));
  }
}
```

Good:

```cpp
void av1_alloc_src_diff_buf(const struct AV1Common *cm, struct macroblock *mb) {
  const int num_planes = av1_num_planes(cm);
  for (int plane = 0; plane < num_planes; ++plane) {
    assert(!mb->plane[plane].src_diff);
    CHECK_MEM_ERROR(cm, mb->plane[plane].src_diff,
                    aom_memalign(32, sizeof(*mb->plane[plane].src_diff) * sb_size));
  }
}
```
