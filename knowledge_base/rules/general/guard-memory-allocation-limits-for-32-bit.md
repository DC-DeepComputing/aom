---
name: libaom/guard-memory-allocation-limits-for-32-bit
description: When defining memory allocation caps or buffer size limit macros, guard values for 32-bit platforms and keep 32-bit limits below INT_MAX
rubric: correctness
severity: should_fix
---

**Rule.** When defining memory allocation caps, maximum buffer sizes, or memory
thresholds that can exceed 2–4 GB on 64-bit platforms, guard the definition for
32-bit targets (such as via '#if SIZE_MAX > (1ULL << 32)'). On 32-bit
architectures, ensure the allocation cap is set below INT_MAX (e.g., '((1ULL <<
31) - (1 << 16))') to prevent integer overflow and allocator or Valgrind
warnings.

**Why.** Hardcoding allocation limits larger than 4 GB exceeds addressable
memory and SIZE_MAX on 32-bit platforms, leading to integer truncation or
comparison overflow. Capping 32-bit allocation thresholds below INT_MAX also
avoids signed-integer overflow hazards and tool warnings.

## Examples

Bad:

```cpp
#ifndef AOM_MAX_ALLOCABLE_MEMORY
#define AOM_MAX_ALLOCABLE_MEMORY 8589934592  // 8 GB
#endif
```

Good:

```cpp
#ifndef AOM_MAX_ALLOCABLE_MEMORY
#if SIZE_MAX > (1ULL << 32)
#define AOM_MAX_ALLOCABLE_MEMORY 8589934592  // 8 GB
#else
// For 32-bit targets keep this below INT_MAX to avoid valgrind warnings.
#define AOM_MAX_ALLOCABLE_MEMORY ((1ULL << 31) - (1 << 16))
#endif
#endif
```
