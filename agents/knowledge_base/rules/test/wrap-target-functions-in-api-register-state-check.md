---
name: libaom/test/wrap-target-functions-in-api-register-state-check
description: When invoking architecture-specific SIMD or assembly target functions in unit and speed tests, wrap calls in API_REGISTER_STATE_CHECK
rubric: readability
severity: should_fix
---

**Rule.** In unit and speed tests under test/, wrap invocations of
architecture-specific SIMD or assembly target functions (such as target_func_ or
params_.func) in API_REGISTER_STATE_CHECK(...). Do not invoke SIMD or assembly
target functions directly without this wrapper.

**Why.** API_REGISTER_STATE_CHECK validates that callee-saved registers (such as
xmm6–xmm15 on Windows x64) and FPU state (such as MMX tag words on x86) are
preserved across function calls. Calling target functions without it misses
register corruption bugs that only manifest on specific platforms or compiler
targets.

## Examples

Bad:

```cpp
// Target SIMD function called directly without register state verification
target_func_(p1, p2, x, &rate_f_mod, &distbysse_f_mod);
```

Good:

```cpp
// Target SIMD function wrapped in API_REGISTER_STATE_CHECK
API_REGISTER_STATE_CHECK(
    target_func_(p1, p2, x, &rate_f_mod, &distbysse_f_mod));
```
