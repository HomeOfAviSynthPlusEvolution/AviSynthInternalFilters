// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
  AIF_COLOR_BARS_SSE2 = 1,
  AIF_COLOR_BARS_SSSE3 = 2,
  AIF_COLOR_BARS_NEON = 4,
  AIF_COLOR_BARS_SSE4 = 8,
  AIF_COLOR_BARS_AVX2 = 16,
  AIF_COLOR_BARS_AVX3 = 32,
  AIF_COLOR_BARS_AVX3_DL = 64,
  AIF_COLOR_BARS_AVX3_ZEN4 = 128,
  AIF_COLOR_BARS_AVX3_SPR = 256,
  AIF_COLOR_BARS_AVX10_2 = 512
};
enum {
  AIF_COLOR_BARS_YUV444,
  AIF_COLOR_BARS_YUV422,
  AIF_COLOR_BARS_YUV420,
  AIF_COLOR_BARS_YUV411,
  AIF_COLOR_BARS_RGBP,
  AIF_COLOR_BARS_BGR24,
  AIF_COLOR_BARS_BGR32,
  AIF_COLOR_BARS_YUY2
};
uint32_t aif_color_bars_supported_cpu(void);
/* Planes are Y/U/V or R/G/B; packed formats use only plane 0. Optional alpha
 * is plane 3. Positive, naturally aligned pitches; U/V pitches match and planar
 * RGB pitches match. Depth 8/10/12/14/16/32; packed RGB 8/16, YUY2/411 8 only.
 * Packed BGR32/64 pitch is a pixel multiple; YUY2 pitch is a four-byte multiple.
 * HD requires YUV444. Visible rows only, padding untouched. */
int aif_color_bars_draw(uint8_t* const planes[4], const int pitches[4], int width, int height, int bits, int layout,
                        int hd, uint32_t cpu);
#ifdef __cplusplus
}
#endif
