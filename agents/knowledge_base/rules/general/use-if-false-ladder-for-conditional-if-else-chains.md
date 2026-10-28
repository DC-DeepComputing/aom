---
name: libaom/use-if-false-ladder-for-conditional-if-else-chains
description: When conditionally compiling branches of an if-else chain with preprocessor guards, start with if (false) to avoid duplicating fallback branches
rubric: readability
severity: nit
---

**Rule.** When conditionally compiling the leading branches of an if-else chain
using #if preprocessor guards, start the chain with 'if (false) {' (or 'if (0)
{'). Structure all conditional and unconditional branches uniformly as '} else
if (...) {' to avoid duplicating subsequent branches across #else preprocessor
blocks.

**Why.** Conditionally compiling the leading 'if' branch requires duplicating
subsequent fallback branches in an #else block or splitting preprocessor guards
awkwardly across control flow. Starting with 'if (false)' enables clean,
symmetrical '} else if' branches across all compile-time configurations without
code duplication.

## Examples

Bad:

```cpp
#if CONFIG_WEBM_IO
  if (using_file && file_is_webm(input.webm_ctx, input.aom_input_ctx)) {
    input.aom_input_ctx->file_type = FILE_TYPE_WEBM;
  } else if (file_is_ivf(input.aom_input_ctx)) {
#else
  if (file_is_ivf(input.aom_input_ctx)) {
#endif
    input.aom_input_ctx->file_type = FILE_TYPE_IVF;
  }
```

Good:

```cpp
  if (false) {
#if CONFIG_WEBM_IO
  } else if (using_file && file_is_webm(input.webm_ctx, input.aom_input_ctx)) {
    input.aom_input_ctx->file_type = FILE_TYPE_WEBM;
#endif
  } else if (file_is_ivf(input.aom_input_ctx)) {
    input.aom_input_ctx->file_type = FILE_TYPE_IVF;
  }
```
