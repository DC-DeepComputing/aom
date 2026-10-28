---
name: libaom/parenthesize-macro-replacement-expressions
description: When defining preprocessor macros, parenthesize compound replacement expressions and parameters to preserve operator precedence, but omit redundant parentheses around single-token literals
rubric: readability
severity: should_fix
---

**Rule.** Wrap macro replacement expressions and parameters in enclosing
parentheses when defining object-like or function-like preprocessor macros to
ensure proper operator precedence and prevent substitution bugs. Conversely, do
not wrap simple single-token literals (such as integer, hex, boolean, or string
constants) in redundant outer parentheses.

**Why.** Macros undergo literal textual substitution before compilation. Without
outer parentheses around compound expressions and individual macro parameters,
surrounding operations with higher precedence can alter evaluation order and
cause subtle logic bugs. However, single-token literals (like `0`, `3`, or
`4096`) cannot cause precedence ambiguities; wrapping them in parentheses adds
unnecessary visual noise and deviates from codebase conventions.

## Examples

Bad:

```cpp
// Missing outer parentheses around compound expression; trailing addition alters precedence
#define ROTATION_COUNT ((ROTATION_RANGE * 2) / ROTATION_STEP) + 1

// Macro parameters not parenthesized
#define SCALE_VAL(x, factor) (x * factor + 1)

// Redundant parentheses around single-literal macro definitions
#define IS_ENC (0)
#define NUM_PLANES (3)
#define MAX_BUFFER_SIZE (4096)
```

Good:

```cpp
// Enclosed in outer parentheses and parameters safely parenthesized
#define ROTATION_COUNT (((ROTATION_RANGE * 2) / ROTATION_STEP) + 1)
#define SCALE_VAL(x, factor) (((x) * (factor)) + 1)

// Simple literals defined without redundant parentheses
#define IS_ENC 0
#define NUM_PLANES 3
#define MAX_BUFFER_SIZE 4096
```
