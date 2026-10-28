---
name: libaom/use-const-char-and-omit-const-on-struct-member-pointers
description: When declaring struct members referencing immutable strings or pointer callbacks, use const char* for strings and omit top-level const on member pointers
rubric: readability
severity: nit
---

**Rule.** When defining struct or class fields that reference immutable strings
(such as string literals, command-line flags, or static names), declare them as
'const char *' rather than mutable 'char *'. Furthermore, omit top-level 'const'
qualifiers from struct member pointers and function pointer fields (e.g. write
'Ret (*fn)(Args)' and 'Type *ptr' instead of '*const fn' or '*const ptr'), as
instance immutability belongs on struct instances and top-level const members
prevent struct assignment.

**Why.** Declaring immutable string members as 'char *' misleads readers about
mutability/ownership and causes const-correctness warnings when assigning string
literals. Adding top-level 'const' to struct member pointers is redundant for
const struct instances and prevents value copying or assignment of mutable
instances.

## Examples

Bad:

```cpp
struct CodecInfo {
  aom_codec_iface_t *(*const interface)(void);
  char *short_name;
  uint32_t fourcc;
};
```

Good:

```cpp
struct CodecInfo {
  aom_codec_iface_t *(*interface)(void);
  const char *short_name;
  uint32_t fourcc;
};
```
