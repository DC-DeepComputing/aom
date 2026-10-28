---
name: libaom/use-inttypes-format-macros-for-fixed-width-integers
description: When formatting fixed-width integers or size_t in printf/scanf, use PRId64/PRIu64 or %zu/%zd instead of manual casts
rubric: readability
severity: should_fix
---

**Rule.** When printing, formatting, or scanning fixed-width integer types (such
as int64_t, uint64_t, int32_t, uint32_t) or size types (size_t, ssize_t,
ptrdiff_t) with printf, fprintf, snprintf, or scanf family functions, use
standard format specifiers: the standard PRI and SCN macros from <inttypes.h>
(e.g., PRId64, PRIu64, PRIx64, SCNd64) for fixed-width integers, and %zu / %zd /
%td for size_t / ssize_t / ptrdiff_t. Do not cast arguments to long long,
unsigned long long, or unsigned long with %lld, %llu, or %lu, and do not cast
size_t to uint64_t with PRIu64.

**Why.** Fixed-width integer types and size_t/ssize_t are defined differently
across architectures (such as 32-bit vs 64-bit systems) and compiler targets
(e.g. long vs long long, unsigned int vs unsigned long). Using standard inttypes
format macros and C99/C++ format specifiers (%zu, %zd) guarantees portable
formatting without redundant manual type casts and prevents format specifier
mismatch warnings.

## Examples

Bad:

```cpp
// Bad: Casting fixed-width integers and size_t to long long or unsigned long
fprintf(stderr, "coeff out of bit range, value: %lld bit %d\n",
        (long long)value, bit);

if (frame_size > 0) {
  fprintf(stderr, " %6lu bytes", (unsigned long)frame_size);
}
```

Good:

```cpp
// Good: Using standard inttypes format macros and %zu format specifier
fprintf(stderr, "coeff out of bit range, value: %" PRId64 " bit %d\n",
        value, bit);

if (frame_size > 0) {
  fprintf(stderr, " %6zu bytes", frame_size);
}
```
