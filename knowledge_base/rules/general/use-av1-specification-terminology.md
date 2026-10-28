---
name: libaom/use-av1-specification-terminology
description: When naming public APIs, CLI options, identifiers, or doc comments, use official AV1 specification terminology rather than external codec abbreviations like SCC
rubric: readability
severity: nit
---

**Rule.** Use official AV1 specification terminology (such as 'screen content
tools' / 'screen_content_tools', 'intra block copy', or 'palette mode') rather
than non-standard abbreviations or terms originating from other video codec
standards (such as HEVC's 'SCC' for Screen Content Coding) across public APIs,
CLI options, doc comments, and internal identifiers.

**Why.** Adhering to AV1 specification naming ensures consistent terminology
across libaom public headers, CLI tools, and encoder implementations, avoiding
confusion caused by acronyms and concepts specific to competing or prior codec
standards.

## Examples

Bad:

```cpp
// Bad: Using HEVC abbreviation 'SCC' (Screen Content Coding) in public API doc and enum
AOM_CONTENT_ENHANCED = 3,  /*!< Regular content, with enhanced SCC content detection */
```

Good:

```cpp
// Good: Using standard AV1 specification terminology ('screen content tools')
/*!\brief Codec control to set the screen content tools detection mode */
AV1E_SET_SCREEN_CONTENT_TOOLS_DETECTION_MODE = 171,
```
