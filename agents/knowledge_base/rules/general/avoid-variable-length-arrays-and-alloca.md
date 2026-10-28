---
name: libaom/avoid-variable-length-arrays-and-alloca
description: When allocating memory or local buffers, do not use variable-length arrays (VLAs) or alloca(); use fixed-capacity stack buffers or aom_malloc/aom_memalign
rubric: correctness
severity: blocking
---

**Rule.** Do not use variable-length arrays (VLAs, e.g. `type
buf[non_const_len]`) or `alloca()` for local stack allocations. Use compile-time
fixed-size stack buffers sized to the worst-case dimension, or allocate dynamic
heap memory using `aom_malloc()` or `aom_memalign()` and release it with
`aom_free()`.

**Why.** Variable-length arrays are optional in C11 and not supported by MSVC or
standard C++, causing compilation failures in MSVC and C++ builds. Furthermore,
both VLAs and `alloca()` risk stack overflow crashes when operating on large or
unexpected input dimensions, and `alloca()` has compiler-specific portability
caveats.

## Examples

Bad:

```cpp
// Variable-length array breaks MSVC/C++ builds and risks stack overflow
uint16_t im_block[im_h * im_stride];
```

Good:

```cpp
// Use libaom dynamic heap allocation or a fixed-capacity buffer
uint16_t *im_block = (uint16_t *)aom_memalign(16, size * sizeof(*im_block));
if (!im_block) return;
// ... process im_block ...
aom_free(im_block);
```
