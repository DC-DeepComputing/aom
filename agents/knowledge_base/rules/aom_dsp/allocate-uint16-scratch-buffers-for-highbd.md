---
name: libaom/aom_dsp/allocate-uint16-scratch-buffers-for-highbd
description: When declaring scratch or intermediate pixel buffers in high-bit-depth (highbd) routines, use uint16_t elements instead of uint8_t to prevent buffer overflows.
rubric: safety
severity: blocking
---

**Rule.** When allocating temporary or intermediate pixel buffers in
high-bit-depth (`highbd`) functions, declare the buffer with `uint16_t` (or 2
bytes per pixel) rather than `uint8_t`. Ensure that scratch buffers passed to or
populated by high-bit-depth prediction, convolution, or variance routines
allocate sufficient storage for 16-bit samples.

**Why.** High-bit-depth functions operate on 16-bit sample values. Reusing or
copying buffer declarations from 8-bit low-bit-depth routines using `uint8_t`
allocates only half the required storage, resulting in out-of-bounds writes,
memory corruption, and undefined behavior when functions write 16-bit values
(such as via `CONVERT_TO_BYTEPTR`).

## Examples

Bad:

```cpp
// Bad: Declaring 8-bit scratch buffer for high-bit-depth intermediate output
DECLARE_ALIGNED(16, uint8_t,
                temp[((MAX_SB_SIZE * 2 + 16) + 16) * MAX_SB_SIZE]);
aom_highbd_convolve8_horiz(src, src_stride, CONVERT_TO_BYTEPTR(temp),
                           MAX_SB_SIZE, filter_x, x0_qn, filter_y, y0_qn, w, h,
                           bd);
```

Good:

```cpp
// Good: Declaring 16-bit scratch buffer for high-bit-depth intermediate output
DECLARE_ALIGNED(16, uint16_t,
                temp[((MAX_SB_SIZE * 2 + 16) + 16) * MAX_SB_SIZE]);
aom_highbd_convolve8_horiz(src, src_stride, CONVERT_TO_BYTEPTR(temp),
                           MAX_SB_SIZE, filter_x, x0_qn, filter_y, y0_qn, w, h,
                           bd);
```
