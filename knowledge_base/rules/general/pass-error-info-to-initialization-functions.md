---
name: libaom/pass-error-info-to-initialization-functions
description: When defining initialization functions for modules using aom_internal_error_info, pass error_info explicitly as a parameter rather than requiring callers to assign struct fields
rubric: readability
severity: should_fix
---

**Rule.** When declaring or defining initialization functions for structs or
modules that report errors via aom_internal_error(), accept struct
aom_internal_error_info *error_info as an explicit parameter in the
initialization signature. Do not require callers to manually populate internal
error_info fields after initialization.

**Why.** Passing the error context directly into initialization functions
ensures that error reporting is configured before any internal allocations or
validation checks execute. It prevents NULL dereferences on early initialization
failures and eliminates brittle out-of-band struct member assignments by
callers.

## Examples

Bad:

```cpp
// Requires callers to manually assign error_info field after init
void od_ec_enc_init(od_ec_enc *enc, uint32_t size);
void aom_start_encode(aom_writer *w, uint8_t *source);

// Caller usage:
od_ec_enc_init(&enc, size);
enc.error_info = error_info;  // Errors inside init cannot use error_info safely
```

Good:

```cpp
// Explicit error_info parameter guarantees error context is set before initialization
void od_ec_enc_init(od_ec_enc *enc, uint32_t size,
                    struct aom_internal_error_info *error_info);
void aom_start_encode(aom_writer *w, uint8_t *source,
                      struct aom_internal_error_info *error_info);

// Caller usage:
od_ec_enc_init(&enc, size, error_info);
```
