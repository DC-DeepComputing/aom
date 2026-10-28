---
name: libaom/prefer-screen-over-sct-in-identifiers
description: When naming screen content codec controls, configuration enums, types, or identifiers, spell out SCREEN rather than using ambiguous SCT acronyms
rubric: readability
severity: nit
---

**Rule.** When naming public codec controls, configuration enums, type
definitions, and internal identifiers for screen content features across libaom
headers and source files, spell out SCREEN or screen_content (such as
AOM_SCREEN_*, aom_screen_detection_mode, and AV1E_SET_SCREEN_*) rather than
using abbreviated acronyms like SCT or sct. Do not use SCT in public API headers
(such as aom/aomcx.h), codec interface implementations, or internal enum
definitions.

**Why.** Public codec controls and API types form the external interface of
libaom, while internal identifiers maintain codebase readability and
searchability. Using ambiguous, non-standard acronyms like SCT obscures
functionality and creates inconsistency with established libaom conventions
(such as AOM_CONTENT_SCREEN and aom_screen_detection_mode). Spelling out SCREEN
ensures clear intent and consistency across both public API headers and internal
codec code.

## Examples

Bad:

```cpp
// Bad: Using abbreviated SCT acronym in public API and internal enum definitions
typedef enum {
  AOM_SCT_MODE_OFF,
  AOM_SCT_MODE_ON,
} SCT_DETECTION_MODE;

AV1E_SET_SCREEN_CONTENT_TOOLS_DETECTION_MODE,

// Bad: Internal enum using SCT prefix
typedef enum {
  SCT_DETECTION_STANDARD = 1,
  SCT_DETECTION_ANTIALIAS_AWARE = 2,
} UENUM1BYTE(SCT_DETECTION_MODE);
```

Good:

```cpp
// Good: Spelled out SCREEN matching libaom public API conventions
typedef enum {
  AOM_SCREEN_DETECTION_STANDARD = 1,
  AOM_SCREEN_DETECTION_ANTIALIASING_AWARE = 2,
} aom_screen_detection_mode;

AV1E_SET_SCREEN_CONTENT_DETECTION_MODE,

// Good: Internal identifiers and enums spelling out SCREEN
typedef enum {
  AOM_SCREEN_DETECTION_STANDARD = 1,
  AOM_SCREEN_DETECTION_ANTIALIAS_AWARE = 2,
} UENUM1BYTE(aom_screen_detection_mode);
```
