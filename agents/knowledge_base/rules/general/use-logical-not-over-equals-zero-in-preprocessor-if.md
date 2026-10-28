---
name: libaom/use-logical-not-over-equals-zero-in-preprocessor-if
description: 'When testing boolean or configuration macros in preprocessor #if directives, use the logical NOT operator (!FLAG) instead of comparing with == 0'
rubric: readability
severity: nit
---

**Rule.** In preprocessor `#if` and `#elif` directives testing binary or boolean
configuration flags (such as `CONFIG_*`), use the logical NOT operator (`!FLAG`)
instead of explicit comparisons to zero (`FLAG == 0` or `(FLAG == 0)`). Mirror
the same concise `!FLAG` syntax in accompanying `#endif` trailing comments.

**Why.** Using `!FLAG` is standard, idiomatic C preprocessor syntax across
libaom. Explicit `== 0` comparisons add unnecessary visual clutter and diverge
from codebase conventions.

## Examples

Bad:

```cpp
#if (CONFIG_REALTIME_ONLY == 0) && (REVERT_NEW_FIRSTPASS_STATS == 0)
double motion_ratio = av1_setup_motion_ratio(cpi);
#endif  // (CONFIG_REALTIME_ONLY == 0) && (REVERT_NEW_FIRSTPASS_STATS == 0)
```

Good:

```cpp
#if !CONFIG_REALTIME_ONLY && !REVERT_NEW_FIRSTPASS_STATS
double motion_ratio = av1_setup_motion_ratio(cpi);
#endif  // !CONFIG_REALTIME_ONLY && !REVERT_NEW_FIRSTPASS_STATS
```
