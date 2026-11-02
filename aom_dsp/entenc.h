/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AOM_DSP_ENTENC_H_
#define AOM_AOM_DSP_ENTENC_H_
#include <stddef.h>
#include "aom_dsp/entcode.h"
#include "aom_util/endian_inl.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__ELF__) && (defined(__GNUC__) || defined(__clang__))
#pragma GCC visibility push(hidden)
#endif

typedef uint64_t od_ec_enc_window;

typedef struct od_ec_enc od_ec_enc;

#define OD_MEASURE_EC_OVERHEAD (0)

/*The entropy encoder context.*/
struct od_ec_enc {
  /*Buffered output.
    This contains only the raw bits until the final call to od_ec_enc_done(),
     where all the arithmetic-coded data gets prepended to it.*/
  unsigned char *buf;
  /*The size of the buffer.*/
  uint32_t storage;
  /*The offset at which the next entropy-coded byte will be written.*/
  uint32_t offs;
  /*The low end of the current range.*/
  od_ec_enc_window low;
  /*The number of values in the current range.*/
  uint16_t rng;
  /*The number of bits of data in the current value.*/
  int16_t cnt;
  /*Nonzero if an error occurred.*/
  int error;
#if OD_MEASURE_EC_OVERHEAD
  double entropy;
  int nb_symbols;
#endif
};

/*See entenc.c for further documentation.*/

void od_ec_enc_init(od_ec_enc *enc, uint32_t size) OD_ARG_NONNULL(1);
void od_ec_enc_reset(od_ec_enc *enc) OD_ARG_NONNULL(1);
void od_ec_enc_clear(od_ec_enc *enc) OD_ARG_NONNULL(1);

void od_ec_enc_normalize_flush(od_ec_enc *enc, od_ec_enc_window low,
                               unsigned rng, int d, int c, int s)
    OD_ARG_NONNULL(1);

#if OD_MEASURE_EC_OVERHEAD
void od_ec_encode_bool_q15(od_ec_enc *enc, int val, unsigned f_q15)
    OD_ARG_NONNULL(1);
void od_ec_encode_cdf_q15(od_ec_enc *enc, int s, const uint16_t *cdf, int nsyms)
    OD_ARG_NONNULL(1) OD_ARG_NONNULL(3);
#else
static inline void od_ec_enc_normalize(od_ec_enc *enc, od_ec_enc_window low,
                                       unsigned rng) {
  const int c = enc->cnt;
  assert(rng <= 65535U);
  const int d = 15 ^ get_msb(rng);
  const int s = c + d;
  if (UNLIKELY(s >= 40)) {
    od_ec_enc_normalize_flush(enc, low, rng, d, c, s);
    return;
  }
  enc->low = low << d;
  enc->rng = rng << d;
  enc->cnt = s;
}

static inline void od_ec_encode_bool_q15(od_ec_enc *enc, int val,
                                         unsigned f_q15) {
  assert(0 < f_q15);
  assert(f_q15 < 32768U);
  od_ec_enc_window l = enc->low;
  unsigned r = enc->rng;
  assert(32768U <= r);
  unsigned v =
      ((r >> 8) * (uint32_t)(f_q15 >> EC_PROB_SHIFT) >> (7 - EC_PROB_SHIFT)) +
      EC_MIN_PROB;
  if (val) l += r - v;
  r = val ? v : r - v;
  od_ec_enc_normalize(enc, l, r);
}

static inline void od_ec_encode_cdf_q15(od_ec_enc *enc, int s,
                                        const uint16_t *icdf, int nsyms) {
  assert(s >= 0);
  assert(s < nsyms);
  assert(icdf[nsyms - 1] == OD_ICDF(CDF_PROB_TOP));
  od_ec_enc_window l = enc->low;
  unsigned r = enc->rng;
  assert(32768U <= r);
  assert(7 - EC_PROB_SHIFT >= 0);
  const int N = nsyms - 1;
  const unsigned r_shifted = r >> 8;
  const unsigned fh = icdf[s];
  const unsigned min_prob_v = (unsigned)(EC_MIN_PROB * (N - s));
  const unsigned v =
      (r_shifted * (uint32_t)(fh >> EC_PROB_SHIFT) >> (7 - EC_PROB_SHIFT)) +
      min_prob_v;
  if (s > 0) {
    const unsigned fl = icdf[s - 1];
    assert(fh <= fl);
    assert(fl <= 32768U);
    if (LIKELY(fl < CDF_PROB_TOP)) {
      const unsigned u =
          (r_shifted * (uint32_t)(fl >> EC_PROB_SHIFT) >> (7 - EC_PROB_SHIFT)) +
          min_prob_v + EC_MIN_PROB;
      l += r - u;
      r = u;
    }
  }
  r -= v;
  od_ec_enc_normalize(enc, l, r);
}
#endif  // OD_MEASURE_EC_OVERHEAD

void od_ec_enc_bits(od_ec_enc *enc, uint32_t fl, unsigned ftb)
    OD_ARG_NONNULL(1);

OD_WARN_UNUSED_RESULT unsigned char *od_ec_enc_done(od_ec_enc *enc,
                                                    uint32_t *nbytes)
    OD_ARG_NONNULL(1) OD_ARG_NONNULL(2);

OD_WARN_UNUSED_RESULT int od_ec_enc_tell(const od_ec_enc *enc)
    OD_ARG_NONNULL(1);
OD_WARN_UNUSED_RESULT uint32_t od_ec_enc_tell_frac(const od_ec_enc *enc)
    OD_ARG_NONNULL(1);

// buf is the frame bitbuffer, offs is where carry to be added
static inline void propagate_carry_bwd(unsigned char *buf, uint32_t offs) {
  uint16_t sum, carry = 1;
  do {
    sum = (uint16_t)buf[offs] + 1;
    buf[offs--] = (unsigned char)sum;
    carry = sum >> 8;
  } while (carry);
}

// Convert to big-endian byte order and write data to buffer adding the
// carry-bit
static inline void write_enc_data_to_out_buf(unsigned char *out, uint32_t offs,
                                             uint64_t output, uint64_t carry,
                                             uint32_t *enc_offs,
                                             uint8_t num_bytes_ready) {
  const uint64_t reg = HToBE64(output << ((8 - num_bytes_ready) << 3));
  memcpy(&out[offs], &reg, 8);
  // Propagate carry backwards if exists
  if (carry) {
    assert(offs > 0);
    propagate_carry_bwd(out, offs - 1);
  }
  *enc_offs = offs + num_bytes_ready;
}

#if defined(__ELF__) && (defined(__GNUC__) || defined(__clang__))
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOM_DSP_ENTENC_H_
