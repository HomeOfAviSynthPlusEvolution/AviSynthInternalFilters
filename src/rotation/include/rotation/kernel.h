// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { AIF_ROTATION_LEFT, AIF_ROTATION_RIGHT, AIF_ROTATION_180, AIF_ROTATION_HORIZONTAL, AIF_ROTATION_VERTICAL };
enum {
  AIF_ROTATION_SSE2 = 1,
  AIF_ROTATION_SSSE3 = 2,
  AIF_ROTATION_NEON = 4,
  AIF_ROTATION_SSE4 = 8,
  AIF_ROTATION_AVX2 = 16,
  AIF_ROTATION_AVX3 = 32,
  AIF_ROTATION_AVX3_DL = 64,
  AIF_ROTATION_AVX3_ZEN4 = 128,
  AIF_ROTATION_AVX3_SPR = 256,
  AIF_ROTATION_AVX10_2 = 512
};
// Disjoint positive-pitch planes, natural sample alignment. Pixel bytes: 1/2/3/4/6/8;
// zero denotes packed YUY2 (rows divisible by 4, even height for quarter turns).
// Source/destination storage must cover their active geometry. Only active bytes touched.
int aif_rotation_apply(const uint8_t* src, uint8_t* dst, int rowsize, int height, int src_pitch, int dst_pitch,
                       int pixel_bytes, int operation, uint32_t cpu);
uint32_t aif_rotation_supported_cpu(void);
#ifdef __cplusplus
}
#endif
