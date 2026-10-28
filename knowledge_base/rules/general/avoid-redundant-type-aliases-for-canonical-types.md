---
name: libaom/avoid-redundant-type-aliases-for-canonical-types
description: When writing C++ code or unit tests, use canonical type and enum names directly instead of defining redundant 1:1 type aliases
rubric: readability
severity: nit
---

**Rule.** Do not define redundant 1:1 type aliases (such as `using TxSize =
TX_SIZE;` or `typedef TX_SIZE TxSize;`) that merely rename canonical C types,
enums, or typedefs into alternative naming styles. Use the canonical type or
enum identifier directly.

**Why.** Redundant 1:1 aliases introduce competing names for the same canonical
type without adding abstraction, encapsulation, or type safety. They clutter the
codebase and create inconsistency across tests and implementation files.

## Examples

Bad:

```cpp
// Redundant 1:1 alias for canonical enum TX_SIZE
using TxSize = TX_SIZE;

void RunTest(TxSize tx_size) {
  // ...
}
```

Good:

```cpp
// Use canonical type name directly
void RunTest(TX_SIZE tx_size) {
  // ...
}
```
