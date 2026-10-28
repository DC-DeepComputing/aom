---
name: libaom/combine-pattern-matching-and-extraction-in-awk
description: When parsing file output in shell scripts, combine pattern matching and field extraction in awk instead of chaining cat, grep, or echo
rubric: readability
severity: nit
---

**Rule.** In shell scripts, avoid redundant `cat | grep` pipelines and
intermediate variable piping when parsing command or file output. Combine
pattern filtering and field extraction directly in a single `awk` command by
supplying the regex filter and input file path (e.g. `awk '/regex/ {print $N}'
"${file}"`).

**Why.** Chaining `cat`, `grep`, and intermediate `echo | awk` pipelines spawns
unnecessary processes and subshells while cluttering scripts. `awk` natively
supports filtering lines by regular expression and reading input files directly.

## Examples

Bad:

```cpp
local line="$(cat ${perfstat_file} | grep "instructions:u")"
local instruction_count="$(echo "${line}" | awk '{print $1}')"
```

Good:

```cpp
local instruction_count="$(awk '/instructions:u/ {print $1}' \
                         "${perfstat_file}")"
```
