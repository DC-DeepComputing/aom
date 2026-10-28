---
name: libaom/use-config-big-endian-macro-not-runtime-checks
description: When handling architecture endianness or byte ordering, use the CONFIG_BIG_ENDIAN macro from config/aom_config.h rather than implementing runtime endianness checks
rubric: readability
severity: should_fix
---

**Rule.** Use the build configuration macro CONFIG_BIG_ENDIAN (from
config/aom_config.h) to handle endianness differences and byte ordering instead
of implementing custom runtime endianness detection functions or pointer-casting
probes. Use boolean indexing like [!!CONFIG_BIG_ENDIAN] or conditionals (if
(CONFIG_BIG_ENDIAN) / #if CONFIG_BIG_ENDIAN).

**Why.** Target architecture endianness is fixed at build configuration time and
exposed through CONFIG_BIG_ENDIAN. Runtime endianness probing introduces
unnecessary overhead, violates type-punning safety, and complicates code
readability compared to compile-time configuration checks.

## Examples

Bad:

```cpp
// Bad: Runtime probing function using pointer casting to detect endianness.
SIMD_INLINE int big_endian(void) {
  const uint16_t t = 0x100;
  return *(const uint8_t *)&t;
}

SIMD_INLINE uint32_t c_v64_low_u32(c_v64 a) {
  return a.u32[big_endian()];
}
```

Good:

```cpp
// Good: Use the build configuration macro from aom_config.h.
#include "config/aom_config.h"

SIMD_INLINE uint32_t c_v64_low_u32(c_v64 a) {
  return a.u32[!!CONFIG_BIG_ENDIAN];
}
```
