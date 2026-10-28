---
name: libaom/unambiguous-cli-help-strings
description: When defining CLI arguments (ARG_DEF) or API options with mode choices, prerequisites, and defaults, provide descriptive explanations and specify per-mode requirements and unambiguous ranges
rubric: readability
severity: should_fix
---

**Rule.** In CLI argument definitions (ARG_DEF, ARG_DEF_ENUM) and API option
docstrings, provide complete and descriptive explanations of mode choices rather
than abbreviated descriptors. Structure and punctuate help strings so trailing
valid range and default value annotations unambiguously associate with the
overall argument rather than attaching only to an adjacent conditional clause.
When a prerequisite flag is only needed for specific modes or values, explicitly
state which specific mode requires the prerequisite (e.g., '--<option>=<val>
requires --<flag>=1') rather than stating that the option as a whole requires
it.

**Why.** Abbreviated descriptors leave target features unclear to users, while
poorly punctuated conditional clauses mislead readers into thinking global
defaults or ranges apply only to specific operating modes. Similarly, stating
that an option as a whole requires a prerequisite flag misleads users
configuring other modes that operate independently of that flag.

## Examples

Bad:

```cpp
// Ambiguous default/range attachment:
.sharpness = ARG_DEF(NULL, "sharpness", 1,
                     "Bias towards block sharpness in rate-distortion "
                     "optimization of transform coefficients. It also "
                     "reduces block edge filtering for better sharpness "
                     "in allintra mode (0..7), default is 0");

// Abbreviated mode descriptor and missing per-mode default context:
.sct_detection_mode =
    ARG_DEF(NULL, "sct-detection-mode", 1,
            "Screen content tools detection mode (1: standard (default), "
            "2: anti-alias aware)");

// Imprecise option-level prerequisite requirement for mode-specific dependency:
.deltaq_mode =
    ARG_DEF(NULL, "deltaq-mode", 1,
            "Delta qindex mode (0: off, 1: deltaq objective (default), "
            "2: deltaq placeholder, 3: key frame visual quality, 4: user "
            "rating based visual quality optimization, 5: HDR video, 6: "
            "Variance Boost); requires --enable-tpl-model=1");
```

Good:

```cpp
// Clear clause grouping for global default and range:
.sharpness =
    ARG_DEF(NULL, "sharpness", 1,
            "Bias towards block sharpness in rate-distortion optimization of "
            "transform coefficients and (in allintra mode only) reduce block "
            "edge filtering for better sharpness (0..7), default is 0");

// Descriptive mode explanations and explicit per-mode default annotations:
.screen_detection_mode =
    ARG_DEF(NULL, "screen-detection-mode", 1,
            "Screen content detection mode (1: standard (default in good "
            "quality and realtime modes), 2: anti-aliased text and graphics "
            "aware (default in all intra mode))");

// Explicit per-mode prerequisite specification:
.deltaq_mode =
    ARG_DEF(NULL, "deltaq-mode", 1,
            "Delta qindex mode (0: off, 1: deltaq objective (default), "
            "2: deltaq placeholder, 3: key frame visual quality, 4: user "
            "rating based visual quality optimization, 5: HDR video, 6: "
            "Variance Boost); --deltaq-mode=1 requires --enable-tpl-model=1");
```
