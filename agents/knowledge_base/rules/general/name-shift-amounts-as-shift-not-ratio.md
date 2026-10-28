---
name: libaom/name-shift-amounts-as-shift-not-ratio
description: When naming variables, struct fields, or types that hold bit shift amounts, use shift or bit_shift rather than ratio or factor
rubric: readability
severity: should_fix
---

**Rule.** Name variables, struct fields, and types that store bit shift amounts
or log2 exponents with 'shift' or 'bit_shift' rather than terms like 'ratio' or
'factor'. Reserve 'ratio', 'factor', and 'scale' for linear multiplicative or
divisive proportions.

**Why.** Identifiers using 'ratio' or 'factor' suggest linear scaling factors or
multipliers rather than logarithmic bit counts. Using 'shift' or 'bit_shift'
accurately communicates that the value represents an exponent used with bitwise
shift operators ('>>' or '<<').

## Examples

Bad:

```cpp
typedef struct {
  int rows[MAX_PARTITIONS];
  int cols[MAX_PARTITIONS];
  int n_partitions;
} TX_PARTITION_RATIO;

sub_txw = txw >> ratio.rows[i];
```

Good:

```cpp
typedef struct {
  int rows[MAX_PARTITIONS];
  int cols[MAX_PARTITIONS];
  int n_partitions;
} TX_PARTITION_BIT_SHIFT;

sub_txw = txw >> bit_shift.rows[i];
```
