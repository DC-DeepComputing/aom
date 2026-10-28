---
name: libaom/separate-non-matching-members-from-doxygen-groups
description: When modifying struct fields enclosed in Doxygen member groups or categorized comment blocks, move any mismatched or converted member outside the group delimiters
rubric: readability
severity: nit
---

**Rule.** When modifying, converting, or declaring struct members within a
categorized comment block or Doxygen member group (e.g., \name ... /**@{*/ ...
/**@}*/), ensure all enclosed fields belong to that category. If a member no
longer fits the group's semantics (such as converting a bitfield member to a
standard integer within a bitfield group), move it outside the enclosing group
block or create a separate group.

**Why.** Placing fields that do not belong to a specific category (such as
non-bitfield members inside a bitfield reduction block) within a labeled Doxygen
group produces inaccurate and misleading generated documentation, confusing
readers about struct layout and field properties.

## Examples

Bad:

```cpp
/*****************************************************************************
 * \name Bitfield for Memory Reduction
 ****************************************************************************/
/**@{*/
uint8_t use_wedge_interintra : 1;
/*! \brief CDEF strength per BLOCK_64X64 */
int8_t cdef_strength;
/**@}*/
```

Good:

```cpp
/*****************************************************************************
 * \name Bitfield for Memory Reduction
 ****************************************************************************/
/**@{*/
uint8_t use_wedge_interintra : 1;
/**@}*/

/*! \brief CDEF strength per BLOCK_64X64 */
int8_t cdef_strength;
```
