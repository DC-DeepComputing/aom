---
name: libaom/place-unreachable-assertions-in-switch-default
description: 'When all valid switch cases return, place unreachable assertions and fallback error returns inside a default: label rather than after the switch block'
rubric: readability
severity: should_fix
---

**Rule.** In switch statements where all valid cases return or handle expected
values, place assertions for unreachable states (such as `assert(0);` or
`assert(false);`) and fallback error returns inside a `default:` label rather
than after the switch block.

**Why.** Placing unreachable assertions and fallback returns inside a `default:`
case keeps the switch statement self-contained, explicitly documents that
unhandled values are invalid, and structures control flow cleanly within the
construct.

## Examples

Bad:

```cpp
switch (ref_idx) {
  case 0: return ec_ctx->drl0_cdf[ctx];
  case 1: return ec_ctx->drl1_cdf[ctx];
  case 2: return ec_ctx->drl2_cdf[ctx];
}

assert(0);
return NULL;
```

Good:

```cpp
switch (ref_idx) {
  case 0: return ec_ctx->drl0_cdf[ctx];
  case 1: return ec_ctx->drl1_cdf[ctx];
  case 2: return ec_ctx->drl2_cdf[ctx];
  default:
    assert(0);
    return NULL;
}
```
