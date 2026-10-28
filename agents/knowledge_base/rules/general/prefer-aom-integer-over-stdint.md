---
name: libaom/prefer-aom-integer-over-stdint
description: When fixed-width integer types or integer limit/format macros are needed, include 'aom/aom_integer.h' rather than directly including <stdint.h> or <inttypes.h>
rubric: readability
severity: nit
---

**Rule.** Include "aom/aom_integer.h" rather than directly including <stdint.h>
or <inttypes.h> when fixed-width integer types (such as uint8_t, int32_t,
uint64_t), limit macros, or format specifiers are needed.

**Why.** "aom/aom_integer.h" is libaom's project-standard wrapper that includes
<stdint.h>, <inttypes.h>, and <stddef.h> while setting up necessary compiler
compatibility macros like __STDC_LIMIT_MACROS and __STDC_FORMAT_MACROS.

## Examples

Bad:

```cpp
// Bad: Direct inclusion of standard integer headers
#include <stdint.h>
#include <inttypes.h>

void process_block(uint8_t *src, int32_t stride);
```

Good:

```cpp
// Good: Include libaom's standard integer wrapper
#include "aom/aom_integer.h"

void process_block(uint8_t *src, int32_t stride);
```
