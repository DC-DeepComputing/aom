---
name: libaom/keep-component-types-in-component-headers
description: When defining or modifying component-specific data types or structures, place them in component-specific headers rather than shared common headers
rubric: design
severity: should_fix
---

**Rule.** Define component-specific data types, structs, typedefs, and sizing
constants in their respective subsystem or component headers (such as
encoder-only definitions in entenc.h or av1/encoder/*.h, and decoder-only
definitions in entdec.h or av1/decoder/*.h) rather than in shared common headers
(such as entcode.h or av1/common/*.h). When modifying types that only affect one
component, keep shared definitions intact to avoid forcing unnecessary type
refactoring or churn in unaffected subsystems.

**Why.** Modifying shared common headers for component-specific requirements
pollutes shared interfaces, breaks subsystem isolation, and forces unaffected
components (such as decoders) to implement compensating type workarounds or
undergo unnecessary recompilation and code churn.

## Examples

Bad:

```cpp
// Bad: Modifying shared common header for an encoder-specific change
// aom_dsp/entcode.h (shared common header)
typedef uint64_t od_ec_window;

// aom_dsp/entdec.h (forces decoder workaround/redefinition)
typedef uint32_t dec_od_ec_window;
```

Good:

```cpp
// Good: Keep component-specific types in their respective headers
// aom_dsp/entcode.h (shared common header remains clean)
typedef uint32_t od_ec_window;

// aom_dsp/entenc.h (encoder-specific header defines its own type)
typedef uint64_t od_ec_enc_window;
```
