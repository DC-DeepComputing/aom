---
name: libaom/define-symbols-in-explanatory-comments
description: In explanatory and formula comments, match referenced variable names to in-scope identifiers and explicitly define undeclared mathematical symbols or domain acronyms
rubric: readability
severity: nit
---

**Rule.** When writing or updating explanatory comments, mathematical
derivations, or algorithmic formulas, ensure identifier references and symbols
are accurate and self-contained:
1. Verify that all referenced variable, parameter, and function names match the exact identifiers in scope rather than retaining stale, draft, or reference-specification names.
2. Explicitly define any mathematical symbols, shorthand variables, or hypothetical condition terms not declared in local scope (e.g., using "where e = ...").
3. Spell out domain-specific abbreviations and acronyms on first use (e.g., restoration unit for RU, coding unit for CU, super-block for SB) rather than using unexplained shorthand.

**Why.** Mismatched variable names or undefined shorthand symbols and domain
acronyms in comments confuse readers, hinder code search, and obscure whether
the implementation correctly matches documented derivations. Keeping identifiers
accurate and symbols well-defined ensures explanations remain maintainable and
self-contained.

## Examples

Bad:

```cpp
// Variant 1: Stale / mismatched variable names in derivation comments
// Min(FloorLog2(N), 2) is 1 for nsymbs == {2, 3} and 2 for all
// nsymbs > 3. So the equation becomes:
//  4 + (count > 15) + (count > 31) + (symbol_count > 3).
const int rate = 4 + (count >> 4) + (nsymbs > 3);

// Variant 2: Undefined shorthand/formula variables in alternative condition comments
/* An alternate condition would be if (e < d). But, this would have
   caused pushing leading 0x00's, which needed special handling. */
if (s >= 40) {
  // ...
}

// Variant 3: Unexplained domain abbreviation in algorithm comment
// The largest scale_threshold is expected to depend on bit-depth and maximum RU size.
```

Good:

```cpp
// Variant 1: Match referenced variable names to in-scope identifiers
// Min(FloorLog2(N), 2) is 1 for nsymbs == {2, 3} and 2 for all
// nsymbs > 3. So the equation becomes:
//  4 + (count > 15) + (count > 31) + (nsymbs > 3).
const int rate = 4 + (count >> 4) + (nsymbs > 3);

// Variant 2: Explicitly define undeclared formula variables inline
/* An alternate condition would be if (e < d), where e = number of leading
   zeros in "low", indicating there is not enough room to accommodate "rng"
   worth of "d"-bits in "low". */
if (s >= 40) {
  // ...
}

// Variant 3: Spell out domain acronyms on first use
// The largest scale_threshold is expected to depend on bit-depth and maximum restoration-unit (RU) size.
```
