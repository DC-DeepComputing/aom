---
name: libaom/use-aom-malloc-and-free
description: When allocating, freeing, or resizing dynamic heap memory, use libaom wrappers (aom_malloc, aom_calloc, aom_memalign, aom_free) instead of stdlib functions, and verify pointers against NULL
rubric: safety
severity: should_fix
---

**Rule.** When dynamically allocating, resizing, or freeing heap memory, use
libaom's memory wrappers (aom_malloc, aom_calloc, aom_memalign, and aom_free)
declared in aom_mem/aom_mem.h rather than standard C library functions (malloc,
calloc, realloc, free, posix_memalign, aligned_alloc). Always check allocation
return values for NULL (or use AOM_CHECK_MEM_ERROR) before dereferencing. Never
mix standard C allocators with aom_* wrappers. Because libaom lacks an
aom_realloc API, handle buffer resizing explicitly by allocating a new buffer
with aom_malloc, copying data if needed, and releasing the old buffer with
aom_free.

**Why.** libaom allocation wrappers enforce SIMD memory alignment (such as
32-byte alignment required for vector instructions), validate allocation sizes
against AOM_MAX_ALLOCABLE_MEMORY to prevent integer overflow, and support custom
allocator hooks. Mixing standard C memory functions with aom_* wrappers or
calling standard free on an aom_malloc pointer causes heap corruption because
aom_memalign and aom_malloc store internal alignment metadata and offset
headers. Checking returned pointers against NULL prevents undefined behavior and
null-pointer dereferences upon allocation failure.

## Examples

Bad:

```cpp
// Anti-pattern 1: Using standard malloc/free without NULL check
uint8_t *buf = (uint8_t *)malloc(width * height * sizeof(*buf));
for (int i = 0; i < width * height; ++i) buf[i] = 0;
free(buf);

// Anti-pattern 2: Using realloc for dynamic buffer resizing
buf2 = (float *)realloc(buf2, new_size * sizeof(*buf2));

// Anti-pattern 3: Mixing standard free with aom_malloc
uint8_t *temp = (uint8_t *)aom_malloc(size);
free(temp);  // Corrupts heap metadata
```

Good:

```cpp
// Recommended 1: Using aom_malloc, checking NULL, and using aom_free
uint8_t *buf = (uint8_t *)aom_malloc(width * height * sizeof(*buf));
if (!buf) return AOM_CODEC_MEM_ERROR;
for (int i = 0; i < width * height; ++i) buf[i] = 0;
aom_free(buf);

// Recommended 2: Explicit buffer resizing without realloc
if (alloc_size < new_size) {
  aom_free(buf2);
  buf2 = (float *)aom_malloc(new_size * sizeof(*buf2));
  if (!buf2) return AOM_CODEC_MEM_ERROR;
  alloc_size = new_size;
}
```
