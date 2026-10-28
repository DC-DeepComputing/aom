---
name: libaom/prefer-explicit-branching-over-enum-arithmetic
description: When returning or selecting enum values based on conditions, use explicit branching or ternary operators instead of arithmetic on enum constants with boolean expressions
rubric: readability
severity: should_fix
---

**Rule.** When selecting or returning enumeration values based on conditional
predicates, write explicit conditional branches (if/else), early returns, or
ternary expressions rather than performing arithmetic on enum constants with
boolean expressions (e.g., ENUM_BASE + (cond && flag)). Do not rely on implicit
sequential integer values of enumerators to compute target enum constants.

**Why.** Boolean arithmetic on enum constants obscures logical intent,
introduces fragile assumptions about enumerator ordering, and forces redundant
evaluation of conditions that early returns avoid.

## Examples

Bad:

```cpp
if (tx_size_sqr_up >= TX_32X32)
  return EXT_TX_SET_DCTONLY + (tx_size_sqr_up == TX_32X32 && is_inter);
```

Good:

```cpp
if (tx_size_sqr_up > TX_32X32) return EXT_TX_SET_DCTONLY;
if (tx_size_sqr_up == TX_32X32)
  return is_inter ? EXT_TX_SET_DCT_IDTX : EXT_TX_SET_DCTONLY;
```
