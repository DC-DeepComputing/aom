---
name: libaom/omit-redundant-pointer-to-in-doxygen-param-descriptions
description: When writing Doxygen \param doc comments for pointer parameters, omit redundant 'Pointer to' prefixes and describe the referenced buffer or object directly
rubric: readability
severity: nit
---

**Rule.** In Doxygen \param (and \return) documentation comments, do not prefix
parameter descriptions with 'Pointer to' or 'ptr to' for pointer arguments.
Describe the underlying data, buffer, struct, or object directly, as the pointer
type is already evident from the C/C++ function signature.

**Why.** Prefixing parameter descriptions with 'Pointer to' adds visual noise
and redundant type commentary without conveying meaningful semantics. Omitting
trivial type restatements keeps API documentation concise and focused on the
role, invariants, and contents of the referenced data.

## Examples

Bad:

```cpp
/*! \brief Apply directional filtering to a frame.
 *
 * \param[in]      cm             Pointer to common frame parameters
 * \param[in,out]  frame          Pointer to compressed frame buffer
 * \param[out]     dst            Pointer to filtered output plane
 * \param[in]      damping        Damping factor value
 */
void cdef_filter_frame(const AV1_COMMON *cm, YV12_BUFFER_CONFIG *frame,
                       uint16_t *dst, int damping);
```

Good:

```cpp
/*! \brief Apply directional filtering to a frame.
 *
 * \param[in]      cm             Common frame parameters
 * \param[in,out]  frame          Compressed frame buffer
 * \param[out]     dst            Filtered output plane
 * \param[in]      damping        Damping factor value
 */
void cdef_filter_frame(const AV1_COMMON *cm, YV12_BUFFER_CONFIG *frame,
                       uint16_t *dst, int damping);
```
