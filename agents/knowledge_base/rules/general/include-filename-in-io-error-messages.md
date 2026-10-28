---
name: libaom/include-filename-in-io-error-messages
description: When reporting file I/O or resource lookup errors via aom_internal_error() or die(), include the specific filename or path in the formatted error message
rubric: readability
severity: nit
---

**Rule.** When reporting file I/O, configuration loading, or resource lookup
failures via aom_internal_error(), aom_set_error(), or die(), include the
specific filename, path, or identifier in the formatted error message string
(e.g., using "%s"). Avoid generic error strings that omit the target resource
name.

**Why.** Generic error messages without the filename or path make diagnosing
missing files, bad paths, or permission issues difficult during debugging and
test runs. Including the resource path provides immediate context on what failed
to load.

## Examples

Bad:

```cpp
if (pfile == NULL) {
  aom_internal_error(cm->error, AOM_CODEC_ERROR, "Can't find input file.");
}
```

Good:

```cpp
if (pfile == NULL) {
  aom_internal_error(cm->error, AOM_CODEC_ERROR, "Can't find input file: %s.",
                     filename);
}
```
