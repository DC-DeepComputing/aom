---
name: libaom/combine-predecrement-in-do-while-condition
description: In SIMD, DSP, and hot loops with positive bounds, prefer pointer-bumping do-while loops with condition pre-decrement and != 0 termination checks
rubric: readability
severity: should_fix
---

**Rule.** In performance-critical SIMD kernels, DSP routines, and hot scalar
loops where loop trip counts or block dimensions are guaranteed to be strictly
positive (such as width >= 4, height >= 2, nsymbs >= 2, transform sizes, or
helper macros like SHIFT_LOOP_HELPER), prefer `do { ... } while (...)` loops
over `for` or entry-checked `while` loops to eliminate redundant initial
condition checks.

In buffer-processing loops, advance source and destination pointers directly (e.g., `in += stride;` or `in += 2 * CDEF_BSTRIDE;`) rather than recomputing row addresses via indexing (`in + i * stride`).

For countdown loops stepping by 1, merge the pre-decrement directly into the loop condition (e.g., `} while (--h != 0);` or `} while (--count != 0);`) instead of placing a separate decrement statement (`h--;` or `--h;`) in the loop body before `while (h > 0);`.

When stepping by more than 1 (e.g., unrolled loops), decrement at the end of the loop body (e.g., `h -= 2;`) and check termination using `!= 0` rather than `> 0` or `>= 0` when step sizes are guaranteed to evenly divide the total iteration count.

**Why.** Standard `for` and `while` loops evaluate their termination condition
prior to executing the first iteration, incurring a conditional branch to guard
against zero-trip executions. When invariants or preconditions guarantee at
least one iteration, `do-while` moves the condition test to the end of the loop,
eliminating redundant loop-entry branch tests, saving branch predictor
resources, and producing tighter inner loop assembly.

Bumping pointers directly eliminates repeated base + index * stride multiplications in vector loads and stores, reducing instruction count and register pressure.

Merging pre-decrements directly into the loop condition (`while (--h != 0)`) produces concise loop structures without redundant trailing decrement statements and maps directly to architecture-level decrement-and-branch instructions. Using `!= 0` instead of `> 0` explicitly documents and asserts that dimensions evenly divide the step size, avoiding masked boundary errors and compiling directly to zero-flag tests.

## Examples

Bad:

```cpp
// 1. Indexed for loop with redundant entry branch check and stride multiplication:
for (int i = 0; i < height; i += 2) {
  uint16x8_t s = load_unaligned_u16_4x2(in + i * CDEF_BSTRIDE, CDEF_BSTRIDE);
  store_unaligned_u8_4x2(dst8 + i * dstride, dstride, res_128);
}

// 2. Trailing decrement statement with '> 0' termination check:
int h = height;
do {
  src_ptr += 2 * src_stride;
  dst_ptr += 2 * 4;
  h -= 2;
} while (h > 0);

// 3. Entry check in hot scalar loop with guaranteed positive count (nsymbs >= 2):
for (int i = 0; i < nsymbs - 1; ++i) {
  if (i < val) {
    cdf[i] += (CDF_PROB_TOP - cdf[i]) >> rate;
  } else {
    cdf[i] -= cdf[i] >> rate;
  }
}
```

Good:

```cpp
// 1. Direct pointer increment with multi-step decrement and '!= 0' check:
int h = height;
do {
  uint16x8_t s = load_unaligned_u16_4x2(in, CDEF_BSTRIDE);
  store_unaligned_u8_4x2(dst8, dstride, res_128);
  in += 2 * CDEF_BSTRIDE;
  dst8 += 2 * dstride;
  h -= 2;
} while (h != 0);

// 2. Direct pointer increments with merged pre-decrement condition:
int h = height;
do {
  src_ptr += src_stride;
  dst_ptr += dst_stride;
} while (--h != 0);

// 3. do-while loop eliminating entry branch for positive scalar trip count:
int i = 0;
do {
  if (i < val) {
    cdf[i] += (CDF_PROB_TOP - cdf[i]) >> rate;
  } else {
    cdf[i] -= cdf[i] >> rate;
  }
} while (++i < nsymbs - 1);
```
