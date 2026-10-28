---
name: libaom/general-no-empty-preprocessor-blocks
description: 'When writing preprocessor conditional blocks (#if, #ifdef, #ifndef), omit empty conditional blocks and unnecessary empty #else directives containing no code'
rubric: readability
severity: nit
---

**Rule.** Do not include empty preprocessor conditional blocks (`#if`, `#ifdef`,
`#ifndef`, or `#elif` followed immediately by `#else`, `#elif`, or `#endif`) or
empty `#else` directives in source code. If a conditional block contains no
statements or declarations, remove the directives entirely. If a conditional
branch requires no fallback logic or macro definitions, omit the `#else`
directive.

**Why.** Empty preprocessor blocks and redundant `#else` directives provide no
functional behavior, add visual noise and boilerplate clutter, and confuse
readers by suggesting implementation logic was accidentally omitted or
misplaced.

## Examples

Bad:

```cpp
// Bad: Empty preprocessor conditional block
    case TX_16X64:
    case TX_64X16:
#if CONFIG_FLEX_PARTITION
#endif  // CONFIG_FLEX_PARTITION
    default:

// Bad: Empty #else branch with no fallback code
#if FIX_GF_INTERVAL_LENGTH
#define FIXED_GF_LENGTH 4
#else
#endif
```

Good:

```cpp
// Good: Removed empty preprocessor conditional block
    case TX_16X64:
    case TX_64X16:
    default:

// Good: Omitted unnecessary #else directive
#if FIX_GF_INTERVAL_LENGTH
#define FIXED_GF_LENGTH 4
#endif
```
