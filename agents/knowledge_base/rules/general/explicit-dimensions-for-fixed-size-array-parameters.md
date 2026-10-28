---
name: libaom/explicit-dimensions-for-fixed-size-array-parameters
description: Specify explicit array bounds in parameter signatures and RTCD prototypes for fixed-size buffers instead of raw pointers, commented sizes, or unpacked scalars, matching bounds across variants
rubric: readability
severity: should_fix
---

**Rule.** When declaring or defining functions and RTCD prototypes (`add_proto`)
that process fixed-size buffers, filter kernels, coefficient vectors, or
contiguous sequences (such as `kernel[4]`, `hbuf[16]`, `sad_array[4]`, or `const
uint8_t *const ref_array[4]`), specify explicit array dimensions in parameter
signatures (e.g., `const int filter_level[2]`, `const uint8_t buf[4]`). Do not
use bare pointers (e.g., `double *kernel`, `int *filter_level`), trailing size
comments (`int16_t *hbuf /*[16]*/`), unsized array brackets (`int16_t hbuf[]`),
commented-out dimensions (`int16_t hbuf[/*16*/]`), or unpacked scalar parameters
(e.g., `const uint8_t a, const uint8_t b, ...`). Ensure array dimensions match
identically across RTCD prototypes, C definitions, SIMD implementations, and
sibling variants (such as `_skip_` or high-bit-depth variants).

**Why.** Explicit array dimensions document caller preconditions directly in the
signature, prevent buffer dimension mismatches, and enable compilers (such as
GCC's `-Wstringop-overflow` and `-Warray-parameter`) to verify buffer bounds at
compile time. Matching explicit dimensions across prototypes and SIMD
implementations avoids compiler warnings and clarifies backend requirements.
Passing fixed-size array parameters directly (with `const` qualification where
appropriate) also eliminates repetitive call-site indexing and avoids ambiguous
trailing comments (`T *buf /*[N]*/`) that can be misread as arrays of pointers
(`T *buf[N]`).

## Examples

Bad:

```cpp
// Bad: Raw pointer hides required buffer size; commented sizes disable checks
static INLINE double bicubic_interp_one(const double *arr, int stride,
                                        double *h_kernel, double *v_kernel);
void aom_int_pro_row_c(int16_t *hbuf /*[16]*/, const uint8_t *ref,
                       const int ref_stride, const int height);

// Bad: RTCD prototype declares sad_array[4], but C definition uses bare pointer
// add_proto qw/void aom_highbd_sad_skip_4x4x4d/, "... uint32_t sad_array[4]";
void aom_highbd_sad_skip_4x4x4d_c(const uint8_t *src, int src_stride,
                                  const uint8_t *const ref[4], int ref_stride,
                                  uint32_t *sad_array);

// Bad: Unpacking contiguous elements into individual scalar arguments
uint32_t av1_get_identity_hash_value(const uint8_t a, const uint8_t b,
                                     const uint8_t c, const uint8_t d);
```

Good:

```cpp
// Good: Explicit array dimensions document caller contract and enable checks
static INLINE double bicubic_interp_one(const double *arr, int stride,
                                        double h_kernel[4],
                                        double v_kernel[4]);
void aom_int_pro_row_c(int16_t hbuf[16], const uint8_t *ref,
                       const int ref_stride, const int height);

// Good: C definition array bounds match RTCD prototype exactly
void aom_highbd_sad_skip_4x4x4d_c(const uint8_t *src, int src_stride,
                                  const uint8_t *const ref[4], int ref_stride,
                                  uint32_t sad_array[4]);

// Good: Passing const array directly simplifies call sites and enforces bounds
uint32_t av1_get_identity_hash_value(const uint8_t buf[4]);
```
