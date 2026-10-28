---
name: libaom/av1/differentiate-external-qp-overrides-from-fixed-q-mode
description: When naming variables, flags, or controls for externally supplied per-frame QP overrides, use external_qp rather than fixed_qp to avoid confusion with constant quality mode.
rubric: readability
severity: should_fix
---

**Rule.** When adding or modifying variables, struct members, or configuration
controls for dynamic, externally supplied per-frame QP overrides, name them with
`external_qp` or `user_config_qp` rather than `fixed_qp`. Reserve 'fixed Q' and
'fixed QP' terminology for constant quality mode (`AOM_Q`).

**Why.** In video encoding, 'fixed QP' is strongly associated with constant
quality mode (AOM_Q). Using 'fixed_qp' for dynamic external QP overrides causes
ambiguity as to whether the encoder is operating in constant quality mode or
accepting external rate control inputs.

## Examples

Bad:

```cpp
// Ambiguous: sounds like constant quality (fixed Q) mode
int use_fixed_qp_one_pass;
int fixed_qp_value;
```

Good:

```cpp
// Clearly indicates externally supplied per-frame QP override
int use_external_qp_one_pass;
int external_qp_value;
```
