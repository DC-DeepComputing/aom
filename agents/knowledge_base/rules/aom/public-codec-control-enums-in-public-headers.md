---
name: libaom/aom/public-codec-control-enums-in-public-headers
description: When defining enum types or structs for public codec controls, declare them in public API headers with aom_<name> and AOM_<VALUE> naming rather than internal headers
rubric: design
severity: should_fix
---

**Rule.** Define enum types and structures used by public codec controls in
public headers (such as aom/aomcx.h or aom/aomdx.h) rather than in internal
encoder or decoder headers. Name public parameter types using the aom_<name>
prefix and enumerators using the AOM_<VALUE> prefix so external API callers can
access them.

**Why.** Codec controls exposed via aom_codec_control form part of libaom's
public API contract and require external callers to supply their parameter
types. Defining parameter enums in internal subsystem headers prevents external
applications from accessing the types and violates API encapsulation boundaries.

## Examples

Bad:

```cpp
// In av1/encoder/encoder.h
enum {
  SCT_DETECTION_STANDARD = 1,
  SCT_DETECTION_ANTIALIAS_AWARE = 2,
} UENUM1BYTE(SCT_DETECTION_MODE);
```

Good:

```cpp
// In aom/aomcx.h
/*!\brief Screen content detection mode */
typedef enum {
  AOM_SCREEN_DETECTION_STANDARD = 1,
  AOM_SCREEN_DETECTION_ANTIALIAS_AWARE = 2,
} aom_screen_detection_mode;
```
