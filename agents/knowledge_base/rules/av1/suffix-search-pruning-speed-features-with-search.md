---
name: libaom/av1/suffix-search-pruning-speed-features-with-search
description: When defining speed features or flags that prune encoder search stages, append an explicit _search suffix to the identifier name
rubric: readability
severity: nit
---

**Rule.** When adding or renaming speed features, struct members, or encoder
flags that prune candidate evaluations during an encoder search pass, include an
explicit `_search` suffix in the identifier name (e.g., `prune_<target>_search`
or `prune_<target>_search_level`). Do not omit `_search` when the pruning
heuristic specifically targets candidate evaluation or search exploration.

**Why.** In the AV1 encoder, many speed features constrain or bypass search
passes. Suffixing search-pruning speed features with `_search` clearly
distinguishes search-pruning heuristics from transform, filtering, or partition
mode flags and maintains consistent naming across `speed_features.h`.

## Examples

Bad:

```cpp
// In IntraBCSpeedFeatures or PartitionSpeedFeatures:
int prune_intrabc_candidate_block_hash;
int prune_part4;
```

Good:

```cpp
// In IntraBCSpeedFeatures or PartitionSpeedFeatures:
int prune_intrabc_candidate_block_hash_search;
int prune_part4_search;
```
