---
name: libaom/aom/describe-option-tradeoffs-neutrally-in-headers
description: When documenting configuration options or tuning modes in public API headers, describe metric and subjective trade-offs neutrally and factually without overly negative phrasing
rubric: readability
severity: nit
---

**Rule.** When documenting configuration options, tuning metrics, or operational
modes in public API headers (such as aom/*.h), describe trade-offs between
competing optimization metrics, compression efficiency, and subjective visual
quality neutrally and factually. Avoid overly negative, disparaging, or
discouraging language that makes a valid mode sound broken; instead, explain the
specific optimization target and clarify when or why the setting does not yield
improvements under alternative evaluation criteria.

**Why.** Public API headers serve as external reference documentation for
library consumers choosing configuration and tuning parameters. Negative or
disparaging phrasing can confuse users into believing an option is defective or
deprecated, whereas neutral, factual trade-off descriptions accurately guide
users on which optimization targets best fit their specific use case.

## Examples

Bad:

```cpp
/*!\brief SSIMULACRA 2 tuning mode.
 *
 * AOM_TUNE_SSIMULACRA2 shares most of the tweaks with AOM_TUNE_IQ. However,
 * AOM_TUNE_SSIMULACRA2 fine-tunes the encoder in ways that have been shown to
 * negatively impact subjective quality in human evaluations.
 */
AOM_TUNE_SSIMULACRA2 = 11,
```

Good:

```cpp
/*!\brief SSIMULACRA 2 tuning mode.
 *
 * AOM_TUNE_SSIMULACRA2 shares most of the tweaks with AOM_TUNE_IQ. However,
 * AOM_TUNE_SSIMULACRA2 fine-tunes the encoder in ways that have been shown to
 * not come with a corresponding positive impact on subjective quality in human
 * evaluations.
 */
AOM_TUNE_SSIMULACRA2 = 11,
```
