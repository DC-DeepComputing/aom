---
name: libaom/prefer-aommin-aommax-and-standard-type-limits
description: Use standard AOMMIN, AOMMAX, clamp, fclamp, and type limits instead of hand-written ternaries, if statements, hex literals, or custom clamp functions
rubric: readability
severity: should_fix
---

**Rule.** When computing minimum/maximum values or constraining values to a
range, use standard helper macros and inline functions from
aom_dsp/aom_dsp_common.h (AOMMIN, AOMMAX, clamp() for int, clamp64() for
int64_t, fclamp() for double/float, and clip_pixel() / clip_pixel_highbd() for
pixel values) instead of open-coded ternary conditional expressions (? :),
manual if statements, or defining custom clamping functions and macros (e.g.,
clampf). Additionally, when clamping or checking bounds against integer type
limits, use standard macros from <limits.h> and <stdint.h> (such as INT_MAX,
INT_MIN, UINT_MAX, INT32_MAX, INT64_MAX, or SIZE_MAX) directly rather than
hardcoding hex constants (like 0x7FFFFFFF or 0xFFFFFFFF) or defining redundant
custom limit macros. When bounding multiple fields of a nested struct, alias the
struct to a local pointer to keep expressions concise.

**Why.** Using project-standard helper macros and functions eliminates
boilerplate branching, ensures consistent idioms across libaom, and avoids
error-prone open-coded comparisons or redundant custom helpers that clutter the
namespace. Using standard <limits.h> and <stdint.h> macros conveys intent,
prevents sign and width mismatches across target architectures, and avoids
redundant macro definitions. Aliasing deeply nested struct paths to local
pointers avoids repetitive multi-level member dereferencing.

## Examples

Bad:

```cpp
// Bad: Open-coded ternary expressions and manual if-statements
const int min_sz = (width < height) ? width : height;
if (val < min_val) val = min_val;
if (val > max_val) val = max_val;

// Bad: Hardcoding hex literals / custom limit macro instead of INT_MAX
#define MAX_ALLOWED_VBR_FRAME_BITS 0x7FFFFFFF
vbr_max_bits = (vbr_max_bits < MAX_ALLOWED_VBR_FRAME_BITS) ? vbr_max_bits : MAX_ALLOWED_VBR_FRAME_BITS;

// Bad: Defining custom clamp helper instead of using fclamp()
static INLINE double clampf(double v, double min, double max) {
  return (v < min) ? min : ((v > max) ? max : v);
}
```

Good:

```cpp
#include "aom_dsp/aom_dsp_common.h"

// Good: Using standard AOMMIN and clamp() helpers
const int min_sz = AOMMIN(width, height);
val = clamp(val, min_val, max_val);

// Good: Using standard AOMMIN directly with standard INT_MAX limit macro
vbr_max_bits = AOMMIN(vbr_max_bits, INT_MAX);

// Good: Using existing fclamp() from aom_dsp/aom_dsp_common.h
*u += fclamp(step_u * DISFLOW_STEP_SIZE, -2.0, 2.0);
```
