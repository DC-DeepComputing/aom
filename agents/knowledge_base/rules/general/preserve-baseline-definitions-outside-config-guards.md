---
name: libaom/preserve-baseline-definitions-outside-config-guards
description: 'When adding feature-flagged code under #if CONFIG_..., preserve baseline constants and logic intact in #else branches without re-expressing them'
rubric: readability
severity: should_fix
---

**Rule.** When adding or modifying feature-flagged code guarded by #if
CONFIG_..., preserve baseline definitions, macro constants, and logic in the
#else or non-config branch exactly as they were in the original baseline. Do not
rewrite or re-express baseline values (even if mathematically equivalent under
the baseline configuration) using new feature-specific parameters or formulas.

**Why.** Unnecessarily modifying or re-expressing baseline constants in #else
branches increases diff noise, complicates code review, and risks introducing
unintended regressions or behavioral changes when the feature flag is disabled
in baseline builds.

## Examples

Bad:

```cpp
#if CONFIG_NEW_INTER_MODES
#define USABLE_REF_MV_STACK_SIZE (MAX_DRL_BITS + 1)
#else
#define USABLE_REF_MV_STACK_SIZE (MAX_DRL_BITS + 2)
#endif  // CONFIG_NEW_INTER_MODES
```

Good:

```cpp
#if CONFIG_NEW_INTER_MODES
#define USABLE_REF_MV_STACK_SIZE (MAX_DRL_BITS + 1)
#else
#define USABLE_REF_MV_STACK_SIZE 4
#endif  // CONFIG_NEW_INTER_MODES
```
