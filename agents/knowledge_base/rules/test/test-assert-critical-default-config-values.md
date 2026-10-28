---
name: libaom/test/test-assert-critical-default-config-values
description: When writing unit tests that rely on default configuration values from aom_codec_enc_config_default(), assert expected critical field values before proceeding
rubric: testing
severity: should_fix
---

**Rule.** When writing unit tests that rely on default encoder or decoder
configuration values initialized by aom_codec_enc_config_default() or
aom_codec_dec_config_default(), explicitly assert expected values for critical
configuration fields (such as g_lag_in_frames or usage-specific parameters) that
the test's preconditions depend on before initializing the codec.

**Why.** When tests rely on specific default configuration settings without
explicit verification, changes to library defaults can silently invalidate test
preconditions or cause unexpected test failures. Explicit assertions document
the test's assumptions and ensure default initialization is validated before
codec execution.

## Examples

Bad:

```cpp
ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
          AOM_CODEC_OK);

aom_codec_ctx_t enc;
ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
```

Good:

```cpp
ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_REALTIME),
          AOM_CODEC_OK);
ASSERT_EQ(cfg.g_lag_in_frames, 0);

aom_codec_ctx_t enc;
ASSERT_EQ(aom_codec_enc_init(&enc, iface, &cfg, 0), AOM_CODEC_OK);
```
