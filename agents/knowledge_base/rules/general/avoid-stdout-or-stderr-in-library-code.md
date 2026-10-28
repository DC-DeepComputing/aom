---
name: libaom/avoid-stdout-or-stderr-in-library-code
description: When writing library code, do not print directly to stdout or stderr; propagate errors via return codes, assertions, or library error handlers.
rubric: readability
severity: should_fix
---

**Rule.** Do not write messages directly to stdout or stderr (such as via
`printf`, `fprintf(stderr, ...)`, `fprintf(stdout, ...)`, `puts`, `fputs`,
`perror`, or `std::cout`/`std::cerr`) from core library code. Communicate
failures or diagnostic conditions by returning error status codes (e.g.,
`aom_codec_err_t` or boolean/NULL flags), signaling errors via library error
mechanisms (`aom_internal_error()`), or using `assert()` for internal invariant
violations.

**Why.** Core library code is linked into diverse host applications (including
web browsers, GUI media players, and embedded frameworks) that manage their own
logging. Direct standard console output pollutes host application streams,
bypasses caller error-handling flows, and cannot be intercepted
programmatically.

## Examples

Bad:

```cpp
// Bad: printing directly to stderr or stdout in library functions
if (!output[j]->buf) {
  fprintf(stderr, "Failed to allocate memory for output buffer.\n");
  return;
}

FILE *pfile = fopen(filename, "r");
if (pfile == NULL) {
  printf("Can't open file %s. Using default deltaq.\n", filename);
  return;
}
```

Good:

```cpp
// Good: propagate errors through return codes or library error contexts
if (!output[j]->buf) {
  return AOM_CODEC_MEM_ERROR;
}

FILE *pfile = fopen(filename, "r");
if (pfile == NULL) {
  aom_internal_error(&cpi->common.error, AOM_CODEC_ERROR,
                     "Can't open file %s", filename);
  return;
}
```
