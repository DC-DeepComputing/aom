---
name: libaom/suffix-64-bit-integer-literals-with-ull
description: When defining 64-bit integer literal constants or initializing 64-bit registers, explicitly suffix unsigned literals with ULL to ensure portable typing and prevent compiler warnings
rubric: readability
severity: should_fix
---

**Rule.** Explicitly suffix 64-bit integer literal constants with 'ULL' for
unsigned values (e.g., '0x...ULL') or 'LL' for signed values (or use standard
'UINT64_C' / 'INT64_C' macros) when defining 64-bit constants, bitmasks, or
initializing SIMD registers (such as with 'vcreate_u64'). Do not rely on
implicit literal type promotion.

**Why.** In C, integer literals without suffixes can trigger compiler warnings
(such as integer constant size warnings or sign conversions) and behave
inconsistently across 32-bit and 64-bit toolchains. Explicit 'ULL' suffixes
guarantee portable 64-bit unsigned typing across all supported compilers.

## Examples

Bad:

```cpp
uint8x8_t idx = vreinterpret_u8_u64(vcreate_u64(0x0004080c1014181c));
const uint64_t mask = 0xff00ff00ff00ff00;
```

Good:

```cpp
uint8x8_t idx = vreinterpret_u8_u64(vcreate_u64(0x0004080c1014181cULL));
const uint64_t mask = 0xff00ff00ff00ff00ULL;
```
