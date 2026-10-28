---
name: libaom/pair-option-names-with-values-in-comments
description: When documenting multiple configuration option or flag defaults in comments, explicitly pair each option with its value rather than listing values positionally with 'respectively'
rubric: readability
severity: nit
---

**Rule.** In comments explaining multiple configuration option or flag defaults
or overrides, explicitly pair each option name with its assigned value (such as
`--flag=value` or `option=value`) rather than listing values positionally with
'respectively'.

**Why.** Positional lists of values (e.g., '0, 2, 4, and 10 respectively') are
ambiguous, difficult to scan, and error-prone when options are modified or
reordered. Explicitly pairing each option with its value makes comments
self-contained and clear.

## Examples

Bad:

```cpp
// Here we set its default values to 0, 2, 4, and 10 respectively when
// --allintra is turned on.
```

Good:

```cpp
// Here we set its default values to --enable-cdef=0,
// --screen-detection-mode=2, --qm-min=4, and --qm-max=10 when --allintra
// is turned on.
```
