---
name: libaom/pass-explicit-buffer-sizes-for-pointer-arguments
description: When defining functions accepting raw pointer buffers or caller-allocated destination arrays, pass explicit buffer size or capacity parameters for bounds validation
rubric: safety
severity: should_fix
---

**Rule.** When declaring or defining functions that operate on raw pointer
buffers, dynamically sized arrays, or caller-allocated destination/output
buffers, pass explicit buffer size, capacity, or element count parameters (e.g.,
`size_t buffer_size` or `size_t pic_block_hash_size`) alongside the pointer
arguments. Callee implementations should use the provided capacity parameters to
assert preconditions and validate bounds against buffer overruns.

**Why.** In C, raw pointers do not carry length or capacity metadata. Passing
explicit buffer sizes or capacities allows callees writing into or reading from
caller-allocated buffers to perform bounds checking or assertions, preventing
buffer overflows and memory corruption if caller allocation assumptions change.

## Examples

Bad:

```cpp
// Bad: Lacks buffer capacity for caller-allocated destination buffer
void av1_generate_block_2x2_hash_value(const YV12_BUFFER_CONFIG *picture,
                                       uint32_t *pic_block_hash);

// Bad: Lacks explicit buffer capacity or size parameter for output array
void av1_generate_block_hash_value(IntraBCHashInfo *intra_bc_hash_info,
                                   const YV12_BUFFER_CONFIG *picture,
                                   int block_size,
                                   const uint32_t *src_pic_block_hash,
                                   uint32_t *dst_pic_block_hash);
```

Good:

```cpp
// Good: Passes buffer capacity to allow destination bounds validation
void av1_generate_block_2x2_hash_value(const YV12_BUFFER_CONFIG *picture,
                                       uint32_t *pic_block_hash,
                                       size_t pic_block_hash_size);

// Good: Passes explicit buffer size to validate bounds
void av1_generate_block_hash_value(IntraBCHashInfo *intra_bc_hash_info,
                                   const YV12_BUFFER_CONFIG *picture,
                                   int block_size,
                                   const uint32_t *src_pic_block_hash,
                                   uint32_t *dst_pic_block_hash,
                                   size_t pic_block_hash_size);
```
