---
name: libaom/aom/document-unsupported-control-options-as-no-effect
description: When documenting public codec controls or parameters unsupported in specific modes, explicitly state they have no effect or are ignored and use standard acronym capitalization
rubric: readability
severity: should_fix
---

**Rule.** In public API doc comments (such as \brief or \attention notes in
aom/*.h) and CLI option definitions (ARG_DEF), when documenting codec controls,
configuration parameters, or options unsupported under specific operational
modes (such as non-RD or realtime mode):
1. Explicitly and concisely state that setting the unsupported value/mode 'has no effect' or is 'silently ignored' alongside any fallback or default value substitution.
2. Directly specify which parameter values are unsupported and their operational preconditions rather than relying solely on verbose conditional statements.
3. Use standard technical acronym capitalization (e.g., 'non-RD' rather than 'non-rd', 'RTC', or 'HDR').

**Why.** Describing only what an unsupported option value is treated as (e.g.,
'will be treated as setting to 0') leaves callers uncertain whether the setting
was rejected, clamped, partially applied, or silently altered encoder state.
Explicitly stating that unsupported settings 'have no effect' or are 'silently
ignored' establishes clear, unambiguous contract semantics for API consumers and
CLI users. Additionally, concise phrasing and consistent technical acronym
capitalization improve documentation clarity and maintainability across headers.

## Examples

Bad:

```cpp
// Variant 1: Codec control description omits that setting has no effect
/*!\brief Codec control to set delta q mode, unsigned int parameter.
 *
 * Setting delta q mode to any other value in non-rd mode will be treated
 * as setting the delta q mode to 0.
 */
AV1E_SET_DELTAQ_MODE = 107,

// Variant 2: Wordy doc comment with non-standard acronym capitalization
* \attention Only delta q mode 6 is supported in non-rd mode. Non-rd mode is
* enabled by setting cpu-used >= 8 (all intra usage) and cpu-used >= 7
* (realtime usage). Setting delta q mode to any other value in non-rd mode
* will be treated as setting the delta q mode to 0.
```

Good:

```cpp
// Variant 1: Explicitly states setting has no effect with fallback behavior
/*!\brief Codec control to set delta q mode, unsigned int parameter.
 *
 * Setting delta q mode to any other value in non-RD mode has no effect
 * and will be treated as setting the delta q mode to 0.
 */
AV1E_SET_DELTAQ_MODE = 107,

// Variant 2: Concise phrasing specifying unsupported values and standard capitalization
* \attention Delta q modes 1-5 are unsupported and are silently ignored in
* non-RD mode. Non-RD mode is enabled by setting cpu-used >= 8 (all intra
* usage) and cpu-used >= 7 (realtime usage).
```
