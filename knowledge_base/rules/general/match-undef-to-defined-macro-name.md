---
name: libaom/match-undef-to-defined-macro-name
description: 'When cleaning up helper macros with #undef, ensure the identifier matches the exact #define spelling to prevent silent macro leaks'
rubric: readability
severity: nit
---

**Rule.** When undefining local or helper macros with `#undef` directives in
source or header files, ensure the macro identifier matches the exact spelling
of the corresponding `#define` directive (including dialect spellings like
American vs. British English, prefixes, and suffixes). Avoid typos or misspelled
identifiers in `#undef` statements.

**Why.** In C and C++, the preprocessor treats `#undef` on an undefined or
misspelled identifier as a silent no-op without issuing a compiler warning or
error. A typographical error in `#undef` fails to undefine the target macro,
leaving it active throughout subsequent translation units and risking unexpected
macro expansions and symbol collisions.

## Examples

Bad:

```cpp
// Variant 1: Dialect spelling mismatch (American vs. British)
#define SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(w, h, padding) ...
// Macro usage...
#undef SPECIALISED_SUBPEL_VARIANCE_WXH_NEON

// Variant 2: Typo in macro name identifier
#define SAD_WXH_AVG_NEON(w, h) sad_wxh_avg_neon_helper(w, h)
SAD_WXH_AVG_NEON(128, 128);
#undef SADP_WXH_AVG_NEON
```

Good:

```cpp
// Variant 1: Exact matching spelling
#define SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON(w, h, padding) ...
// Macro usage...
#undef SPECIALIZED_SUBPEL_VARIANCE_WXH_NEON

// Variant 2: Exact matching identifier
#define SAD_WXH_AVG_NEON(w, h) sad_wxh_avg_neon_helper(w, h)
SAD_WXH_AVG_NEON(128, 128);
#undef SAD_WXH_AVG_NEON
```
