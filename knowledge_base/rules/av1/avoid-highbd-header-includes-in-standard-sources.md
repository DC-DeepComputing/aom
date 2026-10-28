---
name: libaom/av1/avoid-highbd-header-includes-in-standard-sources
description: When implementing standard bit-depth DSP or SIMD routines, do not include high bit-depth headers (highbd_*.h); move shared helpers into common or standard bit-depth headers
rubric: correctness
severity: should_fix
---

**Rule.** Do not include high bit-depth headers (such as highbd_*.h) or depend
on high bit-depth symbols in standard bit-depth source files. Move any shared
helper functions, macros, or lookup tables into common or standard bit-depth
headers so they are available in all build configurations.

**Why.** In standard bit-depth-only builds (CONFIG_AV1_HIGHBITDEPTH=0), high
bit-depth source files and tables are excluded from compilation and linking.
Including high bit-depth headers or symbols in standard bit-depth source files
introduces dependencies on excluded files, causing build and linker failures in
standard bit-depth-only configurations.

## Examples

Bad:

```cpp
// In av1/common/arm/convolve_sve2.c (standard bit-depth source)
#include "av1/common/arm/convolve_sve2.h"
#include "av1/common/arm/highbd_convolve_sve2.h"

void av1_convolve_2d_sr_sve2(const uint8_t *src, int src_stride,
                             uint8_t *dst, int dst_stride, ...) {
  // Uses helper declared in highbd_convolve_sve2.h
}
```

Good:

```cpp
// In av1/common/arm/convolve_sve2.h (shared/standard bit-depth header)
static inline void aom_tbl2x2_s16(int16x8_t t0[2], int16x8_t t1[2],
                                  uint16x8_t tbl, int16x8_t res[2]) {
  res[0] = aom_tbl2_s16(t0[0], t1[0], tbl);
  res[1] = aom_tbl2_s16(t0[1], t1[1], tbl);
}

// In av1/common/arm/convolve_sve2.c (standard bit-depth source)
#include "av1/common/arm/convolve_sve2.h"
```
