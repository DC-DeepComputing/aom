---
name: libaom/use-directional-qualifiers-in-hierarchical-flag-names
description: When declaring boolean flags or struct members representing hierarchical or reference relationships, use explicit directional qualifiers (e.g., has_lower_quality_layer rather than has_quality_layer)
rubric: readability
severity: nit
---

**Rule.** When declaring boolean flags, struct members, or configuration
variables that represent hierarchical, layered, or relational dependencies (such
as SVC spatial or temporal layers, reference frames, or pipeline stages), use
explicit directional or relational qualifiers (such as lower, higher, parent,
child, prev, or next; e.g., has_lower_quality_layer rather than
has_quality_layer). The identifier must clearly indicate whether the property
describes the current entity itself or a relationship to a referenced entity in
the hierarchy.

**Why.** In hierarchical architectures such as SVC multi-layer coding or frame
reference structures, unqualified names like has_quality_layer are ambiguous
because readers and callers cannot tell whether the current layer possesses that
property or whether a relative reference layer (e.g., a lower or higher quality
layer) exists. Explicit relational qualifiers eliminate directional ambiguity
and prevent logic errors in mode decision, reference selection, and rate
control.

## Examples

Bad:

```cpp
// Ambiguous: unclear whether current layer is a quality layer or references one:
struct SvcLayerContext {
  int has_quality_layer;
};

if (svc->has_quality_layer) {
  use_lower_layer_reference = 1;
}
```

Good:

```cpp
// Explicit: directional qualifier clarifies reference relationship to a lower-quality layer:
struct SvcLayerContext {
  int has_lower_quality_layer;
};

if (svc->has_lower_quality_layer) {
  use_lower_layer_reference = 1;
}
```
