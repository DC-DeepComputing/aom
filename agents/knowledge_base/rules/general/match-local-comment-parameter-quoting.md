---
name: libaom/match-local-comment-parameter-quoting
description: When referencing function parameters or variable names in comments, match the surrounding file's existing quoting style such as 'param', |param|, or `param`
rubric: readability
severity: nit
---

**Rule.** When referencing function parameters, variables, or symbol names in
comments, adhere to the enclosing file's established delimiting style (such as
single quotes 'param', pipes |param|, or backticks `param`). Avoid using bare
unquoted identifier names or introducing mismatched delimiter styles in files
with an established convention.

**Why.** Consistently delimiting identifier names within each file makes code
comments easier to read, clearly distinguishes variable references from regular
English words, and avoids inconsistent visual notation across comments in the
codebase.

## Examples

Bad:

```cpp
// Stores the color range in the y4m_ctx, returning 1 if successfully parsed.
static int parse_color_range(y4m_input *y4m_ctx, const char *buf);
```

Good:

```cpp
// Stores the color range in 'y4m_ctx', returning 1 if successfully parsed.
static int parse_color_range(y4m_input *y4m_ctx, const char *buf);
```
