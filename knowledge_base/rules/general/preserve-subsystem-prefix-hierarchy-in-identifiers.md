---
name: libaom/preserve-subsystem-prefix-hierarchy-in-identifiers
description: When naming new types, functions, or macros, follow the established subsystem prefix hierarchy rather than placing role or module qualifiers ahead of the subsystem prefix
rubric: readability
severity: nit
---

**Rule.** Follow the established subsystem and module prefix hierarchy when
naming new types, structs, typedefs, functions, or macros (e.g.,
'<subsystem>_<module>_<name>' such as 'od_ec_enc_*' rather than placing role
qualifiers before the prefix like 'enc_od_ec_*'). Do not invert or prepend role
qualifiers ahead of the owning subsystem prefix.

**Why.** In C codebases without language-level namespaces, hierarchical
identifier prefixes act as pseudo-namespaces to organize subsystems and prevent
symbol collisions. Inverting the prefix hierarchy breaks naming consistency
across the subsystem API and impairs codebase search and symbol navigation.

## Examples

Bad:

```cpp
// Bad: Role qualifier 'enc_' placed before the subsystem prefix 'od_ec_'
typedef uint64_t enc_od_ec_window;

void enc_od_ec_init(od_ec_enc *enc);
```

Good:

```cpp
// Good: Preserves hierarchical prefix '<subsystem>_<module>_<name>'
typedef uint64_t od_ec_enc_window;

void od_ec_enc_init(od_ec_enc *enc);
```
