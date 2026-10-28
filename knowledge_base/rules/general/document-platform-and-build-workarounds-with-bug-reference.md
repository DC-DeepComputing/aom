---
name: libaom/document-platform-and-build-workarounds-with-bug-reference
description: When adding platform-, architecture-, or build-specific workarounds, tightly scope preprocessor guards to affected configurations and document the rationale with a bug reference
rubric: correctness
severity: should_fix
---

**Rule.** When adding platform-, compiler-, architecture-, or
build-configuration-specific workarounds (such as overriding thread stack sizes,
compiler pragma overrides, or OS-specific limits), tightly scope the
preprocessor conditions with precise OS, AOM_ARCH_*, sanitizer, and build-mode
macros rather than applying them globally. Always document the underlying
constraints and rationale and reference the tracking bug URL or issue ID in a
code comment.

**Why.** Broadly applied workarounds needlessly alter behavior or increase
resource allocations across unaffected platforms. Documenting the specific
constraints and tracking bugs ensures future maintainers understand why the
workaround exists and prevents accidental regressions or premature removal.

## Examples

Bad:

```cpp
// Overly broad workaround without bug documentation
#if defined(AOM_ADDRESS_SANITIZER)
size_t stacksize;
if (!pthread_attr_getstacksize(&attr, &stacksize)) {
  const size_t kMinStackSize = 1024 * 1024;  // 1 MiB
  if (stacksize < kMinStackSize &&
      pthread_attr_setstacksize(&attr, kMinStackSize)) {
    pthread_attr_destroy(&attr);
    goto Error2;
  }
}
#endif
```

Good:

```cpp
// Debug ASan builds require at least ~1MiB of stack; prevents
// failures on macOS arm64 where the default is 512KiB.
// See: https://crbug.com/aomedia/3379
#if defined(AOM_ADDRESS_SANITIZER) && defined(__APPLE__) && AOM_ARCH_ARM && \
    !defined(NDEBUG)
const size_t kMinStackSize = 1024 * 1024;
#else
const size_t kMinStackSize = 256 * 1024;
#endif
size_t stacksize;
if (!pthread_attr_getstacksize(&attr, &stacksize)) {
  if (stacksize < kMinStackSize &&
      pthread_attr_setstacksize(&attr, kMinStackSize)) {
    pthread_attr_destroy(&attr);
    goto Error2;
  }
}
```
