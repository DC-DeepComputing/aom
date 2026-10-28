---
name: libaom/match-backup-variables-to-saved-members
description: When creating temporary backup variables to save struct members or state, prefix backup_ directly to the member name without redundant type suffixes like _info
rubric: readability
severity: nit
---

**Rule.** When creating a temporary local variable or struct member to save and
restore state, name it by prepending 'backup_' directly to the base identifier
or member name (such as 'backup_cm_error' for 'cm->error' or
'backup_current_qindex' for 'current_qindex'). Do not append redundant type or
structural suffixes such as '_info', '_ptr', or '_struct' to the backup variable
name.

**Why.** Directly prefixing 'backup_' to the target member or variable name
clearly signals what data is being saved and restored without redundant type
noise, keeping identifier naming concise and consistent across the codebase.

## Examples

Bad:

```cpp
// Redundant type suffix '_info' added to backup variable
struct aom_internal_error_info *backup_cm_error_info = cm->error;
cm->error = &thread_data->error;
// ...
cm->error = backup_cm_error_info;
```

Good:

```cpp
// Match backup variable directly to the struct member being saved
struct aom_internal_error_info *backup_cm_error = cm->error;
cm->error = &thread_data->error;
// ...
cm->error = backup_cm_error;
```
