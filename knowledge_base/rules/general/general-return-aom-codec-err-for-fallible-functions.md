---
name: libaom/general-return-aom-codec-err-for-fallible-functions
description: When writing fallible internal helper functions, return aom_codec_err_t instead of bool or calling aom_internal_error directly, allowing callers to handle error reporting
rubric: readability
severity: should_fix
---

**Rule.** Return aom_codec_err_t (e.g., AOM_CODEC_OK, AOM_CODEC_MEM_ERROR,
AOM_CODEC_INVALID_PARAM) rather than bool from fallible internal functions and
helpers whose status is propagated to callers or reported via
aom_internal_error(). Do not invoke aom_internal_error(), aom_set_error(), or
aom_internal_error_copy() directly inside helper functions that can be called
across multiple contexts with differing setjmp states; instead, return an
aom_codec_err_t status code to the caller so the top-level caller can capture
the specific error and dispatch aom_internal_error() on its designated
error_info struct.

**Why.** Returning bool collapses distinct failure causes (such as memory
allocation failure versus invalid parameter) into a generic flag, forcing
callers to guess or hardcode error codes. Furthermore, calling
aom_internal_error() inside shared helpers triggers longjmp() when
error_info->setjmp is active, while aom_set_error() asserts that setjmp is
disabled; calling them directly inside mixed-context helpers can corrupt error
context or cause unexpected jumps. Returning aom_codec_err_t preserves specific
failure causes and lets the caller manage error reporting safely.

## Examples

Bad:

```cpp
// Bad: Helper returns bool or calls aom_internal_error directly from mixed contexts
bool av1_check_initial_width(AV1_COMP *cpi, int use_highbitdepth,
                             int subsampling_x, int subsampling_y) {
  if (!av1_tf_info_alloc(&cpi->ppi->tf_info, cpi)) return false;
  return true;
}

// Caller is forced to guess/hardcode error code:
if (!av1_check_initial_width(cpi, use_highbitdepth, sub_x, sub_y)) {
  aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR, "av1_check_initial_width() failed");
}
```

Good:

```cpp
// Good: Return aom_codec_err_t to preserve exact error code without direct jumps
aom_codec_err_t av1_check_initial_width(AV1_COMP *cpi, int use_highbitdepth,
                                        int subsampling_x, int subsampling_y) {
  if (!av1_tf_info_alloc(&cpi->ppi->tf_info, cpi))
    return AOM_CODEC_MEM_ERROR;
  return AOM_CODEC_OK;
}

// Caller captures exact code and invokes aom_internal_error on its error context:
aom_codec_err_t err = av1_check_initial_width(cpi, use_highbitdepth, sub_x, sub_y);
if (err != AOM_CODEC_OK) {
  aom_internal_error(cm->error, err, "av1_check_initial_width() failed");
}
```
