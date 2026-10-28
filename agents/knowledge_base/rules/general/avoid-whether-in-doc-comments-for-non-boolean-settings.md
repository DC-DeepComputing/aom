---
name: libaom/avoid-whether-in-doc-comments-for-non-boolean-settings
description: When writing doc comments or brief descriptions for non-boolean or multi-valued settings, avoid 'Whether' and use 'Controls how', 'Specifies', or 'Mode for'
rubric: readability
severity: should_fix
---

**Rule.** Do not use 'Whether ...' in doc comments or brief descriptions for
non-boolean, multi-valued configuration options, struct fields, or codec
controls; use phrasing like 'Controls how ...', 'Specifies ...', 'Determines
...', or 'Mode for ...' instead. Reserve 'Whether ...' strictly for binary
boolean flags and predicates.

**Why.** The word 'Whether' implies a binary (true/false) condition. Using
'Whether' for settings or fields that accept multiple modes, numeric levels, or
non-boolean values (such as 0, 1, 2) is misleading and obscures the multi-valued
nature of the configuration.

## Examples

Bad:

```cpp
/*!\brief Whether encoder should use fixed QP offsets.
 *
 * If a value of 1 is provided, encoder will use fixed QP offsets...
 * If a value of 2 is provided, encoder will use the same QP...
 * If a value of 0 is provided, encoder will NOT use fixed QP offsets.
 */
unsigned int use_fixed_qp_offsets;
```

Good:

```cpp
/*!\brief Controls how the encoder applies fixed QP offsets.
 *
 * If a value of 0 is provided, encoder will adaptively choose QP offsets...
 * If a value of 1 is provided, encoder will use fixed QP offsets...
 * If a value of 2 is provided, encoder will use the same QP...
 */
unsigned int use_fixed_qp_offsets;
```
