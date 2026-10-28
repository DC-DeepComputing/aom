---
name: libaom/descriptive-parameter-names-in-macros
description: When defining macro helpers or utility functions with positional arguments, use descriptive parameter names reflecting domain semantics rather than single-letter placeholders
rubric: readability
severity: nit
---

**Rule.** When defining preprocessor macros or helper functions with positional
parameters (such as direction, coordinate, or bitmask arguments), use
descriptive parameter names that convey their domain semantics rather than
opaque single-letter placeholders (such as a, b, c, d). Name parameters after
their logical roles (e.g., left, down, right, up or src_x, src_y) so that
parameter order and bit positions are self-documenting at definitions and call
sites.

**Why.** Opaque placeholders obscure the positional meaning of arguments in
macro expansions and helper signatures, making it difficult to verify call-site
correctness and increasing the likelihood of transposed-argument bugs.

## Examples

Bad:

```cpp
// Opaque single-letter parameter names obscure positional bitmask semantics.
#define NEIGHBOR_MASK4(a, b, c, d) ((a) | ((b) << 1) | ((c) << 2) | ((d) << 3))
```

Good:

```cpp
// Descriptive parameter names clearly document spatial/positional roles.
#define NEIGHBOR_MASK_DIA(left, down, right, up) \
  ((left) | ((down) << 1) | ((right) << 2) | ((up) << 3))
```
