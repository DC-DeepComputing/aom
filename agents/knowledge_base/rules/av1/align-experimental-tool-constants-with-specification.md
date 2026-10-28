---
name: libaom/av1/align-experimental-tool-constants-with-specification
description: When porting or enabling experimental coding tools, configure constants to match the adopted codec specification rather than obsolete research parameters
rubric: correctness
severity: should_fix
---

**Rule.** When porting, standardizing, or enabling experimental coding tools
guarded by #if CONFIG_..., verify that tool configuration macros and candidate
limits (such as reference motion vector list sizes) reflect the adopted
specification values rather than preliminary or outdated experimental
parameters.

**Why.** Carrying over obsolete or exploratory experimental constants into
standardized tool configurations introduces specification discrepancies, breaks
interoperability with specification-compliant bitstreams, and confuses
downstream reviewers.

## Examples

Bad:

```cpp
// Bad: Obsolete research parameter used for adopted tool configuration
#if CONFIG_NEW_INTER_MODES
#define MAX_DRL_BITS 5
#endif
```

Good:

```cpp
// Good: Constant updated to match adopted specification limit
#if CONFIG_NEW_INTER_MODES
#define MAX_DRL_BITS 3
#endif
```
