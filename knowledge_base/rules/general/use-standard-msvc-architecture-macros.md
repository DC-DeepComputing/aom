---
name: libaom/use-standard-msvc-architecture-macros
description: When guarding x86 or x86_64 code across compilers, check MSVC macros like _M_X64 and _M_IX86 alongside __x86_64__ or __i386__, avoiding invalid _M_X86
rubric: correctness
severity: should_fix
---

**Rule.** When guarding architecture-specific code (such as x86_64 or 32-bit
x86) in cross-compiler files or Windows builds, include MSVC predefined macros
alongside GCC/Clang macros (e.g. `(defined(__x86_64__) || defined(_M_X64))` or
`(defined(__i386__) || defined(_M_IX86))`). Always use valid MSVC architecture
macros (`_M_X64` or `_M_AMD64` for x86_64, `_M_IX86` for 32-bit x86); do not
check only `__x86_64__` or use non-existent macros like `_M_X86`.

**Why.** MSVC does not define GCC/Clang preprocessor macros like `__x86_64__` or
`__i386__`, nor does it define invalid macro names like `_M_X86`. Relying only
on `__x86_64__` causes architecture-specific code to be omitted in MSVC builds,
while using undefined macros like `_M_X86` silently evaluates to false in
preprocessor expressions.

## Examples

Bad:

```cpp
// Bad: relying only on GCC/Clang macro omits MSVC builds on Windows
#if defined(__x86_64__) && defined(_WIN32)
// x86_64 Windows code
#endif

// Bad: _M_X86 is not a valid MSVC macro (silently evaluates to false)
#if defined(__x86_64__) || defined(_M_X86)
// x86_64 specific code
#endif
```

Good:

```cpp
// Good: checks both GCC/Clang and MSVC x86_64 predefined macros
#if (defined(__x86_64__) || defined(_M_X64)) && defined(_WIN32)
// x86_64 Windows code
#endif

// Good: uses valid MSVC macro _M_X64 (or _M_IX86 for 32-bit x86)
#if defined(__x86_64__) || defined(_M_X64)
// x86_64 specific code
#endif
```
