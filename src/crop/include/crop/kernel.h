// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
  AIF_CROP_SSE2 = 1,
  AIF_CROP_SSSE3 = 2,
  AIF_CROP_NEON = 4,
  AIF_CROP_SSE4 = 8,
  AIF_CROP_AVX2 = 16,
  AIF_CROP_AVX3 = 32,
  AIF_CROP_AVX3_DL = 64,
  AIF_CROP_AVX3_ZEN4 = 128,
  AIF_CROP_AVX3_SPR = 256,
  AIF_CROP_AVX10_2 = 512
};
uint32_t aif_crop_supported_cpu(void);
/* Disjoint positive-pitch image planes, byte dimensions. Pattern is one sample
 * or packed pixel (1,2,3,4,6,8 bytes); row sizes and left are pattern multiples.
 * Source and destination storage must cover their visible rows. Padding is untouched. */
int aif_crop_add_borders(const uint8_t* src, int src_pitch, int src_row, int src_height, uint8_t* dst, int dst_pitch,
                         int dst_row, int dst_height, int left, int top, const void* pattern, int pattern_bytes,
                         uint32_t cpu);
#ifdef __cplusplus
}
#endif
