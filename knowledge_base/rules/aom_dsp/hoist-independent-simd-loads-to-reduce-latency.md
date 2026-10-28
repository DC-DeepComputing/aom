---
name: libaom/aom_dsp/hoist-independent-simd-loads-to-reduce-latency
description: When implementing SIMD kernels with multiple vector loads, group and issue independent loads before operating on the registers to reduce load-to-use latency
rubric: safety
severity: should_fix
---

**Rule.** In SIMD intrinsic routines, group and issue independent vector memory
loads before performing dependent arithmetic, transposition, or bitwise
operations on the loaded registers rather than consuming each vector immediately
after loading it.

**Why.** Consuming loaded vector registers immediately in the following
instruction introduces pipeline stalls due to multi-cycle load-to-use latency.
Hoisting and clustering independent memory loads allows the CPU memory pipeline
to fetch data in parallel and hides memory latency before dependent compute
instructions execute.

## Examples

Bad:

```cpp
const uint8x8_t d0 = vld1_u8(src);
src += src_stride;
const uint8x8_t d1 = vld1_u8(src);
const uint8x8x2_t z0 = vzip_u8(d0, d1);
src += src_stride;
const uint8x8_t d2 = vld1_u8(src);
src += src_stride;
const uint8x8_t d3 = vld1_u8(src);
const uint8x8x2_t z1 = vzip_u8(d2, d3);
```

Good:

```cpp
const uint8x8_t d0 = vld1_u8(src);
src += src_stride;
const uint8x8_t d1 = vld1_u8(src);
src += src_stride;
const uint8x8_t d2 = vld1_u8(src);
src += src_stride;
const uint8x8_t d3 = vld1_u8(src);
src += src_stride;
const uint8x8x2_t z0 = vzip_u8(d0, d1);
const uint8x8x2_t z1 = vzip_u8(d2, d3);
```
