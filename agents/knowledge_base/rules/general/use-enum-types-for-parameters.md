---
name: libaom/use-enum-types-for-parameters
description: When declaring parameters, struct members, or variables for modes, states, metadata, or block sizes, use specific enum types rather than raw integers
rubric: readability
severity: should_fix
---

**Rule.** When declaring function parameters, return types, struct members, or
variables that represent discrete enumerated modes, states, configurations,
metadata (such as aom_color_range_t, aom_matrix_coefficients_t,
aom_color_primaries_t, aom_img_fmt_t), or codec sizes (such as BLOCK_SIZE,
TX_SIZE, PREDICTION_MODE, TX_TYPE, CDEF_PICK_METHOD), use the specific typedef'd
enum type rather than generic integer types (int, unsigned int, uint8_t,
unsigned char). Reserve the identifier bsize for variables and parameters of
type BLOCK_SIZE, and query pixel dimensions through standard lookup tables (such
as block_size_wide[bsize] and block_size_high[bsize]) or use descriptive names
(like block_w, block_h) for scalar dimensions. Only use raw integer types when
bitmask operations or arithmetic indexing across arbitrary bounds is explicitly
required.

**Why.** Using explicit enum types provides compile-time type safety, clearly
documents valid domain values directly in function signatures and structs,
prevents accidental argument transposition or invalid value assignment, and
enables compiler warnings for unhandled enum values in switch statements.
Furthermore, reserving enum identifiers like bsize and using standard lookup
tables prevents confusion between enum indices and pixel dimensions,
particularly for rectangular partitions.

## Examples

Bad:

```cpp
// Bad: Raw integer types obscure enum domains and conflate identifiers with dimensions
struct SvcContext {
  int framedrop_mode;
};

int aom_calc_butteraugli(const YV12_BUFFER_CONFIG *source,
                         const YV12_BUFFER_CONFIG *distorted, int bit_depth,
                         int color_range, int matrix_coefficients);

void av1_inv_txfm2d_add_4x4(const int32_t *coeff, uint16_t *output,
                            int stride, TX_TYPE tx_type, unsigned char mode);

static double get_mse_block(Y_BUFFER_CONFIG buf1, Y_BUFFER_CONFIG buf2,
                            int16_t row, int16_t col, int bsize) {
  return ((double)sse) / (bsize * bsize);
}
```

Good:

```cpp
// Good: Explicit enum types and lookup tables ensure type safety and clarity
struct SvcContext {
  AOM_SVC_FRAME_DROP_MODE framedrop_mode;
};

int aom_calc_butteraugli(const YV12_BUFFER_CONFIG *source,
                         const YV12_BUFFER_CONFIG *distorted, int bit_depth,
                         aom_color_range_t color_range,
                         aom_matrix_coefficients_t matrix_coefficients);

void av1_inv_txfm2d_add_4x4(const int32_t *coeff, uint16_t *output,
                            int stride, TX_TYPE tx_type, PREDICTION_MODE mode);

static double get_mse_block(Y_BUFFER_CONFIG buf1, Y_BUFFER_CONFIG buf2,
                            int16_t row, int16_t col, BLOCK_SIZE bsize) {
  const int block_w = block_size_wide[bsize];
  const int block_h = block_size_high[bsize];
  return ((double)sse) / (block_w * block_h);
}
```
