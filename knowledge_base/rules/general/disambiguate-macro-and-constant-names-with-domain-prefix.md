---
name: libaom/disambiguate-macro-and-constant-names-with-domain-prefix
description: Prefix and qualify macros, constants, unscoped enums, and lookup tables with concrete subsystem, feature, or domain tags rather than generic names or bare numeric suffixes
rubric: readability
severity: should_fix
---

**Rule.** Prefix preprocessor macros, constants, unscoped enum values, lookup
tables, and transform basis matrices with their specific domain, subsystem,
feature, or entity tag (such as `TX_` for transform, `MV_` for motion vectors,
`CDEF_` for filtering, `AVX_WORKER_` for worker thread status, `DIFFWTD_` for
difference-weighted mode, or `lgt_` under `CONFIG_LGT`). Avoid bare, unqualified
names (like `MAX_PARTITIONS`, `DEFAULT_THRESHOLD`, or `klt4_inter`) and overly
broad category prefixes (like `INTERINTER_` where a sub-mode tag like
`DIFFWTD_`, `single_`, or `comp_` applies). Additionally, when defining
constants or macros for codec levels, profiles, tiers, or bit depths, include
the explicit domain qualifier (`LEVEL_`, `PROFILE_`, `TIER_`, or `BITDEPTH_`)
before numeric conditions or limits rather than appending bare digits (e.g.,
`_LEVEL_7_AND_ABOVE` instead of `_7_AND_ABOVE`).

**Why.** In a large C codebase lacking namespaces, macros, constants, lookup
tables, and unscoped enum values reside in a shared global namespace across
translation units. Generic names, bare identifiers, or overly broad prefixes
cause symbol collisions, create ambiguity between distinct subsystems or build
configurations, and obscure which specific tool, prediction mode, or level bound
applies. Explicit domain, feature, and entity qualifiers make symbols
self-describing, distinct, and easily searchable.

## Examples

Bad:

```cpp
// Bad: Generic macro name and enum prefix in shared header
#define MAX_PARTITIONS 4
typedef enum { AVX_STATUS_NOT_OK = 0, AVX_STATUS_OK } AVxWorkerStatus;

// Bad: Overly broad category prefix and bare feature table name
#define INTERINTER_MASK_CONTEXTS 4
#if CONFIG_LGT
static const int32_t klt4_inter[16] = { /* ... */ };
#endif  // CONFIG_LGT

// Bad: Ambiguous numeric suffix lacking domain qualifier
#define MAX_TILE_AREA_7_AND_ABOVE (4096 * 4608)
```

Good:

```cpp
// Good: Prefixed with specific subsystem and entity domain
#define MAX_TX_PARTITIONS 4
typedef enum { AVX_WORKER_STATUS_NOT_OK = 0, AVX_WORKER_STATUS_OK } AVxWorkerStatus;

// Good: Prefixed with concrete sub-mode and feature tags
#define DIFFWTD_MASK_CONTEXTS 4
#if CONFIG_LGT
static const int32_t lgt_klt4_inter[16] = { /* ... */ };
#endif  // CONFIG_LGT

// Good: Explicit domain qualifier before numeric limit
#define MAX_TILE_AREA_LEVEL_7_AND_ABOVE (4096 * 4608)
```
