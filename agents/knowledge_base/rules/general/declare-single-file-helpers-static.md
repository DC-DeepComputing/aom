---
name: libaom/declare-single-file-helpers-static
description: Define translation-unit-local helpers, constants, macros, typedefs, and structs with static linkage in source files, factor shared macros into headers, and co-locate inline table accessors
rubric: readability
severity: should_fix
---

**Rule.** Define helper functions, comparator callbacks, macros, buffer offsets
(e.g., ROW_OFFSET), internal constants, lookup tables, typedefs (such as
internal function pointer types), and private structs in the implementation
source file (.c / .cc) with internal linkage (static, static const, static
INLINE, or static AOM_INLINE) or in architecture-specific headers whenever they
are used only within a single translation unit or guarded by optional feature
flags (e.g., #if CONFIG_*), and #undef strictly local helper macros after use.
Do not expose file-local helper declarations, macros, typedefs, or tuning
constants in shared generic header files (.h / .hpp), and do not define
file-local functions or constants with default external linkage in source files
unless explicitly exported for cross-module use or RTCD. When macro constants or
inline helpers are shared across multiple translation units or SIMD backends,
factor them into a single common header rather than duplicating definitions.
Conversely, when constant lookup tables are indexed by static inline accessor
functions defined in a header for use across translation units, define the
tables directly in the header as static const alongside their accessors (instead
of declaring them extern const with a definition in a .c file) to enable
constant folding and inlining optimizations.

**Why.** In C/C++, functions and file-scope const variables have external
linkage by default unless declared static. Exposing translation-unit-local
helpers, macros, buffer offsets, typedefs, or constants in shared headers
pollutes header namespaces, leaks private implementation details, increases
build coupling, risks symbol and macro name collisions, and triggers unnecessary
recompilations. Conversely, duplicating macro definitions or inline helpers
across files creates maintenance overhead and logic divergence across backends.
Confining file-local helpers to source files with internal (static) linkage
enables compiler optimizations like dead-code elimination and local inlining;
co-locating cross-unit lookup tables with their static inline header accessors
allows the compiler to perform constant folding during cross-unit inlining
rather than blocking optimizations behind extern const references.

## Examples

Bad:

```cpp
// Variant 1: Exposing TU-local helpers, macros, or tables in shared headers
// In av1/common/onyxc_int.h:
static INLINE void reset_frame_buffers(AV1_COMMON *cm) { /* only used in decodeframe.c */ }
#define VAR_BOOST_MAX_DELTAQ_RANGE 80

// In warped_motion.h: table only used in warped_motion.c leaks into header
static const int sine_values[64] = { ... };

// In warped_motion.h: inline accessor separated from extern table in .c
extern const int error_measure_lut[512];
static INLINE int error_measure(int err) { return error_measure_lut[255 + err]; }

// Variant 2: Missing static linkage on TU-local constants/helpers in source file
// In av1/common/cnn.c:
const float RGBtoYUVMatrix[9] = { 0.2126f, 0.7152f, 0.0722f, /* ... */ };
int check_tensor_equal_dims(const TENSOR *t1, const TENSOR *t2) { /* ... */ }

// Variant 3: Duplicating shared macros/helpers across headers or leaking arch offsets
// In av1/common/cnn_wrapper.h and av1/common/cnn_restore_models.h:
#define MIN_CNN_Q_INDEX 100
// In av1/common/resize.h (architecture-specific offset leaked to generic header):
#define ROW_OFFSET 5
```

Good:

```cpp
// Variant 1: Keep TU-local helpers, macros, and tables static in source file
// In av1/decoder/decodeframe.c:
static INLINE void reset_frame_buffers(AV1_COMMON *cm) { /* ... */ }

// In av1/encoder/allintra_vis.c:
#define VAR_BOOST_MAX_DELTAQ_RANGE 80

// In warped_motion.c:
static const int sine_values[64] = { ... };

// In warped_motion.h: table co-located with inline accessor for constant folding
static const int error_measure_lut[512] = { ... };
static INLINE int error_measure(int err) { return error_measure_lut[255 + err]; }

// Variant 2: File-local helpers and constants declared static in source file
// In av1/common/cnn.c:
static const float RGBtoYUVMatrix[9] = { 0.2126f, 0.7152f, 0.0722f, /* ... */ };
static INLINE int check_tensor_equal_dims(const TENSOR *t1, const TENSOR *t2) { /* ... */ }

// Variant 3: Factor shared macros into common header; keep arch offsets local
// In av1/common/cnn_restore_models.h:
#define MIN_CNN_Q_INDEX 100
// In av1/common/cnn_wrapper.h:
#include "av1/common/cnn_restore_models.h"

// In av1/common/x86/resize_avx2.c and resize_sse2.c:
#define ROW_OFFSET 5
#undef ROW_OFFSET
```
