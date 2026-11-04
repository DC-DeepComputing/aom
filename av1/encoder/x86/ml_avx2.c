/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <immintrin.h>

#include "config/av1_rtcd.h"
#include "av1/encoder/block.h"
#include "av1/encoder/ml.h"
#include "av1/encoder/x86/ml_sse3.h"

static inline void compute_gradient_16_avx2(const uint8_t *row_above,
                                            const uint8_t *row_cur,
                                            const uint8_t *row_below,
                                            PixelLevelGradientInfo *row_info,
                                            int c) {
  const __m256i a0 =
      _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)&row_above[c - 1]));
  const __m256i a1 =
      _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)&row_above[c]));
  const __m256i a2 =
      _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)&row_above[c + 1]));
  const __m256i m0 =
      _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)&row_cur[c - 1]));
  const __m256i m2 =
      _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)&row_cur[c + 1]));
  const __m256i b0 =
      _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)&row_below[c - 1]));
  const __m256i b1 =
      _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)&row_below[c]));
  const __m256i b2 =
      _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)&row_below[c + 1]));

  const __m256i v_dx = _mm256_add_epi16(
      _mm256_sub_epi16(_mm256_add_epi16(a2, b2), _mm256_add_epi16(a0, b0)),
      _mm256_slli_epi16(_mm256_sub_epi16(m2, m0), 1));
  const __m256i v_dy = _mm256_add_epi16(
      _mm256_sub_epi16(_mm256_add_epi16(b0, b2), _mm256_add_epi16(a0, a2)),
      _mm256_slli_epi16(_mm256_sub_epi16(b1, a1), 1));
  const __m256i v_adx = _mm256_abs_epi16(v_dx);
  const __m256i v_ady = _mm256_abs_epi16(v_dy);
  const __m256i v_sum = _mm256_add_epi16(v_adx, v_ady);
  const __m256i zero = _mm256_setzero_si256();
  if (_mm256_testz_si256(v_sum, v_sum)) {
    _mm256_storeu_si256((__m256i *)&row_info[c], zero);
    _mm256_storeu_si256((__m256i *)&row_info[c + 8], zero);
    return;
  }

  // Step 0: is_upper (bins 16..31) when (dx ^ dy) >= 0 or (adx * 3194 >> 16) >=
  // ady
  const __m256i v_xor = _mm256_xor_si256(v_dx, v_dy);
  const __m256i p15 = _mm256_mulhi_epu16(v_adx, _mm256_set1_epi16(3194));
  const __m256i le15 = _mm256_cmpeq_epi16(_mm256_subs_epu16(v_ady, p15), zero);
  const __m256i is_upper =
      _mm256_or_si256(_mm256_cmpgt_epi16(v_xor, _mm256_set1_epi16(-1)), le15);

  // Step 1 (+8): test t_idx = 7 (6767, hi=1) vs 23 (59426, hi=0)
  const __m256i mlo1 = _mm256_blendv_epi8(
      _mm256_set1_epi16(6767), _mm256_set1_epi16((short)59426), is_upper);
  const __m256i prod1 = _mm256_add_epi16(_mm256_mulhi_epu16(v_adx, mlo1),
                                         _mm256_andnot_si256(is_upper, v_adx));
  const __m256i cond8 = _mm256_xor_si256(
      _mm256_cmpeq_epi16(_mm256_subs_epu16(v_ady, prod1), zero), is_upper);

  // Build byte-pair index idx = 0x0100 + k * 0x0101 where k in {0, 4, 8, 12}
  __m256i idx = _mm256_add_epi16(
      _mm256_add_epi16(_mm256_set1_epi16(0x0100),
                       _mm256_and_si256(is_upper, _mm256_set1_epi16(0x0808))),
      _mm256_and_si256(cond8, _mm256_set1_epi16(0x0404)));

  // Step 2 (+4): test t_idx in {3, 11, 19, 27}
  const __m256i step2_mlo =
      _mm256_setr_epi16((short)52086, 0, 30982, 0, 23479, 0, 7522, 0,
                        (short)52086, 0, 30982, 0, 23479, 0, 7522, 0);
  const __m256i mlo2 = _mm256_shuffle_epi8(step2_mlo, idx);
  const __m256i hi2 = _mm256_andnot_si256(_mm256_xor_si256(is_upper, cond8),
                                          _mm256_slli_epi16(v_adx, 1));
  const __m256i prod2 = _mm256_add_epi16(_mm256_mulhi_epu16(v_adx, mlo2), hi2);
  const __m256i cond4 = _mm256_xor_si256(
      _mm256_cmpeq_epi16(_mm256_subs_epu16(v_ady, prod2), zero), is_upper);
  idx =
      _mm256_add_epi16(idx, _mm256_and_si256(cond4, _mm256_set1_epi16(0x0202)));

  // Step 3 (+2): test t_idx in {1, 5, 9, 13, 17, 21, 25, 29}
  const __m256i step3_mlo = _mm256_setr_epi16(
      (short)48582, (short)43795, (short)48579, 16398, 9749, (short)39306,
      22857, (short)65031, (short)48582, (short)43795, (short)48579, 16398,
      9749, (short)39306, 22857, (short)65031);
  const __m256i step3_mhi =
      _mm256_setr_epi16(6, 1, 0, 0, 0, 0, 1, 3, 6, 1, 0, 0, 0, 0, 1, 3);
  const __m256i mlo3 = _mm256_shuffle_epi8(step3_mlo, idx);
  const __m256i mhi3 = _mm256_shuffle_epi8(step3_mhi, idx);
  const __m256i prod3 = _mm256_adds_epu16(_mm256_mulhi_epu16(v_adx, mlo3),
                                          _mm256_mullo_epi16(v_adx, mhi3));
  const __m256i cond2 = _mm256_xor_si256(
      _mm256_cmpeq_epi16(_mm256_subs_epu16(v_ady, prod3), zero), is_upper);

  // Set high byte bit 7 (0x80) so vpshufb zeros the high byte at Step 4, and
  // add 0/1 to low byte
  const __m256i idx4 =
      _mm256_sub_epi16(_mm256_or_si256(idx, _mm256_set1_epi16(-0x8000)), cond2);

  // Step 4 (+1): test t_idx in {0, 2, 4, ..., 30}
  const __m256i step4_lo8 = _mm256_setr_epi8(
      (char)255, (char)229, 64, 39, (char)254, 104, (char)149, (char)243,
      (char)156, 50, 40, (char)228, (char)145, 53, (char)152, (char)232,
      (char)255, (char)229, 64, 39, (char)254, 104, (char)149, (char)243,
      (char)156, 50, 40, (char)228, (char)145, 53, (char)152, (char)232);
  const __m256i step4_hi8 =
      _mm256_setr_epi8(90, (char)253, 29, 89, (char)231, (char)153, 91, 37, 12,
                       64, 121, (char)189, 26, (char)171, (char)203, (char)189,
                       90, (char)253, 29, 89, (char)231, (char)153, 91, 37, 12,
                       64, 121, (char)189, 26, (char)171, (char)203, (char)189);
  const __m256i step4_mhi =
      _mm256_setr_epi8(20, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 6, 20, 3,
                       2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 6);
  const __m256i mlo4 = _mm256_or_si256(
      _mm256_shuffle_epi8(step4_lo8, idx4),
      _mm256_slli_epi16(_mm256_shuffle_epi8(step4_hi8, idx4), 8));
  const __m256i mhi4 = _mm256_shuffle_epi8(step4_mhi, idx4);
  const __m256i prod4 = _mm256_adds_epu16(_mm256_mulhi_epu16(v_adx, mlo4),
                                          _mm256_mullo_epi16(v_adx, mhi4));
  const __m256i cond1 = _mm256_xor_si256(
      _mm256_cmpeq_epi16(_mm256_subs_epu16(v_ady, prod4), zero), is_upper);

  __m256i v_bin = _mm256_sub_epi16(
      _mm256_and_si256(_mm256_slli_epi16(idx4, 1), _mm256_set1_epi16(0x001E)),
      cond1);
  v_bin = _mm256_blendv_epi8(v_bin, _mm256_set1_epi16(32),
                             _mm256_cmpeq_epi16(v_dx, zero));

  const __m256i lo = _mm256_unpacklo_epi16(v_sum, v_bin);
  const __m256i hi = _mm256_unpackhi_epi16(v_sum, v_bin);
  _mm256_storeu_si256((__m256i *)&row_info[c],
                      _mm256_permute2x128_si256(lo, hi, 0x20));
  _mm256_storeu_si256((__m256i *)&row_info[c + 8],
                      _mm256_permute2x128_si256(lo, hi, 0x31));
}

