---
name: libaom/no-includes-in-inc-files
description: 'When writing or modifying .inc files, do not add #include directives; rely on enclosing translation units to provide header dependencies'
rubric: readability
severity: should_fix
---

**Rule.** Do not add `#include` directives inside `.inc` files. Rely on the
enclosing `.c` or `.cc` translation unit that textually embeds the `.inc` file
to provide all required header dependencies.

**Why.** Files with the `.inc` extension are textual code fragments intended to
be included directly inside specific macro definitions, functions, or
compilation contexts. Adding `#include` directives inside `.inc` files can drag
headers into local scopes or namespaces, cause circular dependencies, and create
unexpected preprocessor side effects.

## Examples

Bad:

```cpp
// av1/common/reconinter_template.inc
#include "av1/common/av1_common_int.h"
#include "av1/common/blockd.h"
#include "av1/common/reconinter.h"

static inline void build_one_inter_predictor(...) {
  // ...
}
```

Good:

```cpp
// av1/common/reconinter_template.inc
// No #include directives; required headers are provided by the enclosing .c file.

static inline void build_one_inter_predictor(...) {
  // ...
}
```
