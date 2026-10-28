---
name: libaom/prefer-while-1-over-while-true
description: When writing infinite while loops in C source files, use while (1) rather than while (true) to follow standard C idioms
rubric: readability
severity: nit
---

**Rule.** In C source and header files, use `while (1)` rather than `while
(true)` for infinite loops. Avoid introducing `<stdbool.h>` dependencies solely
for loop conditions.

**Why.** `while (1)` is the established C idiom across libaom for infinite
loops. Using `while (true)` introduces unnecessary `<stdbool.h>` dependencies
and diverges from codebase conventions.

## Examples

Bad:

```cpp
#include <stdbool.h>

void worker_thread_loop(void *arg) {
  while (true) {
    if (check_exit_condition(arg)) break;
    process_task(arg);
  }
}
```

Good:

```cpp
void worker_thread_loop(void *arg) {
  while (1) {
    if (check_exit_condition(arg)) break;
    process_task(arg);
  }
}
```