void av1_lowbd_compute_gradient_info_sb_avx2(
    const uint8_t *src, int stride, PixelLevelGradientInfo *grad_info_sb,
    int sb_height, int sb_width) {
  src += stride;
  const int last_c = sb_width - 17;
  for (int r = 1; r < sb_height - 1; ++r) {
    const uint8_t *row_above = src - stride;
    const uint8_t *row_cur = src;
    const uint8_t *row_below = src + stride;
    PixelLevelGradientInfo *const row_info = grad_info_sb + r * sb_width;
    int c = 1;
    for (;;) {
      compute_gradient_16_avx2(row_above, row_cur, row_below, row_info, c);
      if (c == last_c) break;
      c += 16;
      if (c > last_c) c = last_c;
    }
    src += stride;
  }
}

static AOM_FORCE_INLINE __m128 nn_reduce_prec_4_avx2(__m128 v) {
  const __m128 v_prec = _mm_set1_ps((float)(1 << 9));
  const __m128 v_inv_prec = _mm_set1_ps((float)(1.0 / (1 << 9)));
  const __m256d vd = _mm256_add_pd(_mm256_cvtps_pd(_mm_mul_ps(v, v_prec)),
                                   _mm256_set1_pd(0.5));
  return _mm_mul_ps(_mm_cvtepi32_ps(_mm256_cvttpd_epi32(vd)), v_inv_prec);
}

static inline void nn_output_prec_reduce_avx2(float *const output,
                                              int num_output) {
  const float prec = (float)(1 << 9);
  const float inv_prec = (float)(1.0 / (1 << 9));
  int i = 0;
  for (; i <= num_output - 4; i += 4) {
    _mm_storeu_ps(&output[i], nn_reduce_prec_4_avx2(_mm_loadu_ps(&output[i])));
  }
  if (num_output & 3) {
    for (; i < num_output; i++) {
      output[i] = ((int)(output[i] * prec + 0.5)) * inv_prec;
    }
  }
}

static AOM_FORCE_INLINE void nn_propagate_4to1_avx2(const float *const inputs,
                                                    const float *const weights,
                                                    __m128 *const output) {
  const __m128 mul = _mm_mul_ps(_mm_loadu_ps(inputs), _mm_loadu_ps(weights));
  const __m128 s1 = _mm_add_ps(mul, _mm_movehdup_ps(mul));
  const __m128 s2 = _mm_add_ss(s1, _mm_movehl_ps(s1, s1));
  *output = _mm_add_ss(*output, s2);
}

static AOM_FORCE_INLINE void nn_propagate_4to4_avx2(const float *const inputs,
                                                    const float *const weights,
                                                    __m128 *const outputs,
                                                    const ptrdiff_t s) {
  const __m128 inputs128 = _mm_loadu_ps(inputs);
  const __m128 mul0 = _mm_mul_ps(_mm_loadu_ps(&weights[0]), inputs128);
  const __m128 mul1 = _mm_mul_ps(_mm_loadu_ps(&weights[s]), inputs128);
  const __m128 hadd0 = _mm_hadd_ps(mul0, mul1);
  const __m128 mul2 = _mm_mul_ps(_mm_loadu_ps(&weights[2 * s]), inputs128);
  const __m128 mul3 = _mm_mul_ps(_mm_loadu_ps(&weights[3 * s]), inputs128);
  const __m128 hadd1 = _mm_hadd_ps(mul2, mul3);
  *outputs = _mm_add_ps(*outputs, _mm_hadd_ps(hadd0, hadd1));
}

