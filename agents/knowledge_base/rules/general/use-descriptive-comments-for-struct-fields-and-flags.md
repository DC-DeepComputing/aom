---
name: libaom/use-descriptive-comments-for-struct-fields-and-flags
description: When documenting struct members, fields, or flags, write descriptive comments explaining controlled conditions and use terminology accurately matching underlying types
rubric: readability
severity: nit
---

**Rule.** When adding or updating comments for struct members, fields,
configuration flags, or boolean settings, write clear, descriptive comments
explaining the exact condition or behavior they control (commonly phrased as
'Whether <condition>' or 'Controls whether ...') rather than cryptic noun
fragments. Additionally, ensure terminology precisely matches the underlying
type or data structure (e.g., referring to a RefBuffer as a 'buffer' rather than
generic 'info') to avoid ambiguity.

**Why.** Vague, truncated, or imprecise comments obscure data semantics and
execution behavior. Using cryptic noun fragments leaves flag conditions
ambiguous, while misnaming underlying data structures (such as calling a buffer
structure generic 'info') confuses distinct types and configuration roles across
callers.

## Examples

Bad:

```cpp
// Cryptic noun fragments for configuration flags
// Max hash 8x8 intrabc blocks
// 0: Hash from 4x4 up to superblock size
// 1: Hash 4x4 and 8x8 only
int hash_max_8x8_intrabc_blocks;

// Fast mode
int fast_mode;

// Imprecise terminology for underlying struct types
// Each Inter frame can reference INTER_REFS_PER_FRAME buffers. This maps each
// (inter) reference frame type to the corresponding reference info.
RefBuffer frame_refs[INTER_REFS_PER_FRAME];
```

Good:

```cpp
// Clear descriptive phrasing explaining controlled condition
// Whether the maximum intrabc block size to hash is 8x8
// 0: Hash from 4x4 up to superblock size
// 1: Hash 4x4 and 8x8 only
int hash_max_8x8_intrabc_blocks;

// Whether to use fast mode heuristics during search.
int fast_mode;

// Terminology accurately matching the underlying RefBuffer type
// Each Inter frame can reference INTER_REFS_PER_FRAME buffers. This maps each
// (inter) reference frame type to the corresponding reference buffer.
RefBuffer frame_refs[INTER_REFS_PER_FRAME];
```
