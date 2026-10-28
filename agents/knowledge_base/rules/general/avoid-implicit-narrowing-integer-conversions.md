---
name: libaom/avoid-implicit-narrowing-integer-conversions
description: Align integer types across function parameters, struct fields, configuration members, loop counters, and step increments to prevent narrowing and mixed-signedness conversions
rubric: correctness
severity: should_fix
---

**Rule.** Align integer types across function parameters, struct members,
configuration fields (such as aom_codec_enc_cfg_t), local variables, loop
counters, step/stride increments, and limit constants (kMax...) with caller
arguments, downstream API parameters, and compared bounds:
- Match function parameter types to caller arguments and source struct/configuration member types (e.g., using unsigned int or uint32_t for dimensions and thread counts from aom_codec_enc_cfg_t, or matching intptr_t, size_t, ptrdiff_t from caller variables) rather than defaulting to mismatched signed int or narrower types.
- When declaring step, increment, or stride variables operating on signed loop counters, pointers, or offsets (such as intptr_t, ptrdiff_t, or int), use signed integer types (such as const int or intptr_t) rather than unsigned types (unsigned int, uint32_t).
- For struct fields and local variables passed to API functions (e.g., write_literal, aom_wb_write_literal), align member types with expected parameter types (or use explicit static_cast / C-style casts when narrowing is intentional).
- For for loops and bounded comparisons, use the exact integer type of the upper bound or container size (e.g., uint32_t for uint32_t block_height, size_t for container size) rather than mixing unsigned int or signed int.

**Why.** Implicit integer type conversions, narrowing across 32-bit and 64-bit
types, and mixed signed/unsigned arithmetic trigger compiler warnings such as
MSVC C4244 ('possible loss of data'), -Wshorten-64-to-32, -Wconversion, and
-Wsign-compare, causing build failures under -Werror. Matching parameter and
field types across caller/callee boundaries, struct configurations, helper
interfaces, and loop boundaries prevents data truncation, eliminates unnecessary
casts, avoids unintended integer promotions, and ensures consistent width and
signedness semantics.

## Examples

Bad:

```cpp
// 1. Function parameters default to signed int instead of matching struct/config fields:
static void set_auto_tiles(TileConfig *const tile_cfg, int width, int height,
                           int threads);  // aom_codec_enc_cfg_t fields are unsigned int

// 2. Helper narrows caller's intptr_t argument to int:
static INLINE void get_min_max_lane_eob(..., int n_coeffs) { ... }
void aom_quantize_b_neon(..., intptr_t n_coeffs, ...) {
  get_min_max_lane_eob(..., n_coeffs);  // -Wshorten-64-to-32
}

// 3. Unsigned step variable mixed with signed intptr_t counter:
const unsigned int step = 8;
while (n_coeffs > 0) {  // n_coeffs is intptr_t
  coeff_ptr += step;
  n_coeffs -= step;
}

// 4. Struct field wider than downstream API parameter (MSVC C4244):
struct DepthRepresentationElement { uint16_t mantissa_len; };
write_literal(wb, element.mantissa, element.mantissa_len);  // expects uint8_t

// 5. Loop counter type mismatches bound parameter:
for (unsigned int row = 0; row < block_height; ++row) { ... }  // block_height is uint32_t
```

Good:

```cpp
// 1. Parameter types match source configuration field types (unsigned int):
static void set_auto_tiles(TileConfig *const tile_cfg, unsigned int width,
                           unsigned int height, unsigned int threads);

// 2. Helper matches caller's intptr_t parameter type:
static INLINE void get_min_max_lane_eob(..., intptr_t n_coeffs) { ... }
void aom_quantize_b_neon(..., intptr_t n_coeffs, ...) {
  get_min_max_lane_eob(..., n_coeffs);
}

// 3. Signed step variable matches signed intptr_t counter:
const int step = 8;
while (n_coeffs > 0) {  // n_coeffs is intptr_t
  coeff_ptr += step;
  n_coeffs -= step;
}

// 4. Struct field matches downstream API parameter type:
struct DepthRepresentationElement { uint8_t mantissa_len; };
write_literal(wb, element.mantissa, element.mantissa_len);

// 5. Loop counter matches bound parameter type:
for (uint32_t row = 0; row < block_height; ++row) { ... }  // block_height is uint32_t
```