static inline void nn_propagate_8to1(const float *const inputs,
                                     const float *const weights,
                                     const float *const bias,
                                     int tot_num_inputs, int num_outputs,
                                     float *const output_nodes,
                                     int is_clip_required) {
  const int num_inputs_to_process = tot_num_inputs & ~7;
  const int rem = tot_num_inputs & 7;
  const ptrdiff_t s = tot_num_inputs;
  const float *w_row = weights;
  int out = 0;
  if (rem == 0 && num_inputs_to_process > 0) {
    const ptrdiff_t s3 = 3 * s;
    const float *const in_end = inputs + num_inputs_to_process;
    for (; out <= num_outputs - 4; out += 4, w_row += 4 * s) {
      __m256 acc0 = _mm256_setzero_ps();
      __m256 acc1 = _mm256_setzero_ps();
      __m256 acc2 = _mm256_setzero_ps();
      __m256 acc3 = _mm256_setzero_ps();
      const float *r0 = w_row;
      const float *in_ptr = inputs;
      do {
        const __m256 inputs256 = _mm256_loadu_ps(in_ptr);
        acc0 = _mm256_add_ps(acc0,
                             _mm256_mul_ps(inputs256, _mm256_loadu_ps(&r0[0])));
        acc1 = _mm256_add_ps(acc1,
                             _mm256_mul_ps(inputs256, _mm256_loadu_ps(&r0[s])));
        acc2 = _mm256_add_ps(
            acc2, _mm256_mul_ps(inputs256, _mm256_loadu_ps(&r0[2 * s])));
        acc3 = _mm256_add_ps(
            acc3, _mm256_mul_ps(inputs256, _mm256_loadu_ps(&r0[s3])));
        in_ptr += 8;
        r0 += 8;
      } while (in_ptr != in_end);
      const __m256 s02 =
          _mm256_add_ps(_mm256_permute2f128_ps(acc0, acc2, 0x20),
                        _mm256_permute2f128_ps(acc0, acc2, 0x31));
      const __m256 s13 =
          _mm256_add_ps(_mm256_permute2f128_ps(acc1, acc3, 0x20),
                        _mm256_permute2f128_ps(acc1, acc3, 0x31));
      const __m256 h = _mm256_hadd_ps(s02, s13);
      const __m128 sum_tot =
          _mm_hadd_ps(_mm256_castps256_ps128(h), _mm256_extractf128_ps(h, 1));
      __m128 v_out = _mm_add_ps(_mm_loadu_ps(&bias[out]), sum_tot);
      if (is_clip_required) v_out = _mm_max_ps(v_out, _mm_setzero_ps());
      _mm_storeu_ps(&output_nodes[out], v_out);
    }
  }
  // Process remaining output rows one at a time.
  for (; out < num_outputs; out++, w_row += s) {
    float bias_val = bias[out];
    if (num_inputs_to_process > 0) {
      __m256 in_result = _mm256_setzero_ps();
      for (int in = 0; in < num_inputs_to_process; in += 8) {
        const __m256 inputs256 = _mm256_loadu_ps(&inputs[in]);
        const __m256 weight0 = _mm256_loadu_ps(&w_row[in]);
        const __m256 mul0 = _mm256_mul_ps(inputs256, weight0);
        in_result = _mm256_add_ps(in_result, mul0);
      }
      const __m128 low_128 = _mm256_castps256_ps128(in_result);
      const __m128 high_128 = _mm256_extractf128_ps(in_result, 1);
      const __m128 sum_par_0 = _mm_add_ps(low_128, high_128);
      const __m128 sum_par_1 =
          _mm_add_ps(sum_par_0, _mm_movehdup_ps(sum_par_0));
      const __m128 sum_tot =
          _mm_add_ss(sum_par_1, _mm_movehl_ps(sum_par_1, sum_par_1));

      bias_val += _mm_cvtss_f32(sum_tot);
    }
    if (rem == 4) {
      __m128 out_ss = _mm_set_ss(bias_val);
      nn_propagate_4to1_avx2(&inputs[num_inputs_to_process],
                             &w_row[num_inputs_to_process], &out_ss);
      bias_val = _mm_cvtss_f32(out_ss);
    } else if (rem != 0) {
      const float *const in_rem = &inputs[num_inputs_to_process];
      const float *const w_rem = &w_row[num_inputs_to_process];
      for (int i = 0; i < rem; ++i) {
        bias_val += in_rem[i] * w_rem[i];
      }
    }
    if (is_clip_required) bias_val = AOMMAX(bias_val, 0);
    output_nodes[out] = bias_val;
  }
}

