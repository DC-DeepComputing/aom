---
name: libaom/aom/use-aom-prefix-for-public-header-includes
description: When including sibling headers in public API headers (aom/*.h), use the 'aom/' directory prefix rather than relative './' paths or bare filenames
rubric: readability
severity: should_fix
---

**Rule.** In public API headers (aom/*.h), do not use relative './' or '../'
paths or bare filenames in #include directives. Always use the
repository-relative 'aom/' directory prefix (e.g., #include "aom/aom_integer.h")
when including sibling public headers.

**Why.** Public headers in aom/ are installed to system or third-party include
directories (such as <include>/aom/) and consumed by external applications whose
search paths expect repository-prefixed includes. Using relative './' paths or
bare filenames violates public header conventions and breaks header resolution
when installed.

## Examples

Bad:

```cpp
#include "./aom_integer.h"
#include "./aom_tpl.h"
#include "aom_image.h"
```

Good:

```cpp
#include "aom/aom_image.h"
#include "aom/aom_integer.h"
#include "aom/aom_tpl.h"
```
