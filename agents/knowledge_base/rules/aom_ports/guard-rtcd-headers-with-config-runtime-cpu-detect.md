---
name: libaom/aom_ports/guard-rtcd-headers-with-config-runtime-cpu-detect
description: 'When including OS- or platform-specific headers in CPU detection files, place headers needed only for runtime detection inside #if CONFIG_RUNTIME_CPU_DETECT'
rubric: correctness
severity: should_fix
---

**Rule.** Place platform-specific and system headers (such as <fcntl.h>,
<unistd.h>, <sys/auxv.h>, or <linux/auxvec.h>) that are only needed for runtime
CPU detection inside the #if CONFIG_RUNTIME_CPU_DETECT block rather than
including them unconditionally at the top of the file. Ensure
"config/aom_config.h" is included before the #if CONFIG_RUNTIME_CPU_DETECT guard
so that configuration macros are defined.

**Why.** When runtime CPU detection is disabled (CONFIG_RUNTIME_CPU_DETECT=0),
stub implementations return constant capability flags and do not require OS- or
platform-specific system headers. Unconditionally including these headers causes
unnecessary dependencies and build failures on minimal, embedded, or non-Linux
cross-compilation targets where RTCD is turned off.

## Examples

Bad:

```cpp
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#include "config/aom_config.h"
#include "aom_ports/ppc.h"

#if CONFIG_RUNTIME_CPU_DETECT
#include <asm/cputable.h>
#include <linux/auxvec.h>
// Full runtime CPU detection implementation...
#else
int ppc_simd_caps(void) { return 0; }
#endif
```

Good:

```cpp
#include "config/aom_config.h"
#include "aom_ports/ppc.h"

#if CONFIG_RUNTIME_CPU_DETECT
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <asm/cputable.h>
#include <linux/auxvec.h>
// Full runtime CPU detection implementation...
#else
int ppc_simd_caps(void) { return 0; }
#endif
```
