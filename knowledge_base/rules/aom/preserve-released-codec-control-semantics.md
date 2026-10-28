---
name: libaom/aom/preserve-released-codec-control-semantics
description: When changing units or semantics of released public codec controls, preserve existing enum IDs with deprecation comments and add new controls with explicit unit suffixes.
rubric: design
severity: should_fix
---

**Rule.** Do not modify the units, scale, or semantics of an already-released
public codec control enum or macro (such as in aom/aomcx.h or aom/aomdx.h).
Preserve the existing control for backward compatibility (annotating it with a
\deprecated comment if appropriate), and add a new codec control enum value with
explicit unit suffixes in its name (e.g., _MS_ for milliseconds).

**Why.** Public codec controls in stable releases form a public API contract
consumed by external libraries and applications. Silently changing their units
or semantics breaks backward compatibility for existing callers.

## Examples

Bad:

```cpp
/*!\brief Codec control to set the maximum number of consecutive frame drops,
 * in units of time (milliseconds), allowed for the frame dropper in 1 pass
 * CBR mode, int parameter. Value of zero has no effect.
 */
AV1E_SET_MAX_CONSEC_FRAME_DROP_CBR = 164,
```

Good:

```cpp
/*!\brief Codec control to set the maximum number of consecutive frame drops,
 * in units of frames, allowed for the frame dropper in 1 pass
 * CBR mode, int parameter. Value of zero has no effect.
 * \deprecated Use the new control AV1E_SET_MAX_CONSEC_FRAME_DROP_MS_CBR.
 */
AV1E_SET_MAX_CONSEC_FRAME_DROP_CBR = 164,

/*!\brief Codec control to set the maximum number of consecutive frame drops,
 * in units of time (milliseconds), allowed for the frame dropper in 1 pass
 * CBR mode, int parameter. Value of zero has no effect.
 */
AV1E_SET_MAX_CONSEC_FRAME_DROP_MS_CBR = 169,
```
