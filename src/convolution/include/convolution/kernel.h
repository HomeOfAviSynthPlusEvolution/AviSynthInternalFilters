// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
  AIF_CONVOLUTION_SSE2 = 1,
  AIF_CONVOLUTION_SSSE3 = 2,
  AIF_CONVOLUTION_NEON = 4,
  AIF_CONVOLUTION_SSE4 = 8,
  AIF_CONVOLUTION_AVX2 = 16,
  AIF_CONVOLUTION_AVX3 = 32,
  AIF_CONVOLUTION_AVX3_DL = 64,
  AIF_CONVOLUTION_AVX3_ZEN4 = 128,
  AIF_CONVOLUTION_AVX3_SPR = 256,
  AIF_CONVOLUTION_AVX10_2 = 512
};
uint32_t aif_convolution_supported_cpu(void);
// Disjoint, naturally aligned planes with positive byte pitches. Matrix dimensions
// 3/5/7/9; bits 8/10/12/14/16/32. Replicate edges, including widths below radius.
// Integer matrix entries are int32_t with 20-bit fixed divisor and integer bias;
// float entries use fdiv/fbias without clipping. Returns nonzero for invalid layout
// or arithmetic outside the signed 64-bit domain. In-place convolution is unsupported.
int aif_convolution_apply(uint8_t* dst, int dp, const uint8_t* src, int sp, int width, int height, const void* matrix,
                          int dim, int bits, int idiv, int ibias, float fdiv, float fbias, uint32_t cpu);
#ifdef __cplusplus
}
#endif
