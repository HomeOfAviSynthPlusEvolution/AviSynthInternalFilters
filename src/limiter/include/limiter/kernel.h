// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
  AIF_LIMITER_SSE2 = 1,
  AIF_LIMITER_SSSE3 = 2,
  AIF_LIMITER_NEON = 4,
  AIF_LIMITER_SSE4 = 8,
  AIF_LIMITER_AVX2 = 16,
  AIF_LIMITER_AVX3 = 32,
  AIF_LIMITER_AVX3_DL = 64,
  AIF_LIMITER_AVX3_ZEN4 = 128,
  AIF_LIMITER_AVX3_SPR = 256,
  AIF_LIMITER_AVX10_2 = 512
};
uint32_t aif_limiter_supported_cpu(void);
/* Limits in sample units, integral for integer depths; finite and ordered. */
typedef struct aif_limiter_limits {
  float min_luma, max_luma, min_chroma, max_chroma;
  int bits;
} aif_limiter_limits;
/* In-place positive naturally aligned pitch, visible samples only. chroma is 0/1;
 * yuy2 alternates luma/chroma and ignores chroma, U8 only, width is the even luma width. */
int aif_limiter_apply(uint8_t* data, int pitch, int width, int height, const aif_limiter_limits* limits, int chroma,
                      int yuy2, uint32_t cpu);
/* Diagnostic layout: 0=444,1=420,2=YUY2. show:1=luma,2=luma_grey,
 * 3=chroma,4=chroma_grey. Y,U,V raw pointers; U/V pitches equal.
 * 420 dimensions even; YUY2 bits=8 and width even. Alpha is not accessed. */
int aif_limiter_show(uint8_t* const data[3], const int pitch[3], int width, int height,
                     const aif_limiter_limits* limits, int layout, int show);
#ifdef __cplusplus
}
#endif
