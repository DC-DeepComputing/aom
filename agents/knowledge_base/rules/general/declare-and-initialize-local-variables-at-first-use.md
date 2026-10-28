---
name: libaom/declare-and-initialize-local-variables-at-first-use
description: Declare local variables, SIMD vectors, and loop indices in narrowest scope at first use with const; avoid dummy initializations, conditional no-op fallbacks, and artificial blocks
rubric: readability
severity: should_fix
---

**Rule.** In C and C++ functions, declare and initialize local variables,
intermediate calculation values, SIMD vectors (e.g., __m128i, __m256i, ARM NEON
vectors, or Highway hn:: types), and loop counters directly at their point of
first meaningful use in the narrowest enclosing scope. Declare loop iteration
variables directly within for loop headers (e.g., 'for (int i = 0; ...)' or 'for
(size_t k = 0; ...)') rather than pre-declaring them at function scope, unless
the counter value is explicitly required after loop exit. Qualify local
variables, vector registers, and constants with const whenever they are not
modified after initialization. Avoid declaring uninitialized variables or
assigning dummy defaults (e.g., 0, false, NULL, nullptr, or vdup_n_*(0)) prior
to an unconditional overwrite. Similarly, when a variable and its dependent
update (e.g., |=, +=, *=, or bit-shifts after aom_read_symbol, aom_read_bit) are
only needed conditionally inside an if block where the default fallback is a
no-op, scope the declaration, calculation, and dependent update directly inside
the if block as const rather than maintaining a mutable default variable in the
outer scope. Do not create artificial inner block scopes ({ ... }) solely to
declare local variables. When multiple declarations start a block, group const
declarations before mutable declarations, and omit redundant placeholder
initializations for pure output parameters.

**Why.** Declaring and initializing variables at their first use in the
narrowest scope clarifies lifetimes, prevents uninitialized reads, avoids dead
stores from dummy assignments, and eliminates artificial C89-style scoping
blocks that introduce redundant indentation. Scoping conditional calculations
and updates inside if blocks removes redundant no-op evaluations on fallback
paths, reduces outer mutable state, and prevents accidental state leakage.
Explicitly qualifying immutable local variables and SIMD vectors as const
communicates single-assignment intent to readers and the compiler, prevents
accidental mutation across DSP pipelines, and maintains consistency across
kernels. Scoping loop counters within for loop headers prevents variable reuse
across successive loops and makes loop invariants clear. Grouping const
constants before mutable state clearly separates invariant configuration from
working state.

## Examples

Bad:

```cpp
// Bad: Pre-declaring variables/loop counters at function scope; dummy init before early return
static INLINE void process_block(const uint8_t *src, const uint8_t *ref, int mode) {
  VP128x128 *vt;
  int i, k;
  uint32x2_t a_u32 = vdup_n_u32(0);
  bool tpl_mt_exit = false;
  if (!is_valid_mode(mode)) return;

  a_u32 = vdup_n_u32(val);
  for (i = 0; i < 8; ++i) { ... }
}

// Bad: Dummy default in outer scope when update is only needed inside conditional block
uint8_t stx_flag = 0;
if (block_signals_sec_tx_type(xd, tx_size, *tx_type, *eob)) {
  stx_flag = aom_read_symbol(r, ec_ctx->stx_cdf[square_tx_size], STX_TYPES, ACCT_STR);
}
*tx_type |= (stx_flag << 4);

// Bad: Artificial nested block scope solely to declare variables; non-const SIMD vector
for (int x = 0; x < width; x += 8) {
  __m128i pred_lo = _mm_madd_epi16(top_weights_x_lo, weight_left_y);
  {
    uint8x16_t row = vld1q_u8(src);
    const __m128i inverted_weights = _mm_sub_epi16(scale_val, cvt(weights));
    pred_lo = _mm_add_epi32(pred_lo, scaled_bottom_left);
  }
}
```

Good:

```cpp
// Good: Early returns first; initialize directly at first use with const; loop counters in header
static INLINE void process_block(const uint8_t *src, const uint8_t *ref, int mode) {
  if (!is_valid_mode(mode)) return;
  VP128x128 *vt;
  const uint32x2_t a_u32 = vdup_n_u32(val);
  const bool tpl_mt_exit = tpl_row_mt->tpl_mt_exit;
  for (int i = 0; i < 8; ++i) { ... }
}

// Good: Scope declaration and dependent bitwise update inside the conditional block as const
if (block_signals_sec_tx_type(xd, tx_size, *tx_type, *eob)) {
  const uint8_t stx_flag = aom_read_symbol(
      r, ec_ctx->stx_cdf[square_tx_size], STX_TYPES, ACCT_STR);
  *tx_type |= (stx_flag << 4);
}

// Good: Mix declarations in enclosing block without artificial braces; const vectors
for (int x = 0; x < width; x += 8) {
  __m128i pred_lo = _mm_madd_epi16(top_weights_x_lo, weight_left_y);
  const uint8x16_t row = vld1q_u8(src);
  const __m128i inverted_weights = _mm_sub_epi16(scale_val, cvt(weights));
  pred_lo = _mm_add_epi32(pred_lo, scaled_bottom_left);
}
```
