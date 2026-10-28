---
name: libaom/assert-invariants-before-unsigned-subtraction
description: When calculating remaining buffer space with unsigned subtraction, assert and document caller invariants (used_size <= buffer_size) to prevent underflow wraparound
rubric: correctness
severity: should_fix
---

**Rule.** When computing remaining buffer capacity, payload limits, or offsets
using unsigned subtraction (such as `buffer_size - used_size`), ensure that the
subtrahend cannot exceed the total size. If the calculation relies on callers or
prior pipeline stages to guarantee non-negative remaining space (`used_size <=
buffer_size`) rather than handling dynamic error return paths, explicitly
document the assumption in a comment and enforce it with `assert()` prior to the
subtraction.

**Why.** Unsigned integer subtraction underflows and wraps around to large
positive values when the subtrahend exceeds the total size, which silently
bypasses downstream bounds checks and causes buffer overflows. Explicit comments
and debug assertions clarify caller contracts and catch invalid size invariants
in debug builds.

## Examples

Bad:

```cpp
int av1_convert_sect5obus_to_annexb(uint8_t *buffer, size_t buffer_sz,
                                    size_t *frame_size) {
  size_t remaining_size = *frame_size;
  // ...
  // Unsigned subtraction underflows if remaining_size exceeds buffer_sz.
  if (needed_size > buffer_sz - remaining_size - output_size) {
    return AOM_CODEC_ERROR;
  }
  return AOM_CODEC_OK;
}
```

Good:

```cpp
int av1_convert_sect5obus_to_annexb(uint8_t *buffer, size_t buffer_size,
                                    size_t *frame_size) {
  // Caller invariant: '*frame_size <= buffer_size', otherwise the input buffer
  // is invalid or overconsumed.
  assert(*frame_size <= buffer_size);
  size_t remaining_size = *frame_size;
  // ...
  if (needed_size > buffer_size - remaining_size - output_size) {
    return AOM_CODEC_ERROR;
  }
  return AOM_CODEC_OK;
}
```
