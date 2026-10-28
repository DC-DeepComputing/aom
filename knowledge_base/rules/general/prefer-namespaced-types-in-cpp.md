---
name: libaom/prefer-namespaced-types-in-cpp
description: When referencing dual C and C++ interface types in C++ source files, prefer namespaced types like aom::AV1RateControlRtcConfig over C-prefixed types like AomAV1RateControlRtcConfig
rubric: readability
severity: should_fix
---

**Rule.** In C++ source files (`.cc`, `.cpp`), prefer using C++ namespaced types
(such as `aom::AV1RateControlRtcConfig` or `aom::AV1RateControlRTC`) instead of
C-compatibility prefixed types (such as `AomAV1RateControlRtcConfig`). Keep
C-prefixed types reserved for pure C headers or tests explicitly exercising C
API bindings.

**Why.** When an API provides both C and C++ interfaces, C types use global
prefixes to avoid collisions, whereas C++ interfaces provide scoped types inside
`namespace aom`. Using namespaced types in C++ code follows C++ style
conventions, avoids global namespace pollution, and maintains consistency across
C++ callers.

## Examples

Bad:

```cpp
static AomAV1RateControlRtcConfig create_rtc_rc_config(
    const aom_codec_enc_cfg_t &cfg, const AppInput &app_input) {
  AomAV1RateControlRtcConfig rc_cfg;
  return rc_cfg;
}
```

Good:

```cpp
static aom::AV1RateControlRtcConfig create_rtc_rc_config(
    const aom_codec_enc_cfg_t &cfg, const AppInput &app_input) {
  aom::AV1RateControlRtcConfig rc_cfg;
  return rc_cfg;
}
```
