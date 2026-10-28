---
name: libaom/aom/avoid-overloading-tuning-enums-with-detection-modes
description: When adding algorithmic options or detection modes to the public API, introduce dedicated codec control IDs instead of overloading content tuning enums
rubric: design
severity: should_fix
---

**Rule.** Do not overload existing configuration enums or codec controls
representing content categories or tuning metrics (such as AV1E_SET_TUNE_CONTENT
/ aom_tune_content) with orthogonal algorithmic options or detection modes.
Introduce dedicated codec control IDs and separate mode enums so users can
configure algorithmic detection independently from content tuning.

**Why.** Content tuning parameters classify the overarching category of content
being encoded (e.g., default, screen, film), whereas detection modes specify how
specific features or content types are detected or processed algorithmically.
Combining them into one enum couples orthogonal settings and prevents users from
tuning content categories and detection behaviors independently.

## Examples

Bad:

```cpp
// In aom/aomcx.h:
typedef enum {
  AOM_CONTENT_DEFAULT,
  AOM_CONTENT_SCREEN,
  AOM_CONTENT_FILM,
  AOM_CONTENT_ENHANCED,  // Bad: couples algorithmic detection into content category enum
} aom_tune_content;
```

Good:

```cpp
// In aom/aomcx.h:
/*!\brief Codec control function to set content type, aom_tune_content */
AV1E_SET_TUNE_CONTENT = 43,

/*!\brief Codec control function to set screen content tools detection mode */
AV1E_SET_SCREEN_CONTENT_TOOLS_DETECTION_MODE = 171,
```
