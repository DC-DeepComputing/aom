---
name: libaom/document-api-configuration-alongside-cli-flags
description: When documenting configuration options, parameter defaults, or overrides in library comments, refer to C library APIs like aom_codec_control() or aom_codec_set_option() rather than solely CLI flags
rubric: readability
severity: nit
---

**Rule.** When documenting configuration options, parameter defaults, or
override behaviors in library comments, describe them in terms of programmatic C
library API mechanisms (such as `aom_codec_control()` or
`aom_codec_set_option()`) rather than referring solely to command-line flags or
'the command line'.

**Why.** libaom is primarily consumed as a programmatic C library by client
applications in addition to CLI tools like aomenc. Comments that describe
configuration solely in terms of command-line flags mislead developers and
obscure the actual C API functions used to pass options and overrides to the
encoder.

## Examples

Bad:

```cpp
// If users set --enable-cdef, --qm-min, or --qm-max from command line,
// the encoder still respects it.

// Callers can always override these settings from the command line,
// or by using aom_codec_control().
```

Good:

```cpp
// If users set --enable-cdef, --screen-detection-mode, --qm-min, or --qm-max,
// either via CLI or aom_codec_control(), the encoder still respects it.

// Callers can always override these settings by using
// aom_codec_set_option() or aom_codec_control().
```
