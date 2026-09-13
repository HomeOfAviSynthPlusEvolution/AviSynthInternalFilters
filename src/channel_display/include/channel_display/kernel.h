// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
  AIF_CHANNEL_DISPLAY_SSE2 = 1,
  AIF_CHANNEL_DISPLAY_SSSE3 = 2,
  AIF_CHANNEL_DISPLAY_NEON = 4,
  AIF_CHANNEL_DISPLAY_SSE4 = 8,
  AIF_CHANNEL_DISPLAY_AVX2 = 16,
  AIF_CHANNEL_DISPLAY_AVX3 = 32,
  AIF_CHANNEL_DISPLAY_AVX3_DL = 64,
  AIF_CHANNEL_DISPLAY_AVX3_ZEN4 = 128,
  AIF_CHANNEL_DISPLAY_AVX3_SPR = 256,
  AIF_CHANNEL_DISPLAY_AVX10_2 = 512
};
uint32_t aif_channel_display_supported_cpu(void);
/* Positive pitches, disjoint buffers, naturally aligned 8/16-bit samples.
 * sc=1: source is a plane, alpha is optional; sc=3/4: packed BGR(A).
 * dc=1: destination planes (Y or R,G,B plus optional alpha), dc=2: YUY2,
 * dc=3/4: packed BGR(A). YUY2 requires 8 bits and even width.
 * Packed RGB is bottom-up; plane buffers and YUY2 are top-down.
 * Missing alpha becomes opaque. Padding is untouched. */
int aif_channel_display_render(const uint8_t* src, int sp, const uint8_t* alpha, int ap, uint8_t* const dst[4],
                               const int dp[4], int width, int height, int bytes, int sc, int dc, int channel,
                               uint32_t cpu);
#ifdef __cplusplus
}
#endif
