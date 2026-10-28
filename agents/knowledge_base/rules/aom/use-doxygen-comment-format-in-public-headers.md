---
name: libaom/aom/use-doxygen-comment-format-in-public-headers
description: Document public API declarations in aom/*.h using Doxygen syntax (/**, /*!), placing comments above declarations to avoid trailing line wraps
rubric: readability
severity: nit
---

**Rule.** In public API headers (aom/*.h), format documentation comments for
enums, struct members, typedefs, macros, and function declarations using Doxygen
syntax (/** ... */, /*! ... */, or /**< ... */) rather than standard C block
comments (/* ... */) or // line comments. When trailing member documentation
(/**< ... */) causes identifiers or values to wrap awkwardly across lines, place
the comment on the preceding line (/** ... */ or /*! ... */) without the '<'
post-declaration marker so Doxygen correctly associates the comment with the
following declaration.

**Why.** Public headers under aom/ are parsed by Doxygen to generate public API
reference documentation. Standard C comments (/* ... */) and regular // comments
are ignored by Doxygen, omitting descriptions from generated manuals. Placing
documentation comments on the preceding line avoids line-wrapping churn from
trailing comments while omitting '<' ensures correct symbol association.

## Examples

Bad:

```cpp
/* Standard screen detection mode */
AOM_SCREEN_DETECTION_STANDARD = 1,

/* Trailing Doxygen comment causes awkward line wrapping */
AOM_SCREEN_DETECTION_ANTIALIASING_AWARE =
    2, /**< Anti-aliased text and graphics aware */

/* Compression configuration parameters */
typedef struct aom_codec_enc_cfg {
  /* Target bitrate in kbps */
  unsigned int rc_target_bitrate;
} aom_codec_enc_cfg_t;

/**< Preceding comments must omit '<' */
AOM_SCREEN_DETECTION_EXTENDED = 3,
```

Good:

```cpp
/** Standard screen detection mode */
AOM_SCREEN_DETECTION_STANDARD = 1,

/** Anti-aliased text and graphics aware */
AOM_SCREEN_DETECTION_ANTIALIASING_AWARE = 2,

/*!\brief Compression configuration parameters */
typedef struct aom_codec_enc_cfg {
  /** Target bitrate in kbps */
  unsigned int rc_target_bitrate;
} aom_codec_enc_cfg_t;

/** Extended screen detection mode */
AOM_SCREEN_DETECTION_EXTENDED = 3,
```
