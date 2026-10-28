---
name: libaom/avoid-ambiguous-abbreviations-in-error-messages
description: When writing error messages or diagnostics, avoid ambiguous abbreviations and informal descriptors; use explicit parameter names, struct member paths, canonical terms, and directional bounds
rubric: readability
severity: nit
---

**Rule.** In error messages, diagnostics, and validation checks (such as via
ERROR(), aom_internal_error(), aom_set_error(), or set_error_detail()), do not
use ambiguous internal abbreviations (e.g., 'AI' for all-intra), informal
descriptors, or vague relational phrasing (e.g., 'at least match'). Instead, use
canonical CLI/codebase terminology, explicit parameter names, or exact struct
member paths (e.g., cpi->orig_source), and state precise directional inequality
bounds (e.g., 'must not exceed' or 'must be greater than or equal to').

**Why.** Ambiguous abbreviations, informal descriptors, and vague relational
phrasing confuse users and developers diagnosing runtime or configuration
failures. Using exact parameter names, struct member paths, canonical
terminology, and precise directional bounds immediately clarifies which resource
failed and what values are valid.

## Examples

Bad:

```cpp
// Ambiguous abbreviation:
ERROR("Perceptual AI (deltaq_mode = 3) can only be set in all intra mode");

// Ambiguous relational phrasing:
if (cfg->g_input_bit_depth > cfg->g_bit_depth)
  ERROR("Codec bitdepth must at least match the input bit depth");

// Informal / vague descriptor:
aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                   "Failed to allocate true source buffer for psnr calculation");
```

Good:

```cpp
// Canonical parameter description:
ERROR("Key frame visual quality (deltaq_mode = 3) can only be set in all intra mode");

// Clear directional bound:
if (cfg->g_input_bit_depth > cfg->g_bit_depth)
  ERROR("Input bit-depth must not exceed codec bit-depth");

// Explicit struct member path:
aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                   "Failed to allocate cpi->orig_source buffer");
```
