---
name: libaom/current-year-in-copyright-license-header
description: Include the standard Alliance for Open Media BSD 2-Clause license header with the creation year in new files; never bump existing years or use upstream variants.
rubric: readability
severity: should_fix
---

**Rule.** When creating new source, header, build, script, or assembly files,
include the standard Alliance for Open Media (AOMedia) copyright and BSD
2-Clause / Patent License 1.0 comment header at the top of the file with the
copyright year set to the current creation year (`Copyright (c) <current_year>,
Alliance for Open Media. All rights reserved.`). Do not copy stale copyright
years from older files, introduce upstream project headers (e.g., WebM/libvpx),
use alternative license variants (such as BSD 3-Clause or BSD 3-Clause Clear),
or insert extraneous blank comment lines before the copyright notice. Adapt
comment syntax appropriately for non-C/C++ files (e.g., `#` for CMake, Python,
or shell scripts; `;` for assembly). When modifying existing files, preserve the
original creation year and never bump or update it.

**Why.** README.md mandates that all repository files use the exact Alliance for
Open Media BSD 2-Clause and Patent License 1.0 template with their initial
creation year for accurate legal metadata, IP protection, and license
compliance. Using upstream headers (like WebM), non-standard license variants
(such as BSD 3-Clause), stale copied years, or malformed comment blocks creates
legal ambiguity and inconsistency. Furthermore, bumping copyright years on
existing files produces unnecessary git churn without legal effect.

## Examples

Bad:

```cpp
// 1. Missing header, outdated copied year, or upstream/alternative license on new file:
/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License...
 */
/*
 * Copyright (c) 2026 The WebM project authors. All Rights Reserved.
 */

// 2. Extraneous blank comment line before copyright notice:
/*
 *
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 */

// 3. Bumping copyright year when modifying an existing file (created in 2016):
/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 */
```

Good:

```cpp
// 1 & 2. Standard AOM BSD 2-Clause and Patent license header on new file (created in 2026):
/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// 3. Original creation year preserved when modifying an existing file (created in 2016):
/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 */
```
