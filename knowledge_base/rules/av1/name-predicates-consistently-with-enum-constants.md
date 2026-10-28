---
name: libaom/av1/name-predicates-consistently-with-enum-constants
description: When naming helper query predicates or variables that check enum types or partition modes, match the corresponding enum constant or ratio rather than internal shift counts
rubric: readability
severity: should_fix
---

**Rule.** When defining helper query predicates, functions, or local variables
that validate or test codec modes, partitions, transform sizes, or enum states,
name them consistently with the corresponding enum constants, domain concepts,
or partition ratios (such as allow_tx_horz4_split and allow_tx_vert4_split for
TX_PARTITION_HORZ4 and TX_PARTITION_VERT4). Do not name them after internal
implementation details or arithmetic constants, such as bit-shift amounts (e.g.,
using horz2 or vert2 because of a >> 2 shift).

**Why.** Naming predicates or variables after internal mechanics (such as
bit-shift counts) rather than domain semantics obscures their purpose, creates
confusion when mapping helpers to enum values in switch cases, and makes the
code error-prone when maintenance or refactoring changes internal arithmetic.

## Examples

Bad:

```cpp
// Horz 4:1 partition check named after bitshift amount (>> 2)
static INLINE int allow_tx_horz2_split(TX_SIZE max_tx_size) {
  const int sub_txw = tx_size_wide[max_tx_size];
  const int sub_txh = tx_size_high[max_tx_size] >> 2;
  return get_tx_size(sub_txw, sub_txh) != TX_INVALID;
}

// Vert 4:1 partition check named after bitshift amount (>> 2)
static INLINE int allow_tx_vert2_split(TX_SIZE max_tx_size) {
  const int sub_txw = tx_size_wide[max_tx_size] >> 2;
  const int sub_txh = tx_size_high[max_tx_size];
  return get_tx_size(sub_txw, sub_txh) != TX_INVALID;
}
```

Good:

```cpp
// Named consistently with TX_PARTITION_HORZ4
static INLINE int allow_tx_horz4_split(TX_SIZE max_tx_size) {
  const int sub_txw = tx_size_wide[max_tx_size];
  const int sub_txh = tx_size_high[max_tx_size] >> 2;
  return get_tx_size(sub_txw, sub_txh) != TX_INVALID;
}

// Named consistently with TX_PARTITION_VERT4
static INLINE int allow_tx_vert4_split(TX_SIZE max_tx_size) {
  const int sub_txw = tx_size_wide[max_tx_size] >> 2;
  const int sub_txh = tx_size_high[max_tx_size];
  return get_tx_size(sub_txw, sub_txh) != TX_INVALID;
}
```
