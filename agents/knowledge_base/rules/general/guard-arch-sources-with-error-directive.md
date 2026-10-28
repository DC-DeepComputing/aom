---
name: libaom/guard-arch-sources-with-error-directive
description: 'When guarding architecture-specific source files conditionally included by the build system, use #if !<ARCH> #error rather than wrapping the file in #if <ARCH>'
rubric: correctness
severity: nit
---

**Rule.** In architecture- or platform-specific source files conditionally
compiled by the build system, verify architecture prerequisites using `#if
!<ARCH_FLAG> \n #error ... \n #endif` instead of enclosing the entire
translation unit in `#if <ARCH_FLAG> ... #endif`.

**Why.** Wrapping an entire C/C++ source file in `#if <ARCH_FLAG>` results in an
empty translation unit when the macro is not set, generating compiler or
archiver warnings (such as empty symbol table warnings on macOS). Using `#if
!<ARCH_FLAG> #error` ensures that build misconfigurations fail immediately at
compile time with a clear diagnostic while keeping valid translation units
non-empty.

## Examples

Bad:

```cpp
#include "config/aom_config.h"

#if AOM_ARCH_X86

void cdef_filter_ssse3(int *dst, const int *src) {
  // ...
}

#endif  // AOM_ARCH_X86
```

Good:

```cpp
#include "config/aom_config.h"

#if !AOM_ARCH_X86
#error "cdef_block_ssse3.c is included for 32-bit x86 only"
#endif  // !AOM_ARCH_X86

void cdef_filter_ssse3(int *dst, const int *src) {
  // ...
}
```
