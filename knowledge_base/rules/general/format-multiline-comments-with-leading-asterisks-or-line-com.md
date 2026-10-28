---
name: libaom/format-multiline-comments-with-leading-asterisks-or-line-com
description: When writing multi-line comments, use consecutive // comments or block comments with leading continuation asterisks aligned under the opening delimiter
rubric: readability
severity: nit
---

**Rule.** When writing multi-line comments, format them either as consecutive //
line comments or as a /* ... */ block comment with a leading asterisk (*) on
each continuation line aligned under the opening delimiter. Do not write bare,
unadorned text lines without leading asterisks inside multi-line block comments,
including comments inside multi-line preprocessor macro definitions.

**Why.** Leading asterisks on continuation lines in block comments (or
consecutive // comments) clearly delineate comment boundaries from code across
diff viewers and code editors. This avoids visual ambiguity, especially in
multi-line preprocessor macro definitions where automated formatters may not
reformat continuation lines.

## Examples

Bad:

```cpp
// Unadorned continuation lines in block comments
/* Increases image quality and consistency, guided by the SSIMULACRA2 metric
   and subjective quality checks. This tune shares the rdmult code with
   AOM_TUNE_SSIM. */

// Unadorned continuation lines in preprocessor macro definitions
/* Pad end pixels to the right, while processing the last pixels in the      \
   row. */                                                                   \
```

Good:

```cpp
// Leading asterisks on continuation lines (or use consecutive // comments)
/* Increases image quality and consistency, guided by the SSIMULACRA2 metric
 * and subjective quality checks. This tune shares the rdmult code with
 * AOM_TUNE_SSIM.
 */

// Leading asterisk aligned under opening delimiter in macro definitions
/* Pad end pixels to the right, while processing the last pixels in the      \
 * row. */                                                                   \
```