static inline void nn_propagate_8to4(const float *const inputs,
                                     const float *const weights,
                                     const float *const bias,
                                     int tot_num_inputs, int num_outputs,
                                     float *const output_nodes,
                                     int is_clip_required) {
  const int num_inputs_to_process = tot_num_inputs & ~7;
  const int rem = tot_num_inputs & 7;
  const ptrdiff_t s = tot_num_inputs;
  const ptrdiff_t s3 = 3 * s;
  const float *w0 = weights;
  for (int out = 0; out < num_outputs; out += 4, w0 += 4 * s) {
    __m128 in_result = _mm_loadu_ps(&bias[out]);
    const float *r0 = w0;
    const float *in_ptr = inputs;
    if (num_inputs_to_process > 0) {
      __m128 acc = _mm_setzero_ps();
      const float *const in_end = inputs + num_inputs_to_process;
      do {
        const __m256 inputs256 = _mm256_loadu_ps(in_ptr);
        const __m256 hadd0 =
            _mm256_hadd_ps(_mm256_mul_ps(inputs256, _mm256_loadu_ps(&r0[0])),
                           _mm256_mul_ps(inputs256, _mm256_loadu_ps(&r0[s])));
        const __m256 hadd1 = _mm256_hadd_ps(
            _mm256_mul_ps(inputs256, _mm256_loadu_ps(&r0[2 * s])),
            _mm256_mul_ps(inputs256, _mm256_loadu_ps(&r0[s3])));

        const __m256 sum_par = _mm256_hadd_ps(hadd0, hadd1);
        const __m128 low_128 = _mm256_castps256_ps128(sum_par);
        const __m128 high_128 = _mm256_extractf128_ps(sum_par, 1);
        const __m128 result = _mm_add_ps(low_128, high_128);

        acc = _mm_add_ps(acc, result);
        in_ptr += 8;
        r0 += 8;
      } while (in_ptr != in_end);
      in_result = _mm_add_ps(acc, in_result);
    }
    if (rem == 4) {
      nn_propagate_4to4_avx2(in_ptr, r0, &in_result, s);
    } else if (rem != 0) {
      if (rem & 4) {
        const __m128 row0 = _mm_loadu_ps(&r0[0]);
        const __m128 row1 = _mm_loadu_ps(&r0[s]);
        const __m128 row2 = _mm_loadu_ps(&r0[2 * s]);
        const __m128 row3 = _mm_loadu_ps(&r0[s3]);
        const __m128 t0 = _mm_unpacklo_ps(row0, row1);
        const __m128 t1 = _mm_unpackhi_ps(row0, row1);
        const __m128 t2 = _mm_unpacklo_ps(row2, row3);
        const __m128 t3 = _mm_unpackhi_ps(row2, row3);
        const __m128 c0 = _mm_movelh_ps(t0, t2);
        const __m128 c1 = _mm_movehl_ps(t2, t0);
        const __m128 c2 = _mm_movelh_ps(t1, t3);
        const __m128 c3 = _mm_movehl_ps(t3, t1);
        in_result =
            _mm_add_ps(in_result, _mm_mul_ps(_mm_set1_ps(in_ptr[0]), c0));
        in_result =
            _mm_add_ps(in_result, _mm_mul_ps(_mm_set1_ps(in_ptr[1]), c1));
        in_result =
            _mm_add_ps(in_result, _mm_mul_ps(_mm_set1_ps(in_ptr[2]), c2));
        in_result =
            _mm_add_ps(in_result, _mm_mul_ps(_mm_set1_ps(in_ptr[3]), c3));
        in_ptr += 4;
        r0 += 4;
      }
      if (rem & 2) {
        const __m128 z = _mm_setzero_ps();
        const __m128 p01 = _mm_loadh_pi(_mm_loadl_pi(z, (const __m64 *)&r0[0]),
                                        (const __m64 *)&r0[s]);
        const __m128 p23 = _mm_loadh_pi(
            _mm_loadl_pi(z, (const __m64 *)&r0[2 * s]), (const __m64 *)&r0[s3]);
        const __m128 c0 = _mm_shuffle_ps(p01, p23, 0x88);
        const __m128 c1 = _mm_shuffle_ps(p01, p23, 0xdd);
        in_result =
            _mm_add_ps(in_result, _mm_mul_ps(_mm_set1_ps(in_ptr[0]), c0));
        in_result =
            _mm_add_ps(in_result, _mm_mul_ps(_mm_set1_ps(in_ptr[1]), c1));
        in_ptr += 2;
        r0 += 2;
      }
      if (rem & 1) {
        const __m128 w_col = _mm_setr_ps(r0[0], r0[s], r0[2 * s], r0[s3]);
        in_result =
            _mm_add_ps(in_result, _mm_mul_ps(_mm_set1_ps(in_ptr[0]), w_col));
      }
    }

    if (is_clip_required) in_result = _mm_max_ps(in_result, _mm_setzero_ps());
    _mm_storeu_ps(&output_nodes[out], in_result);
  }
}

