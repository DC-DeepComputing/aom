---
name: libaom/document-mathematical-derivation-of-spec-formulas
description: When implementing bitwise or arithmetic optimizations of specification formulas, document the step-by-step mathematical derivation in a comment and remove commented-out formula fragments
rubric: readability
severity: nit
---

**Rule.** When implementing or simplifying mathematical, bitwise, or algebraic
calculations derived from codec specifications, provide an explanatory comment
documenting the step-by-step mathematical derivation and equivalence to the
specification. Do not leave obsolete, partial, or commented-out spec equation
fragments inline with the code.

**Why.** Non-obvious bitwise tricks and algebraic simplifications of
specification formulas cannot be easily verified or maintained from the C code
alone. Documenting the formal derivation from the specification clarifies the
mathematical invariants for future maintainers while removing confusing
commented-out code fragments.

## Examples

Bad:

```cpp
  // + (cdf[nsymbs] > 15) + (cdf[nsymbs] > 31)
  // + get_msb(nsymbs) - 1
  rate = 4 + (count >> 4) + (nsymbs > 3);
```

Good:

```cpp
  // rate is computed in the spec as:
  //   3 + (cdf[N] > 15) + (cdf[N] > 31) + Min(FloorLog2(N), 2)
  // In this case cdf[N] is count (bounded by 32).
  // Min(FloorLog2(N), 2) is 1 for nsymbs in {2, 3} and 2 for nsymbs > 3.
  // For count in [0, 32], (count > 15) + (count > 31) simplifies to (count >> 4).
  // Thus, rate = 4 + (count >> 4) + (nsymbs > 3).
  const int rate = 4 + (count >> 4) + (nsymbs > 3);
```
