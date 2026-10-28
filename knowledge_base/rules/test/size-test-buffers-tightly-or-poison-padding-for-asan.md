---
name: libaom/test/size-test-buffers-tightly-or-poison-padding-for-asan
description: In unit tests for fixed-size or parameterized kernels, size test buffers tightly to tested dimensions or poison unused stride padding with ASAN_POISON_MEMORY_REGION.
rubric: testing
severity: should_fix
---

**Rule.** When writing unit or regression tests for fixed-size functions (such
as 1D/2D transforms or filtering kernels) or parameterized kernels across
variable block dimensions, declare input, output, and reference buffers with the
exact dimensions required rather than using oversized arrays copied from
maximum-capacity templates. For parameterized kernels tested with row strides or
margin padding, size buffers to the exact dimensions under test or poison unused
row stride and padding regions using ASAN_POISON_MEMORY_REGION (unpoisoning with
ASAN_UNPOISON_MEMORY_REGION after kernel execution if buffers are subsequently
inspected or freed).

**Why.** Allocating oversized test buffers (such as 64-element arrays for
4-point transforms or fixed 64x64 buffers for small block configurations)
obscures expected function dimensions and masks out-of-bounds reads or writes
from AddressSanitizer (ASan). Sizing buffers to exact dimensions or poisoning
stride padding ensures ASan immediately catches buffer over-reads and
over-writes across all tested block sizes.

## Examples

Bad:

```cpp
// Variant 1: Testing a 4-point transform kernel with an oversized 64-element buffer
int32_t input[64] = { 300000, 0, 100000, 160000 };
int32_t output[64];
av1_iadst4(input, output, 12, range_bit);
EXPECT_GT(output[0], 0);

// Variant 2: Fixed maximum-size buffer hides out-of-bounds writes within 64x64 from ASan
Pixel dst_ref[64 * 64];
Pixel dst_tst[64 * 64];
Predict(speedtest, tx, above, left, dst_ref, dst_tst, stride);
```

Good:

```cpp
// Variant 1: Sizing buffers to match the exact 4-point dimension required by av1_iadst4
int32_t input[4] = { 300000, 0, 300000, 300000 };
int32_t output[4];
av1_iadst4(input, output, 12, range_bit);
EXPECT_GT(output[0], 0);

// Variant 2: Allocate buffer to exact tested height and poison unused stride area per row
const int dst_stride = MAX_TX_SIZE + 16;
std::vector<Pixel> dst_ref(dst_stride * bh_);
std::vector<Pixel> dst_tst(dst_stride * bh_);
for (int r = 0; r < bh_; ++r) {
  ASAN_POISON_MEMORY_REGION(&dst_ref[r * dst_stride + bw_],
                            (dst_stride - bw_) * sizeof(Pixel));
  ASAN_POISON_MEMORY_REGION(&dst_tst[r * dst_stride + bw_],
                            (dst_stride - bw_) * sizeof(Pixel));
}
Predict(speedtest, tx, above, left, dst_ref.data(), dst_tst.data(), dst_stride);
for (int r = 0; r < bh_; ++r) {
  ASAN_UNPOISON_MEMORY_REGION(&dst_ref[r * dst_stride + bw_],
                              (dst_stride - bw_) * sizeof(Pixel));
  ASAN_UNPOISON_MEMORY_REGION(&dst_tst[r * dst_stride + bw_],
                              (dst_stride - bw_) * sizeof(Pixel));
}
```
