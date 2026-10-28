---
name: libaom/use-negative-one-as-error-sentinel-for-index-returns
description: When handling 0-based array indices, lookup IDs, or slot positions, use -1 or invalid sentinel constants rather than 0 for error returns and search initialization
rubric: correctness
severity: should_fix
---

**Rule.** In functions and search routines that handle 0-based array indices,
table IDs, or slot positions, use -1 (or a dedicated invalid sentinel constant
such as INVALID_INDEX) rather than 0 to represent failure, error, or missing
elements:
1. Functions returning 0-based indices or lookup IDs must return -1 (or the invalid sentinel) on lookup failure or allocation/validation error rather than returning 0. Callers must check for negative return values or the sentinel before using the index.
2. Search tracking variables (such as best_idx, match_idx, or slot positions) must be initialized to -1 (or the invalid sentinel) before entering search loops rather than initialized to 0.

**Why.** In 0-based indexing (C and C++ arrays and tables), index 0 is a valid
slot corresponding to the first element. Initializing tracking variables to 0 or
returning 0 on failure conflates errors and 'not found' states with valid index
0. This causes downstream validity checks (such as if (idx < 0) or if (id ==
-1)) to fail to detect the error, mistakenly processing invalid lookups as valid
first elements or corrupting table entries at slot 0.

## Examples

Bad:

```cpp
// Variant 1: Initializing search index tracking variable to 0
int best_idx = 0;
for (int i = 0; i < num_elements; ++i) {
  if (items[i].is_valid && items[i].score > best_score) {
    best_score = items[i].score;
    best_idx = i;
  }
}
if (best_idx < 0) return;  // Never reached when no items match; treats items[0] as match

// Variant 2: Returning 0 on lookup or allocation failure
int find_or_add_symbol(Dictionary *dict, const char *str) {
  int id = lookup_symbol(dict, str);
  if (id != -1) return id;
  dict->strs[dict->num_strs] = strdup(str);
  if (!dict->strs[dict->num_strs]) {
    return 0;  // Bug: 0 is a valid index for dict->strs[0]
  }
  return dict->num_strs++;
}
```

Good:

```cpp
// Variant 1: Initializing search index tracking variable to -1 sentinel
int best_idx = -1;
for (int i = 0; i < num_elements; ++i) {
  if (items[i].is_valid && items[i].score > best_score) {
    best_score = items[i].score;
    best_idx = i;
  }
}
if (best_idx < 0) return;  // Correctly triggers bailout when no items match

// Variant 2: Returning -1 sentinel on lookup or allocation failure
int find_or_add_symbol(Dictionary *dict, const char *str) {
  int id = lookup_symbol(dict, str);
  if (id != -1) return id;
  dict->strs[dict->num_strs] = strdup(str);
  if (!dict->strs[dict->num_strs]) {
    return -1;  // Returns -1 to signal failure to the caller
  }
  return dict->num_strs++;
}
```
