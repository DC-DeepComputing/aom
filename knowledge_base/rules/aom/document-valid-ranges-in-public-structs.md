---
name: libaom/aom/document-valid-ranges-in-public-structs
description: When defining public API structs and codec controls in headers, document member semantics, allowable ranges, and interdependent field mappings on struct declarations, referencing example code
rubric: readability
severity: nit
---

**Rule.** In public API headers, document struct member fields, valid value
ranges (such as `(0..7)`, `[0, 255]`, or `0 or 1`), and interdependent field
mappings directly on the struct declaration rather than in associated codec
control enum comments; reference example code or specs for complex usage
patterns. Keep codec control enum documentation concise by referencing the
parameter struct type.

**Why.** Co-locating field semantics, valid ranges, and mapping relationships
with struct declarations keeps data structures self-contained and clear without
bloating control enums. Providing references to sample code ensures API
consumers understand non-trivial configurations without inspecting internal
codec source code.

## Examples

Bad:

```cpp
/*!\brief Codec control function to set reference frame config:
 * the reference flags, which indicate which of 7 references are used;
 * the ref_idx, which maps a reference to one of 8 buffer slots;
 * aom_svc_ref_frame_config_t* parameter
 */
AV1E_SET_SVC_REF_FRAME_CONFIG = 133,

...

typedef struct aom_svc_ref_frame_config {
  int reference[7]; /**< Reference flag for each of the 7 references. */
  /*! Buffer slot index for each of 7 references indexed above. */
  int ref_idx[7];
  int refresh[8]; /**< Refresh flag for each of the 8 slots. */
} aom_svc_ref_frame_config_t;
```

Good:

```cpp
/*!\brief Codec control function to set the reference frame config:
 * aom_svc_ref_frame_config_t* parameter
 */
AV1E_SET_SVC_REF_FRAME_CONFIG = 133,

...

/*!\brief Parameters for setting reference frame configuration */
typedef struct aom_svc_ref_frame_config {
  // reference[7]: boolean flag (0 or 1) indicating which references are used.
  // ref_idx[7]: maps each reference (0..6) to a buffer slot (0..7).
  // refresh[8]: boolean flag (0 or 1) indicating if a buffer slot is updated.
  // Usage examples: examples/svc_encoder_rtc.c.
  int reference[7]; /**< Reference flag (0 or 1) for each reference. */
  /*! Buffer slot index (0..7) for each reference. */
  int ref_idx[7];
  int refresh[8]; /**< Refresh flag (0 or 1) for each buffer slot. */
} aom_svc_ref_frame_config_t;
```
