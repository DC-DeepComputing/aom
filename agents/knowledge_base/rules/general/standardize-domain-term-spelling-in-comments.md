---
name: libaom/standardize-domain-term-spelling-in-comments
description: When writing doc comments and code documentation, maintain consistent spelling, hyphenation, and terminology for domain terms rather than mixing spelling variants in prose
rubric: readability
severity: nit
---

**Rule.** Maintain consistent spelling, hyphenation, and terminology for
technical and domain concepts (such as standardizing on 'bit-depth' in prose
rather than mixing 'bit-depth', 'bit depth', 'bitdepth', and 'bit_depth') across
comments and documentation within the same struct, header, or module.

**Why.** Mixing multiple spelling, hyphenation, or identifier variants for the
same technical concept within adjacent comments creates inconsistency, impairs
searchability, and diminishes documentation readability.

## Examples

Bad:

```cpp
/*!\\brief Bit-depth of the input source
 *
 * This value identifies the actual bit_depth of the input source in bits.
 * Note that the frames passed as input to the encoder must match codec bit
 * depth. If there is a mismatch between source bit depth and codec bit depth,
 * ... So, source bitdepth must not exceed codec bit depth.
 */
unsigned int g_input_bit_depth;
```

Good:

```cpp
/*!\\brief Bit-depth of the input source
 *
 * This value identifies the actual bit-depth of the input source in bits.
 * Note that the frames passed as input to the encoder must match codec
 * bit-depth. If there is a mismatch between source bit-depth and codec
 * bit-depth, ... So, source bit-depth must not exceed codec bit-depth.
 */
unsigned int g_input_bit_depth;
```
