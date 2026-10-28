---
name: libaom/retain-constexpr-in-dual-c-cpp-headers
description: When defining constants in dual C/C++ headers, use anonymous enums with kCamelCase names for integers and align constexpr/#define names for non-integers
rubric: readability
severity: should_fix
---

**Rule.** In C and C++ dual-compatible header files, define compile-time integer
constants once using an anonymous enum with kCamelCase names rather than untyped
#define macros (avoid kCamelCase naming on preprocessor macros) or duplicating
definitions across #ifdef __cplusplus (as constexpr) and #else (as #define). For
non-integer constants (such as floating-point values) where enums cannot be
used, retain typed constexpr definitions under #ifdef __cplusplus and provide
matching #define definitions under #else with identical identifier names across
both branches (avoiding ALL_CAPS in C vs. kCamelCase in C++) so shared struct
fields, array bounds, and function prototypes do not branch on __cplusplus.

**Why.** Anonymous enums are valid in both C and C++, allowing compile-time
integer constants to be shared unconditionally without preprocessor duplication
while preserving type information, debugger symbol visibility, and kCamelCase
naming conventions. For non-integer constants where #ifdef __cplusplus branching
is necessary, aligning identifier names between C++ constexpr and C #define
allows shared structs, array bounds, and function signatures to be declared
unconditionally, eliminating repetitive #ifdef blocks and preventing
configuration drift.

## Examples

Bad:

```cpp
// Bad 1: Defining integer constants as #define macros with kCamelCase names
#define kAomMaxLayers 32

// Bad 2: Duplicating integer constants across #ifdef __cplusplus and #else
#ifdef __cplusplus
static constexpr size_t kAomAV1MaxLayers = 32;
#else
#define kAomAV1MaxLayers 32
#endif

// Bad 3: Divergent constant names forcing preprocessor branching in shared structs
#ifdef __cplusplus
static constexpr float kMinRateFactor = 0.01f;
#else
#define MIN_RATE_FACTOR 0.01f
#endif

struct AV1RateControlRtcConfig {
#ifdef __cplusplus
  float min_rate[kMinRateFactor];
#else
  float min_rate[MIN_RATE_FACTOR];
#endif
};
```

Good:

```cpp
// Good 1: Define integer constants once using an anonymous enum with kCamelCase names
enum {
  kAomAV1MaxLayers = 32,
  kAomAV1MaxTemporalLayers = 8,
  kAomAV1MaxSpatialLayers = 4,
};

// Good 2: For non-integer constants, retain typed constexpr and matching #define with identical names
#ifdef __cplusplus
static constexpr float kMinRateFactor = 0.01f;
#else
#define kMinRateFactor 0.01f
#endif

// Good 3: Identical constant names allow unconditional struct definitions
struct AV1RateControlRtcConfig {
  int layer_target_bitrate[kAomAV1MaxLayers];
};
```
