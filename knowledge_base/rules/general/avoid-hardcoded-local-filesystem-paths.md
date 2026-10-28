---
name: libaom/avoid-hardcoded-local-filesystem-paths
description: When referencing files or directories in tools, applications, or source files, avoid hardcoded local filesystem paths; pass paths via CLI arguments or environment variables.
rubric: readability
severity: should_fix
---

**Rule.** Do not hardcode machine-specific or user-specific local filesystem
paths (such as absolute paths under /Users/, /home/, or Windows user
directories) in sample apps, tools, tests, or library source files. Accept
filesystem and input file paths dynamically via command-line arguments,
configuration parameters, test fixture helpers, or environment variables.

**Why.** Hardcoded local filesystem paths are non-portable across different
developer environments and CI runners, causing tools and tests to fail on other
machines while leaking local environment details into the codebase.

## Examples

Bad:

```cpp
// Bad: Hardcoded machine- or user-specific absolute path
std::string input_path =
    "/Users/username/aomedia/aom/build/cmake/bus_cif.y4m";
FILE *f = fopen("/home/developer/test_vectors/input.yuv", "rb");
```

Good:

```cpp
// Good: Pass paths dynamically via CLI arguments or environment variables
std::string input_path = argv[1];
const char *test_dir = getenv("LIBAOM_TEST_DATA_PATH");
std::string file_path = std::string(test_dir ? test_dir : ".") + "/input.yuv";
FILE *f = fopen(file_path.c_str(), "rb");
```
