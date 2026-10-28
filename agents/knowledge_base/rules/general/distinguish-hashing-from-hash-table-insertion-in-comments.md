---
name: libaom/distinguish-hashing-from-hash-table-insertion-in-comments
description: When writing comments or documentation for hash operations, distinguish computing a hash value from inserting or adding entries into a hash table
rubric: readability
severity: nit
---

**Rule.** In comments and documentation, use precise terminology rather than
overloading the verb 'hash' to mean inserting into a hash table. Clearly
distinguish computing or calculating a hash value from adding, inserting, or
indexing entries into a hash map or table.

**Why.** The verb 'hash' standardly refers to computing a hash digest or
checksum over data. Using 'hash' to describe adding or inserting elements into a
hash table creates ambiguity about whether an operation is calculating a key or
modifying container state.

## Examples

Bad:

```cpp
// Explore the entire frame hierarchically to hash intrabc candidate blocks
void add_intrabc_candidates_to_hash_table(...);
```

Good:

```cpp
// Explore the entire frame hierarchically to add intrabc candidate blocks to
// the hash table
void add_intrabc_candidates_to_hash_table(...);
```
