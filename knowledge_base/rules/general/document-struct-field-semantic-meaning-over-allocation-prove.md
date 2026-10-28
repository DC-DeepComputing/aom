---
name: libaom/document-struct-field-semantic-meaning-over-allocation-prove
description: When documenting struct members holding buffer or resource indices, describe their primary semantic purpose rather than transient allocation provenance
rubric: readability
severity: should_fix
---

**Rule.** When documenting struct members that hold buffer indices, resource
slots, or handles, describe their primary semantic meaning and role in the state
machine (such as identifying the current frame being encoded or decoded) rather
than where or how they were initially acquired (such as being the next free
buffer in a pool).

**Why.** Documenting a struct member by its transient allocation origin (such as
being the next available slot) obscures its functional purpose throughout
processing and confuses readers about what entity the field represents.

## Examples

Bad:

```cpp
// Index to the next free frame buffer, in the buffer pool 'cm->buffer_pool'.
int new_fb_idx;
```

Good:

```cpp
// Index to the 'new' frame (i.e. the frame currently being encoded) in the
// buffer pool 'cm->buffer_pool'.
int new_fb_idx;
```
