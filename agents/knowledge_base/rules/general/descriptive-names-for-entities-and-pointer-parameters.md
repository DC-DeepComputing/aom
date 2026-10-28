---
name: libaom/descriptive-names-for-entities-and-pointer-parameters
description: When naming functions and pointer parameters, avoid generic identifiers like buffer or ptr; use specific names identifying domain entities and pointee roles
rubric: readability
severity: should_fix
---

**Rule.** Avoid overly generic identifiers (such as assign_buffer, process_data,
or bare parameter ptr) for functions and pointer parameters. Use specific names
that identify the concrete domain entity being manipulated (such as
assign_frame_buffer) and clearly communicate what pointer parameters reference
or output (such as idx_ptr).

**Why.** Generic names like buffer, data, or ptr are ambiguous across subsystems
with multiple buffer types and indirection levels. Explicit entity and parameter
names make domain concepts and pointee roles immediately clear without
inspecting function internals.

## Examples

Bad:

```cpp
static INLINE void assign_buffer(RefCntBuffer *bufs, int *ptr, int new_idx) {
  const int old_idx = *ptr;
  if (old_idx >= 0) {
    bufs[old_idx].ref_count--;
  }
  *ptr = new_idx;
  if (new_idx >= 0) {
    bufs[new_idx].ref_count++;
  }
}
```

Good:

```cpp
static INLINE void assign_frame_buffer(RefCntBuffer *bufs, int *idx_ptr,
                                       int new_idx) {
  const int old_idx = *idx_ptr;
  if (old_idx >= 0) {
    bufs[old_idx].ref_count--;
  }
  *idx_ptr = new_idx;
  if (new_idx >= 0) {
    bufs[new_idx].ref_count++;
  }
}
```
