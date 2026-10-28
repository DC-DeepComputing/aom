---
name: libaom/mark-unmodified-local-pointers-const
description: Qualify unmodified local variables, SIMD vectors, non-reassigned pointers (*const), and read-only parameters/tables with const (using static const for fixed lookup tables), leaving mutated outputs non-const
rubric: readability
severity: should_fix
---

**Rule.** Qualify local variables (e.g., bool, int, int64_t, size_t, double,
float), structs/enums (e.g., BLOCK_SIZE, TX_SIZE, MB_MODE_INFO, MACROBLOCKD),
and SIMD vector registers (e.g., __m128i, __m256i, __m512i, int16x8_t,
uint8x16_t) with const when their values are initialized and not modified in
scope. Initialize immutable locals directly at definition rather than using
dummy assignments, use const bool (or bool) rather than int for boolean flags,
and separate const declarations from mutable variables (such as loop indices or
accumulators) into distinct statements.

Additionally:
- For function-local constant lookup tables, filter matrices, permutation vectors, mask arrays, and SIMD initialization constants, declare them as static const (or file-scope static const) rather than local mutable arrays or non-static const.
- For local pointers, qualify them as Type *const ptr when the pointer itself is not reassigned, and as const Type *const ptr when neither the referenced memory nor the pointer address is modified.
- For function parameters, plane/image descriptors (e.g., const YV12_BUFFER_CONFIG *, const aom_image_t *), array parameters (e.g., const double kernel[4]), and references to constant lookup tables/structs (e.g., const struct YuvConstants *yuv = &kYuvH709Constants), qualify read-only pointees as const Type * rather than casting away const.
- Leave mutated output buffer or struct parameters non-const (e.g., YV12_BUFFER_CONFIG *dst) to clearly distinguish inputs from outputs.

**Why.** Const-qualifying immutable local variables, SIMD vector registers,
pointers (*const), and read-only targets (const Type *) clearly communicates
intent, documents dataflow invariants, catches accidental mutations or
reassignments at compile time, and prevents unsafe const-discarding casts.
Declaring fixed tables and mask arrays as static const places data in read-only
storage (.rodata), eliminating per-invocation stack allocation, copy overhead,
and redundant stack memory loads in performance-critical paths. Leaving mutated
output parameters non-const ensures function signatures accurately reflect side
effects without misleading callers.

## Examples

Bad:

```cpp
// 1. Missing const on unmodified scalars/bools and combined mutable declarations
int filter_height_half = layer_config->filter_height >> 1;
int im_stride = 8, i;
int overshoot_buffer_low = (cpi->rc.rc_1_frame == -1 && rc->sad > 1000);

// 2. Missing const on SIMD vector registers and non-reassigned pointers
__m128i s0 = _mm_loadl_epi64((const __m128i *)src);
int16x8_t res = vld1q_s16(src + ofs0);
uint8_t *src_buf = frame_to_show->y_buffer;
AV1_COMP *cpi = ctx->ppi->cpi;

// 3. Local lookup tables / SIMD constants reinitialized on stack
const int gau_filter[3][3] = { { 1, 2, 1 }, { 2, 4, 2 }, { 1, 2, 1 } };
const uint16_t sse1 = 0;
uint16x8_t q1 = vld1q_dup_u16(&sse1);

// 4. Missing const on read-only parameters/tables, or const on mutated output
void process(YV12_BUFFER_CONFIG *src, double kernel[4], const YV12_BUFFER_CONFIG *dst) {
  struct YuvConstants *yuv = (struct YuvConstants *)&kYuvH709Constants;
}
```

Good:

```cpp
// 1. Const scalars/bools and separated mutable declarations
const int filter_height_half = layer_config->filter_height >> 1;
const int im_stride = 8;
int i;
const bool overshoot_buffer_low =
    cpi->rc.rc_1_frame == -1 && rc->sad > 1000;

// 2. Const SIMD registers and properly qualified pointers
const __m128i s0 = _mm_loadl_epi64((const __m128i *)src);
const int16x8_t res = vld1q_s16(src + ofs0);
const uint8_t *const src_buf = frame_to_show->y_buffer;
AV1_COMP *const cpi = ctx->ppi->cpi;

// 3. Fixed tables and SIMD constants in .rodata
static const int gau_filter[3][3] = { { 1, 2, 1 }, { 2, 4, 2 }, { 1, 2, 1 } };
static const uint16_t sse1 = 0;
const uint16x8_t q1 = vld1q_dup_u16(&sse1);

// 4. Pointer-to-const on read-only parameters/tables; mutated output left non-const
void process(const YV12_BUFFER_CONFIG *src, const double kernel[4], YV12_BUFFER_CONFIG *dst) {
  const struct YuvConstants *const yuv = &kYuvH709Constants;
}
```
