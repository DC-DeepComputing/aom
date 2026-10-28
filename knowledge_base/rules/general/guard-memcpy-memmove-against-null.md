---
name: libaom/guard-memcpy-memmove-against-null
description: When copying memory with memcpy or memmove where pointers may be NULL, guard the operation with a non-zero count or non-null check
rubric: safety
severity: should_fix
---

**Rule.** Do not pass a NULL pointer to memcpy() or memmove(), even when the
byte count is zero. When copying dynamically allocated or optional buffers that
may be NULL when empty, explicitly guard the call with a non-zero count check
(e.g., if (count != 0)) or a non-null pointer check.

**Why.** The C and C++ standards specify that passing a NULL pointer to memcpy()
or memmove() is undefined behavior, even if the byte count is zero. Compilers
and sanitizers (such as UBSan) may optimize based on the assumption that pointer
arguments are non-null or report runtime errors.

## Examples

Bad:

```cpp
if (num_corners <= MAX_CORNERS) {
  // Undefined behavior if frame_corners_xy is NULL when num_corners is 0:
  memcpy(corners->corners, frame_corners_xy,
         sizeof(*frame_corners_xy) * num_corners);
  corners->num_corners = num_corners;
}
```

Good:

```cpp
if (num_corners <= MAX_CORNERS) {
  if (num_corners != 0) {
    memcpy(corners->corners, frame_corners_xy,
           sizeof(*frame_corners_xy) * num_corners);
  }
  corners->num_corners = num_corners;
}
```
