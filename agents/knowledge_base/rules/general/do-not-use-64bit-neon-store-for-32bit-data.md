---
name: libaom/do-not-use-64bit-neon-store-for-32bit-data
description: When storing 32-bit data with ARM NEON, avoid 64-bit vector stores like vst1_u8; use vst1_lane_u32 or packed scalar stores to prevent buffer overruns.
rubric: safety
severity: blocking
---

**Rule.** Do not use 64-bit ARM NEON vector store intrinsics (such as vst1_u8,
vst1_u16, or vst1_u32) to write 32-bit or sub-64-bit data. Use single-lane store
intrinsics (such as vst1_lane_u32) or unaligned/packed scalar stores to avoid
writing past the target buffer.

**Why.** vst1_* intrinsics without _lane operate on 64-bit vector registers
(such as uint8x8_t) and write 8 bytes to memory. Using them to store a 32-bit
(4-byte) value overruns the destination buffer by 4 bytes, risking memory
corruption.

## Examples

Bad:

```cpp
SIMD_INLINE void u32_store_unaligned(void *p, uint32_t a) {
  // vst1_u8 writes 8 bytes, overrunning a 4-byte destination buffer
  vst1_u8((uint8_t *)p, vreinterpret_u8_u64(a));
}
```

Good:

```cpp
SIMD_INLINE void u32_store_unaligned(void *p, uint32_t a) {
#if __CC_ARM
  *(__packed uint32_t *)p = a;
#elif __GNUC__
  *((__attribute((packed)) uint32_t *)p) = a;
#else
  vst1_lane_u32((uint32_t *)p, vreinterpret_u32_s64(a), 0);
#endif
}
```
