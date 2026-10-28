---
name: libaom/use-symbolic-constants-for-derived-values
description: 'When defining derived constants, enum counts, or feature-flagged configuration macros, use symbolic references and fallback #else definitions instead of raw literals'
rubric: readability
severity: should_fix
---

**Rule.** When defining macro constants, enumeration values, or total count
limits that derive from established base parameters, define them in terms of
existing symbolic constants or preceding enumerators (e.g., LAST_ENUM + 1)
rather than duplicating raw numeric literals. When introducing feature-flagged
configuration parameters under #if CONFIG_..., define fallback default macros or
constants in the #else branch and reference the named constant across all call
sites rather than hardcoding literal numbers.

**Why.** Hardcoding raw numeric literals obscures semantic relationships between
limits and causes silent inconsistencies or build breakages when base parameters
change. Defining fallback named constants in #else branches ensures call sites
across decoder, encoder, and bitstream files stay consistent across feature
configurations without relying on magic numbers.

## Examples

Bad:

```cpp
// Bad: Hardcoding raw literals breaks on refactoring or toggling build flags
#define EXTREF_FRAME 8
#define TOTAL_FRAME_BUFFERS 13  // 8 ref frames + 1 scratch + 4 inter refs

enum {
  RESTORE_NONE,
  RESTORE_WIENER,
#if CONFIG_LOOP_RESTORE_CNN
  RESTORE_CNN,
#endif  // CONFIG_LOOP_RESTORE_CNN
  RESTORE_SWITCHABLE,
  RESTORE_TYPES = 5,  // Hardcoded count invalid when CONFIG is disabled
};

#if CONFIG_NEW_INTER_MODES
#define MAX_DRL_BITS 5
#endif
// Magic number 3 used when feature is disabled:
int range = AOMMIN(xd->ref_mv_count[ref_frame_type] - 1, 3);
```

Good:

```cpp
// Good: Define derived constants symbolically and provide #else fallbacks
#define EXTREF_FRAME REF_FRAMES
#define TOTAL_FRAME_BUFFERS (REF_FRAMES + 1 + INTER_REFS_PER_FRAME)

enum {
  RESTORE_NONE,
  RESTORE_WIENER,
#if CONFIG_LOOP_RESTORE_CNN
  RESTORE_CNN,
#endif  // CONFIG_LOOP_RESTORE_CNN
  RESTORE_SWITCHABLE,
  RESTORE_TYPES = RESTORE_SWITCHABLE + 1,
};

#if CONFIG_NEW_INTER_MODES
#define MAX_DRL_BITS 5
#else
#define MAX_DRL_BITS 3
#endif
int range = AOMMIN(xd->ref_mv_count[ref_frame_type] - 1, MAX_DRL_BITS);
```
