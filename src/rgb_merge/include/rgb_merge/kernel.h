// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
  AIF_RGB_MERGE_SSE2 = 1,
  AIF_RGB_MERGE_SSSE3 = 2,
  AIF_RGB_MERGE_NEON = 4,
  AIF_RGB_MERGE_SSE4 = 8,
  AIF_RGB_MERGE_AVX2 = 16,
  AIF_RGB_MERGE_AVX3 = 32,
  AIF_RGB_MERGE_AVX3_DL = 64,
  AIF_RGB_MERGE_AVX3_ZEN4 = 128,
  AIF_RGB_MERGE_AVX3_SPR = 256,
  AIF_RGB_MERGE_AVX10_2 = 512
};
uint32_t aif_rgb_merge_supported_cpu(void);
/* Arrays are R,G,B,A. layout=1 plane, 2 YUY2, 3/4 BGR(A).
 * dc=1 planar RGB(A), 3/4 packed BGR(A). Optional source alpha defaults to zero.
 * Packed RGB is bottom-up, planes/YUY2 top-down. Positive pitches, naturally
 * aligned samples, disjoint output, no padding access. bytes=1/2/4; float only
 * for planar input/output. YUY2 requires 8 bits and even width. */
int aif_rgb_merge_render(const uint8_t* const src[4], const int sp[4], const int layout[4], uint8_t* const dst[4],
                         const int dp[4], int w, int h, int bytes, int dc, uint32_t cpu);
#ifdef __cplusplus
}
#endif
