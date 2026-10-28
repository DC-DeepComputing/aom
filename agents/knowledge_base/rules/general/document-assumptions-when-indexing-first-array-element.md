---
name: libaom/document-assumptions-when-indexing-first-array-element
description: When inspecting or operating on only the first element of a multi-entry array configuration, document the limitation or assumption with an explanatory comment or TODO
rubric: readability
severity: should_fix
---

**Rule.** When inspecting, validating, or operating on only the first element
(e.g., `[0]`) of an array parameter or multi-element configuration structure
where multiple entries could exist, document the assumption or limitation with
an explanatory comment or attributed `TODO`. Clarify why other entries are
unhandled (such as scalable or multi-layer coding not yet being supported).

**Why.** Multi-element array fields imply multi-entry support (such as multiple
operating points, layers, or channels). Accessing only index `[0]` without
documentation leaves ambiguity as to whether multi-entry handling was forgotten,
is an intentional invariant, or is deferred work.

## Examples

Bad:

```cpp
#if CONFIG_CWG_C013
  else if (level_params->target_seq_level_idx[0] >= SEQ_LEVEL_7_0 &&
           level_params->target_seq_level_idx[0] <= SEQ_LEVEL_8_3) {
    // Process level for index 0 without documenting why other entries are ignored.
  }
#endif
```

Good:

```cpp
#if CONFIG_CWG_C013
  // TODO(username): Currently target level is only handled for the 0th operating
  // point; scalable coding is not yet supported.
  else if (level_params->target_seq_level_idx[0] >= SEQ_LEVEL_7_0 &&
           level_params->target_seq_level_idx[0] <= SEQ_LEVEL_8_3) {
    // Process level for index 0 with documented limitation.
  }
#endif
```
