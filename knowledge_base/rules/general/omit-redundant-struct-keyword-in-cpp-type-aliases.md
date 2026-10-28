---
name: libaom/omit-redundant-struct-keyword-in-cpp-type-aliases
description: When defining C++ type aliases with using or referencing types, omit redundant struct or class keywords unless required for disambiguation
rubric: readability
severity: nit
---

**Rule.** In C++ type alias declarations (`using Alias = Type;`) and type
references, do not include redundant elaborated type specifiers such as `struct`
or `class` before the type name unless necessary for name disambiguation. In
C++, struct and class tag names are directly available as type names in scope.

**Why.** In C++, struct and class tags automatically introduce type names into
scope, unlike C where struct tags require explicit tag specifiers or typedefs.
Adding 'struct' or 'class' in C++ type aliases introduces unnecessary C-style
boilerplate.

## Examples

Bad:

```cpp
// Redundant 'struct' keyword in C++ type aliases
using AV1RateControlRtcConfig = struct AomAV1RateControlRtcConfig;
using AV1FrameParams = struct AomAV1FrameParamsRTC;
```

Good:

```cpp
// Omit redundant elaborated type specifiers in C++
using AV1RateControlRtcConfig = AomAV1RateControlRtcConfig;
using AV1FrameParams = AomAV1FrameParamsRTC;
```
