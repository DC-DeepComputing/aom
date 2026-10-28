---
name: libaom/prefer-pointer-param-for-buffer-offsets
description: When function callers pass buffer offsets or slice pointers into larger buffers, use pointer syntax rather than fixed-size array parameters
rubric: readability
severity: nit
---

**Rule.** Do not use fixed-size array parameter syntax (e.g., const T p[N] or T
p[N]) for function arguments when callers pass an offset pointer into a larger
buffer, slice, or image plane rather than a dedicated array of size N. Use
pointer syntax (const T *p or T *p) instead, reserving fixed-size array syntax
for inputs that are truly standalone fixed-length arrays.

**Why.** In C and C++, array parameter declarations decay to pointers at compile
time, but fixed-size array syntax (T p[N]) misleads readers into expecting a
distinct standalone array object of exactly N elements. When callers pass
interior pointers such as &arr[i * stride - 1], pointer syntax accurately
reflects that the argument is a buffer offset or slice.

## Examples

Bad:

```cpp
// Misleading: callers pass slice offsets like &arr[i * stride - 1]
static INLINE double get_cubic_value_dbl(const double p[4],
                                         const double kernel[4]) {
  return kernel[0] * p[0] + kernel[1] * p[1] + kernel[2] * p[2] +
         kernel[3] * p[3];
}
```

Good:

```cpp
// Accurate: p is a buffer slice/offset pointer; kernel is a fixed 4-tap filter
static INLINE double get_cubic_value_dbl(const double *p,
                                         const double kernel[4]) {
  return kernel[0] * p[0] + kernel[1] * p[1] + kernel[2] * p[2] +
         kernel[3] * p[3];
}
```
