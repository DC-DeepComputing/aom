---
name: libaom/omit-redundant-public-specifier-in-struct
description: 'When defining a struct or declaring its initial members and constructors, omit redundant public: access specifiers since struct members are public by default'
rubric: readability
severity: nit
---

**Rule.** Do not add an explicit `public:` access specifier at the start of a
`struct` definition or inside conditional C++ blocks within a struct. In C++,
struct members and base classes have public accessibility by default, making an
initial `public:` specifier redundant.

**Why.** In C++, all members in a `struct` default to public access. Adding an
explicit `public:` specifier at the top of a struct or around initial members
introduces redundant boilerplate without changing semantics.

## Examples

Bad:

```cpp
struct RateControlConfig {
#if defined(__cplusplus)
 public:
  RateControlConfig();
#endif
  int target_bitrate;
};
```

Good:

```cpp
struct RateControlConfig {
#if defined(__cplusplus)
  RateControlConfig();
#endif
  int target_bitrate;
};
```
