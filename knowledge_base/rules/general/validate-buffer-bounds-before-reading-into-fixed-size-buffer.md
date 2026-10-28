---
name: libaom/validate-buffer-bounds-before-reading-into-fixed-size-buffer
description: When reading or buffering data into fixed-size buffers, validate that the requested size plus current offset does not exceed capacity
rubric: safety
severity: should_fix
---

**Rule.** When reading, copying, or buffering data into a fixed-size buffer
(such as via fread, read, or memcpy), validate that the requested read size plus
the current buffer position or offset does not exceed the buffer's capacity (or
assert bounds if callers guarantee it). Terminate or return an error before
performing the I/O or copy operation if the bounds are exceeded.

**Why.** Reading or copying data into a fixed-size buffer without validating the
remaining capacity risks writing past the buffer boundary, causing heap or stack
buffer overflows, memory corruption, and security vulnerabilities.

## Examples

Bad:

```cpp
size_t input_to_detect_buf(struct AvxInputContext *input_ctx, size_t n) {
  const size_t buffered_bytes =
      input_ctx->detect.buf_read - input_ctx->detect.position;
  size_t read_n;
  if (buffered_bytes == 0) {
    // Missing check: n + input_ctx->detect.position can exceed DETECT_BUF_SZ
    read_n = fread(input_ctx->detect.buf + input_ctx->detect.buf_read, 1, n,
                   input_ctx->file);
    input_ctx->detect.buf_read += read_n;
    ...
  }
}
```

Good:

```cpp
size_t input_to_detect_buf(struct AvxInputContext *input_ctx, size_t n) {
  if (n + input_ctx->detect.position > DETECT_BUF_SZ) {
    die("Failed to store in the detect buffer, maximum size exceeded.");
  }
  const size_t buffered_bytes =
      input_ctx->detect.buf_read - input_ctx->detect.position;
  size_t read_n;
  ...
}
```
