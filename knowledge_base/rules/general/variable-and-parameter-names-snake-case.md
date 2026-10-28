---
name: libaom/variable-and-parameter-names-snake-case
description: When declaring variables, function parameters, struct members, or SIMD vectors, use lowercase snake_case without Hungarian prefixes (g_), trailing underscores, camelCase, or uppercase notation
rubric: readability
severity: should_fix
---

**Rule.** Name local variables, global/namespace-scope variables, function
parameters, struct members, and temporary SIMD vectors in all-lowercase
`snake_case` (e.g., `alpha_max`, `round_offset`, `gxa_lo`, `gx`, `n`,
`bit_count`, `pack_bs_mutex`). Do not use camelCase (e.g., `alphaMax`),
PascalCase, uppercase identifiers (e.g., `N`), or mathematical capital notation
(e.g., `Gx`, `Gxa_lo`). Additionally, do not use Hungarian scope prefixes such
as `g_` (e.g., `g_bit_count`, `g_crc_initialized`) on global variables or struct
members, and do not use trailing underscores (e.g., `mutex_`, `pack_bs_mutex_`)
in C variables or struct fields, even when wrapping or referencing legacy
identifiers. Reserve `kCamelCase` strictly for compile-time/static constants.

**Why.** Consistent lowercase `snake_case` naming aligns with the Google C/C++
style followed by libaom and ensures uniform readability across codec modules
and test files. It clearly differentiates variables, parameters, and struct
fields from constants (`kConstant`), preprocessor macros / enum literals
(`ALL_CAPS`), and types (`PascalCase`). Hungarian prefixes like `g_` are
discouraged and misleadingly imply global lifetime when used on struct members,
while trailing underscores are a C++ private class member convention not used in
C struct fields or local variables.

## Examples

Bad:

```cpp
// Bad: camelCase, PascalCase, or uppercase variables / parameters
static AOM_FORCE_INLINE void highbd_dr_prediction(
    int N, __m128i *dst, int dx) {
  const __m128i alphaMax = _mm_set1_epi8(AOM_BLEND_A64_MAX_ALPHA);
}
uint16x8_t Gxa_lo = vaddl_u8(vget_low_u8(mat[0]), vget_low_u8(mat[2]));

// Bad: Hungarian g_ prefix on global variables or struct members
int64_t g_bit_count = 0;
typedef struct {
  CRC32C crc_calculator;
  int g_crc_initialized;
} IntraBCHashInfo;

// Bad: trailing underscore on C local variables or struct members
pthread_mutex_t *pack_bs_mutex_ = pack_bs_sync->mutex_;
```

Good:

```cpp
// Good: lowercase snake_case variables and parameters
static AOM_FORCE_INLINE void highbd_dr_prediction(
    int n, __m128i *dst, int dx) {
  const __m128i alpha_max = _mm_set1_epi8(AOM_BLEND_A64_MAX_ALPHA);
}
uint16x8_t gxa_lo = vaddl_u8(vget_low_u8(mat[0]), vget_low_u8(mat[2]));

// Good: standard snake_case without g_ prefix
int64_t bit_count = 0;
typedef struct {
  CRC32C crc_calculator;
  int crc_initialized;
} IntraBCHashInfo;

// Good: standard snake_case without trailing underscore
pthread_mutex_t *const pack_bs_mutex = pack_bs_sync->mutex_;
```
