---
name: libaom/name-buffer-addition-helpers-with-add
description: When naming buffer manipulation or DSP helpers that add residual samples to destination buffers, use add or accumulate instead of write
rubric: readability
severity: should_fix
---

**Rule.** Name buffer manipulation and DSP helper functions according to the
exact operation performed: use 'add' (or 'accumulate') rather than 'write' (or
'store') when reading existing destination buffer samples, adding residual or
transform values, and storing the accumulated result back. Reserve 'write',
'store', or 'put' for functions that unconditionally overwrite destination
buffers without reading existing contents.

**Why.** Naming an accumulation helper with 'write' or 'store' falsely implies
that destination buffer contents are completely replaced, obscuring the
read-modify-write dependency and risking data hazards or incorrect usage in
place of direct assignment.

## Examples

Bad:

```cpp
// Misleadingly named 'write' despite reading, adding, and storing back to output
static INLINE void lowbd_write_buffer_8xn_neon(int16x8_t *in, uint8_t *output,
                                               int stride, int flipud,
                                               const int height) {
  for (int i = 0; i < height; ++i) {
    uint8x8_t out_u8 = vld1_u8(output);
    int16x8_t out_s16 = vreinterpretq_s16_u16(vmovl_u8(out_u8));
    out_s16 = vaddq_s16(out_s16, in[i]);
    vst1_u8(output, vqmovun_s16(out_s16));
    output += stride;
  }
}
```

Good:

```cpp
// Accurately named 'add' to reflect the read-modify-write accumulation
static INLINE void lowbd_add_buffer_8xn_neon(int16x8_t *in, uint8_t *output,
                                             int stride, int flipud,
                                             const int height) {
  for (int i = 0; i < height; ++i) {
    uint8x8_t out_u8 = vld1_u8(output);
    int16x8_t out_s16 = vreinterpretq_s16_u16(vmovl_u8(out_u8));
    out_s16 = vaddq_s16(out_s16, in[i]);
    vst1_u8(output, vqmovun_s16(out_s16));
    output += stride;
  }
}
```
