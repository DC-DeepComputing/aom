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

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "common/obudec.h"
#include "common/tools_common.h"
#include "gtest/gtest.h"
#include "test/video_source.h"

namespace {

// Section 5 (low-overhead bitstream) OBU header byte: the forbidden bit and the
// reserved bit are 0, there is no extension, and obu_has_size_field is set so
// the demuxer reads an explicit LEB128 payload size.
uint8_t SizedObuHeaderByte(int obu_type) {
  return static_cast<uint8_t>((obu_type << 3) | 0x02);
}

// Appends the unsigned LEB128 encoding of |value| to |out|.
void AppendLeb128(std::vector<uint8_t> *out, uint64_t value) {
  do {
    uint8_t byte = static_cast<uint8_t>(value & 0x7f);
    value >>= 7;
    if (value != 0) byte |= 0x80;
    out->push_back(byte);
  } while (value != 0);
}

// Appends a complete OBU (header + LEB128 size + |payload_size| zero bytes) and
// returns the number of file bytes it occupies, which is also the amount by
// which obudec_read_temporal_unit() grows obu_ctx->bytes_buffered for it.
size_t AppendSizedObu(std::vector<uint8_t> *out, int obu_type,
                      size_t payload_size) {
  const size_t start = out->size();
  out->push_back(SizedObuHeaderByte(obu_type));
  AppendLeb128(out, payload_size);
  out->insert(out->end(), payload_size, 0);
  return out->size() - start;
}

// Regression test for a heap buffer overflow in obudec_read_one_obu().
//
// When a temporal unit fills obu_ctx->buffer to within OBU_MAX_HEADER_SIZE (18)
// bytes of capacity, the next OBU takes the "grow the buffer" branch. That
// branch used to recompute the free space as
//   available_buffer_capacity += AOMMAX(*obu_buffer_capacity,
//   OBU_MAX_HEADER_SIZE);
// but *obu_buffer_capacity already held the post-grow capacity, so the free
// space was over-counted by (roughly) the pre-grow capacity. The inflated value
// let the payload-fit check below pass without a second, actually-needed grow,
// and obudec_read_obu_payload() then wrote the payload past the allocation.
//
// This builds a single temporal unit that reaches that state and drives an OBU
// whose payload fits the inflated estimate but not the real buffer. Before the
// fix this overflows the heap (caught by AddressSanitizer); after the fix the
// buffer is grown correctly and the whole temporal unit is read back.
TEST(ObudecTest, GrowBufferNearCapacityNoOverflow) {
  // Must match the internal constants in common/obudec.c.
  constexpr size_t kObuBufferSize = 500 * 1024;  // Initial obu_ctx capacity.
  constexpr size_t kObuMaxHeaderSize = 1 + 1 + 2 * 8;

  std::vector<uint8_t> stream;

  // First OBU: a temporal delimiter with an empty payload. file_is_obu()
  // detects this and primes obu_ctx->buffer with these bytes.
  const size_t td_bytes = AppendSizedObu(&stream, OBU_TEMPORAL_DELIMITER, 0);

  // Second OBU: a frame whose total size leaves the buffer exactly
  // |kObuMaxHeaderSize - slack| bytes short of capacity, so the third OBU hits
  // the near-capacity grow branch. We target a post-read bytes_buffered of
  // (kObuBufferSize - 10); the TD already accounts for |td_bytes|.
  const size_t kTargetBuffered = kObuBufferSize - 10;
  const size_t kFrameBObuBytes = kTargetBuffered - td_bytes;
  // Header (1) + 3-byte LEB128 size => 4 bytes of overhead for this length.
  const size_t kFrameBPayload = kFrameBObuBytes - 4;
  const size_t frame_b_bytes =
      AppendSizedObu(&stream, OBU_FRAME, kFrameBPayload);
  ASSERT_EQ(frame_b_bytes, kFrameBObuBytes);
  ASSERT_LT(kObuBufferSize - (td_bytes + frame_b_bytes), kObuMaxHeaderSize);

  // Third OBU: a frame whose payload exceeds the true free space after one
  // grow (new capacity - bytes_buffered) but fits the pre-fix inflated estimate
  // (new capacity itself), so the buggy code skipped the required second grow.
  constexpr size_t kFrameCPayload = 600000;
  AppendSizedObu(&stream, OBU_FRAME, kFrameCPayload);

  const size_t kExpectedTuSize = td_bytes + frame_b_bytes + 4 + kFrameCPayload;

  libaom_test::TempOutFile temp;
  ASSERT_NE(temp.file(), nullptr);
  ASSERT_EQ(fwrite(stream.data(), 1, stream.size(), temp.file()),
            stream.size());
  ASSERT_EQ(fflush(temp.file()), 0);
  rewind(temp.file());

  AvxInputContext avx_ctx;
  memset(&avx_ctx, 0, sizeof(avx_ctx));
  avx_ctx.file = temp.file();
  avx_ctx.filename = temp.file_name().c_str();

  ObuDecInputContext obu_ctx;
  memset(&obu_ctx, 0, sizeof(obu_ctx));
  obu_ctx.avx_ctx = &avx_ctx;
  obu_ctx.is_annexb = 0;

  ASSERT_EQ(file_is_obu(&obu_ctx), 1);

  uint8_t *tu_buffer = nullptr;
  size_t bytes_read = 0;
  size_t buffer_size = 0;
  // Pre-fix: AddressSanitizer reports a heap-buffer-overflow WRITE inside
  // obudec_read_obu_payload() here. Post-fix: the temporal unit is read whole.
  const int result = obudec_read_temporal_unit(&obu_ctx, &tu_buffer,
                                               &bytes_read, &buffer_size);
  EXPECT_EQ(result, 0);
  EXPECT_EQ(bytes_read, kExpectedTuSize);
  EXPECT_EQ(buffer_size, kExpectedTuSize);

  free(tu_buffer);
  obudec_free(&obu_ctx);
}

}  // namespace
