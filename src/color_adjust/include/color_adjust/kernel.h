// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
  AIF_COLOR_ADJUST_SSE2 = 1,
  AIF_COLOR_ADJUST_SSSE3 = 2,
  AIF_COLOR_ADJUST_NEON = 4,
  AIF_COLOR_ADJUST_SSE4 = 8,
  AIF_COLOR_ADJUST_AVX2 = 16,
  AIF_COLOR_ADJUST_AVX3 = 32,
  AIF_COLOR_ADJUST_AVX3_DL = 64,
  AIF_COLOR_ADJUST_AVX3_ZEN4 = 128,
  AIF_COLOR_ADJUST_AVX3_SPR = 256,
  AIF_COLOR_ADJUST_AVX10_2 = 512
};
uint32_t aif_color_adjust_supported_cpu(void);
// Map one channel, in place or between disjoint frames. Positive pitches, natural
// alignment, 8/10/12/14/16-bit samples; step is 1..4 samples. LUT has 2^bits entries.
// Values above the declared bit depth are clamped before indexing (ColorYUV rule).
int aif_color_adjust_map(uint8_t* dst, int dp, const uint8_t* src, int sp, int width, int height, const void* table,
                         int bits, int step, uint32_t cpu);
#ifdef __cplusplus
}
#endif
