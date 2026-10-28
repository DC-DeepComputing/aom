---
name: libaom/track-allocated-buffer-sizes-for-realloc
description: When managing cached or persistent buffers across frame or sequence updates, track allocated buffer sizes directly rather than caching upstream header flags or irrelevant dimensions
rubric: design
severity: should_fix
---

**Rule.** When managing dynamically sized, cached, or persistent buffers (such
as line buffers, column buffers, or worker scratch memory) across frame,
sequence, or tile configuration updates, track the allocated buffer size or
capacity directly in the owning struct rather than caching upstream header
flags, syntax elements, or dimensions to detect reallocation needs. Determine
whether to reallocate by comparing the newly computed required buffer size
against the tracked allocated size. Additionally, when tracking individual
dimension parameters instead of byte sizes, only track and compare parameters
that strictly determine the buffer's dimensions, avoiding irrelevant
configuration fields (such as vertical subsampling for horizontal line buffers).

**Why.** Caching upstream syntax flags or independent configuration parameters
(such as subsampling_y for horizontal buffers or enable flags) creates fragile,
tightly coupled memory management logic that requires updates whenever syntax
elements change buffer sizing. It also causes spurious deallocations and
reallocations when unrelated parameters change, degrading performance and
increasing memory fragmentation. Tracking allocated buffer sizes directly
simplifies reallocation checks, avoids stale flag synchronization bugs, and
guarantees memory is resized only when required capacity changes.

## Examples

Bad:

```cpp
// Anti-pattern 1: Checking multiple cached syntax flags / upstream header fields
if (cm->subsampling_x != cdef_info->allocated_subsampling_x ||
    cm->seq_params.enable_cdef != cdef_info->prev_cdef_enable_flag) {
  realloc_linebufs(cm);
}

// Anti-pattern 2: Triggering reallocation on vertical subsampling changes
// when horizontal line buffer sizing only depends on width / subsampling_x
const int is_sub_sampling_changed =
    (cdef_info->allocated_subsampling_x != cm->seq_params.subsampling_x ||
     cdef_info->allocated_subsampling_y != cm->seq_params.subsampling_y);
if (is_sub_sampling_changed) {
  av1_free_cdef_linebuf(cm);
}
```

Good:

```cpp
// Preferred: Compare newly computed required size directly against tracked allocation
for (int plane = 0; plane < num_planes; ++plane) {
  if (new_linebuf_size[plane] != cdef_info->allocated_linebuf_size[plane]) {
    aom_free(cdef_info->linebuf[plane]);
    cdef_info->linebuf[plane] = aom_malloc(new_linebuf_size[plane]);
    cdef_info->allocated_linebuf_size[plane] = new_linebuf_size[plane];
  }
}

// Alternative (when size is dimension-based): Compare only relevant dimensions
const int is_sub_sampling_changed =
    cdef_info->allocated_subsampling_x != cm->seq_params.subsampling_x;
if (is_sub_sampling_changed) {
  av1_free_cdef_linebuf(cm);
}
```
