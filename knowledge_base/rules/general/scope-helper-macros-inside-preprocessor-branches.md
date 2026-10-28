---
name: libaom/scope-helper-macros-inside-preprocessor-branches
description: 'When defining helper macros used only within a specific preprocessor conditional branch (#if or #else), scope them inside that branch rather than unconditionally'
rubric: readability
severity: should_fix
---

**Rule.** Define helper macros inside the specific preprocessor conditional
branch (#if, #elif, or #else) where they are used rather than defining them
unconditionally at outer scope when they only apply to that branch. Do not leave
branch-specific helper macros exposed across configurations that do not use
them.

**Why.** Defining helper macros unconditionally when they are only needed in a
specific preprocessor branch pollutes the global namespace, leaves dead or
misleading macros defined in inactive configurations, and obscures configuration
dependencies.

## Examples

Bad:

```cpp
// Bad: Helper macro defined unconditionally above conditional block
#define CHECK_BACKWARD_REFS(ref_frame) \
  (((ref_frame) >= BWDREF_FRAME) && ((ref_frame) <= ALTREF_FRAME))

#if CONFIG_NEW_REF_SIGNALING
#define IS_BACKWARD_REF_FRAME(ref_frame) \
  (get_dir_rank(cm, ref_frame, NULL) == 1)
#else
#define IS_BACKWARD_REF_FRAME(ref_frame) CHECK_BACKWARD_REFS(ref_frame)
#endif  // CONFIG_NEW_REF_SIGNALING
```

Good:

```cpp
// Good: Helper macro scoped strictly inside the preprocessor branch that uses it
#if CONFIG_NEW_REF_SIGNALING
#define IS_BACKWARD_REF_FRAME(ref_frame) \
  (get_dir_rank(cm, ref_frame, NULL) == 1)
#else
#define CHECK_BACKWARD_REFS(ref_frame) \
  (((ref_frame) >= BWDREF_FRAME) && ((ref_frame) <= ALTREF_FRAME))
#define IS_BACKWARD_REF_FRAME(ref_frame) CHECK_BACKWARD_REFS(ref_frame)
#endif  // CONFIG_NEW_REF_SIGNALING
```
