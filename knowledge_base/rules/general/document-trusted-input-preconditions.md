---
name: libaom/document-trusted-input-preconditions
description: When declaring or implementing functions that assume valid bitstream or buffer data without full validation, document the trusted-input precondition and warn against untrusted callers
rubric: readability
severity: nit
---

**Rule.** When declaring or defining functions, parsers, or conversion helpers
that operate on buffers or bitstreams under the assumption that the input has
already been validated, explicitly document this precondition in the header
declaration. Warn callers not to invoke the function directly on untrusted or
external input without prior validation.

**Why.** Functions that assume well-formed or pre-validated inputs omit boundary
or format checks for efficiency. If callers or future maintainers unknowingly
invoke these functions on unvalidated or untrusted data, malformed inputs can
lead to buffer over-reads, out-of-bounds writes, or other security
vulnerabilities.

## Examples

Bad:

```cpp
int av1_convert_sect5obus_to_annexb(uint8_t *buffer, size_t buffer_size,
                                    size_t *input_size);
```

Good:

```cpp
// This function assumes that the input buffer contains valid OBUs. It should
// not be called on untrusted input.
int av1_convert_sect5obus_to_annexb(uint8_t *buffer, size_t buffer_size,
                                    size_t *input_size);
```
