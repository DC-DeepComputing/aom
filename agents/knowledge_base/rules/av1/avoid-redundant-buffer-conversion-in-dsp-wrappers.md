---
name: libaom/av1/avoid-redundant-buffer-conversion-in-dsp-wrappers
description: When intermediate buffers are already allocated and format-converted, invoke internal processing kernels directly instead of calling wrappers that duplicate allocation and conversion
rubric: safety
severity: should_fix
---

**Rule.** Do not invoke higher-level DSP or filter wrapper functions that
perform internal buffer allocation and format conversion when the caller has
already prepared, converted, or padded the intermediate buffer. Call the
lower-level internal core routines directly, passing the pre-allocated
intermediate buffers.

**Why.** Higher-level wrapper functions often allocate local stack or scratch
buffers and repeat format conversions (such as 8-bit to 16-bit unpacking, high
bit-depth copies, or border extension). Calling wrappers when intermediate
buffers are already prepared causes redundant memory passes and unnecessary
allocations, degrading pipeline throughput.

## Examples

Bad:

```cpp
if (highbd) {
  src_convert_hbd_copy(src, src_stride, dgd16, dgd16_stride, width, height);
} else {
  src_convert_u8_to_u16(src, src_stride, dgd16, dgd16_stride, width, height);
}

// Bad: wrapper re-allocates scratch buffers and repeats src_convert_*
av1_selfguided_restoration_neon(dat8, width, height, stride, flt0, flt1,
                                width, eps, bit_depth, highbd);
```

Good:

```cpp
if (highbd) {
  src_convert_hbd_copy(src, src_stride, dgd16, dgd16_stride, width, height);
} else {
  src_convert_u8_to_u16(src, src_stride, dgd16, dgd16_stride, width, height);
}

// Good: invoke internal core kernels directly on the pre-converted buffer
if (params->r[0] > 0) {
  restoration_fast_internal(dgd16, width, height, dgd16_stride, flt0, width,
                            bit_depth, eps, 0);
}
if (params->r[1] > 0) {
  restoration_internal(dgd16, width, height, dgd16_stride, flt1, width,
                       bit_depth, eps, 1);
}
```
