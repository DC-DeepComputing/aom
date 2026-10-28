---
name: libaom/prefer-direct-pointer-arithmetic-over-address-of-subscript
description: When passing arrays or calculating buffer offsets, use direct pointer arithmetic (ptr + offset) or array decay instead of redundant address-of syntax (&arr, &ptr[0], &ptr[offset])
rubric: readability
severity: should_fix
---

**Rule.** When passing arrays, buffer pointers, or calculating memory offsets,
use direct pointer identifiers or pointer arithmetic (e.g., ptr, arr, ptr +
offset, or ptr + (y * stride)) instead of redundant address-of syntax (such as
&arr, &ptr[0], &arr[0], or &ptr[offset]). Allow arrays to decay naturally to
element pointers when passed to functions (such as qsort, memory operations, or
buffer utilities), and format offset calculations as pointer arithmetic rather
than subscripting and taking the address back with &amp;.

**Why.** In C and C++, array names automatically decay to pointers to their
first element (Type *) in function argument expressions. Applying &amp;array
creates a pointer to the entire array (Type (*)[N]), which is non-idiomatic and
risks type mismatches. Furthermore, taking the address of index zero
(&amp;ptr[0]) or an offset (&amp;ptr[offset]) redundantly dereferences and takes
the address of the element. Direct pointer arithmetic and natural array decay
produce cleaner, more concise, and idiomatic code for buffer and video
processing operations.

## Examples

Bad:

```cpp
// Anti-pattern 1: redundant address-of on array variable
unsigned int variances[64];
qsort(&variances, 64, sizeof(variances[0]), comp_func);

// Anti-pattern 2: redundant address-of index zero
aom_calc_ssim(orig, recon, &weight[0], &frame_ssim2[0]);

// Anti-pattern 3: address-of subscripted offset
uint8_t *dst_row = &dst[y * dst_stride];
convolve_copy(&src[y * src_stride], src_stride, &dst[y * dst_stride], dst_stride);
```

Good:

```cpp
// Idiomatic: natural array decay
unsigned int variances[64];
qsort(variances, 64, sizeof(variances[0]), comp_func);

// Idiomatic: pass pointer/array directly
aom_calc_ssim(orig, recon, weight, frame_ssim2);

// Idiomatic: direct pointer arithmetic
uint8_t *dst_row = dst + (y * dst_stride);
convolve_copy(src + (y * src_stride), src_stride, dst + (y * dst_stride), dst_stride);
```
