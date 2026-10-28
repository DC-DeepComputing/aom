---
name: libaom/name-data-transfer-helpers-with-action-and-direction
description: When naming I/O, buffering, or data-transfer helper functions, use explicit action verbs and data-flow direction rather than ambiguous or fragmented phrases
rubric: readability
severity: should_fix
---

**Rule.** Name I/O, buffering, and data-transfer helper functions using clear
action verbs and explicit source-to-destination direction (such as
input_to_detect_buf, read_from_stream, or write_to_buffer). Avoid cryptic,
fragmented, or directionless names (such as detect_buf_in or stream_data_buf)
where the data source, destination, or transfer operation is ambiguous.

**Why.** I/O and buffering helpers frequently move bytes between external
streams and internal staging buffers. Ambiguous or directionless names obscure
whether a function reads from or writes into a buffer, confusing callers and
increasing the risk of inverted argument passing or incorrect data-flow
assumptions.

## Examples

Bad:

```cpp
// Ambiguous naming: unclear whether data is read from or written into the buffer.
size_t detect_buf_in(struct AvxInputContext *input_ctx, size_t n);
int stream_data_buf(uint8_t *dst, struct StreamContext *ctx, size_t len);
```

Good:

```cpp
// Explicit source, destination, and transfer action.
size_t input_to_detect_buf(struct AvxInputContext *input_ctx, size_t n);
int read_from_stream_to_buf(uint8_t *dst, struct StreamContext *ctx, size_t len);
```
