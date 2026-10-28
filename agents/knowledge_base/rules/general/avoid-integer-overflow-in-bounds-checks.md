---
name: libaom/avoid-integer-overflow-in-bounds-checks
description: When validating buffer bounds, offsets, and copy sizes, structure capacity checks using subtraction (`offset > capacity - size`) instead of addition to prevent integer overflow
rubric: safety
severity: should_fix
---

**Rule.** When checking whether an offset and chunk or copy size exceed a
buffer's capacity (such as before memory writes, memcpy, or memmove), structure
the bounds check using subtraction (e.g., offset > capacity - size after
verifying size <= capacity, or dest_offset > available_capacity - copy_length)
rather than addition (offset + size > capacity). Verify that all subtracted
operands are non-negative and do not underflow.

**Why.** In additive bounds checks (offset + size > capacity), adding two
integer values (such as size_t or uint32_t) can overflow and wrap around,
causing the check to evaluate to false and allowing subsequent memory operations
to write past allocated buffer bounds. Restructuring the comparison using
subtraction from total or available capacity prevents arithmetic overflow
regardless of integer width.

## Examples

Bad:

```cpp
// Additive check can overflow if move_offset + frame_size wraps around.
if (move_offset + cpi_data.frame_size > ctx->cx_data_sz) {
  aom_internal_error(&ppi->error, AOM_CODEC_ERROR, NULL);
}

// Adding header sizes and offsets directly risks integer overflow.
if (length_of_obu_size + obu_header_size - obu_bytes_read >
    buffer_size - remaining_size - output_size) {
  return AOM_CODEC_ERROR;
}
memmove(buff_ptr + length_of_obu_size + obu_header_size,
        buff_ptr + obu_bytes_read, remaining_size - obu_bytes_read);
```

Good:

```cpp
// Check chunk size first, then use subtraction against capacity.
if (cpi_data.frame_size > ctx->cx_data_sz) {
  aom_internal_error(&ppi->error, AOM_CODEC_ERROR, "Buffer overflow");
}
if (move_offset > ctx->cx_data_sz - cpi_data.frame_size) {
  aom_internal_error(&ppi->error, AOM_CODEC_ERROR, "Buffer full");
}

// Restructure using subtraction from available capacity to prevent overflow.
if (length_of_obu_size + obu_header_size >
    buffer_size - output_size - (remaining_size - obu_bytes_read)) {
  return AOM_CODEC_ERROR;
}
memmove(buff_ptr + length_of_obu_size + obu_header_size,
        buff_ptr + obu_bytes_read, remaining_size - obu_bytes_read);
```