static inline void nn_propagate_8to8(const float *const inputs,
                                     const float *const weights,
                                     const float *const bias,
                                     int tot_num_inputs, int num_outputs,
                                     float *const output_nodes,
                                     int is_clip_required) {
  const int num_inputs_to_process = tot_num_inputs & ~7;
  const int rem = tot_num_inputs & 7;
  const ptrdiff_t s = tot_num_inputs;
  const ptrdiff_t s3 = 3 * s;
  const float *w0 = weights;
  const float *w4 = weights + 4 * s;
  for (int out = 0; out < num_outputs; out += 8, w0 += 8 * s, w4 += 8 * s) {
    __m256 in_result = _mm256_loadu_ps(&bias[out]);
    const float *r0 = w0;
    const float *r4 = w4;
    const float *in_ptr = inputs;
    if (num_inputs_to_process > 0) {
      __m256 acc = _mm256_setzero_ps();
      const float *const in_end = inputs + num_inputs_to_process;
      do {
        const __m256 inputs256 = _mm256_loadu_ps(in_ptr);
        const __m256 hadd0 =
            _mm256_hadd_ps(_mm256_mul_ps(inputs256, _mm256_loadu_ps(&r0[0])),
                           _mm256_mul_ps(inputs256, _mm256_loadu_ps(&r0[s])));
        const __m256 hadd1 = _mm256_hadd_ps(
            _mm256_mul_ps(inputs256, _mm256_loadu_ps(&r0[2 * s])),
            _mm256_mul_ps(inputs256, _mm256_loadu_ps(&r0[s3])));
        const __m256 hadd2 =
            _mm256_hadd_ps(_mm256_mul_ps(inputs256, _mm256_loadu_ps(&r4[0])),
                           _mm256_mul_ps(inputs256, _mm256_loadu_ps(&r4[s])));
        const __m256 hadd3 = _mm256_hadd_ps(
            _mm256_mul_ps(inputs256, _mm256_loadu_ps(&r4[2 * s])),
            _mm256_mul_ps(inputs256, _mm256_loadu_ps(&r4[s3])));
        const __m256 hh0 = _mm256_hadd_ps(hadd0, hadd1);
        const __m256 hh1 = _mm256_hadd_ps(hadd2, hadd3);

        __m256 ht_0 = _mm256_permute2f128_ps(hh0, hh1, 0x20);
        __m256 ht_1 = _mm256_permute2f128_ps(hh0, hh1, 0x31);

        __m256 result = _mm256_add_ps(ht_0, ht_1);
        acc = _mm256_add_ps(acc, result);
        in_ptr += 8;
        r0 += 8;
        r4 += 8;
      } while (in_ptr != in_end);
      in_result = _mm256_add_ps(acc, in_result);
    }
    if (rem == 4) {
      const __m256 in4 = _mm256_broadcast_ps((const __m128 *)in_ptr);
      const __m256 w04 =
          _mm256_set_m128(_mm_loadu_ps(&r4[0]), _mm_loadu_ps(&r0[0]));
      const __m256 w15 =
          _mm256_set_m128(_mm_loadu_ps(&r4[s]), _mm_loadu_ps(&r0[s]));
      const __m256 w26 =
          _mm256_set_m128(_mm_loadu_ps(&r4[2 * s]), _mm_loadu_ps(&r0[2 * s]));
      const __m256 w37 =
          _mm256_set_m128(_mm_loadu_ps(&r4[s3]), _mm_loadu_ps(&r0[s3]));
      const __m256 h0 =
          _mm256_hadd_ps(_mm256_mul_ps(in4, w04), _mm256_mul_ps(in4, w15));
      const __m256 h1 =
          _mm256_hadd_ps(_mm256_mul_ps(in4, w26), _mm256_mul_ps(in4, w37));
      in_result = _mm256_add_ps(in_result, _mm256_hadd_ps(h0, h1));
    } else if (rem != 0) {
      if (rem & 4) {
        const __m256 row0 =
            _mm256_set_m128(_mm_loadu_ps(&r4[0]), _mm_loadu_ps(&r0[0]));
        const __m256 row1 =
            _mm256_set_m128(_mm_loadu_ps(&r4[s]), _mm_loadu_ps(&r0[s]));
        const __m256 row2 =
            _mm256_set_m128(_mm_loadu_ps(&r4[2 * s]), _mm_loadu_ps(&r0[2 * s]));
        const __m256 row3 =
            _mm256_set_m128(_mm_loadu_ps(&r4[s3]), _mm_loadu_ps(&r0[s3]));
        const __m256 t0 = _mm256_unpacklo_ps(row0, row1);
        const __m256 t1 = _mm256_unpackhi_ps(row0, row1);
        const __m256 t2 = _mm256_unpacklo_ps(row2, row3);
        const __m256 t3 = _mm256_unpackhi_ps(row2, row3);
        const __m256 c0 = _mm256_shuffle_ps(t0, t2, _MM_SHUFFLE(1, 0, 1, 0));
        const __m256 c1 = _mm256_shuffle_ps(t0, t2, _MM_SHUFFLE(3, 2, 3, 2));
        const __m256 c2 = _mm256_shuffle_ps(t1, t3, _MM_SHUFFLE(1, 0, 1, 0));
        const __m256 c3 = _mm256_shuffle_ps(t1, t3, _MM_SHUFFLE(3, 2, 3, 2));
        in_result = _mm256_add_ps(in_result,
                                  _mm256_mul_ps(_mm256_set1_ps(in_ptr[0]), c0));
        in_result = _mm256_add_ps(in_result,
                                  _mm256_mul_ps(_mm256_set1_ps(in_ptr[1]), c1));
        in_result = _mm256_add_ps(in_result,
                                  _mm256_mul_ps(_mm256_set1_ps(in_ptr[2]), c2));
        in_result = _mm256_add_ps(in_result,
                                  _mm256_mul_ps(_mm256_set1_ps(in_ptr[3]), c3));
        in_ptr += 4;
        r0 += 4;
        r4 += 4;
      }
      if (rem & 2) {
        const __m128 z = _mm_setzero_ps();
        const __m256 p01 =
            _mm256_set_m128(_mm_loadh_pi(_mm_loadl_pi(z, (const __m64 *)&r4[0]),
                                         (const __m64 *)&r4[s]),
                            _mm_loadh_pi(_mm_loadl_pi(z, (const __m64 *)&r0[0]),
                                         (const __m64 *)&r0[s]));
        const __m256 p23 = _mm256_set_m128(
            _mm_loadh_pi(_mm_loadl_pi(z, (const __m64 *)&r4[2 * s]),
                         (const __m64 *)&r4[s3]),
            _mm_loadh_pi(_mm_loadl_pi(z, (const __m64 *)&r0[2 * s]),
                         (const __m64 *)&r0[s3]));
        const __m256 c0 = _mm256_shuffle_ps(p01, p23, 0x88);
        const __m256 c1 = _mm256_shuffle_ps(p01, p23, 0xdd);
        in_result = _mm256_add_ps(in_result,
                                  _mm256_mul_ps(_mm256_set1_ps(in_ptr[0]), c0));
        in_result = _mm256_add_ps(in_result,
                                  _mm256_mul_ps(_mm256_set1_ps(in_ptr[1]), c1));
        in_ptr += 2;
        r0 += 2;
        r4 += 2;
      }
      if (rem & 1) {
        const __m256 w_col = _mm256_setr_ps(r0[0], r0[s], r0[2 * s], r0[s3],
                                            r4[0], r4[s], r4[2 * s], r4[s3]);
        in_result = _mm256_add_ps(
            in_result, _mm256_mul_ps(_mm256_set1_ps(in_ptr[0]), w_col));
      }
    }
    if (is_clip_required)
      in_result = _mm256_max_ps(in_result, _mm256_setzero_ps());
    _mm256_storeu_ps(&output_nodes[out], in_result);
  }
}

static inline void nn_propagate_layer_avx2(
    const float *const inputs, const float *const weights,
    const float *const bias, int tot_num_inputs, bool is_output_layer,
    int num_outputs, float *const output_nodes) {
  const int is_clip_required = !is_output_layer;
  if (num_outputs % 8 == 0) {
    nn_propagate_8to8(inputs, weights, bias, tot_num_inputs, num_outputs,
                      output_nodes, is_clip_required);
  } else if (num_outputs % 4 == 0) {
    nn_propagate_8to4(inputs, weights, bias, tot_num_inputs, num_outputs,
                      output_nodes, is_clip_required);
  } else {
    nn_propagate_8to1(inputs, weights, bias, tot_num_inputs, num_outputs,
                      output_nodes, is_clip_required);
  }
}

