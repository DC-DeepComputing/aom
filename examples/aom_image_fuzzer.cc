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

/*
 * See build_aom_image_fuzzer.sh for building instructions.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <limits>

#include "aom/aom_image.h"

namespace {

constexpr unsigned int kMaxDimension = 64;
constexpr unsigned int kMaxOperations = 64;

struct FuzzReader {
  const uint8_t *data;
  size_t size;

  uint8_t ReadU8() {
    if (size == 0) return 0;
    const uint8_t value = *data++;
    --size;
    return value;
  }
};

aom_img_fmt_t PickFormat(uint8_t value) {
  static constexpr aom_img_fmt_t kFormats[] = {
    AOM_IMG_FMT_YV12,   AOM_IMG_FMT_I420,   AOM_IMG_FMT_I422,
    AOM_IMG_FMT_I444,   AOM_IMG_FMT_NV12,   AOM_IMG_FMT_I42016,
    AOM_IMG_FMT_YV1216, AOM_IMG_FMT_I42216, AOM_IMG_FMT_I44416,
  };
  return kFormats[value % (sizeof(kFormats) / sizeof(kFormats[0]))];
}

unsigned int PickDimension(uint8_t value) {
  return value == UINT8_MAX ? 0 : 1u + value % kMaxDimension;
}

unsigned int PickWithin(uint8_t value, unsigned int limit) {
  return value % (limit + 1);
}

struct BufferBounds {
  uintptr_t begin;
  uintptr_t end;
};

void CheckPlanePointers(const aom_image_t *img, const BufferBounds &bounds) {
  for (int plane = AOM_PLANE_Y; plane <= AOM_PLANE_V; ++plane) {
    if (img->planes[plane] == nullptr) continue;
    const uintptr_t address = reinterpret_cast<uintptr_t>(img->planes[plane]);
    if (address < bounds.begin || address > bounds.end) abort();
  }
}

void TouchImage(aom_image_t *img) {
  if (img->d_w == 0 || img->d_h == 0) return;
  const size_t bytes_per_sample = (img->fmt & AOM_IMG_FMT_HIGHBITDEPTH) ? 2 : 1;

  for (int plane = AOM_PLANE_Y; plane <= AOM_PLANE_V; ++plane) {
    if (img->fmt == AOM_IMG_FMT_NV12 && plane == AOM_PLANE_V) continue;
    if (img->planes[plane] == nullptr) continue;

    size_t row_bytes =
        static_cast<size_t>(aom_img_plane_width(img, plane)) * bytes_per_sample;
    if (img->fmt == AOM_IMG_FMT_NV12 && plane == AOM_PLANE_U) {
      row_bytes *= 2;
    }
    const unsigned int height = aom_img_plane_height(img, plane);
    if (row_bytes == 0 || height == 0) continue;

    const unsigned int rows[] = { 0, height - 1 };
    for (const unsigned int row_index : rows) {
      volatile uint8_t *row =
          img->planes[plane] +
          static_cast<ptrdiff_t>(row_index) * img->stride[plane];
      row[0] = 0;
      row[row_bytes - 1] = 0;
    }
  }
}

void SetRandomRect(aom_image_t *img, unsigned int border, FuzzReader *reader) {
  const unsigned int x = PickWithin(reader->ReadU8(), img->w);
  const unsigned int y = PickWithin(reader->ReadU8(), img->h);
  const unsigned int w = PickWithin(reader->ReadU8(), img->w - x);
  const unsigned int h = PickWithin(reader->ReadU8(), img->h - y);
  if (aom_img_set_rect(img, x, y, w, h, border) != 0) abort();
}

void SetEmptyHeight(aom_image_t *img, unsigned int border, FuzzReader *reader) {
  const unsigned int x = PickWithin(reader->ReadU8(), img->w);
  const unsigned int y = PickWithin(reader->ReadU8(), img->h);
  const unsigned int w = PickWithin(reader->ReadU8(), img->w - x);
  if (aom_img_set_rect(img, x, y, w, 0, border) != 0) abort();
}

void SetEmptyWidth(aom_image_t *img, unsigned int border, FuzzReader *reader) {
  const unsigned int x = PickWithin(reader->ReadU8(), img->w);
  const unsigned int y = PickWithin(reader->ReadU8(), img->h);
  const unsigned int h = PickWithin(reader->ReadU8(), img->h - y);
  if (aom_img_set_rect(img, x, y, 0, h, border) != 0) abort();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  FuzzReader reader = { data, size };
  const aom_img_fmt_t format = PickFormat(reader.ReadU8());
  const unsigned int width = PickDimension(reader.ReadU8());
  const unsigned int height = PickDimension(reader.ReadU8());
  const unsigned int align = 1u << (reader.ReadU8() % 7);
  const unsigned int size_align = 1u << (reader.ReadU8() % 3);
  static constexpr unsigned int kBorders[] = { 0, 1, 4, 16 };
  const unsigned int border =
      kBorders[reader.ReadU8() % (sizeof(kBorders) / sizeof(kBorders[0]))];

  aom_image_t *img = aom_img_alloc_with_border(nullptr, format, width, height,
                                               align, size_align, border);
  if (img == nullptr) return 0;

  const BufferBounds bounds = {
    reinterpret_cast<uintptr_t>(img->img_data),
    reinterpret_cast<uintptr_t>(img->img_data) + img->sz,
  };
  CheckPlanePointers(img, bounds);
  TouchImage(img);

  for (unsigned int operation = 0;
       operation < kMaxOperations && reader.size > 0; ++operation) {
    switch (reader.ReadU8() % 8) {
      case 0: aom_img_flip(img); break;
      case 1: SetRandomRect(img, border, &reader); break;
      case 2: SetEmptyHeight(img, border, &reader); break;
      case 3:
        if (aom_img_set_rect(img, 0, 0, img->w, img->h, border) != 0) abort();
        break;
      case 4: SetEmptyWidth(img, border, &reader); break;
      case 5:
        aom_img_flip(img);
        aom_img_flip(img);
        break;
      case 6:
        if (aom_img_set_rect(img, std::numeric_limits<unsigned int>::max(),
                             std::numeric_limits<unsigned int>::max(), 1, 1,
                             border) == 0) {
          abort();
        }
        break;
      default: break;
    }
    CheckPlanePointers(img, bounds);
    TouchImage(img);
  }

  aom_img_free(img);
  return 0;
}
