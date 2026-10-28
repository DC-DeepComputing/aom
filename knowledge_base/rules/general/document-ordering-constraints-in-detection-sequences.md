---
name: libaom/document-ordering-constraints-in-detection-sequences
description: When format probing or stream detection checks have side effects or buffering limitations, document the required evaluation ordering in an explanatory comment or TODO
rubric: readability
severity: nit
---

**Rule.** When chaining format detection, stream probing, or multi-stage parsing
checks where specific tests have side effects, lack rewind/buffering support, or
consume stream data, add an explanatory comment or tracking TODO documenting why
those checks must be evaluated in a specific order.

**Why.** Maintainers frequently assume probing and inspection functions are
side-effect-free and may reorder them during refactoring. Documenting stream
mutations, missing buffering support, or evaluation order dependencies prevents
regressions and clarifies prerequisites for future cleanup.

## Examples

Bad:

```cpp
// Webm type does not support reading from stdin.
if (false) {
#if CONFIG_WEBM_IO
} else if (using_file && file_is_webm(input.webm_ctx, input.aom_input_ctx)) {
  file_type = FILE_TYPE_WEBM;
#endif
} else if (file_is_ivf(input.aom_input_ctx)) {
  file_type = FILE_TYPE_IVF;
}
```

Good:

```cpp
// TODO(https://crbug.com/aomedia/1706): webm type does not support reading
// from stdin yet, and file_is_webm is not using the detect buffer when
// determining the type. Therefore it should only be checked when using a file
// and needs to be checked prior to other types.
if (false) {
#if CONFIG_WEBM_IO
} else if (using_file && file_is_webm(input.webm_ctx, input.aom_input_ctx)) {
  file_type = FILE_TYPE_WEBM;
#endif
} else if (file_is_ivf(input.aom_input_ctx)) {
  file_type = FILE_TYPE_IVF;
}
```
