---
name: libaom/include-rtcd-headers-for-prototype-verification
description: When defining functions declared in RTCD or header files, include the declaration headers (such as config/*_rtcd.h) in C source files for prototype verification
rubric: readability
severity: should_fix
---

**Rule.** Always include the corresponding RTCD or declaration header (such as
`config/aom_dsp_rtcd.h`, `config/av1_rtcd.h`, or `config/aom_scale_rtcd.h`) in C
source files that define functions declared within it, even if the file does not
call any routines from that header. Never omit or remove RTCD headers from
implementation files defining C or SIMD functions.

**Why.** In C, defining functions without their prototype declarations in scope
prevents the compiler from verifying that the function definition matches its
declared signature. Including the declaring RTCD header ensures compiler
prototype checking and catches signature mismatches across C reference and
architecture-specific implementations.

## Examples

Bad:

```cpp
// In aom_dsp/arm/highbd_intrapred_neon.c defining av1_highbd_dr_prediction_z3_neon():
#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
// config/av1_rtcd.h omitted because av1_highbd_dr_prediction_z3_c() is not called here
```

Good:

```cpp
// In aom_dsp/arm/highbd_intrapred_neon.c defining av1_highbd_dr_prediction_z3_neon():
#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"
```
