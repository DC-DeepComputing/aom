#!/bin/bash
#
# Copyright (c) 2026, Alliance for Open Media. All rights reserved.
#
# This source code is subject to the terms of the BSD 2 Clause License and
# the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
# was not distributed with this source code in the LICENSE file, you can
# obtain it at www.aomedia.org/license/software. If the Alliance for Open
# Media Patent License 1.0 was not distributed with this source code in the
# PATENTS file, you can obtain it at www.aomedia.org/license/patent.
#
###############################################################################
# Fuzzer for the libaom image API.
# ================================
# Requirements
# ---------------------
# Clang6.0 or above (must support -fsanitize=fuzzer -fsanitize=fuzzer-no-link)
#
# References:
# ---------------------
# http://llvm.org/docs/LibFuzzer.html
# https://github.com/google/oss-fuzz
#
# Steps to build / run
# ---------------------

set -eu

# Have a copy of AOM and a build directory ready.
if [[ $# -ne 2 ]]; then
  echo "Pass in the AOM source tree as first argument, and a build directory "
  echo "as the second argument. The AOM source tree can be obtained via: "
  echo "  git clone https://aomedia.googlesource.com/aom"
  exit 2
fi
if [[ -z "${CC:-}" ]]; then
  echo "Set the CC environment variable to point to your C compiler."
  exit 2
fi
if [[ -z "${CXX:-}" ]]; then
  echo "Set the CXX environment variable to point to your C++ compiler."
  exit 2
fi

AOM_DIR=$1
BUILD_DIR=$2
# The image API is codec independent, so neither codec is needed. Build with
# address sanitizer to catch the out-of-bounds plane pointers this target
# looks for.
cd "${BUILD_DIR}"
cmake "${AOM_DIR}" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCONFIG_PIC=1 \
  -DENABLE_APPS=0 -DENABLE_EXAMPLES=0 -DENABLE_DOCS=0 -DENABLE_TESTS=0 \
  -DSANITIZE=fuzzer-no-link,address

# Build the codec.
make -j$(nproc)

# Build the image fuzzer
$CXX -std=c++17 -I${AOM_DIR} -I${BUILD_DIR} \
    -g -fsanitize=fuzzer,address \
    ${AOM_DIR}/examples/aom_image_fuzzer.cc -o ${BUILD_DIR}/aom_image_fuzzer \
    ${BUILD_DIR}/libaom.a

echo "Fuzzer built at ${BUILD_DIR}/aom_image_fuzzer."
echo "The target drives the image API directly and decodes no bitstream, so"
echo "an empty corpus directory is sufficient:"
echo "  mkdir CORPUS && aom_image_fuzzer CORPUS"
