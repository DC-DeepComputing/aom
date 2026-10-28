---
name: libaom/name-cli-options-for-data-scope-with-natural-order
description: When defining CLI arguments (ARG_DEF) or codec options, name them specifically for their target data scope with modifiers preceding nouns
rubric: readability
severity: nit
---

**Rule.** When defining command-line argument names (ARG_DEF, ARG_DEF_ENUM) or
codec option strings, qualify the name specifically according to the scope of
data or feature set it affects (e.g., prefixing with feature qualifiers like
'hbd-' when restricted to high bit depth) rather than using overly broad names,
leaving generic names available for general functionality. Place noun modifiers
before the noun they modify (e.g., 'validate-hbd-input' rather than
'validate-input-hbd' or 'validate-input') to adhere to natural English grammar
and consistency across options.

**Why.** Using overly broad option names for scope-restricted functionality is
misleading to users and preempts generic names from being used when
comprehensive support is added later. Placing modifiers before nouns ensures
consistent and natural phrasing across CLI arguments and configuration options.

## Examples

Bad:

```cpp
// Bad: Overly broad name for an HBD-only check, or unnatural modifier placement
.validate_input_hbd =
    ARG_DEF(NULL, "validate-input", 1,
            "Check that input samples are within the valid range "
            "for the chosen bit depth with high bit depth encoding"),
.validate_input_hbd_alt =
    ARG_DEF(NULL, "validate-input-hbd", 1,
            "Check that input samples are within the valid range "
            "for the chosen bit depth with high bit depth encoding"),
```

Good:

```cpp
// Good: Specifically scoped name with natural modifier order (modifier before noun)
.validate_input_hbd =
    ARG_DEF(NULL, "validate-hbd-input", 1,
            "Check that input samples are within the valid range "
            "for the chosen bit depth with high bit depth encoding"),
```
