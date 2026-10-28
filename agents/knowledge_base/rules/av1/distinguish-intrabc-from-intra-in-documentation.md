---
name: libaom/av1/distinguish-intrabc-from-intra-in-documentation
description: When documenting or naming Intra Block Copy features, flags, or parameters, use 'intrabc' or 'intra BC' rather than 'intra' to prevent confusion with standard intra prediction
rubric: readability
severity: nit
---

**Rule.** When documenting or naming features, speed flags, configuration
options, or parameters specific to Intra Block Copy (IntraBC), explicitly use
'intrabc' or 'intra BC' (or 'intra block copy') rather than generic 'intra'. Do
not refer to IntraBC operations as simply 'intra' in comments or identifier
descriptions, as IntraBC is motion compensation within the current picture and
distinct from standard spatial intra prediction.

**Why.** In AV1, standard intra prediction (spatial directional prediction) and
Intra Block Copy (IntraBC, block-vector-based motion compensation within
reconstructed areas of the current frame for screen content) are fundamentally
different coding tools. Referring to IntraBC features as 'intra' creates
ambiguity about whether an optimization or heuristic affects spatial intra modes
or IntraBC screen-content coding.

## Examples

Bad:

```cpp
// Prune intra candidate block hash search
int prune_intrabc_candidate_block_hash_search;

// Fast search for intra mode evaluation
int fast_intra_hash_search;
```

Good:

```cpp
// Prune intrabc candidate block hash search
int prune_intrabc_candidate_block_hash_search;

// Fast search for intrabc mode evaluation
int fast_intrabc_hash_search;
```
