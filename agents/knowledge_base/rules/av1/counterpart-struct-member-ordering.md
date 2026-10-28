---
name: libaom/av1/counterpart-struct-member-ordering
description: When defining or updating counterpart encoder and decoder structs, maintain consistent member ordering and group logically related synchronization fields together
rubric: readability
severity: nit
---

**Rule.** When defining or modifying parallel or counterpart structures in
encoder and decoder modules (such as row multithreading sync structs), keep
member field declarations in a consistent, symmetric order across both
structures. Group logically related fields (such as sync range and extra sync
delay settings) together rather than appending new fields at the end of the
struct.

**Why.** Encoder and decoder subsystems frequently mirror each other's state and
synchronization logic. Maintaining symmetric struct layouts and grouping related
fields makes cross-referencing between encoder and decoder code straightforward
and reduces confusion when tracing parallel multithreading workflows.

## Examples

Bad:

```cpp
// Decoder struct with extra delay appended at the end, diverging from encoder layout
typedef struct AV1DecRowMTSyncData {
  int sync_range;
  int mi_rows;
  int mi_cols;
  int mi_rows_parse_done;
  int mi_rows_decode_started;
  int num_threads_working;
  int intrabc_extra_top_right_sb_delay;
} AV1DecRowMTSync;
```

Good:

```cpp
// Group related sync parameters (sync_range, intrabc delay) matching encoder struct
typedef struct AV1DecRowMTSyncData {
  int sync_range;
  int intrabc_extra_top_right_sb_delay;
  int mi_rows;
  int mi_cols;
  int mi_rows_parse_done;
  int mi_rows_decode_started;
  int num_threads_working;
} AV1DecRowMTSync;
```
