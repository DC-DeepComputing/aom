---
name: libaom/qualify-metric-identifiers-with-scope-granularity
description: When defining CLI flags, metric variables, inspection structs (insp_mi_data), or serialization keys (put_block_info), qualify identifiers with their scope granularity and maintain consistent naming
rubric: readability
severity: should_fix
---

**Rule.** When defining CLI arguments (`ARG_DEF`), metric variables, inspection
structs (`struct insp_mi_data`), or serialized block/frame info dictionary keys
(`put_block_info`), qualify identifiers with their hierarchical scope or
granularity prefix (such as `sb_bits`, `tile_bits`, or `frame_bits` instead of
generic `bits`). Ensure naming remains consistent across CLI short/long option
flags, struct member fields, and serialized output keys rather than using bare
metric names.

**Why.** Generic metric names like `bits` or `size` are ambiguous regarding
whether they capture sequence-, GOP-, frame-, tile-, superblock- (`sb`), or
block-level data. Qualifying identifiers with their granularity prefix prevents
confusion across analyzers, CLI tools, and codec statistics APIs. Maintaining
consistent naming across CLI flags, internal struct members, and JSON/dictionary
serialization keys ensures that inspection outputs and code interfaces align
seamlessly.

## Examples

Bad:

```cpp
// Ambiguous granularity for superblock bits CLI option:
static const arg_def_t dump_bits_arg = ARG_DEF("b", "bits", 0, "Dump Bits");

// Ambiguous metric name in inspection serialization:
buf += put_block_info(buf, NULL, "bits", offsetof(insp_mi_data, bits), 0);
```

Good:

```cpp
// Explicit scope granularity in short/long flag and description:
static const arg_def_t dump_bits_arg =
    ARG_DEF("sb", "sb_bits", 0, "Dump Bits of each superblock");

// Explicit granularity across struct field and serialization key:
buf += put_block_info(buf, NULL, "sb_bits", offsetof(insp_mi_data, sb_bits), 0);
```
