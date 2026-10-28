---
name: libaom/use-size-t-arithmetic-for-buffer-allocations
description: When declaring or calculating buffer and allocation sizes, use size_t variables and size_t arithmetic to prevent integer overflow and truncation
rubric: correctness
severity: should_fix
---

**Rule.** When declaring variables that store buffer sizes, byte counts, or
allocation sizes, and when calculating them from dimensions (such as width,
height, stride, or element counts with sizeof), use size_t rather than signed
32-bit integer types (int, int32_t). Furthermore, ensure all intermediate
multiplications are evaluated using size_t arithmetic by declaring dimensions as
size_t or explicitly casting at least one operand to (size_t) before
multiplying, rather than performing 32-bit signed arithmetic prior to assignment
or passing to allocators (such as aom_malloc, aom_calloc, aom_memalign, malloc).

**Why.** Memory allocation routines (e.g. aom_malloc, aom_memalign, malloc) and
the sizeof operator operate natively on size_t. Using size_t for size variables
ensures cross-platform consistency and prevents signed/unsigned comparison or
conversion warnings. When intermediate multiplication is performed using 32-bit
signed int (e.g., height * stride or count * sizeof(...)), arithmetic overflows
32-bit signed integers for large frame dimensions or high bit depths before
being assigned or passed to an allocator, causing undefined behavior, integer
truncation, and hazardous buffer under-allocations.

## Examples

Bad:

```cpp
// Intermediate multiplication overflows 32-bit int before assignment:
const int stride_argb = width * 4;
const size_t buffer_size = height * stride_argb;
uint8_t *buf = (uint8_t *)aom_malloc(buffer_size);

// Declaring allocation byte size as int:
const int buf_size = 3 * sizeof(int32_t) * RESTORATION_PROC_UNIT_PELS;
int32_t *rest_buf = aom_memalign(8, buf_size);
```

Good:

```cpp
// Intermediate multiplication promoted to size_t:
const int stride_argb = width * 4;
const size_t buffer_size = (size_t)height * stride_argb;
uint8_t *buf = (uint8_t *)aom_malloc(buffer_size);

// Allocation byte size declared as size_t:
const size_t buf_size = 3 * sizeof(int32_t) * RESTORATION_PROC_UNIT_PELS;
int32_t *rest_buf = aom_memalign(8, buf_size);
```
