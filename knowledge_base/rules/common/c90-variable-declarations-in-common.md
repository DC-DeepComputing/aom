---
name: libaom/common/c90-variable-declarations-in-common
description: When writing or modifying C utility code in common/, declare local variables at the top of the block rather than mixing declarations with statements for C90 compatibility
rubric: readability
severity: should_fix
---

**Rule.** In C utility code under `common/` (such as `common/y4minput.c` and
`common/tools_common.c`), declare all local variables at the beginning of the
function or enclosing block before any executable statements. Do not mix
variable declarations with executable statements.

**Why.** C source files in `common/` provide shared CLI and utility tools that
are frequently backported to or shared with libvpx, which requires C90
compliance. Mixing variable declarations and statements is a C99/C++ feature
that breaks C90-only builds and complicates porting.

## Examples

Bad:

```cpp
static int parse_tags(y4m_input *y4m_ctx, FILE *file) {
  y4m_ctx->pic_w = -1;
  y4m_ctx->pic_h = -1;

  char tag[256];
  char end;
  do {
    // ...
  } while (end != '\n');
  return 0;
}
```

Good:

```cpp
static int parse_tags(y4m_input *y4m_ctx, FILE *file) {
  char tag[256];
  char end;
  y4m_ctx->pic_w = -1;
  y4m_ctx->pic_h = -1;

  do {
    // ...
  } while (end != '\n');
  return 0;
}
```