static AOM_FORCE_INLINE __m256 nn_layer_8to8_relu_avx2(__m256 inputs256,
                                                       const float *weights,
                                                       const float *bias) {
  __m256 hadd[4];
  for (int i = 0; i < 4; i++) {
    const __m256 weight0 = _mm256_loadu_ps(&weights[16 * i]);
    const __m256 weight1 = _mm256_loadu_ps(&weights[16 * i + 8]);
    hadd[i] = _mm256_hadd_ps(_mm256_mul_ps(inputs256, weight0),
                             _mm256_mul_ps(inputs256, weight1));
  }
  const __m256 hh0 = _mm256_hadd_ps(hadd[0], hadd[1]);
  const __m256 hh1 = _mm256_hadd_ps(hadd[2], hadd[3]);
  const __m256 res = _mm256_add_ps(_mm256_permute2f128_ps(hh0, hh1, 0x20),
                                   _mm256_permute2f128_ps(hh0, hh1, 0x31));
  return _mm256_max_ps(_mm256_add_ps(res, _mm256_loadu_ps(bias)),
                       _mm256_setzero_ps());
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void nn_predict_multi_layer_avx2(const float *input_nodes,
                                        const NN_CONFIG *const nn_config,
                                        int reduce_prec, float *const output) {
  int num_inputs = nn_config->num_inputs;
  float buf[2][NN_MAX_NODES_PER_LAYER];
  int buf_index = 0;

  for (int layer = 0; layer <= nn_config->num_hidden_layers; layer++) {
    const float *layer_weights = nn_config->weights[layer];
    const float *layer_bias = nn_config->bias[layer];
    bool is_output_layer = layer == nn_config->num_hidden_layers;
    float *const output_nodes = is_output_layer ? output : &buf[buf_index][0];
    const int num_outputs = is_output_layer
                                ? nn_config->num_outputs
                                : nn_config->num_hidden_nodes[layer];
    assert(num_outputs > 0 && num_outputs <= NN_MAX_NODES_PER_LAYER);

    nn_propagate_layer_avx2(input_nodes, layer_weights, layer_bias, num_inputs,
                            is_output_layer, num_outputs, output_nodes);
    input_nodes = output_nodes;
    num_inputs = num_outputs;
    buf_index = 1 - buf_index;
  }
  if (reduce_prec) nn_output_prec_reduce_avx2(output, nn_config->num_outputs);
}

void av1_nn_predict_avx2(const float *input_nodes,
                         const NN_CONFIG *const nn_config, int reduce_prec,
                         float *const output) {
  const int num_inputs = nn_config->num_inputs;
  assert(num_inputs > 0 && num_inputs <= NN_MAX_NODES_PER_LAYER);
  const int num_hidden_layers = nn_config->num_hidden_layers;
  if (num_hidden_layers == 0) {
    const int num_outputs = nn_config->num_outputs;
    if (num_outputs == 4 && (num_inputs == 4 || num_inputs == 8)) {
      __m128 out_v;
      if (num_inputs == 4) {
        out_v = _mm_loadu_ps(nn_config->bias[0]);
        nn_propagate_4to4_avx2(input_nodes, nn_config->weights[0], &out_v, 4);
      } else {
        const float *const w0 = nn_config->weights[0];
        const __m256 in0 = _mm256_loadu_ps(input_nodes);
        const __m256 h0 =
            _mm256_hadd_ps(_mm256_mul_ps(in0, _mm256_loadu_ps(&w0[0])),
                           _mm256_mul_ps(in0, _mm256_loadu_ps(&w0[8])));
        const __m256 h1 =
            _mm256_hadd_ps(_mm256_mul_ps(in0, _mm256_loadu_ps(&w0[16])),
                           _mm256_mul_ps(in0, _mm256_loadu_ps(&w0[24])));
        const __m256 sum_par = _mm256_hadd_ps(h0, h1);
        out_v = _mm_add_ps(_mm_add_ps(_mm256_castps256_ps128(sum_par),
                                      _mm256_extractf128_ps(sum_par, 1)),
                           _mm_loadu_ps(nn_config->bias[0]));
      }
      if (reduce_prec) out_v = nn_reduce_prec_4_avx2(out_v);
      _mm_storeu_ps(output, out_v);
      return;
    } else if (num_outputs == 8 && num_inputs == 32) {
      const float *r0 = nn_config->weights[0];
      const float *in_ptr = input_nodes;
      __m256 acc = _mm256_setzero_ps();
      int k = 4;
      do {
        const __m256 in = _mm256_loadu_ps(in_ptr);
        const __m256 h0 =
            _mm256_hadd_ps(_mm256_mul_ps(in, _mm256_loadu_ps(&r0[0])),
                           _mm256_mul_ps(in, _mm256_loadu_ps(&r0[32])));
        const __m256 h1 =
            _mm256_hadd_ps(_mm256_mul_ps(in, _mm256_loadu_ps(&r0[64])),
                           _mm256_mul_ps(in, _mm256_loadu_ps(&r0[96])));
        const __m256 h2 =
            _mm256_hadd_ps(_mm256_mul_ps(in, _mm256_loadu_ps(&r0[128])),
                           _mm256_mul_ps(in, _mm256_loadu_ps(&r0[160])));
        const __m256 h3 =
            _mm256_hadd_ps(_mm256_mul_ps(in, _mm256_loadu_ps(&r0[192])),
                           _mm256_mul_ps(in, _mm256_loadu_ps(&r0[224])));
        const __m256 hh0 = _mm256_hadd_ps(h0, h1);
        const __m256 hh1 = _mm256_hadd_ps(h2, h3);
        acc = _mm256_add_ps(
            acc, _mm256_add_ps(_mm256_permute2f128_ps(hh0, hh1, 0x20),
                               _mm256_permute2f128_ps(hh0, hh1, 0x31)));
        in_ptr += 8;
        r0 += 8;
      } while (--k);
      const __m256 out_v =
          _mm256_add_ps(acc, _mm256_loadu_ps(nn_config->bias[0]));
      __m128 out_lo = _mm256_castps256_ps128(out_v);
      __m128 out_hi = _mm256_extractf128_ps(out_v, 1);
      if (reduce_prec) {
        out_lo = nn_reduce_prec_4_avx2(out_lo);
        out_hi = nn_reduce_prec_4_avx2(out_hi);
      }
      _mm_storeu_ps(&output[0], out_lo);
      _mm_storeu_ps(&output[4], out_hi);
      return;
    }
  }
  if (num_hidden_layers == 1 && nn_config->num_outputs == 4) {
    const int num_hidden = nn_config->num_hidden_nodes[0];
    if ((num_inputs == 4 && num_hidden == 8) ||
        (num_inputs == 8 && num_hidden == 16)) {
      __m128 out_v;
      if (num_inputs == 4) {
        const float *const w0 = nn_config->weights[0];
        const __m256 in0 = _mm256_broadcast_ps((const __m128 *)input_nodes);
        const __m256 w0_04 =
            _mm256_set_m128(_mm_loadu_ps(&w0[16]), _mm_loadu_ps(&w0[0]));
        const __m256 w0_15 =
            _mm256_set_m128(_mm_loadu_ps(&w0[20]), _mm_loadu_ps(&w0[4]));
        const __m256 w0_26 =
            _mm256_set_m128(_mm_loadu_ps(&w0[24]), _mm_loadu_ps(&w0[8]));
        const __m256 w0_37 =
            _mm256_set_m128(_mm_loadu_ps(&w0[28]), _mm_loadu_ps(&w0[12]));
        const __m256 h0 = _mm256_hadd_ps(_mm256_mul_ps(in0, w0_04),
                                         _mm256_mul_ps(in0, w0_15));
        const __m256 h1 = _mm256_hadd_ps(_mm256_mul_ps(in0, w0_26),
                                         _mm256_mul_ps(in0, w0_37));
        __m256 hidden = _mm256_add_ps(_mm256_loadu_ps(nn_config->bias[0]),
                                      _mm256_hadd_ps(h0, h1));
        hidden = _mm256_max_ps(hidden, _mm256_setzero_ps());

        const float *const w1 = nn_config->weights[1];
        const __m256 m0 =
            _mm256_hadd_ps(_mm256_mul_ps(hidden, _mm256_loadu_ps(&w1[0])),
                           _mm256_mul_ps(hidden, _mm256_loadu_ps(&w1[8])));
        const __m256 m1 =
            _mm256_hadd_ps(_mm256_mul_ps(hidden, _mm256_loadu_ps(&w1[16])),
                           _mm256_mul_ps(hidden, _mm256_loadu_ps(&w1[24])));
        const __m256 sum_par = _mm256_hadd_ps(m0, m1);
        out_v = _mm_add_ps(_mm_add_ps(_mm256_castps256_ps128(sum_par),
                                      _mm256_extractf128_ps(sum_par, 1)),
                           _mm_loadu_ps(nn_config->bias[1]));
      } else {
        const float *const w0 = nn_config->weights[0];
        const float *const b0 = nn_config->bias[0];
        const __m256 in0 = _mm256_loadu_ps(input_nodes);
        const __m256 hidden0 = nn_layer_8to8_relu_avx2(in0, &w0[0], &b0[0]);
        const __m256 hidden1 = nn_layer_8to8_relu_avx2(in0, &w0[64], &b0[8]);

        const float *const w1 = nn_config->weights[1];
        const __m256 m0_0 =
            _mm256_hadd_ps(_mm256_mul_ps(hidden0, _mm256_loadu_ps(&w1[0])),
                           _mm256_mul_ps(hidden0, _mm256_loadu_ps(&w1[16])));
        const __m256 m0_1 =
            _mm256_hadd_ps(_mm256_mul_ps(hidden0, _mm256_loadu_ps(&w1[32])),
                           _mm256_mul_ps(hidden0, _mm256_loadu_ps(&w1[48])));
        const __m256 sum_par0 = _mm256_hadd_ps(m0_0, m0_1);
        const __m128 res0 = _mm_add_ps(_mm256_castps256_ps128(sum_par0),
                                       _mm256_extractf128_ps(sum_par0, 1));

        const __m256 m1_0 =
            _mm256_hadd_ps(_mm256_mul_ps(hidden1, _mm256_loadu_ps(&w1[8])),
                           _mm256_mul_ps(hidden1, _mm256_loadu_ps(&w1[24])));
        const __m256 m1_1 =
            _mm256_hadd_ps(_mm256_mul_ps(hidden1, _mm256_loadu_ps(&w1[40])),
                           _mm256_mul_ps(hidden1, _mm256_loadu_ps(&w1[56])));
        const __m256 sum_par1 = _mm256_hadd_ps(m1_0, m1_1);
        const __m128 res1 = _mm_add_ps(_mm256_castps256_ps128(sum_par1),
                                       _mm256_extractf128_ps(sum_par1, 1));

        out_v = _mm_add_ps(_mm_add_ps(res0, res1),
                           _mm_loadu_ps(nn_config->bias[1]));
      }
      if (reduce_prec) out_v = nn_reduce_prec_4_avx2(out_v);
      _mm_storeu_ps(output, out_v);
      return;
    }
  }
  if (num_hidden_layers == 1 && nn_config->num_outputs == 1) {
    const int num_hidden = nn_config->num_hidden_nodes[0];
    if (((num_inputs == 8 || num_inputs == 4 || num_inputs == 12) &&
         (num_hidden & 7) == 0) ||
        (num_inputs == 14 && num_hidden == 16)) {
      const float *r0 = nn_config->weights[0];
      const float *b0 = nn_config->bias[0];
      const float *w1 = nn_config->weights[1];
      __m256 in_result = _mm256_setzero_ps();
      if (num_inputs == 8) {
        const __m256 in0 = _mm256_loadu_ps(input_nodes);
        for (int h = 0; h < num_hidden; h += 8) {
          const __m256 hidden =
              nn_layer_8to8_relu_avx2(in0, &r0[h * 8], &b0[h]);
          in_result = _mm256_add_ps(
              in_result, _mm256_mul_ps(hidden, _mm256_loadu_ps(&w1[h])));
        }
      } else if (num_inputs == 4) {
        const __m256 in4 = _mm256_broadcast_ps((const __m128 *)input_nodes);
        for (int h = 0; h < num_hidden; h += 8, r0 += 32) {
          const __m256 w04 =
              _mm256_set_m128(_mm_loadu_ps(&r0[16]), _mm_loadu_ps(&r0[0]));
          const __m256 w15 =
              _mm256_set_m128(_mm_loadu_ps(&r0[20]), _mm_loadu_ps(&r0[4]));
          const __m256 w26 =
              _mm256_set_m128(_mm_loadu_ps(&r0[24]), _mm_loadu_ps(&r0[8]));
          const __m256 w37 =
              _mm256_set_m128(_mm_loadu_ps(&r0[28]), _mm_loadu_ps(&r0[12]));
          const __m256 h0 =
              _mm256_hadd_ps(_mm256_mul_ps(in4, w04), _mm256_mul_ps(in4, w15));
          const __m256 h1 =
              _mm256_hadd_ps(_mm256_mul_ps(in4, w26), _mm256_mul_ps(in4, w37));
          const __m256 hidden = _mm256_max_ps(
              _mm256_add_ps(_mm256_loadu_ps(&b0[h]), _mm256_hadd_ps(h0, h1)),
              _mm256_setzero_ps());
          in_result = _mm256_add_ps(
              in_result, _mm256_mul_ps(hidden, _mm256_loadu_ps(&w1[h])));
        }
      } else if (num_inputs == 12) {
        const __m256 in0 = _mm256_loadu_ps(input_nodes);
        const __m256 in4 =
            _mm256_broadcast_ps((const __m128 *)(input_nodes + 8));
        for (int h = 0; h < num_hidden; h += 8, r0 += 96) {
          const __m256 hadd0 =
              _mm256_hadd_ps(_mm256_mul_ps(in0, _mm256_loadu_ps(&r0[0])),
                             _mm256_mul_ps(in0, _mm256_loadu_ps(&r0[12])));
          const __m256 hadd1 =
              _mm256_hadd_ps(_mm256_mul_ps(in0, _mm256_loadu_ps(&r0[24])),
                             _mm256_mul_ps(in0, _mm256_loadu_ps(&r0[36])));
          const __m256 hadd2 =
              _mm256_hadd_ps(_mm256_mul_ps(in0, _mm256_loadu_ps(&r0[48])),
                             _mm256_mul_ps(in0, _mm256_loadu_ps(&r0[60])));
          const __m256 hadd3 =
              _mm256_hadd_ps(_mm256_mul_ps(in0, _mm256_loadu_ps(&r0[72])),
                             _mm256_mul_ps(in0, _mm256_loadu_ps(&r0[84])));
          const __m256 hh0 = _mm256_hadd_ps(hadd0, hadd1);
          const __m256 hh1 = _mm256_hadd_ps(hadd2, hadd3);
          const __m256 res0 =
              _mm256_add_ps(_mm256_permute2f128_ps(hh0, hh1, 0x20),
                            _mm256_permute2f128_ps(hh0, hh1, 0x31));
          const __m256 w04 =
              _mm256_set_m128(_mm_loadu_ps(&r0[56]), _mm_loadu_ps(&r0[8]));
          const __m256 w15 =
              _mm256_set_m128(_mm_loadu_ps(&r0[68]), _mm_loadu_ps(&r0[20]));
          const __m256 w26 =
              _mm256_set_m128(_mm_loadu_ps(&r0[80]), _mm_loadu_ps(&r0[32]));
          const __m256 w37 =
              _mm256_set_m128(_mm_loadu_ps(&r0[92]), _mm_loadu_ps(&r0[44]));
          const __m256 h0 =
              _mm256_hadd_ps(_mm256_mul_ps(in4, w04), _mm256_mul_ps(in4, w15));
          const __m256 h1 =
              _mm256_hadd_ps(_mm256_mul_ps(in4, w26), _mm256_mul_ps(in4, w37));
          const __m256 hidden = _mm256_max_ps(
              _mm256_add_ps(_mm256_add_ps(res0, _mm256_loadu_ps(&b0[h])),
                            _mm256_hadd_ps(h0, h1)),
              _mm256_setzero_ps());
          in_result = _mm256_add_ps(
              in_result, _mm256_mul_ps(hidden, _mm256_loadu_ps(&w1[h])));
        }
      } else {
        const __m256 in0 = _mm256_loadu_ps(input_nodes);
        int h_end = 2;
        do {
          const float *in_rem = input_nodes + 8;
          const float *const r4 = r0 + 56;
          const __m256 hadd0 =
              _mm256_hadd_ps(_mm256_mul_ps(in0, _mm256_loadu_ps(&r0[0])),
                             _mm256_mul_ps(in0, _mm256_loadu_ps(&r0[14])));
          const __m256 hadd1 =
              _mm256_hadd_ps(_mm256_mul_ps(in0, _mm256_loadu_ps(&r0[28])),
                             _mm256_mul_ps(in0, _mm256_loadu_ps(&r0[42])));
          const __m256 hadd2 =
              _mm256_hadd_ps(_mm256_mul_ps(in0, _mm256_loadu_ps(&r4[0])),
                             _mm256_mul_ps(in0, _mm256_loadu_ps(&r4[14])));
          const __m256 hadd3 =
              _mm256_hadd_ps(_mm256_mul_ps(in0, _mm256_loadu_ps(&r4[28])),
                             _mm256_mul_ps(in0, _mm256_loadu_ps(&r4[42])));
          const __m256 hh0 = _mm256_hadd_ps(hadd0, hadd1);
          const __m256 hh1 = _mm256_hadd_ps(hadd2, hadd3);
          __m256 hidden = _mm256_add_ps(
              _mm256_add_ps(_mm256_permute2f128_ps(hh0, hh1, 0x20),
                            _mm256_permute2f128_ps(hh0, hh1, 0x31)),
              _mm256_loadu_ps(b0));
          const __m128 z = _mm_setzero_ps();
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 3
#endif
          for (int j = 0; j < 6; j += 2) {
            const __m256 p01 = _mm256_set_m128(
                _mm_loadh_pi(_mm_loadl_pi(z, (const __m64 *)&r4[j + 8]),
                             (const __m64 *)&r4[j + 22]),
                _mm_loadh_pi(_mm_loadl_pi(z, (const __m64 *)&r0[j + 8]),
                             (const __m64 *)&r0[j + 22]));
            const __m256 p23 = _mm256_set_m128(
                _mm_loadh_pi(_mm_loadl_pi(z, (const __m64 *)&r4[j + 36]),
                             (const __m64 *)&r4[j + 50]),
                _mm_loadh_pi(_mm_loadl_pi(z, (const __m64 *)&r0[j + 36]),
                             (const __m64 *)&r0[j + 50]));
            hidden = _mm256_add_ps(
                hidden, _mm256_mul_ps(_mm256_set1_ps(in_rem[j]),
                                      _mm256_shuffle_ps(p01, p23, 0x88)));
            hidden = _mm256_add_ps(
                hidden, _mm256_mul_ps(_mm256_set1_ps(in_rem[j + 1]),
                                      _mm256_shuffle_ps(p01, p23, 0xdd)));
          }
          hidden = _mm256_max_ps(hidden, _mm256_setzero_ps());
          in_result = _mm256_add_ps(in_result,
                                    _mm256_mul_ps(hidden, _mm256_loadu_ps(w1)));
          r0 += 112;
          b0 += 8;
          w1 += 8;
        } while (--h_end);
      }
      const __m128 low_128 = _mm256_castps256_ps128(in_result);
      const __m128 high_128 = _mm256_extractf128_ps(in_result, 1);
      const __m128 sum_par_0 = _mm_add_ps(low_128, high_128);
      const __m128 sum_par_1 =
          _mm_add_ps(sum_par_0, _mm_movehdup_ps(sum_par_0));
      const __m128 sum_tot =
          _mm_add_ss(sum_par_1, _mm_movehl_ps(sum_par_1, sum_par_1));
      float out_val = nn_config->bias[1][0] + _mm_cvtss_f32(sum_tot);
      if (reduce_prec) {
        out_val =
            ((int)(out_val * (float)(1 << 9) + 0.5)) * (float)(1.0 / (1 << 9));
      }
      output[0] = out_val;
      return;
    }
  }
  nn_predict_multi_layer_avx2(input_nodes, nn_config, reduce_prec, output);
}
