---
name: libaom/av1/scale-high-bitdepth-buffer-size-by-shift
description: When allocating and setting up temporary scratch prediction or reconstruction buffers, scale sizes with << is_high_bitdepth and wrap uint16_t pointers using CONVERT_TO_BYTEPTR
rubric: correctness
severity: should_fix
---

**Rule.** When allocating and setting up temporary scratch prediction or
reconstruction buffers, support high bit-depth mode (e.g. via is_cur_buf_hbd()
or seq_params->use_highbitdepth). Compute buffer size by bit-shifting base byte
dimensions (base_size << is_high_bitdepth) rather than duplicating dimension
math across ternary branches, and convert uint16_t* pointers using
CONVERT_TO_BYTEPTR before passing destination or prediction buffers to pixel
processing routines.

**Why.** High bit-depth pixel processing operates on 16-bit (2-byte) samples.
Failing to double buffer allocation sizes causes buffer overflows, omitting
CONVERT_TO_BYTEPTR causes invalid pointer math when routines unpack via
CONVERT_TO_SHORTPTR, and duplicating dimension arithmetic in ternary branches
adds redundant code prone to divergence.

## Examples

Bad:

```cpp
// Misses high bit-depth allocation and pointer conversion, or duplicates math in ternary branches
const size_t buf_size =
    is_high_bitdepth ? buf_width * buf_height * sizeof(*pred_buffer) * 2
                     : buf_width * buf_height * sizeof(*pred_buffer);
CHECK_MEM_ERROR(cm, pred_buffer, aom_memalign(32, buf_size));
uint8_t *dst_buffer = pred_buffer + buf_width * offset + offset;
```

Good:

```cpp
// Scaled cleanly with bit-shift and wrapped with CONVERT_TO_BYTEPTR for high bit-depth
const int is_high_bitdepth = is_cur_buf_hbd(xd);
const size_t buf_size =
    (buf_width * buf_height * sizeof(*pred_buffer)) << is_high_bitdepth;
CHECK_MEM_ERROR(cm, pred_buffer, aom_memalign(32, buf_size));
uint8_t *dst_buffer = pred_buffer;
if (is_high_bitdepth) {
  dst_buffer = CONVERT_TO_BYTEPTR((uint16_t *)pred_buffer);
}
```
