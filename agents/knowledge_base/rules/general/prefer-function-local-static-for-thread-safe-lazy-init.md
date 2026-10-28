---
name: libaom/prefer-function-local-static-for-thread-safe-lazy-init
description: When lazily initializing singletons or shared resources in C++, use thread-safe function-local static initialization instead of unsynchronized check-and-assign pointer checks
rubric: safety
severity: should_fix
---

**Rule.** When implementing lazily initialized singletons, interpreters, or
shared resources in C++, use standard dynamic initialization of function-local
static variables (`static T *var = Init();` or immediately invoked lambdas)
instead of manual unsynchronized `if (var == nullptr)` check-and-set logic on
file-scope or function-scope static pointers. Dynamic initialization of
function-scope static variables is guaranteed to be thread-safe in C++11 and
later (magic statics).

**Why.** In C++11 and later, dynamic initialization of function-local static
variables is guaranteed by the language standard to be executed exactly once in
a thread-safe manner without requiring explicit mutex locks. In contrast, manual
lazy initialization using file-scope or function-scope pointers with
unsynchronized check-and-set introduces data races and undefined behavior when
multiple threads access the accessor concurrently.

## Examples

Bad:

```cpp
// Bad: Manual unsynchronized check-and-set on file-scope or function-scope pointers
namespace {
tflite::Interpreter *g_interpreter = nullptr;
}
tflite::Interpreter *GetInterpreter() {
  if (g_interpreter == nullptr) {
    g_interpreter = BuildInterpreter();  // Race condition on concurrent access!
  }
  return g_interpreter;
}

tflite::Interpreter *get_or_init_interpreter() {
  static tflite::Interpreter *interpreter = nullptr;
  if (interpreter == nullptr) {
    interpreter = create_interpreter();  // Race condition on concurrent access!
  }
  return interpreter;
}
```

Good:

```cpp
// Good: Dynamic initialization of function-local static variables is guaranteed thread-safe in C++11+
tflite::Interpreter *GetInterpreter() {
  static tflite::Interpreter *interpreter = BuildInterpreter();
  return interpreter;
}

tflite::Interpreter *get_or_init_interpreter() {
  static tflite::Interpreter *interpreter = create_interpreter();
  return interpreter;
}
```
