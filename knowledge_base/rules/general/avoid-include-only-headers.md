---
name: libaom/avoid-include-only-headers
description: When adding or organizing header files, do not create include-only headers that declare no functions, types, or macros; include dependencies directly in source files
rubric: readability
severity: should_fix
---

**Rule.** Do not create header files that only aggregate or forward `#include`
directives without declaring any functions, types, macros, or constants. Include
required dependencies directly in the source (`.c`/`.cc`) or header files that
actually use them.

**Why.** Creating umbrella or forwarder header files that declare no interfaces
of their own introduces unnecessary file boilerplate, obscures true dependency
relationships, and adds build maintenance overhead. Directly including needed
headers in source files keeps dependencies explicit and self-contained.

## Examples

Bad:

```cpp
// av1/common/riscv/highbd_convolve_rvv.h
#ifndef AOM_AV1_COMMON_RISCV_HIGHBD_CONVOLVE_RVV_H_
#define AOM_AV1_COMMON_RISCV_HIGHBD_CONVOLVE_RVV_H_

#include "aom_dsp/riscv/mem_rvv.h"
#include "av1/common/convolve.h"
#include "av1/common/riscv/convolve_rvv.h"

#endif  // AOM_AV1_COMMON_RISCV_HIGHBD_CONVOLVE_RVV_H_
```

Good:

```cpp
// av1/common/riscv/highbd_convolve_rvv.c
#include "aom_dsp/riscv/mem_rvv.h"
#include "aom_ports/mem.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/riscv/convolve_rvv.h"
```
