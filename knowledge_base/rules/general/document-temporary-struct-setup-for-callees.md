---
name: libaom/document-temporary-struct-setup-for-callees
description: When initializing temporary or dummy structures and pointer indirections solely to satisfy downstream callee preconditions, add an explanatory comment naming the expecting function.
rubric: readability
severity: nit
---

**Rule.** When initializing temporary or dummy structures, local context
buffers, or pointer indirections solely to satisfy implicit preconditions of a
downstream callee function, add an explanatory comment above the setup. State
which downstream function expects the structure or field and why the setup is
required.

**Why.** Temporary struct setups and pointer redirections that are not directly
used in the local function appear redundant or confusing to readers. An
explanatory comment prevents future refactors from inadvertently removing
necessary initialization or struggling to determine why dummy fields exist.

## Examples

Bad:

```cpp
MB_MODE_INFO mbmi;
memset(&mbmi, 0, sizeof(mbmi));
MB_MODE_INFO *mbmi_ptr = &mbmi;
xd->mi = &mbmi_ptr;
cm->quant_params.base_qindex = cpi->oxcf.rc_cfg.cq_level;
av1_frame_init_quantizer(cpi);
```

Good:

```cpp
// xd->mi must be set up because av1_frame_init_quantizer expects
// valid macroblock mode info when computing quantizer parameters.
MB_MODE_INFO mbmi;
memset(&mbmi, 0, sizeof(mbmi));
MB_MODE_INFO *mbmi_ptr = &mbmi;
xd->mi = &mbmi_ptr;
cm->quant_params.base_qindex = cpi->oxcf.rc_cfg.cq_level;
av1_frame_init_quantizer(cpi);
```
