---
name: libaom/append-new-fields-to-serialized-and-api-structs
description: When extending structs used in serialization, multi-pass statistics, or public APIs, append new fields to the end instead of inserting them in the middle
rubric: readability
severity: should_fix
---

**Rule.** When adding new fields to structs representing serialized data,
multi-pass statistics (such as FIRSTPASS_STATS), rate control contexts, or
public API records, append them to the end of the struct definition rather than
inserting them in the middle. Do not reorder or insert fields among existing
members.

**Why.** Inserting fields into the middle of an existing struct alters the
memory offsets of all subsequent members. This breaks binary and serialization
compatibility across multi-pass workflows and causes data corruption or ABI
breakage for external API consumers.

## Examples

Bad:

```cpp
typedef struct FIRSTPASS_STATS {
  double coded_error;
  double log_intra_error;  // Bad: inserted in the middle of existing fields
  double log_coded_error;
  double sr_coded_error;
} FIRSTPASS_STATS;
```

Good:

```cpp
typedef struct FIRSTPASS_STATS {
  double coded_error;
  double sr_coded_error;
  /* ... existing fields ... */
  // Good: appended to the end to preserve field offsets
  double log_intra_error;
  double log_coded_error;
} FIRSTPASS_STATS;
```
