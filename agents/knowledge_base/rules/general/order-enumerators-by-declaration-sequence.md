---
name: libaom/order-enumerators-by-declaration-sequence
description: When testing multiple enum values or flags in conditions, assertions, or switch cases, order operands by declaration sequence and match accompanying comments
rubric: readability
severity: nit
---

**Rule.** When checking multiple enum values, modes, or flags of the same type
in compound conditionals (|| or &&), assertions, expression lists, or
consecutive switch cases, order the enumerators to match their canonical
declaration order in the enum definition. Additionally, when accompanying the
check with an explanatory comment listing the cases, align the comment and
operand sequence with that same canonical declaration order instead of permuting
or arbitrarily ordering them.

**Why.** Ordering enumerator checks and switch cases by their header declaration
sequence makes compound logic predictable, reduces cognitive load during review,
and simplifies cross-referencing against header definitions. Ensuring comments
and operand lists follow this same order prevents discrepancies and helps avoid
inadvertently omitted cases.

## Examples

Bad:

```cpp
// Intra prediction modes (i.e., DC, SMOOTH, SMOOTH_H, SMOOTH_V, and PAETH).
// In enums.h, SMOOTH_V_PRED is declared before SMOOTH_H_PRED.
assert(mode == PAETH_PRED || mode == DC_PRED || mode == SMOOTH_PRED ||
       mode == SMOOTH_H_PRED || mode == SMOOTH_V_PRED);

switch (mode) {
  case PAETH_PRED:
  case SMOOTH_H_PRED:
  case SMOOTH_V_PRED:
  case DC_PRED: break;
}
```

Good:

```cpp
// Intra prediction modes (i.e., DC, SMOOTH, SMOOTH_V, SMOOTH_H, and PAETH).
// Enumerators match their declaration order in enums.h.
assert(mode == DC_PRED || mode == SMOOTH_PRED || mode == SMOOTH_V_PRED ||
       mode == SMOOTH_H_PRED || mode == PAETH_PRED);

switch (mode) {
  case DC_PRED:
  case SMOOTH_PRED:
  case SMOOTH_V_PRED:
  case SMOOTH_H_PRED:
  case PAETH_PRED: break;
}
```
