---
name: libaom/portability-macros-in-low-level-headers-not-codec-headers
description: When defining generic portability macros or type helpers used across subsystems, place them in low-level porting headers like aom_ports/ rather than codec-specific headers
rubric: design
severity: nit
---

**Rule.** Define generic portability macros, fixed-width enum helpers (such as
UENUM1BYTE), and cross-subsystem type definitions in low-level porting headers
(such as aom_ports/mem.h or aom_ports/aom_integer.h) rather than codec-specific
headers (such as av1/common/enums.h). Ensure low-level utility and porting
modules never depend on high-level codec headers.

**Why.** Placing generic type and portability helpers in codec headers forces
low-level utility modules (aom_ports, aom_util, aom_dsp) to include high-level
codec headers, creating inverted dependencies and layering violations.

## Examples

Bad:

```cpp
// av1/common/enums.h
#define UENUM1BYTE(enumvar) typedef uint8_t enumvar;

// aom_util/aom_thread.h
#include "av1/common/enums.h"
```

Good:

```cpp
// aom_ports/mem.h
#define UENUM1BYTE(enumvar) typedef uint8_t enumvar;

// av1/common/enums.h
#include "aom_ports/mem.h"
```
