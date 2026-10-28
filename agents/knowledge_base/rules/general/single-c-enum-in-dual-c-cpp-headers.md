---
name: libaom/single-c-enum-in-dual-c-cpp-headers
description: In dual C/C++ headers, declare typedef enums with matching tag names and maintain declaration order parity for C++ aliases
rubric: readability
severity: should_fix
---

**Rule.** In header files shared between C and C++, declare enums once using a
single C-compatible typedef enum with an explicit enum tag matching the typedef
identifier (e.g., 'typedef enum Name { ... } Name;') rather than anonymous enums
('typedef enum { ... } Name;') or parallel #ifdef __cplusplus enum class and
#else enum definitions. When providing C++ using type aliases and constexpr
constant aliases inside #ifdef __cplusplus (such as in namespace aom), maintain
the same declaration order as the underlying C definitions.

**Why.** Defining an enum with a matching tag name ('typedef enum Name { ... }
Name;') ensures C code can refer to the type as either 'Name' or 'enum Name' and
enables forward declarations. Using a single C enum definition guarantees ABI
consistency without preprocessor duplication, while maintaining declaration
order for C++ aliases preserves readability across the dual C/C++ interface.

## Examples

Bad:

```cpp
// Anti-pattern 1: Anonymous typedef enum lacks tag name for C forward declarations.
typedef enum {
  kAomKeyFrame,
  kAomInterFrame,
} AomFrameType;

// Anti-pattern 2: Duplicating enum class vs C enum across preprocessor branches.
#ifdef __cplusplus
enum class FrameDropDecision {
  kOk,
  kDrop,
};
#else
typedef enum AomFrameDropDecision {
  kAomFrameDropDecisionOk,
  kAomFrameDropDecisionDrop,
} AomFrameDropDecision;
#endif

// Anti-pattern 3: C++ aliases declared out of order relative to C definitions.
typedef enum AomFrameType {
  kAomKeyFrame,
  kAomInterFrame,
} AomFrameType;
typedef enum AomFrameDropDecision {
  kAomFrameDropDecisionOk,
  kAomFrameDropDecisionDrop,
} AomFrameDropDecision;

#ifdef __cplusplus
namespace aom {
using FrameDropDecision = AomFrameDropDecision;  // Out of order
using FrameType = AomFrameType;
}  // namespace aom
#endif
```

Good:

```cpp
// Declare enums once using C-compatible typedefs with explicit matching tag names.
typedef enum AomFrameType {
  kAomKeyFrame,
  kAomInterFrame,
} AomFrameType;

typedef enum AomFrameDropDecision {
  kAomFrameDropDecisionOk,
  kAomFrameDropDecisionDrop,
} AomFrameDropDecision;

// C++ aliases in namespace aom maintaining declaration order parity.
#ifdef __cplusplus
namespace aom {
using FrameType = AomFrameType;
using FrameDropDecision = AomFrameDropDecision;
constexpr FrameDropDecision kFrameDropDecisionOk = kAomFrameDropDecisionOk;
constexpr FrameDropDecision kFrameDropDecisionDrop = kAomFrameDropDecisionDrop;
}  // namespace aom
#endif
```
