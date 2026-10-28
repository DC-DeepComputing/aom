---
name: libaom/name-boolean-state-flags-for-specific-phase
description: When declaring boolean state or initialization flags in structs, name them specifically after the tracked phase or subsystem rather than using generic names like initial_setup.
rubric: readability
severity: nit
---

**Rule.** Name boolean state and initialization flags in structs specifically
after the exact subsystem, property, or pipeline phase they track (such as
frame_size_related_setup_done or tables_initialized). Avoid generic, ambiguous
identifiers like initial_setup, is_setup, or bare is_initialized in multi-stage
structs where multiple distinct initialization phases exist.

**Why.** In complex or multi-stage context structs, overly broad flag names like
initial_setup obscure which specific resources or parameters have been
initialized. Specific identifiers clarify the exact state transition being
tracked and prevent incorrect assumptions about overall struct readiness.

## Examples

Bad:

```cpp
struct AV1_COMP {
  // Generic names obscure which exact subsystem or phase was prepared
  bool initial_setup;
  bool is_initialized;
};
```

Good:

```cpp
struct AV1_COMP {
  // Explicitly identifies the specific subsystem and setup phase
  bool frame_size_related_setup_done;
  bool tables_initialized;
};
```
