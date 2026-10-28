---
name: libaom/cite-external-specifications-and-papers-for-algorithms
description: When implementing or adapting algorithms or heuristics described in external specifications, papers, or reference codebases, include a citation or URL in doc comments or entry-point functions
rubric: readability
severity: nit
---

**Rule.** When introducing or adapting an algorithm, mode, or heuristic derived
from an external specification, research paper, technical appendix, or reference
codebase (such as SVT-AV1, libvpx, or standard documents), provide a citation,
document title, or URL in the public API enum/control doc comment and at the
primary implementation function. Document key references, mathematical formulas,
and section numbers so reviewers and maintainers can understand the design
rationale without reverse-engineering the code.

**Why.** Complex encoding heuristics, rate-control models, and filtering
algorithms often rely on mathematical derivations, empirical tuning, or
architectural designs documented in external papers or reference
implementations. Providing explicit citations or URLs gives maintainers
immediate access to algorithmic specifications and visual explanations, reducing
ambiguity and preventing regressions during refactoring.

## Examples

Bad:

```cpp
/*!\brief Codec control function to set delta Q mode:
 * - 0 = no delta quant
 * - 6 = use modulation for allintra using Variance Boost
 */
AV1E_SET_DELTAQ_MODE = 107,

int av1_get_sbq_variance_boost(const AV1_COMP *cpi, const MACROBLOCK *x);
```

Good:

```cpp
/*!\brief Codec control function to set delta Q mode:
 * - 0 = no delta quant
 * - 6 = use modulation for allintra using Variance Boost (see
 *       gitlab.com/AOMediaCodec/SVT-AV1/-/blob/master/Docs/Appendix-Variance-Boost.md)
 */
AV1E_SET_DELTAQ_MODE = 107,

// Variance Boost: variance-adaptive quantization implementation.
// Reference: SVT-AV1 Appendix - Variance Boost
// https://gitlab.com/AOMediaCodec/SVT-AV1/-/blob/master/Docs/Appendix-Variance-Boost.md
int av1_get_sbq_variance_boost(const AV1_COMP *cpi, const MACROBLOCK *x);
```
