---
name: libaom/prefix-c-api-types-and-enum-constants
description: When declaring types, enums, constants, or type-specific helper functions in C/C++ API headers, prefix identifiers with matching project and subsystem names
rubric: readability
severity: should_fix
---

**Rule.** Prefix types (structs, enums, unions, typedefs), enumerator constants,
and type-specific helper functions declared in C or dual C/C++ global headers
with the appropriate project or subsystem prefix (such as Aom for type names and
functions, kAom or kAomAV1 for constants and enum values, or aom_ for C
identifiers). Never expose unprefixed type names or generic enumerator
identifiers (e.g., kOk, kDrop) in global headers without C++ namespace
encapsulation. Furthermore, when defining constants, enums, or helper functions
(such as <Type>InitDefault) associated with a specific struct, match the full
project and codec prefix of the target type (e.g.,
AomAV1RateControlRtcConfigInitDefault and kAomAV1MaxLayers for
AomAV1RateControlRtcConfig).

**Why.** In C and C-compatible headers lacking namespace encapsulation, types,
enums, and global symbols reside in the global namespace of client applications.
Unprefixed names or generic enumerator identifiers risk collisions with
third-party libraries and system headers. Consistent prefixing across types,
associated constants, and helper routines avoids ambiguity across codecs and
ensures identifiers are predictable and searchable.

## Examples

Bad:

```cpp
// Bad: Unprefixed struct/enum types, generic enumerators, and mismatched helper prefixes.
enum FrameDropDecision {
  kOk,
  kDrop,
};

#define kAomMaxLayers 32

struct AomAV1RateControlRtcConfig {
  int layer_target_bitrate[kAomMaxLayers];
};

// Missing the 'Aom' library prefix present on the struct type
void AV1RateControlRtcConfigInitDefault(AomAV1RateControlRtcConfig *config);
```

Good:

```cpp
// Good: Prefixed with project/subsystem identifier, enum names, and matching helper prefix.
typedef enum AomFrameDropDecision {
  kAomFrameDropDecisionOk,
  kAomFrameDropDecisionDrop,
} AomFrameDropDecision;

enum {
  kAomAV1MaxLayers = 32,
};

typedef struct AomAV1RateControlRtcConfig {
  int layer_target_bitrate[kAomAV1MaxLayers];
} AomAV1RateControlRtcConfig;

// Function prefix matches the full struct type name exactly
void AomAV1RateControlRtcConfigInitDefault(AomAV1RateControlRtcConfig *config);
```
