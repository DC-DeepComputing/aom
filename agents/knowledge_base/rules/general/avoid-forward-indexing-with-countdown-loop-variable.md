---
name: libaom/avoid-forward-indexing-with-countdown-loop-variable
description: When writing countdown loops terminating at zero, do not use the loop counter for forward buffer or stride indexing to prevent out-of-bounds access
rubric: correctness
severity: should_fix
---

**Rule.** Do not use a countdown loop variable (such as in `do { ... } while
(--i != 0);`) as a forward array index or row stride multiplier (`&buf[i *
stride]`). When refactoring loops to count down towards zero, advance buffer
pointers directly per iteration (`buf += stride;`) or adjust indexing to avoid
out-of-bounds access at row `h` and skipping row `0`.

**Why.** In countdown loops starting at `h` and terminating at zero, using the
counter directly as an array index accesses row `h` (which is out of bounds for
an `h`-row buffer) and never accesses row `0`. Stepping buffer pointers directly
ensures correct forward traversal while retaining efficient decrement-and-branch
loop termination.

## Examples

Bad:

```cpp
int i = h;
do {
  const uint16x8_t dst_16x8 = vmovl_u8(vld1_u8(&dst[i * dstride]));
  const uint16x8_t src_16x8 = vld1q_u16(&src[i * sstride]);
  COMPUTE_MSE_16BIT(src_16x8, dst_16x8)
} while (--i != 0);
```

Good:

```cpp
const uint8_t *dst_ptr = dst;
const uint16_t *src_ptr = src;
int i = h;
do {
  const uint16x8_t dst_16x8 = vmovl_u8(vld1_u8(dst_ptr));
  const uint16x8_t src_16x8 = vld1q_u16(src_ptr);
  COMPUTE_MSE_16BIT(src_16x8, dst_16x8)
  dst_ptr += dstride;
  src_ptr += sstride;
} while (--i != 0);
```
