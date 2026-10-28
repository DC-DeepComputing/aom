---
name: libaom/aom/no-bool-in-public-headers
description: When declaring boolean parameters, struct members, or flags in public C API headers (aom/*.h), use int instead of bool
rubric: readability
severity: should_fix
---

**Rule.** Do not use bool or include <stdbool.h> in public C API headers
(aom/*.h). Use int (or unsigned int) for boolean flags, struct members, and
function parameters to preserve C89/C99 compatibility and consistent C ABI
interoperability.

**Why.** Public headers in aom/*.h define the external C ABI for libaom and are
consumed by third-party C and C++ applications. Avoiding bool and <stdbool.h> in
public headers prevents C89/C99 compiler compatibility issues and ABI
discrepancies across different toolchains.

## Examples

Bad:

```cpp
#include <stdbool.h>

int aom_img_add_metadata_v2(aom_image_t *img, uint32_t type,
                            const uint8_t *data, size_t sz,
                            aom_metadata_insert_flags_t insert_flag,
                            bool is_layer_specific_obu);
```

Good:

```cpp
int aom_img_add_metadata_v2(aom_image_t *img, uint32_t type,
                            const uint8_t *data, size_t sz,
                            aom_metadata_insert_flags_t insert_flag,
                            int is_layer_specific_obu);
```
