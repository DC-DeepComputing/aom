---
name: libaom/extern-c-guards-in-c-headers
description: When declaring functions or types in C and dual C/C++ headers, wrap C APIs in conditional extern "C" blocks and place C++ declarations in namespace aom
rubric: correctness
severity: should_fix
---

**Rule.** In C and dual C/C++ header files (.h), wrap all C-callable function
and type declarations with `#ifdef __cplusplus \n extern "C" { \n #endif` after
`#include` directives and matching `#ifdef __cplusplus \n } // extern "C" \n
#endif` before the trailing header `#endif` guard. Place C++-only functions,
classes, and types inside a project namespace (`namespace aom`); do not expose
unnamespaced free functions without `extern "C"` at global scope in headers.
Declare shared C interfaces once within the `extern "C"` block rather than
duplicating prototypes across separate `#ifdef __cplusplus` and `#else` blocks,
and do not attach `#else` branches to top-level `#ifndef` header include guards.
Keep internal implementation helpers file-local in source files.

**Why.** C headers are frequently included by C++ translation units, such as
GoogleTest unit tests and C++ tools. Without `extern "C"` linkage
specifications, C++ compilers mangle C symbol names, causing undefined reference
linker errors against C object files. Conversely, exposing unnamespaced C++
functions in dual headers pollutes the global namespace. Duplicating function
prototypes across `#ifdef __cplusplus` and `#else` branches creates unnecessary
maintenance overhead and risks declaration drift, while attaching `#else`
branches to `#ifndef` header guards leaves declarations unreachable upon initial
inclusion. Keeping internal helpers out of headers reduces symbol bloat.

## Examples

Bad:

```cpp
// Bad 1: C header omitting extern "C" guards entirely
#ifndef AOM_AV1_COMMON_SPHERICAL_PRED_H_
#define AOM_AV1_COMMON_SPHERICAL_PRED_H_

#include "config/aom_config.h"

void av1_sphere_to_plane_erp(double x, double y, int width, int height,
                             double *phi, double *theta);

#endif  // AOM_AV1_COMMON_SPHERICAL_PRED_H_

// Bad 2: Duplicating prototypes across #ifdef __cplusplus and #else on header guard
#ifndef AOM_AV1_RATECTRL_RTC_H_
#define AOM_AV1_RATECTRL_RTC_H_

#ifdef __cplusplus
extern "C" {
int av1_ratecontrol_rtc_create(const Config *cfg);
}
#else  // Bug: #else attached to header guard instead of __cplusplus
int av1_ratecontrol_rtc_create(const Config *cfg);
#endif  // AOM_AV1_RATECTRL_RTC_H_

// Bad 3: Declaring dual C/C++ functions at global scope without extern "C" or namespace
struct AV1RateControlRtcConfig { ... };
void AV1RateControlRtcConfigInitDefault(struct AV1RateControlRtcConfig *config);
```

Good:

```cpp
// Good 1: Enclose shared C declarations once within matching extern "C" guards
#ifndef AOM_AV1_COMMON_SPHERICAL_PRED_H_
#define AOM_AV1_COMMON_SPHERICAL_PRED_H_

#include "config/aom_config.h"

#ifdef __cplusplus
extern "C" {
#endif

void av1_sphere_to_plane_erp(double x, double y, int width, int height,
                             double *phi, double *theta);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_COMMON_SPHERICAL_PRED_H_

// Good 2: In dual headers, enclose public C APIs in extern "C" and C++ APIs in namespace aom
#ifdef __cplusplus
extern "C" {
#endif
void av1_ratecontrol_rtc_init_ratecontrol_config(
    AomAV1RateControlRtcConfig *config);
#ifdef __cplusplus
}  // extern "C"

namespace aom {
void InitDefault(AV1RateControlRtcConfig *config);
}  // namespace aom
#endif
```
