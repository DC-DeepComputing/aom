---
name: libaom/reference-internal-interfaces-in-signature-todos
description: When writing TODO comments proposing signature or prototype changes to codec functions, reference internal interface typedefs (aom_codec_*_fn_t) rather than public API functions
rubric: readability
severity: nit
---

**Rule.** Do not reference public API functions (such as `aom_codec_*()`) in
TODO comments proposing signature or prototype refactorings. Reference the
underlying internal driver interface typedefs (such as `aom_codec_*_fn_t` in
`aom/internal/aom_codec_internal.h`) instead to preserve public API and ABI
backward compatibility.

**Why.** Public API function prototypes exported in `aom/*.h` form a stable
contract consumed by external applications and cannot be modified without
breaking backward compatibility. Refactoring internal driver function pointer
typedefs (`aom_codec_*_fn_t`) allows modifying internal parameters and error
handling plumbing while keeping public wrapper function signatures intact.

## Examples

Bad:

```cpp
// TODO(aomedia:3519): Change the prototype of aom_codec_get_frame() to
// facilitate error handling.
while ((img = aom_codec_get_frame(&decoder, &iter))) {
```

Good:

```cpp
// TODO(aomedia:3519): Change the prototype of aom_codec_get_frame_fn_t to
// facilitate error handling.
while ((img = aom_codec_get_frame(&decoder, &iter))) {
```
