---
name: libaom/aom/align-cicp-enums-with-itu-t-h273
description: When declaring or updating CICP color format enums in aom_image.h, match standardized ITU-T H.273 identifiers rather than labeling standard indices as reserved
rubric: correctness
severity: nit
---

**Rule.** When declaring or updating Coding-Independent Code Points (CICP)
enumerations in aom/aom_image.h (such as aom_color_primaries_t,
aom_transfer_characteristics_t, and aom_matrix_coefficients_t) and corresponding
CLI argument tables in av1/arg_defs.c, align enumerator identifiers and integer
values with standard ITU-T H.273 (and AVM) specifications. Do not mark standard
code point indices as reserved when standard names (such as AOM_CICP_MC_IPT_C2,
AOM_CICP_MC_YCGCO_RE, or AOM_CICP_MC_YCGCO_RO) exist.

**Why.** ITU-T H.273 and reference codec specifications define standardized
integer code points for color primaries, transfer characteristics, and matrix
coefficients. Leaving standardized indices as reserved or assigning non-standard
values prevents interoperability across codecs, confuses CLI users, and requires
backwards-compatibility aliases when standard names are later introduced.

## Examples

Bad:

```cpp
// Bad: Marking standardized ITU-T H.273 matrix coefficient index 15 as reserved
typedef enum aom_matrix_coefficients {
  AOM_CICP_MC_ICTCP = 14,       /**< BT.2100 ICtCp */
  AOM_CICP_MC_RESERVED_15 = 15, /**< For future use */
  AOM_CICP_MC_YCGCO_RE = 16,    /**< YCgCo-Re */
} aom_matrix_coefficients_t;
```

Good:

```cpp
// Good: Naming standard ITU-T H.273 / AVM code points explicitly
typedef enum aom_matrix_coefficients {
  AOM_CICP_MC_ICTCP = 14,       /**< BT.2100 ICtCp */
  AOM_CICP_MC_RESERVED_15 = 15, /**< For backward compatibility */
  AOM_CICP_MC_IPT_C2 = 15,      /**< IPT-C2 */
  AOM_CICP_MC_YCGCO_RE = 16,    /**< YCgCo-Re */
  AOM_CICP_MC_YCGCO_RO = 17,    /**< YCgCo-Ro */
  AOM_CICP_MC_RESERVED_18 = 18, /**< For future use (values 18-255) */
} aom_matrix_coefficients_t;
```
